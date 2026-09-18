/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ZedAudioProcessor::ZedAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
,parameters (*this, nullptr, Identifier ("Zed"), {
std::make_unique<AudioParameterFloat> ("drive",        // parameterID
                                       "Drive",        // parameter name
                                       NormalisableRange<float> (0.5f, 5.0f, 0.01f),
                                       1.0f //default val
                                       ),

std::make_unique<AudioParameterFloat> ("cutoff",        // parameterID
                                       "Cutoff",        // parameter name
                                       NormalisableRange<float> (12.0f, 135.0f, 1.0f),
                                       57.0f //default val
                                       ),

std::make_unique<AudioParameterFloat> ("resonance",        // parameterID
                                       "Resonance",        // parameter name
                                       NormalisableRange<float> (0.01f, 1.1f, 0.01f),
                                       0.7f //default val
                                       ),

std::make_unique<AudioParameterChoice> (
    zed::filterConfigurationID, "Filter configuration",
    StringArray(zed::configurationNames.data(), static_cast<int>(zed::configurationNames.size())),
    static_cast<int>(zed::FilterConfiguration::svfLP))

})
{
    inputDriveParameter           = parameters.getRawParameterValue("drive");
    cutoffParameter               = parameters.getRawParameterValue("cutoff");
    resParameter                  = parameters.getRawParameterValue("resonance");
    filterConfigurationParameter = parameters.getRawParameterValue(zed::filterConfigurationID);
}

ZedAudioProcessor::~ZedAudioProcessor()
{
}

//==============================================================================
const juce::String ZedAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ZedAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ZedAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ZedAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ZedAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ZedAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ZedAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ZedAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ZedAudioProcessor::getProgramName (int index)
{
    return {};
}

void ZedAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void ZedAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // The callback argument is authoritative, including fractional host rates.
    svfL.init(sampleRate);
    svfR.init(sampleRate);
    korgFilterL.init(sampleRate);
    korgFilterR.init(sampleRate);
    moogLadderL.init(sampleRate);
    moogLadderR.init(sampleRate);
    diodeLadderL.init(sampleRate);
    diodeLadderR.init(sampleRate);

    smootherCutoff.setCutoff(4.0f, sampleRate);
    smootherRes.setCutoff(4.0f, sampleRate);
    dspPrepared = true;
    reset();
}

void ZedAudioProcessor::reset()
{
    // Called serially with processing by the host. No allocation, notifications
    // or GUI work; clear both channels and every topology, including inactive ones.
    svfL.reset(); svfR.reset();
    korgFilterL.reset(); korgFilterR.reset();
    moogLadderL.reset(); moogLadderR.reset();
    diodeLadderL.reset(); diodeLadderR.reset();
    dcblocker1.reset(); dcblocker2.reset();

    const float cutoff = cutoffParameter->load(std::memory_order_relaxed);
    const float resonance = resParameter->load(std::memory_order_relaxed);
    const float drive = inputDriveParameter->load(std::memory_order_relaxed);
    smootherCutoff.reset(cutoff);
    smootherRes.reset(resonance);

    // reset() is also safe before the first prepare: no coefficient calculation
    // may use an unconfigured sample rate. Parameters themselves are never changed.
    if (!dspPrepared)
        return;

    svfL.setQ(resonance); svfR.setQ(resonance);
    korgFilterL.setResonance(map(resonance, 0.0f, 1.1f, 0.0f, 0.9f));
    korgFilterR.setResonance(map(resonance, 0.0f, 1.1f, 0.0f, 0.9f));
    moogLadderL.setResonance(map(resonance, 0.0f, 1.1f, 0.0f, 1.05f));
    moogLadderR.setResonance(map(resonance, 0.0f, 1.1f, 0.0f, 1.05f));
    diodeLadderL.setResonance(map(resonance, 0.0f, 1.1f, 0.0f, 1.125f));
    diodeLadderR.setResonance(map(resonance, 0.0f, 1.1f, 0.0f, 1.125f));

    svfL.setCutoff(cutoff); svfR.setCutoff(cutoff);
    korgFilterL.setCutoff(cutoff); korgFilterR.setCutoff(cutoff);
    moogLadderL.setCutoff(cutoff); moogLadderR.setCutoff(cutoff);
    diodeLadderL.setCutoff(cutoff); diodeLadderR.setCutoff(cutoff);
    svfL.setDrive(drive); svfR.setDrive(drive);
    korgFilterL.setDrive(drive); korgFilterR.setDrive(drive);
    moogLadderL.setDrive(drive); moogLadderR.setDrive(drive);
    diodeLadderL.setDrive(drive); diodeLadderR.setDrive(drive);
}

void ZedAudioProcessor::releaseResources()
{
    // No dynamic DSP resources to release. Clear tails at the end of playback;
    // a later prepare configures rates and clears history again.
    reset();
    dspPrepared = false;
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ZedAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();
    return (input == juce::AudioChannelSet::mono()
         || input == juce::AudioChannelSet::stereo())
        && output == input;
}
#endif

void ZedAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    svfL.setFilterType(3.0f);
    svfR.setFilterType(3.0f);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto* lChannelData = buffer.getWritePointer(0);
    // Mono uses the existing left filter state; never request a second channel.
    auto* rChannelData = totalNumInputChannels > 1 && buffer.getNumChannels() > 1
                      ? buffer.getWritePointer(1) : nullptr;

    svfL.setDrive(*inputDriveParameter);
    svfR.setDrive(*inputDriveParameter);

    korgFilterL.setDrive(*inputDriveParameter);
    korgFilterR.setDrive(*inputDriveParameter);

    diodeLadderL.setDrive(*inputDriveParameter);
    diodeLadderR.setDrive(*inputDriveParameter);

    moogLadderL.setDrive(*inputDriveParameter);
    moogLadderR.setDrive(*inputDriveParameter);

    // One stable configuration snapshot per block; never repair or notify parameters here.
    const auto selection = zed::selectionFor(getFilterConfiguration());
    const auto filtertype = selection.model;

    // Preserve the existing valid-response setter order, including inactive filters.
    switch (selection.response)
    {
        case zed::FilterResponse::lp:
            svfL.setFilterType(3.0f);
            svfR.setFilterType(3.0f);
            korgFilterL.setFilterType(0);
            korgFilterR.setFilterType(0);
            break;
        case zed::FilterResponse::hp:
            svfL.setFilterType(1.0f);
            svfR.setFilterType(1.0f);
            korgFilterL.setFilterType(1);
            korgFilterR.setFilterType(1);
            break;
        case zed::FilterResponse::bp:
            svfL.setFilterType(2.0f);
            svfR.setFilterType(2.0f);
            break;
        case zed::FilterResponse::br:
            svfL.setFilterType(4.0f);
            svfR.setFilterType(4.0f);
            break;
    }

    for (int j=0;j<buffer.getNumSamples();++j){

        float smoothCutoff = smootherCutoff.dsp(*cutoffParameter);
        float smoothRes = smootherRes.dsp(*resParameter);

        svfL.setCutoff(smoothCutoff);
        korgFilterL.setCutoff(smoothCutoff);
        moogLadderL.setCutoff(smoothCutoff);
        diodeLadderL.setCutoff(smoothCutoff);

        svfR.setCutoff(smoothCutoff);
        korgFilterR.setCutoff(smoothCutoff);
        moogLadderR.setCutoff(smoothCutoff);
        diodeLadderR.setCutoff(smoothCutoff);

        svfL.setQ(smoothRes);
        korgFilterL.setResonance(map(smoothRes, 0.0f, 1.1f, 0.0f, 0.9f));
        moogLadderL.setResonance(map(smoothRes, 0.0f, 1.1f, 0.0f, 1.05f));
        diodeLadderL.setResonance(map(smoothRes, 0.0f, 1.1f, 0.0f, 1.125f));

        svfR.setQ(smoothRes);
        korgFilterR.setResonance(map(smoothRes, 0.0f, 1.1f, 0.0f, 0.9f));
        moogLadderR.setResonance(map(smoothRes, 0.0f, 1.1f, 0.0f, 1.05f));
        diodeLadderR.setResonance(map(smoothRes, 0.0f, 1.1f, 0.0f, 1.125f));

        float outL = 0.0f;
        float outR = 0.0f;

        switch(filtertype){
            case zed::FilterModel::svf: //state variable filter mode
                outL = svfL.dsp(lChannelData[j]);
                if (rChannelData != nullptr)
                    outR = svfR.dsp(rChannelData[j]);
                break;
            case zed::FilterModel::sallenKey: //sallen key mode
                outL = korgFilterL.dsp(lChannelData[j] * 2.0f);
                if (rChannelData != nullptr)
                    outR = korgFilterR.dsp(rChannelData[j] * 2.0f);
                break;
            case zed::FilterModel::transistorLadder: //transistor ladder mode
                outL = moogLadderL.dsp(lChannelData[j]) * 3.25f;
                if (rChannelData != nullptr)
                    outR = moogLadderR.dsp(rChannelData[j]) * 3.25f;
                break;
            case zed::FilterModel::diodeLadder:
                outL = diodeLadderL.dsp(lChannelData[j]) * 10.0f;
                if (rChannelData != nullptr)
                    outR = diodeLadderR.dsp(rChannelData[j]) * 10.0f;
                break;
            default:
                break;
                //out = filter.dsp(lChannelData[j]);
        };

        lChannelData[j] = outL;
        if (rChannelData != nullptr)
            rChannelData[j] = outR;
    }
}

//==============================================================================
bool ZedAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ZedAudioProcessor::createEditor()
{
    return new ZedAudioProcessorEditor (*this, parameters);
}

//==============================================================================
void ZedAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ZedAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.state.getType()))
            parameters.replaceState (ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZedAudioProcessor();
}
