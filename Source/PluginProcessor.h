/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ZDSVF.h"
#include "OnePoleLP.h"
#include "ZDSK.h"
#include "ZDSKHPF.h"
#include "ZDSKmm.h"
#include "ZDOnePole.h"
#include "ZDML.h"
#include "ZDDL.h"
#include "DCBlocker.h"
#include "DSPMath.h"
#include "FilterConfiguration.h"

//==============================================================================
/**
*/
class ZedAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    ZedAudioProcessor();
    ~ZedAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    zed::FilterConfiguration getFilterConfiguration() const noexcept
    {
        return zed::configurationFromRaw(filterConfigurationParameter->load(std::memory_order_relaxed));
    }

private:
    friend struct ZedLifecycleTestAccess;
    friend struct ZedTopologyTestAccess;
    enum class Engine { none, svf, sallenKeyLP, sallenKeyHP, transistorLadder, diodeLadder };
    static Engine engineFor(zed::FilterConfiguration) noexcept;
    void resetEngine(Engine) noexcept;
    Engine activeEngine = Engine::none;
    bool dspPrepared = false;
    AudioProcessorValueTreeState parameters;

    //---------  Parameters
    std::atomic<float>* inputDriveParameter  = nullptr;
    std::atomic<float>* cutoffParameter  = nullptr;
    std::atomic<float>* resParameter     = nullptr;

    std::atomic<float>* filterConfigurationParameter = nullptr;

    ZDSVF svfL;
    ZDSVF svfR;

    ZDSKmm korgFilterL;
    ZDSKmm korgFilterR;

    ZDML moogLadderL;
    ZDML moogLadderR;

    ZDDL diodeLadderL;
    ZDDL diodeLadderR;

    OnePoleLP smootherCutoff;
    OnePoleLP smootherRes;

    DCBlocker dcblocker1;
    DCBlocker dcblocker2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZedAudioProcessor)
};
