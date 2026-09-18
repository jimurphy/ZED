/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ZedAudioProcessorEditor::ZedAudioProcessorEditor (ZedAudioProcessor& p, AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p), audioProcessor (p), valueTreeState (vts),
      configurationParameter (*vts.getParameter(zed::filterConfigurationID))
{
    setLookAndFeel(&zedLookAndFeel);

    addAndMakeVisible(filterWindow);

    setSize (310, 200);

    tooltip_window->setMillisecondsBeforeTipAppears(250);
    tooltip_window->setLookAndFeel(&zedLookAndFeel);

    tooltip_window->setColour(TooltipWindow::backgroundColourId, juce::Colours::green);

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
        std::string freqLabelString = std::to_string((int)p2f(freqSlider.getValue())).substr(0,5) + " HZ";
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

    driveSlider.setSliderStyle (Slider::LinearVertical);
    driveSlider.setTextBoxStyle (Slider::NoTextBox, false, 100, 0);
    driveSlider.setRange (1.0f, 10.0f);
    driveSlider.setPopupDisplayEnabled (false, false, this);
    driveSlider.setTextValueSuffix ("x");
    driveSlider.setColour(juce::Slider::trackColourId, sliderColour);
    driveSlider.setValue(1.0f);
    addAndMakeVisible(&driveSlider);
    driveSlider.onValueChange = [this] {
        std::string driveLabelString = "DRIVE: " + std::to_string(driveSlider.getValue()).substr(0,3);
        driveLabel.setText(driveLabelString, dontSendNotification);
    };
    driveAttachment.reset (new SliderAttachment (valueTreeState, "drive", driveSlider));

    //buttons
    addAndMakeVisible(lpfButton);
    lpfButton.setRadioGroupId(FilterModeButtons);
    lpfButton.setClickingTogglesState(false);
    lpfButton.setButtonText("LP");
    lpfButton.setColour(TextButton::buttonColourId, Colour(buttonOffColour));
    lpfButton.setColour(TextButton::buttonOnColourId, Colour(buttonOnColour));

    addAndMakeVisible(hpfButton);
    hpfButton.setRadioGroupId(FilterModeButtons);
    hpfButton.setClickingTogglesState(false);
    hpfButton.setButtonText("HP");
    hpfButton.setColour(TextButton::buttonColourId, Colour(buttonOffColour));
    hpfButton.setColour(TextButton::buttonOnColourId, Colour(buttonOnColour));

    addAndMakeVisible(bpfButton);
    bpfButton.setRadioGroupId(FilterModeButtons);
    bpfButton.setClickingTogglesState(false);
    bpfButton.setButtonText("BP");
    bpfButton.setColour(TextButton::buttonColourId, Colour(buttonOffColour));
    bpfButton.setColour(TextButton::buttonOnColourId, Colour(buttonOnColour));

    addAndMakeVisible(brfButton);
    brfButton.setRadioGroupId(FilterModeButtons);
    brfButton.setClickingTogglesState(false);
    brfButton.setButtonText("BR");
    brfButton.setColour(TextButton::buttonColourId, Colour(buttonOffColour));
    brfButton.setColour(TextButton::buttonOnColourId, Colour(buttonOnColour));

    addAndMakeVisible(svfButton);
    svfButton.setRadioGroupId(FilterTypeButtons);
    svfButton.setClickingTogglesState(false);
    svfButton.setButtonText("SVF");
    svfButton.setTooltip("STATE VARIABLE FILTER");
    svfButton.setColour(TextButton::buttonColourId, Colour(buttonOffColour));
    svfButton.setColour(TextButton::buttonOnColourId, Colour(buttonOnColour));

    addAndMakeVisible(skButton);
    skButton.setRadioGroupId(FilterTypeButtons);
    skButton.setClickingTogglesState(false);
    skButton.setButtonText("SK");
    skButton.setTooltip("SALLEN-KEY FILTER");
    skButton.setColour(TextButton::buttonColourId, Colour(buttonOffColour));
    skButton.setColour(TextButton::buttonOnColourId, Colour(buttonOnColour));

    addAndMakeVisible(tlfButton);
    tlfButton.setRadioGroupId(FilterTypeButtons);
    tlfButton.setClickingTogglesState(false);
    tlfButton.setButtonText("TL");
    tlfButton.setTooltip("TRANSISTOR LADDER FILTER");
    tlfButton.setColour(TextButton::buttonColourId, Colour(buttonOffColour));
    tlfButton.setColour(TextButton::buttonOnColourId, Colour(buttonOnColour));

    addAndMakeVisible(dlfButton);
    dlfButton.setRadioGroupId(FilterTypeButtons);
    dlfButton.setClickingTogglesState(false);
    dlfButton.setButtonText("DL");
    dlfButton.setTooltip("DIODE LADDER FILTER");
    dlfButton.setColour(TextButton::buttonColourId, Colour(buttonOffColour));
    dlfButton.setColour(TextButton::buttonOnColourId, Colour(buttonOnColour));

    // A click changes only the combined parameter. Radio state is projected back
    // with no notifications, avoiding callbacks from deselected radio buttons.
    const std::array<TextButton*, 4> models { &svfButton, &skButton, &tlfButton, &dlfButton };
    const std::array<zed::FilterModel, 4> modelValues {
        zed::FilterModel::svf, zed::FilterModel::sallenKey,
        zed::FilterModel::transistorLadder, zed::FilterModel::diodeLadder
    };
    for (size_t i = 0; i < models.size(); ++i)
        models[i]->onClick = [this, model = modelValues[i]] {
            chooseConfiguration(zed::selectModel(audioProcessor.getFilterConfiguration(), model));
        };

    const std::array<TextButton*, 4> responses { &lpfButton, &hpfButton, &bpfButton, &brfButton };
    const std::array<zed::FilterResponse, 4> responseValues {
        zed::FilterResponse::lp, zed::FilterResponse::hp,
        zed::FilterResponse::bp, zed::FilterResponse::br
    };
    for (size_t i = 0; i < responses.size(); ++i)
        responses[i]->onClick = [this, response = responseValues[i]] {
            const auto model = zed::selectionFor(audioProcessor.getFilterConfiguration()).model;
            // Recheck the latest value: automation may have changed the model
            // since the last message-thread refresh enabled this button.
            if (zed::responseAvailable(model, response))
                chooseConfiguration(zed::configurationFor(model, response));
            else
                refreshConfiguration();
        };
    refreshConfiguration();

    //labels
    auto labelFont = Font(10.0);

    freqLabel.setFont(labelFont);
    freqLabel.setBorderSize(BorderSize< int >(0));
    //freqLabel.setText("220.0 HZ", dontSendNotification);
    freqLabel.setColour(Label::textColourId, backgroundColour);
    freqLabel.setJustificationType(Justification::left);
    addAndMakeVisible(freqLabel);

    resLabel.setFont(labelFont);
    resLabel.setBorderSize(BorderSize< int >(0));
    //resLabel.setText("RES: 0.1", dontSendNotification);
    resLabel.setColour(Label::textColourId, backgroundColour);
    resLabel.setJustificationType(Justification::left);
    addAndMakeVisible(resLabel);

    driveLabel.setFont(labelFont);
    driveLabel.setBorderSize(BorderSize< int >(0));
    driveLabel.setText("DRIVE: 1.0", dontSendNotification);
    driveLabel.setColour(Label::textColourId, backgroundColour);
    driveLabel.setJustificationType(Justification::left);
    addAndMakeVisible(driveLabel);

    startTimerHz(60);
}

ZedAudioProcessorEditor::~ZedAudioProcessorEditor()
{
    stopTimer();
    tooltip_window->setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

//==============================================================================
void ZedAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(backgroundColour);

    g.setColour(sliderColour);
    g.setFont (11.0f);
    g.drawFittedText ("INPUT", 12, 174, 100, 100, 9, 1.0f);
    g.drawFittedText ("DRIVE", 12, 184, 100, 100, 9, 1.0f);

    g.setColour(sliderColour);
    g.setFont (16.0f);
    g.drawFittedText ("ZED", 213, 171, 100, 100, 9, 1.0f);
    g.setFont (9.0f);
    g.drawFittedText ("SOUTH COAST", 235, 163, 100, 100, 9, 1.0f);
    g.drawFittedText ("SYNTHESIS", 246, 170, 100, 100, 9, 1.0f);
}

void ZedAudioProcessorEditor::resized()
{
    filterWindow.setBounds(51, 20, 219, 110);

    freqSlider.setBounds(42, 140, 237, 20);
    resSlider.setBounds(280, 12, 20, 125);
    driveSlider.setBounds(15, 132, 20, 50);

    lpfButton.setBounds(10, 20, 31, 20);
    hpfButton.setBounds(10, 50, 31, 20);
    bpfButton.setBounds(10, 80, 31, 20);
    brfButton.setBounds(10, 110, 31, 20);

    svfButton.setBounds(50, 168, 31, 20);
    skButton.setBounds (90, 168, 31, 20);
    tlfButton.setBounds(130, 168, 31, 20);
    dlfButton.setBounds(170, 168, 31, 20);

    freqLabel.setBounds(225, 23, 50, 10);
    resLabel.setBounds(225, 33, 50, 10);
    driveLabel.setBounds(225, 43, 50, 10);
}

void ZedAudioProcessorEditor::chooseConfiguration(zed::FilterConfiguration configuration)
{
    configurationParameter.beginChangeGesture();
    configurationParameter.setValueNotifyingHost(
        configurationParameter.convertTo0to1(static_cast<float>(configuration)));
    configurationParameter.endChangeGesture();
    refreshConfiguration();
}

void ZedAudioProcessorEditor::refreshConfiguration()
{
    const auto selection = zed::selectionFor(audioProcessor.getFilterConfiguration());
    const std::array<TextButton*, 4> models { &svfButton, &skButton, &tlfButton, &dlfButton };
    const std::array<zed::FilterModel, 4> modelValues {
        zed::FilterModel::svf, zed::FilterModel::sallenKey,
        zed::FilterModel::transistorLadder, zed::FilterModel::diodeLadder
    };
    for (size_t i = 0; i < models.size(); ++i)
        models[i]->setToggleState(selection.model == modelValues[i], dontSendNotification);

    const std::array<TextButton*, 4> responses { &lpfButton, &hpfButton, &bpfButton, &brfButton };
    const std::array<zed::FilterResponse, 4> responseValues {
        zed::FilterResponse::lp, zed::FilterResponse::hp,
        zed::FilterResponse::bp, zed::FilterResponse::br
    };
    for (size_t i = 0; i < responses.size(); ++i)
    {
        responses[i]->setToggleState(selection.response == responseValues[i], dontSendNotification);
        responses[i]->setEnabled(zed::responseAvailable(selection.model, responseValues[i]));
    }
    filterWindow.setMode(static_cast<int>(selection.response));
}

void ZedAudioProcessorEditor::timerCallback()
{
    // JUCE Timer runs on the message thread. Poll the parameter directly, not
    // audio-thread status: automation/restoration also works while audio is stopped.
    // No parameter listeners, cross-thread Component access or posted callbacks.
    refreshConfiguration();
    repaint();
}
