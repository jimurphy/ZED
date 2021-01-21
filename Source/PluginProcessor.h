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
#include "ZDOnePole.h"

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

    //atomic to hold filter mode
    std::atomic<int> filtermodeAtom = {0};

private:
    //==============================================================================
    enum FilterTypes{
        SVFMode,
        SKMode,
        TLFMode,
        DLFMode
    };
    
    AudioProcessorValueTreeState parameters;

    //---------  Parameters
    std::atomic<float>* inputDriveParameter  = nullptr;
    std::atomic<float>* cutoffParameter  = nullptr;
    std::atomic<float>* resParameter     = nullptr;
    
    std::atomic<float>* lpfModeParameter = nullptr;
    std::atomic<float>* hpfModeParameter = nullptr;
    std::atomic<float>* bpfModeParameter = nullptr;
    std::atomic<float>* brfModeParameter = nullptr;
    
    std::atomic<float>* svfTypeParameter = nullptr;
    std::atomic<float>* skTypeParameter = nullptr;
    std::atomic<float>* tlfTypeParameter = nullptr; //transistor ladder
    std::atomic<float>* dlfTypeParameter = nullptr; //diode ladder

    ZDSVF filter;
    ZDSK korgFilterLP;
    ZDSKHPF korgFilterHP;
    OnePoleLP smootherCutoff;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZedAudioProcessor)
};
