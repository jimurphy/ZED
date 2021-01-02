/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ZedLookAndFeel.h"
#include "FilterSpline.h"

//==============================================================================
/**
*/
class ZedAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    ZedAudioProcessorEditor (ZedAudioProcessor&);
    ~ZedAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    ZedAudioProcessor& audioProcessor;
    ZedLookAndFeel zedLookAndFeel;
    FilterSpline filterWindow;
    
    //COLOURS
    Colour backgroundColour = juce::Colour(0xFF3E4E50);
    Colour sliderColour = juce::Colour(0xFFFACFAD);

    //SLIDERS
    Slider freqSlider;
    Slider resSlider;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZedAudioProcessorEditor)
};
