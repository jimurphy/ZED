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
#include "CustomTooltip.h"

//==============================================================================
/**
*/

struct SnappingSlider  : public Slider
{
    float minVal = 0.6f;
    float maxVal = 1.4f;

    void setMinMax(float min, float max){
        minVal = min;
        maxVal = max;
    }

    double snapValue (double attemptedValue, DragMode dragMode) override
    {
        if (dragMode == notDragging)
            return attemptedValue;  // if they're entering the value in the text-box, don't mess with it.

        if (attemptedValue > minVal && attemptedValue < maxVal)
            return((minVal+maxVal)/2.0f);

        return attemptedValue;
    }
};

class ZedAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                 private Timer

{
public:
    typedef AudioProcessorValueTreeState::SliderAttachment SliderAttachment;

    ZedAudioProcessorEditor (ZedAudioProcessor&, AudioProcessorValueTreeState&);
    ~ZedAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:

    enum RadioButtonIds
    {
        FilterModeButtons = 1001,
        FilterTypeButtons = 1002
    };

    void timerCallback() override;
    void refreshConfiguration();
    void chooseConfiguration(zed::FilterConfiguration);

    //Single instance of tooltip window
    SharedResourcePointer<CustomTooltip> tooltip_window;

    ZedAudioProcessor& audioProcessor;
    AudioProcessorValueTreeState& valueTreeState;

    ZedLookAndFeel zedLookAndFeel;
    FilterSpline filterWindow{valueTreeState};

    //COLOURS
    Colour backgroundColour = juce::Colour(0xFF684A52);
    Colour sliderColour     = juce::Colour(0xFF87A0B2);
    Colour buttonOffColour  = juce::Colour(0xFF756E7A);
    Colour buttonOnColour   = juce::Colour(0xFF808D9C);
    Colour logoColour       = juce::Colour(0xFFA4BEF3);

    //SLIDERS
    Slider freqSlider;
    Slider resSlider;
    SnappingSlider driveSlider;

    //LABELS
    Label freqLabel;
    Label resLabel;
    Label driveLabel;

    //BUTTONS
    TextButton lpfButton;
    TextButton hpfButton;
    TextButton bpfButton;
    TextButton brfButton;

    TextButton svfButton;
    TextButton skButton;
    TextButton tlfButton;
    TextButton dlfButton;

    //Attachments
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> freqAttachment;
    std::unique_ptr<SliderAttachment> resAttachment;

    juce::RangedAudioParameter& configurationParameter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZedAudioProcessorEditor)
};
