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
#include "DSPMath.h"

//==============================================================================
/**
*/
class ZedAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                 private Timer

{
public:
    typedef AudioProcessorValueTreeState::SliderAttachment SliderAttachment;
    typedef AudioProcessorValueTreeState::ButtonAttachment ButtonAttachment;

    ZedAudioProcessorEditor (ZedAudioProcessor&, AudioProcessorValueTreeState&);
    ~ZedAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    
    enum RadioButtonIds
    {
        FilterModeButtons = 1001
    };
    
    void timerCallback() override;
    
    ZedAudioProcessor& audioProcessor;
    AudioProcessorValueTreeState& valueTreeState;

    ZedLookAndFeel zedLookAndFeel;
    FilterSpline filterWindow{valueTreeState};
    
    //COLOURS
    Colour backgroundColour = juce::Colour(0xFF684A52);
    Colour sliderColour = juce::Colour(0xFF87A0B2);
    Colour buttonOffColour = juce::Colour(0xFF756E7A);
    Colour buttonOnColour = juce::Colour(0xFF808D9C);

    //SLIDERS
    Slider freqSlider;
    Slider resSlider;
    Slider driveSlider;
    
    //LABELS
    Label freqLabel;
    Label resLabel;

    //BUTTONS
    TextButton lpfButton;
    TextButton hpfButton;
    TextButton bpfButton;
    TextButton brfButton;

    
    //Attachments
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> freqAttachment;
    std::unique_ptr<SliderAttachment> resAttachment;
    std::unique_ptr<ButtonAttachment> lpfModeAttachment;
    std::unique_ptr<ButtonAttachment> hpfModeAttachment;
    std::unique_ptr<ButtonAttachment> bpfModeAttachment;
    std::unique_ptr<ButtonAttachment> brfModeAttachment;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZedAudioProcessorEditor)
};
