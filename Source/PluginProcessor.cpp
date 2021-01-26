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
                                       NormalisableRange<float> (0.0f, 127.0f, 1.0f),
                                       0.0f //default val
                                       ),

std::make_unique<AudioParameterFloat> ("resonance",        // parameterID
                                       "Resonance",        // parameter name
                                       NormalisableRange<float> (0.0f, 1.1f, 0.01f),
                                       0.7f //default val
                                       ),

std::make_unique<AudioParameterFloat> ("lpfmode",         // parameterID
                                       "LPFMode",         // parameter name
                                       0,                 // minimum value
                                       1,                 // maximum value
                                       1),                // default value
    
std::make_unique<AudioParameterFloat> ("hpfmode",         // parameterID
                                       "HPFMode",         // parameter name
                                       0,                 // minimum value
                                       1,                 // maximum value
                                       0),                // default value
    
std::make_unique<AudioParameterFloat> ("bpfmode",         // parameterID
                                       "BPFMode",         // parameter name
                                       0,                 // minimum value
                                       1,                 // maximum value
                                       0),                // default value
    
std::make_unique<AudioParameterFloat> ("brfmode",         // parameterID
                                       "BRF Mode",        // parameter name
                                       0,                 // minimum value
                                       1,                 // maximum value
                                       0),                 // default value
    
std::make_unique<AudioParameterFloat> ("svftype",         // parameterID
                                       "SVF Type",        // parameter name
                                       0,                 // minimum value
                                       1,                 // maximum value
                                       1),                // default value
    
std::make_unique<AudioParameterFloat> ("sktype",          // parameterID
                                       "SK Type",         // parameter name
                                       0,                 // minimum value
                                       1,                 // maximum value
                                       0),                // default value
    
std::make_unique<AudioParameterFloat> ("tlftype",         // parameterID
                                       "TLF Type",        // parameter name
                                       0,                 // minimum value
                                       1,                 // maximum value
                                       0),                // default value
    
std::make_unique<AudioParameterFloat> ("dlftype",         // parameterID
                                       "DLF Type",        // parameter name
                                       0,                 // minimum value
                                       1,                 // maximum value
                                       0)                 // default value

})
{
    inputDriveParameter           = parameters.getRawParameterValue("drive");
    cutoffParameter               = parameters.getRawParameterValue("cutoff");
    resParameter                  = parameters.getRawParameterValue("resonance");
    lpfModeParameter              = parameters.getRawParameterValue("lpfmode");
    hpfModeParameter              = parameters.getRawParameterValue("hpfmode");
    bpfModeParameter              = parameters.getRawParameterValue("bpfmode");
    brfModeParameter              = parameters.getRawParameterValue("brfmode");
    svfTypeParameter              = parameters.getRawParameterValue("svftype");
    skTypeParameter               = parameters.getRawParameterValue("sktype");
    tlfTypeParameter              = parameters.getRawParameterValue("tlftype");
    dlfTypeParameter              = parameters.getRawParameterValue("dlftype");
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
    smootherCutoff.setCutoff(4.0f, AudioProcessor::getSampleRate());
    
    korgFilter.init(AudioProcessor::getSampleRate());
    korgFilter.setCutoff(64.0f);
    
    moogLadder.init(AudioProcessor::getSampleRate());
    moogLadder.setCutoff(64.0f);
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
    filter.setFilterType(3.0f);
    
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto* lChannelData = buffer.getWritePointer(0);
    auto* rChannelData = buffer.getWritePointer(1);

    filter.setDrive(*inputDriveParameter);
    korgFilter.setDrive(*inputDriveParameter);

    int filtermode = (*lpfModeParameter * 1) + (*hpfModeParameter * 2) + (*bpfModeParameter * 3) + (*brfModeParameter * 4);
    
    int filtertype = (*svfTypeParameter * 0) + (*skTypeParameter * 1) + (*tlfTypeParameter * 2) + (*dlfTypeParameter * 3);
    
    filtertypeAtom.store(filtertype);
    
    switch(filtermode){
        case 1:
            //LPF
            filter.setFilterType(3.0f); //3 = lp
            korgFilter.setFilterType(0); //0 = lp
            filtermodeAtom.store(1);
            break;
        case 2:
            //HPF
            filter.setFilterType(1.0f); //1 = hp
            korgFilter.setFilterType(1); //1 = hp
            filtermodeAtom.store(2);
            break;
        case 3:
            //BPF
            if(filtertype == SVFMode){
                filter.setFilterType(2.0f); //2 = bp
                filtermodeAtom.store(3);
            }
            else{
                //kick it back to LPF if not an SVF
                Value lateMixParamVal = parameters.getParameterAsValue("lpfmode");
                lateMixParamVal.setValue(1);
                filter.setFilterType(3.0f); //3 = lp
                filtermodeAtom.store(1);
            }
            break;
        case 4:
            //BRF
            if(filtertype == SVFMode){
                filter.setFilterType(4.0f); //4 = br/notch
                filtermodeAtom.store(4);
            }
            else{
                //kick it back to LPF if not an SVF
                filter.setFilterType(3.0f); //3 = lp
                filtermodeAtom.store(1);
            }
            break;
    }
    
    for (int j=0;j<buffer.getNumSamples();++j){
        
        float smoothCutoff = smootherCutoff.dsp(*cutoffParameter);
        
        filter.setCutoff(smoothCutoff);
        korgFilter.setCutoff(smoothCutoff);
        moogLadder.setCutoff(smoothCutoff);

        filter.setQ(*resParameter);
        korgFilter.setResonance(*resParameter);
        moogLadder.setResonance(*resParameter);

        float out = 0.0f;
        
        switch(filtertype){
            case SVFMode: //state variable filter mode
                out = filter.dsp(lChannelData[j]);
                break;
            case SKMode: //sallen key mode
                out = korgFilter.dsp(lChannelData[j]);
                break;
            case TLFMode: //transistor ladder mode
                out = moogLadder.dsp(lChannelData[j]);
            case DLFMode:
                break;
            default:
                break;
                //out = filter.dsp(lChannelData[j]);
        };

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
