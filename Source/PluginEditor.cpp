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
    freqSlider.setRange (0.0f, 127.0f);
    freqSlider.setPopupDisplayEnabled (false, false, this);
    freqSlider.setTextValueSuffix (" S");
    freqSlider.setColour(juce::Slider::trackColourId, sliderColour);
    freqSlider.setValue(64.0f);
    addAndMakeVisible(&freqSlider);
    freqSlider.onValueChange = [this] { filterWindow.setPitch(freqSlider.getValue());
        freqLabel.setText(std::to_string(p2f(freqSlider.getValue())), dontSendNotification);
    };

    resSlider.setSliderStyle (Slider::LinearVertical);
    resSlider.setTextBoxStyle (Slider::NoTextBox, false, 100, 0);
    resSlider.setRange (0.0f, 1.1f);
    resSlider.setPopupDisplayEnabled (false, false, this);
    resSlider.setTextValueSuffix (" S");
    resSlider.setColour(juce::Slider::trackColourId, sliderColour);
    resSlider.setValue(0.7f);
    addAndMakeVisible(&resSlider);
    resSlider.onValueChange = [this] { filterWindow.setRes(resSlider.getValue());};
    
    //labels
    auto labelFont = Font(10.0);

    addAndMakeVisible(freqLabel);
    freqLabel.setFont(labelFont);
    freqLabel.setBorderSize(BorderSize< int >(0));
    freqLabel.setText("220.0 HZ", dontSendNotification);
    freqLabel.setColour(Label::textColourId, backgroundColour);
    freqLabel.setJustificationType(Justification::centredTop);
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
    resSlider.setBounds(280, 12, 20, 125);
    
    freqLabel.setBounds(222, 23, 50, 50);
}
