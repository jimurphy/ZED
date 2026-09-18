#include "PluginProcessor.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

juce::RangedAudioParameter* findParameter(ZedAudioProcessor& processor, const char* id)
{
    for (auto* parameter : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            if (ranged->paramID == id)
                return ranged;
    return nullptr;
}

juce::AudioParameterChoice& choice(ZedAudioProcessor& processor)
{
    auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(findParameter(processor, zed::filterConfigurationID));
    require(parameter != nullptr, "Missing AudioParameterChoice");
    return *parameter;
}

void select(ZedAudioProcessor& processor, int index)
{
    auto& parameter = choice(processor);
    parameter.setValueNotifyingHost(parameter.convertTo0to1(static_cast<float>(index)));
}

struct Notifications : juce::AudioProcessorParameter::Listener
{
    std::atomic<int> changes { 0 }, begins { 0 }, ends { 0 }, phase { 0 };
    void parameterValueChanged(int, float) override
    {
        ++changes;
        phase = phase == 1 ? 2 : -1;
    }
    void parameterGestureChanged(int, bool starting) override
    {
        if (starting)
        {
            ++begins;
            phase = phase == 0 ? 1 : -1;
        }
        else
        {
            ++ends;
            phase = phase == 2 ? 3 : -1;
        }
    }
    void reset() { changes = 0; begins = 0; ends = 0; phase = 0; }
};

void checkContractAndMapping()
{
    using Model = zed::FilterModel;
    using Response = zed::FilterResponse;
    ZedAudioProcessor processor;
    auto& parameter = choice(processor);
    const juce::StringArray names { "SVF: LP", "SVF: HP", "SVF: BP", "SVF: BR",
        "Sallen-Key: LP", "Sallen-Key: HP", "Transistor ladder: LP", "Diode ladder: LP" };
    require(parameter.choices == names && static_cast<juce::RangedAudioParameter&>(parameter).getNumSteps() == 8, "Choice contract changed");
    require(processor.getParameters().size() == 4, "Unexpected parameter count");
    require(parameter.getIndex() == 0 && static_cast<juce::RangedAudioParameter&>(parameter).getDefaultValue() == 0.0f,
            "Default must remain SVF: LP");
    for (auto* id : { "svftype", "sktype", "tlftype", "dlftype", "lpfmode", "hpfmode", "bpfmode", "brfmode" })
        require(findParameter(processor, id) == nullptr, "Legacy selector retained");
    const char* ids[] { "drive", "cutoff", "resonance" };
    const float starts[] { 0.5f, 12.0f, 0.01f }, ends[] { 5.0f, 135.0f, 1.1f };
    const float steps[] { 0.01f, 1.0f, 0.01f }, defaults[] { 1.0f, 57.0f, 0.7f };
    for (int i = 0; i < 3; ++i)
    {
        auto* continuous = findParameter(processor, ids[i]);
        require(continuous != nullptr, "Continuous parameter removed");
        const auto& range = continuous->getNormalisableRange();
        require(range.start == starts[i] && range.end == ends[i] && range.interval == steps[i],
                "Continuous range changed");
        require(std::abs(continuous->convertFrom0to1(continuous->getDefaultValue()) - defaults[i]) < 1.0e-6f,
                "Continuous default changed");
    }
    const Model models[] { Model::svf, Model::svf, Model::svf, Model::svf,
        Model::sallenKey, Model::sallenKey, Model::transistorLadder, Model::diodeLadder };
    const Response responses[] { Response::lp, Response::hp, Response::bp, Response::br,
        Response::lp, Response::hp, Response::lp, Response::lp };
    for (int index = 0; index < 8; ++index)
    {
        select(processor, index);
        const auto configuration = processor.getFilterConfiguration();
        const auto selection = zed::selectionFor(configuration);
        require(static_cast<int>(configuration) == index && selection.model == models[index]
                && selection.response == responses[index], "Incorrect configuration mapping");
        for (int model = 0; model < 4; ++model)
            for (int response = 1; response <= 4; ++response)
            {
                const bool available = model == 0 || (model == 1 ? response <= 2 : response == 1);
                require(zed::responseAvailable(static_cast<Model>(model), static_cast<Response>(response)) == available,
                        "Incorrect response availability");
            }
        for (int model = 0; model < 4; ++model)
        {
            const int response = static_cast<int>(responses[index]);
            const bool preserve = model == 0 || (model == 1 ? response <= 2 : response == 1);
            const auto result = zed::selectionFor(zed::selectModel(configuration, static_cast<Model>(model)));
            require(result.model == static_cast<Model>(model)
                    && result.response == (preserve ? responses[index] : Response::lp), "Model click rule failed");
        }
    }
    for (float invalid : { -1.0f, 8.0f, 1.5f, std::numeric_limits<float>::infinity(),
                           -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
        require(zed::configurationFromRaw(invalid) == zed::FilterConfiguration::svfLP, "Unsafe raw-value conversion");
    std::cout << "PASS: parameter contract, default, defensive conversion, model/response rules\n";
}

void prepare(ZedAudioProcessor& processor, int channels)
{
    auto layout = processor.getBusesLayout();
    layout.inputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    layout.outputBuses = layout.inputBuses;
    require(processor.setBusesLayout(layout), "Layout rejected");
    processor.setRateAndBufferSizeDetails(48000.0, 512);
    processor.prepareToPlay(48000.0, 512);
}

void fill(juce::AudioBuffer<float>& buffer, int position)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample(channel, sample, 0.2f * std::sin((channel + 1) * 0.031f * (position + sample)));
}

void checkStateAndSwitching()
{
    for (int index = 0; index < 8; ++index)
    {
        ZedAudioProcessor saved;
        select(saved, index);
        juce::MemoryBlock state;
        saved.getStateInformation(state);
        for (int repetition = 0; repetition < 3; ++repetition)
        {
            ZedAudioProcessor restored, direct;
            select(restored, (index + 1) % 8);
            restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            require(choice(restored).getIndex() == index
                    && static_cast<int>(restored.getFilterConfiguration()) == index, "State round-trip failed");
            restored.getStateInformation(state);
            select(direct, index);
            const int channels = repetition % 2 + 1;
            prepare(restored, channels);
            prepare(direct, channels);
            juce::MidiBuffer midi;
            for (int count : { 0, 1, 64, 257, 512 })
            {
                juce::AudioBuffer<float> a(channels, count), b(channels, count);
                fill(a, count);
                b.makeCopyOf(a);
                restored.processBlock(a, midi);
                direct.processBlock(b, midi);
                for (int channel = 0; channel < channels; ++channel)
                    for (int sample = 0; sample < count; ++sample)
                        require(a.getSample(channel, sample) == b.getSample(channel, sample),
                                "Restored DSP path differs from direct choice");
            }
        }
    }
    for (int channels : { 1, 2 })
    {
        ZedAudioProcessor processor;
        prepare(processor, channels);
        auto& parameter = choice(processor);
        Notifications listener;
        parameter.addListener(&listener);
        juce::MidiBuffer midi;
        // Every ordered pair, including SVF BP/BR to SK/ladders: formerly repair-prone transitions.
        for (int repetition = 0; repetition < 8; ++repetition)
            for (int from = 0; from < 8; ++from)
                for (int to = 0; to < 8; ++to)
                    for (int index : { from, to })
                    {
                        select(processor, index);
                        for (int count : { 0, 1, 17, 128 })
                        {
                            juce::AudioBuffer<float> buffer(channels, count);
                            fill(buffer, repetition + from + to);
                            const float before = static_cast<juce::RangedAudioParameter&>(parameter).getValue();
                            listener.reset();
                            processor.processBlock(buffer, midi);
                            require(static_cast<juce::RangedAudioParameter&>(parameter).getValue() == before && listener.changes == 0
                                    && listener.begins == 0 && listener.ends == 0, "Audio thread notified/mutated parameter");
                            for (int channel = 0; channel < channels; ++channel)
                                for (int sample = 0; sample < count; ++sample)
                                    require(std::isfinite(buffer.getSample(channel, sample)), "Non-finite rapid-switch output");
                        }
                    }
        parameter.removeListener(&listener);
    }
    std::cout << "PASS: all eight state round-trips/DSP paths and rapid switches without parameter mutation\n";
}

juce::TextButton& button(juce::AudioProcessorEditor& editor, const char* label)
{
    for (auto* component : editor.getChildren())
        if (auto* candidate = dynamic_cast<juce::TextButton*>(component))
            if (candidate->getButtonText() == label)
                return *candidate;
    throw std::runtime_error("Missing editor button");
}

bool editorMatches(juce::AudioProcessorEditor& editor, int index)
{
    const char* models[] { "SVF", "SK", "TL", "DL" };
    const char* responses[] { "LP", "HP", "BP", "BR" };
    const int modelIndex[] { 0, 0, 0, 0, 1, 1, 2, 3 };
    const int responseIndex[] { 0, 1, 2, 3, 0, 1, 0, 0 };
    for (int i = 0; i < 4; ++i)
    {
        if (button(editor, models[i]).getToggleState() != (i == modelIndex[index]))
            return false;
        const bool enabled = modelIndex[index] == 0 || (modelIndex[index] == 1 ? i < 2 : i == 0);
        if (button(editor, responses[i]).getToggleState() != (i == responseIndex[index])
            || button(editor, responses[i]).isEnabled() != enabled)
            return false;
    }
    return true;
}

void waitForEditor(juce::AudioProcessorEditor& editor, int index)
{
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        juce::Timer::callPendingTimersSynchronously();
        if (editorMatches(editor, index))
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    require(false, "Editor did not synchronize without processing");
}

void checkEditor()
{
    ZedAudioProcessor processor, other;
    select(other, 7);
    std::unique_ptr<juce::AudioProcessorEditor> otherEditor(other.createEditor());
    for (int repetition = 0; repetition < 3; ++repetition)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        require(editorMatches(*editor, choice(processor).getIndex()), "Initial editor state incorrect");
        for (int index = 0; index < 8; ++index)
        {
            // Simulate a host parameter callback on a non-message thread.
            std::thread automation([&] { select(processor, index); });
            automation.join();
            Notifications listener;
            choice(processor).addListener(&listener);
            waitForEditor(*editor, index);
            require(listener.changes == 0 && listener.begins == 0 && listener.ends == 0,
                    "Parameter-driven refresh fed back to host");
            choice(processor).removeListener(&listener);
            juce::MemoryBlock state;
            processor.getStateInformation(state);
            select(processor, (index + 1) % 8);
            waitForEditor(*editor, (index + 1) % 8);
            std::thread restoration([&] {
                processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            });
            restoration.join();
            waitForEditor(*editor, index);
            require(editorMatches(*otherEditor, 7), "Editor instances interfered");
        }
        // Invoke the actual wired callbacks on the message thread; no screen clicks required.
        const char* models[] { "SVF", "SK", "TL", "DL" };
        for (int index = 0; index < 8; ++index)
            for (int model = 0; model < 4; ++model)
            {
                select(processor, index);
                waitForEditor(*editor, index);
                Notifications listener;
                choice(processor).addListener(&listener);
                button(*editor, models[model]).onClick();
                choice(processor).removeListener(&listener);
                const int expected = static_cast<int>(zed::selectModel(static_cast<zed::FilterConfiguration>(index),
                                                                       static_cast<zed::FilterModel>(model)));
                require(choice(processor).getIndex() == expected && editorMatches(*editor, expected), "Model callback failed");
                require(listener.changes == 1 && listener.begins == 1 && listener.ends == 1 && listener.phase == 3,
                        "Click must notify once inside one gesture");
            }
        const char* responses[] { "LP", "HP", "BP", "BR" };
        for (int index = 0; index < 8; ++index)
            for (int response = 0; response < 4; ++response)
            {
                select(processor, index);
                waitForEditor(*editor, index);
                if (!button(*editor, responses[response]).isEnabled())
                    continue;
                Notifications listener;
                choice(processor).addListener(&listener);
                button(*editor, responses[response]).onClick();
                choice(processor).removeListener(&listener);
                const auto model = zed::selectionFor(static_cast<zed::FilterConfiguration>(index)).model;
                const int expected = static_cast<int>(zed::configurationFor(model, static_cast<zed::FilterResponse>(response + 1)));
                require(choice(processor).getIndex() == expected && editorMatches(*editor, expected), "Response callback failed");
                require(listener.changes == 1 && listener.begins == 1 && listener.ends == 1 && listener.phase == 3,
                        "Response click gesture failed");
            }
        // A stale enabled BP button must not overwrite a newer automated ladder choice.
        select(processor, 2);
        waitForEditor(*editor, 2);
        select(processor, 6);
        Notifications listener;
        choice(processor).addListener(&listener);
        button(*editor, "BP").onClick();
        choice(processor).removeListener(&listener);
        require(choice(processor).getIndex() == 6 && listener.changes == 0, "Stale response overwrote automation");
    }
    std::cout << "PASS: editor automation/restoration, gesture callbacks, stale clicks and repeated/multiple editors\n";
}
}

void runConfigurationTests()
{
    checkContractAndMapping();
    checkStateAndSwitching();
    checkEditor();
}
