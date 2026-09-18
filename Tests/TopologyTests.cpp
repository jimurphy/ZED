#include "PluginProcessor.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
void requireTopology(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
}

// Private seam, no counters or runtime instrumentation. Expected output is
// rendered from copies of the actual destination, independently of resetEngine.
struct ZedTopologyTestAccess
{
    using Engine = ZedAudioProcessor::Engine;
    static Engine engine(int choice)
    { return ZedAudioProcessor::engineFor(static_cast<zed::FilterConfiguration>(choice)); }
    static Engine active(const ZedAudioProcessor& p) { return p.activeEngine; }

    template<class Filter>
    static void renderCopy(Filter filter, const juce::AudioBuffer<float>& input,
                           std::vector<float>& output, int channel, bool clear,
                           float inputGain = 1, float outputGain = 1)
    {
        if (clear) filter.reset();
        for (int sample = 0; sample < input.getNumSamples(); ++sample)
            output[sample * input.getNumChannels() + channel] =
                filter.dsp(input.getSample(channel, sample) * inputGain) * outputGain;
    }
    static std::vector<float> expected(const ZedAudioProcessor& p, int choice,
                                       const juce::AudioBuffer<float>& input, bool clear)
    {
        std::vector<float> output(static_cast<size_t>(input.getNumChannels() * input.getNumSamples()));
        for (int channel = 0; channel < input.getNumChannels(); ++channel)
        {
            if (choice < 4)
            {
                auto filter = channel == 0 ? p.svfL : p.svfR;
                const float response[] { 3, 1, 2, 4 };
                filter.setFilterType(response[choice]);
                renderCopy(filter, input, output, channel, clear);
            }
            else if (choice == 4)
                renderCopy(channel == 0 ? p.korgFilterL.lpf : p.korgFilterR.lpf,
                           input, output, channel, clear, 2);
            else if (choice == 5)
                renderCopy(channel == 0 ? p.korgFilterL.hpf : p.korgFilterR.hpf,
                           input, output, channel, clear, 2);
            else if (choice == 6)
                renderCopy(channel == 0 ? p.moogLadderL : p.moogLadderR,
                           input, output, channel, clear, 1, 3.25f);
            else
                renderCopy(channel == 0 ? p.diodeLadderL : p.diodeLadderR,
                           input, output, channel, clear, 1, 10);
        }
        return output;
    }
    static std::vector<float> probe(const ZedAudioProcessor& p, int choice, bool clear)
    {
        juce::AudioBuffer<float> input(2, 32);
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < 32; ++s) input.setSample(c, s, s == 0 ? 0.3f : 0.0f);
        return expected(p, choice, input, clear);
    }
    static void exciteRight(ZedAudioProcessor& p)
    {
        // Exercise reset of the right collection even in a mono processing layout.
        for (int i = 0; i < 97; ++i)
        {
            p.svfR.dsp(0.2f); p.korgFilterR.lpf.dsp(0.2f); p.korgFilterR.hpf.dsp(0.2f);
            p.moogLadderR.dsp(0.2f); p.diodeLadderR.dsp(0.2f);
        }
    }
};

namespace
{
using Access = ZedTopologyTestAccess;
using Engine = Access::Engine;
constexpr std::array<int, 5> representatives { 0, 4, 5, 6, 7 };

void setTopologyParameter(ZedAudioProcessor& p, const char* id, float value)
{
    for (auto* parameter : p.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            if (ranged->paramID == id)
            {
                ranged->setValueNotifyingHost(ranged->convertTo0to1(value));
                return;
            }
    throw std::runtime_error("Missing topology parameter");
}
void select(ZedAudioProcessor& p, int choice)
{ setTopologyParameter(p, zed::filterConfigurationID, static_cast<float>(choice)); }
void prepare(ZedAudioProcessor& p, int channels, double rate = 48000,
             float cutoff = 64, float resonance = 0.5f, float drive = 1)
{
    auto layout = p.getBusesLayout();
    layout.inputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    layout.outputBuses = layout.inputBuses;
    requireTopology(p.setBusesLayout(layout), "Topology layout rejected");
    setTopologyParameter(p, "cutoff", cutoff);
    setTopologyParameter(p, "resonance", resonance);
    setTopologyParameter(p, "drive", drive);
    p.setRateAndBufferSizeDetails(rate, 512);
    p.prepareToPlay(rate, 512);
    requireTopology(Access::active(p) == Engine::none, "Prepare did not invalidate tracking");
}
struct Notifications : juce::AudioProcessorParameter::Listener
{
    int count = 0;
    void parameterValueChanged(int, float) override { ++count; }
    void parameterGestureChanged(int, bool) override { ++count; }
};

// Conservative bounds derived from the existing equations for the tested input
// and fixed-control profiles, not a click threshold or a production limiter.
// SVF: |fasttanh| <= 1.25 (coefficient-wise rational bound), |bp|,|lp|<=1,
// g<3.1, r in [-.2,1.98], D in [.96, 1+3.96*3.1+3.1^2].
// SK at cutoff<=120 and rates>=44100 has g<1: LP gain<=1, HP L1<=2,
// and k>=2*.01*.9/1.1. Ladders are bounded by their existing saturators.
float outputBound(int choice)
{
    if (choice == 0 || choice == 2) return 1.00001f;
    if (choice == 1 || choice == 3)
    {
        const double g = 3.1, dMax = 1 + 3.96*g + g*g;
        const double z1 = ((1.25 + g + 1) * 0.625 + 1) * dMax;
        return static_cast<float>((1.25 + (3.96 + g)*z1 + g + 1)/0.96 + 1.01);
    }
    if (choice == 4) return 2.00001f;
    if (choice == 5) return 2.0f / (2.0f * 0.01f * 0.9f / 1.1f) + 0.01f;
    if (choice == 6) return 3.25f * 1.25f + 0.01f;
    return 10.00001f;
}

juce::AudioBuffer<float> input(int channels, int count, int kind, int position, int active = -2)
{
    juce::AudioBuffer<float> buffer(channels, count);
    for (int sample = 0; sample < count; ++sample)
    {
        const auto n = static_cast<std::uint32_t>(position + sample);
        const auto hash = (n * 1664525u + 1013904223u) ^ ((n + 17u) * 2246822519u);
        const float value = kind == 0 ? 0.25f * std::sin(0.037f * (position + sample))
                          : kind == 1 ? (static_cast<float>(hash >> 8) / 16777215.0f - 0.5f) * 0.5f
                          : kind == 2 ? (position + sample == 0 ? 0.4f : 0.0f) : 0.0f;
        for (int c = 0; c < channels; ++c)
            buffer.setSample(c, sample, active == -2 || active == c ? value : 0.0f);
    }
    return buffer;
}
std::vector<float> process(ZedAudioProcessor& p, juce::AudioBuffer<float>& buffer)
{
    const int choice = static_cast<int>(p.getFilterConfiguration());
    std::vector<float> parameterValues;
    Notifications listener;
    for (auto* param : p.getParameters())
    { parameterValues.push_back(param->getValue()); param->addListener(&listener); }
    juce::MidiBuffer midi;
    p.processBlock(buffer, midi);
    int i = 0;
    for (auto* param : p.getParameters())
    {
        param->removeListener(&listener);
        requireTopology(param->getValue() == parameterValues[i++], "Switching mutated a parameter");
    }
    requireTopology(listener.count == 0, "Switching notified a host parameter");
    std::vector<float> output;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto value = buffer.getSample(channel, sample);
            requireTopology(std::isfinite(value), "Non-finite topology output");
            requireTopology(std::abs(value) <= outputBound(choice), "Topology output exceeded equation-derived bound");
            output.push_back(value);
        }
    return output;
}
void block(ZedAudioProcessor& p, int channels, int count = 512, int kind = 0)
{ auto buffer = input(channels, count, kind, 0); process(p, buffer); }

void checkMappingAndTransitions()
{
    const std::array<Engine, 8> expected { Engine::svf, Engine::svf, Engine::svf, Engine::svf,
        Engine::sallenKeyLP, Engine::sallenKeyHP, Engine::transistorLadder, Engine::diodeLadder };
    for (int choice = 0; choice < 8; ++choice)
        requireTopology(Access::engine(choice) == expected[choice], "Incorrect stateful engine mapping");
    for (int from = 0; from < 8; ++from)
        for (int to = 0; to < 8; ++to)
        {
            for (int channels : { 1, 2 })
                for (int kind = 0; kind < 4; ++kind)
                {
                    ZedAudioProcessor p;
                    prepare(p, channels);
                    // Establish stale destination history before the tested transition.
                    select(p, to); block(p, channels);
                    select(p, from); block(p, channels);
                    Access::exciteRight(p);
                    std::array<std::vector<float>, 5> before, cleared;
                    for (size_t i = 0; i < representatives.size(); ++i)
                    {
                        before[i] = Access::probe(p, representatives[i], false);
                        cleared[i] = Access::probe(p, representatives[i], true);
                    }
                    const bool changing = expected[from] != expected[to];
                    select(p, to);
                    block(p, channels, 0); // Transition must happen once even with no audio.
                    requireTopology(Access::active(p) == expected[to], "Empty block did not commit engine");
                    for (size_t i = 0; i < representatives.size(); ++i)
                    {
                        const bool destination = expected[representatives[i]] == expected[to];
                        if (destination) requireTopology(before[i] != cleared[i], "Reset witness had no excited history");
                        requireTopology(Access::probe(p, representatives[i], false)
                                            == (changing && destination ? cleared[i] : before[i]),
                                        "Wrong engine history reset/preserved");
                    }
                    auto buffer = input(channels, 64, kind, 0);
                    const auto freshEquivalent = Access::expected(p, to, buffer, false);
                    requireTopology(process(p, buffer) == freshEquivalent,
                                    "Destination output differs from equivalent engine state");
                    // Non-zero history must survive a repeated empty block and repeated config.
                    block(p, channels);
                    const auto running = Access::probe(p, to, false);
                    requireTopology(running != Access::probe(p, to, true), "Repeated-block witness is silent");
                    block(p, channels, 0);
                    requireTopology(Access::probe(p, to, false) == running, "Repeated block reset active engine");
                }
            std::cout << "PASS: topology transition " << from << " -> " << to
                      << " (mono/stereo; sine/noise/impulse/silence; history oracle)\n";
        }
}

void checkStressAndIndependence()
{
    struct Profile { float cutoff, resonance, drive; };
    const Profile profiles[] { {64,0.5f,1}, {64,1.1f,1}, {64,0.5f,5},
                               {12,1.1f,5}, {120,1.1f,5} };
    const int sizes[] { 0, 1, 7, 64, 257, 3, 0, 128 };
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (const auto profile : profiles)
        {
            ZedAudioProcessor mono, left, right, silent;
            prepare(mono, 1, rate, profile.cutoff, profile.resonance, profile.drive);
            prepare(left, 2, rate, profile.cutoff, profile.resonance, profile.drive);
            prepare(right, 2, rate, profile.cutoff, profile.resonance, profile.drive);
            prepare(silent, 1, rate, profile.cutoff, profile.resonance, profile.drive);
            float peak = 0;
            int position = 0;
            for (int iteration = 0; iteration < 2048; ++iteration)
            {
                // Includes all pairs, returns to stale engines and same-engine SVF changes.
                const int choice = iteration % 2 == 0 ? (iteration / 16) % 8 : (iteration / 2) % 8;
                for (auto* p : { &mono, &left, &right, &silent }) select(*p, choice);
                const int count = sizes[iteration % 8], kind = (iteration / 64) % 4;
                auto m = input(1, count, kind, position), l = input(2, count, kind, position, 0);
                auto r = input(2, count, kind, position, 1), s = input(1, count, 3, position);
                const auto mo = process(mono, m), lo = process(left, l), ro = process(right, r), so = process(silent, s);
                for (size_t i = 0; i < mo.size(); ++i)
                {
                    requireTopology(mo[i] == lo[2*i] && mo[i] == ro[2*i+1]
                                    && so[i] == lo[2*i+1] && so[i] == ro[2*i], "Switching broke channel independence");
                    peak = std::max(peak, std::abs(mo[i]));
                }
                position = kind == 2 ? 0 : position + count;
            }
            requireTopology(peak > 1.0e-8f, "Unexpectedly silent switching stress");
            std::cout << "PASS: stress " << rate << " Hz, cutoff=" << profile.cutoff
                      << ", resonance=" << profile.resonance << ", drive=" << profile.drive
                      << ", 2048 blocks/instance, peak=" << peak << '\n';
        }
}

void checkLifecycle()
{
    for (int channels : { 1, 2 })
        for (int choice = 0; choice < 8; ++choice)
        {
            ZedAudioProcessor p;
            requireTopology(Access::active(p) == Engine::none, "Constructor tracking not empty");
            prepare(p, channels);
            for (int action = 0; action < 5; ++action)
            {
                for (int c = 0; c < 8; ++c) { select(p, c); block(p, channels); }
                select(p, choice);
                if (action == 0) p.reset();
                if (action == 1) prepare(p, channels, 96000);
                if (action == 2) { p.releaseResources(); requireTopology(Access::active(p) == Engine::none,
                    "Release retained tracking"); prepare(p, channels); }
                if (action == 3) prepare(p, channels);
                if (action == 4)
                {
                    juce::MemoryBlock state;
                    p.getStateInformation(state);
                    select(p, choice == 7 ? 0 : 7); block(p, channels);
                    const auto prior = Access::active(p);
                    p.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
                    requireTopology(Access::active(p) == prior, "Restoration wrote audio-owned tracking");
                }
                else requireTopology(Access::active(p) == Engine::none, "Lifecycle retained tracking");
                auto buffer = input(channels, 64, 0, 0);
                const auto expected = Access::expected(p, choice, buffer, true);
                requireTopology(process(p, buffer) == expected, "Lifecycle/restoration activation reused stale state");
                requireTopology(Access::active(p) == Access::engine(choice), "First block tracking incorrect");
            }
        }
    std::cout << "PASS: switching after reset/reprepare/rate change/release/state restore\n";
}
}

void runTopologyTests()
{
    checkMappingAndTransitions();
    checkStressAndIndependence();
    checkLifecycle();
}
