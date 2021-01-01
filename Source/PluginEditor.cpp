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
}
