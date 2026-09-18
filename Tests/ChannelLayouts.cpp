#include "PluginProcessor.h"
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

void runConfigurationTests();
void runLifecycleTests();
void runTopologyTests();

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

juce::AudioProcessor::BusesLayout layout(juce::AudioChannelSet input,
                                        juce::AudioChannelSet output)
{
    juce::AudioProcessor::BusesLayout result;
    result.inputBuses.add(input);
    result.outputBuses.add(output);
    return result;
}

void setParameter(ZedAudioProcessor& processor, const char* id, float value)
{
    for (auto* parameter : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            if (ranged->paramID == id)
            {
                ranged->setValueNotifyingHost(ranged->convertTo0to1(value));
                return;
            }
    throw std::runtime_error("Missing parameter");
}

void checkLayouts()
{
    ZedAudioProcessor processor;
    using Set = juce::AudioChannelSet;
    const std::vector<Set> sets { Set::disabled(), Set::mono(), Set::stereo(),
                                Set::createLCR(), Set::quadraphonic(), Set::create5point1() };
    for (const auto& input : sets)
        for (const auto& output : sets)
        {
            const bool expected = input == output && (input == Set::mono() || input == Set::stereo());
            require(processor.isBusesLayoutSupported(layout(input, output)) == expected,
                    "Incorrect layout support decision");
            require(processor.setBusesLayout(layout(input, output)) == expected,
                    "Incorrect layout configuration decision");
        }
    std::cout << "PASS: all 36 layout combinations\n";
}

// Each render uses fresh processor state. No second channel exists in mono buffers.
std::vector<float> render(int channels, int topology, int mode, int activeChannel, double sampleRate = 48000.0)
{
    ZedAudioProcessor processor;
    const auto set = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    require(processor.setBusesLayout(layout(set, set)), "Layout rejected");
    processor.setRateAndBufferSizeDetails(sampleRate, 512);
    const auto configuration = zed::configurationFor(static_cast<zed::FilterModel>(topology),
                                                       static_cast<zed::FilterResponse>(mode + 1));
    setParameter(processor, zed::filterConfigurationID, static_cast<float>(configuration));
    processor.prepareToPlay(sampleRate, 512);
    juce::MidiBuffer midi;
    std::vector<float> output;
    int position = 0;
    double energy = 0.0;
    // Includes empty blocks before/after audio and variable sizes up to the prepared maximum.
    for (int repeat = 0; repeat < 4; ++repeat)
        for (int count : { 0, 1, 17, 64, 257, 512, 3, 0 })
        {
            juce::AudioBuffer<float> buffer(channels, count);
            buffer.clear();
            if (activeChannel >= 0)
                for (int sample = 0; sample < count; ++sample)
                    buffer.setSample(activeChannel, sample,
                        0.2f * std::sin(0.031f * static_cast<float>(position + sample))
                        + (position + sample == 0 ? 0.5f : 0.0f));
            processor.processBlock(buffer, midi);
            require(processor.getFilterConfiguration() == configuration, "Processing mutated configuration");
            require(buffer.getNumChannels() == channels, "Buffer channel count changed");
            for (int sample = 0; sample < count; ++sample)
                for (int channel = 0; channel < channels; ++channel)
                {
                    const float value = buffer.getSample(channel, sample);
                    require(std::isfinite(value), "Non-finite output");
                    if (channel == activeChannel)
                        energy += static_cast<double>(value) * value;
                    output.push_back(value);
                }
            position += count;
        }
    if (activeChannel >= 0)
        require(energy > 1.0e-10, "Unexpectedly silent output");
    processor.releaseResources();
    return output;
}
}

int main(int argc, char** argv)
{
    try
    {
        juce::ScopedJuceInitialiser_GUI initialiseJuce;
        const bool dumpStereo = argc == 3 && juce::String(argv[1]) == "--dump-stereo";
        const bool dump44100 = argc == 3 && juce::String(argv[1]) == "--dump-44100";
        const bool dumpConfigurations = dump44100 || (argc == 3 && juce::String(argv[1]) == "--dump-configurations");
        const double sampleRate = dump44100 ? 44100.0 : 48000.0;
        require(argc == 1 || dumpStereo || dumpConfigurations,
                "Usage: ZEDChannelTests [--dump-stereo file | --dump-configurations directory | --dump-44100 directory]");
        if (dumpConfigurations)
            std::filesystem::create_directories(argv[2]);
        std::ofstream dump;
        if (dumpStereo)
        {
            dump.open(argv[2], std::ios::binary);
            require(dump.good(), "Cannot open stereo output file");
        }
        checkLayouts();
        if (!dumpStereo && !dumpConfigurations)
        {
            runConfigurationTests();
            runLifecycleTests();
            runTopologyTests();
        }
        for (int topology = 0; topology < 4; ++topology)
            for (int mode = 0; mode < (topology == 0 ? 4 : topology == 1 ? 2 : 1); ++mode)
            {
                const auto left = render(2, topology, mode, 0, sampleRate);
                const auto right = render(2, topology, mode, 1, sampleRate);
                if (dumpConfigurations)
                {
                    const auto mono = render(1, topology, mode, 0, sampleRate);
                    std::ofstream file(std::filesystem::path(argv[2]) /
                                       (std::to_string(topology) + "-" + std::to_string(mode) + ".bin"),
                                       std::ios::binary);
                    for (const auto* data : { &mono, &left, &right })
                        file.write(reinterpret_cast<const char*>(data->data()),
                                   static_cast<std::streamsize>(data->size() * sizeof(float)));
                    require(file.good(), "Configuration output write failed");
                }
                else if (dumpStereo)
                {
                    for (const auto* data : { &left, &right })
                        dump.write(reinterpret_cast<const char*>(data->data()),
                                   static_cast<std::streamsize>(data->size() * sizeof(float)));
                }
                else
                {
                    const auto mono = render(1, topology, mode, 0, sampleRate);
                    const auto silence = render(1, topology, mode, -1, sampleRate);
                    require(left.size() == mono.size() * 2, "Output length mismatch");
                    for (size_t i = 0; i < mono.size(); ++i)
                    {
                        require(mono[i] == left[i * 2], "Mono differs from stereo left");
                        require(mono[i] == right[i * 2 + 1], "Independent right differs from mono");
                        require(silence[i] == left[i * 2 + 1], "Left input affected right state");
                        require(silence[i] == right[i * 2], "Right input affected left state");
                    }
                }
                std::cout << "PASS: topology " << topology << ", mode " << mode << '\n';
            }
        if (dumpStereo)
            require(dump.good(), "Stereo output write failed");
        std::cout << "PASS: channel regression tests\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
