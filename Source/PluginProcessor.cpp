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
std::make_unique<AudioParameterFloat> ("cutoff",        // parameterID
                                       "Cutoff",        // parameter name
                                       NormalisableRange<float> (0.0f, 127.0f, 1.0f),
                                       0.0f //default val
                                       ),
std::make_unique<AudioParameterFloat> ("resonance",        // parameterID
                                       "Resonance",        // parameter name
                                       NormalisableRange<float> (0.0f, 1.1f, 0.01f),
                                       0.7f //default val
                                    ),
})
{
    cutoffParameter               = parameters.getRawParameterValue("cutoff");
    resParameter                  = parameters.getRawParameterValue("resonance");
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
    filter.init(AudioProcessor::getSampleRate());
}

void ZedAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ZedAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void ZedAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    filter.setCutoff(*cutoffParameter);
    filter.setQ(*resParameter);
    filter.setFilterType(3.0f);
    
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto* lChannelData = buffer.getWritePointer(0);
    auto* rChannelData = buffer.getWritePointer(1);

    for (int j=0;j<buffer.getNumSamples();++j){
        
        float out = filter.dsp(lChannelData[j]);
                
        lChannelData[j] = out;
        rChannelData[j] = out;
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
