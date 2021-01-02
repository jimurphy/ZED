/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ZedAudioProcessorEditor::ZedAudioProcessorEditor (ZedAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel(&zedLookAndFeel);
    
    addAndMakeVisible(filterWindow);
    
    setSize (310, 200);
    
    freqSlider.setSliderStyle (Slider::LinearHorizontal);
    freqSlider.setTextBoxStyle (Slider::NoTextBox, false, 100, 0);
    freqSlider.setPopupDisplayEnabled (true, false, this);
    freqSlider.setTextValueSuffix (" S");
    freqSlider.setColour(juce::Slider::trackColourId, sliderColour);
    freqSlider.setValue(0.0f);
    addAndMakeVisible(&freqSlider);
    
    resSlider.setSliderStyle (Slider::LinearVertical);
    resSlider.setTextBoxStyle (Slider::NoTextBox, false, 100, 0);
    resSlider.setPopupDisplayEnabled (true, false, this);
    resSlider.setTextValueSuffix (" S");
    resSlider.setColour(juce::Slider::trackColourId, sliderColour);
    resSlider.setValue(0.0f);
    addAndMakeVisible(&resSlider);
}

ZedAudioProcessorEditor::~ZedAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//==============================================================================
void ZedAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(backgroundColour);

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("ZED", getLocalBounds(), juce::Justification::centred, 1);
}

void ZedAudioProcessorEditor::resized()
{
    filterWindow.setBounds(51, 20, 219, 110);
    freqSlider.setBounds(42, 140, 237, 20);
    resSlider.setBounds(289, 20, 20, 110);

}
