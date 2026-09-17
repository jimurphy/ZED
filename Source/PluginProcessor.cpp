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
    svfL.init(AudioProcessor::getSampleRate());
    svfR.init(AudioProcessor::getSampleRate());

    korgFilterL.init(AudioProcessor::getSampleRate());
    korgFilterR.init(AudioProcessor::getSampleRate());

    korgFilterL.setCutoff(64.0f);
    korgFilterR.setCutoff(64.0f);

    moogLadderL.init(AudioProcessor::getSampleRate());
    moogLadderR.init(AudioProcessor::getSampleRate());

    moogLadderL.setCutoff(64.0f);
    moogLadderR.setCutoff(64.0f);

    diodeLadderL.init(AudioProcessor::getSampleRate());
    diodeLadderR.init(AudioProcessor::getSampleRate());

    diodeLadderL.setCutoff(64.0f);
    diodeLadderR.setCutoff(64.0f);

    smootherCutoff.setCutoff(4.0f, AudioProcessor::getSampleRate());
    smootherRes.setCutoff(4.0f, AudioProcessor::getSampleRate());
}

void ZedAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
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

    int filtermode = (*lpfModeParameter * 1) + (*hpfModeParameter * 2) + (*bpfModeParameter * 3) + (*brfModeParameter * 4);

    int filtertype = (*svfTypeParameter * 0) + (*skTypeParameter * 1) + (*tlfTypeParameter * 2) + (*dlfTypeParameter * 3);

    filtertypeAtom.store(filtertype);

    switch(filtermode){
        case 1:
            //LPF
            svfL.setFilterType(3.0f); //3 = lp
            svfR.setFilterType(3.0f); //3 = lp
            korgFilterL.setFilterType(0); //0 = lp
            korgFilterR.setFilterType(0); //0 = lp
            filtermodeAtom.store(1);
            break;
        case 2:
            //HPF
            if(filtertype == SVFMode || filtertype == SKMode){
                svfL.setFilterType(1.0f); //1 = hp
                svfR.setFilterType(1.0f); //1 = hp
                korgFilterL.setFilterType(1); //1 = hp
                korgFilterR.setFilterType(1); //1 = hp
                filtermodeAtom.store(2);
            }
            else{
                //kick it back to LPF if not an SVF or SKF
                Value lpfParamVal = parameters.getParameterAsValue("lpfmode");
                lpfParamVal.setValue(1);
                svfL.setFilterType(3.0f); //3 = lp
                svfR.setFilterType(3.0f); //3 = lp
                filtermodeAtom.store(1);
            }
            break;
        case 3:
            //BPF
            if(filtertype == SVFMode){
                svfL.setFilterType(2.0f); //2 = bp
                svfR.setFilterType(2.0f); //2 = bp
                filtermodeAtom.store(3);
            }
            else{
                //kick it back to LPF if not an SVF
                Value lpfParamVal = parameters.getParameterAsValue("lpfmode");
                lpfParamVal.setValue(1);
                svfL.setFilterType(3.0f); //3 = lp
                svfR.setFilterType(3.0f); //3 = lp
                filtermodeAtom.store(1);
            }
            break;
        case 4:
            //BRF
            if(filtertype == SVFMode){
                svfL.setFilterType(4.0f); //4 = br/notch
                svfR.setFilterType(4.0f); //4 = br/notch
                filtermodeAtom.store(4);
            }
            else{
                //kick it back to LPF if not an SVF
                Value lpfParamVal = parameters.getParameterAsValue("lpfmode");
                lpfParamVal.setValue(1);

                svfL.setFilterType(3.0f); //3 = lp
                svfR.setFilterType(3.0f); //3 = lp
                filtermodeAtom.store(1);
            }
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
            case SVFMode: //state variable filter mode
                outL = svfL.dsp(lChannelData[j]);
                if (rChannelData != nullptr)
                    outR = svfR.dsp(rChannelData[j]);
                break;
            case SKMode: //sallen key mode
                outL = korgFilterL.dsp(lChannelData[j] * 2.0f);
                if (rChannelData != nullptr)
                    outR = korgFilterR.dsp(rChannelData[j] * 2.0f);
                break;
            case TLFMode: //transistor ladder mode
                outL = moogLadderL.dsp(lChannelData[j]) * 3.25f;
                if (rChannelData != nullptr)
                    outR = moogLadderR.dsp(rChannelData[j]) * 3.25f;
                break;
            case DLFMode:
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
