/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ZedAudioProcessorEditor::ZedAudioProcessorEditor (ZedAudioProcessor& p, AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p), audioProcessor (p), valueTreeState (vts)
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
    freqSlider.onValueChange = [this] {
        filterWindow.setPitch(freqSlider.getValue());
        std::string freqLabelString = std::to_string((int)p2f(freqSlider.getValue())).substr(0,4) + " HZ";
        freqLabel.setText(freqLabelString, dontSendNotification);
    };
    freqAttachment.reset (new SliderAttachment (valueTreeState, "cutoff", freqSlider));

    resSlider.setSliderStyle (Slider::LinearVertical);
    resSlider.setTextBoxStyle (Slider::NoTextBox, false, 100, 0);
    resSlider.setRange (0.0f, 1.1f);
    resSlider.setPopupDisplayEnabled (false, false, this);
    resSlider.setTextValueSuffix (" S");
    resSlider.setColour(juce::Slider::trackColourId, sliderColour);
    resSlider.setValue(0.7f);
    addAndMakeVisible(&resSlider);
    resSlider.onValueChange = [this] {
        filterWindow.setRes(resSlider.getValue());
        std::string resLabelString = "RES: " + std::to_string(resSlider.getValue()).substr(0,4);
        resLabel.setText(resLabelString, dontSendNotification);
    };
    resAttachment.reset (new SliderAttachment (valueTreeState, "resonance", resSlider));

    //buttons
    addAndMakeVisible(lpfButton);
    lpfButton.setRadioGroupId(FilterModeButtons);
    lpfButton.setClickingTogglesState(true);
    lpfButton.setButtonText("LP");
    lpfButton.setColour(TextButton::buttonColourId, Colour(buttonOffColour));
    lpfButton.setColour(TextButton::buttonOnColourId, Colour(buttonOnColour));
    lpfModeAttachment.reset(new ButtonAttachment(valueTreeState, "lpfmode", lpfButton));

    
    addAndMakeVisible(hpfButton);
    hpfButton.setRadioGroupId(FilterModeButtons);
    hpfButton.setClickingTogglesState(true);
    hpfButton.setButtonText("HP");
    hpfButton.setColour(TextButton::buttonColourId, Colour(buttonOffColour));
    hpfButton.setColour(TextButton::buttonOnColourId, Colour(buttonOnColour));
    hpfModeAttachment.reset(new ButtonAttachment(valueTreeState, "hpfmode", hpfButton));

    addAndMakeVisible(bpfButton);
    bpfButton.setRadioGroupId(FilterModeButtons);
    bpfButton.setClickingTogglesState(true);
    bpfButton.setButtonText("BP");
    bpfButton.setColour(TextButton::buttonColourId, Colour(buttonOffColour));
    bpfButton.setColour(TextButton::buttonOnColourId, Colour(buttonOnColour));
    bpfModeAttachment.reset(new ButtonAttachment(valueTreeState, "bpfmode", bpfButton));

    addAndMakeVisible(brfButton);
    brfButton.setRadioGroupId(FilterModeButtons);
    brfButton.setClickingTogglesState(true);
    brfButton.setButtonText("BR");
    brfButton.setColour(TextButton::buttonColourId, Colour(buttonOffColour));
    brfButton.setColour(TextButton::buttonOnColourId, Colour(buttonOnColour));
    brfModeAttachment.reset(new ButtonAttachment(valueTreeState, "brfmode", brfButton));

    //labels
    auto labelFont = Font(10.0);

    addAndMakeVisible(freqLabel);
    freqLabel.setFont(labelFont);
    freqLabel.setBorderSize(BorderSize< int >(0));
    freqLabel.setText("220.0 HZ", dontSendNotification);
    freqLabel.setColour(Label::textColourId, backgroundColour);
    freqLabel.setJustificationType(Justification::left);
    
    addAndMakeVisible(resLabel);
    resLabel.setFont(labelFont);
    resLabel.setBorderSize(BorderSize< int >(0));
    resLabel.setText("RES: 0.1", dontSendNotification);
    resLabel.setColour(Label::textColourId, backgroundColour);
    resLabel.setJustificationType(Justification::left);
    
    startTimerHz(60);
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
    
    lpfButton.setBounds(10, 20, 31, 20);
    hpfButton.setBounds(10, 50, 31, 20);
    bpfButton.setBounds(10, 80, 31, 20);
    brfButton.setBounds(10, 110, 31, 20);

    freqLabel.setBounds(225, 23, 50, 10);
    resLabel.setBounds(225, 33, 50, 10);
}

void ZedAudioProcessorEditor::timerCallback()
{
    int filterMode = audioProcessor.filtermodeAtom.load();
    filterWindow.setMode(filterMode);
    repaint();
}
