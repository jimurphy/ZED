/*
  ==============================================================================

    voice.h
    Created: 17 Jun 2020 6:36:49pm
    Author:  Jim Murphy

  ==============================================================================
*/

#pragma once
#include "oscillatormm.h"
#include "ZDSVF.h"
#include "Adsr.h"
#include "gain.h"
#include "lfo.h"

class Voice {
public:
    Voice(){
    }
    
    ~Voice(){};

    inline void init(float samplerate){
        fs = samplerate;
        adsr1.init(fs);
    }
    
    inline void setOscPitchMod(float oct1, float semi1, float cent1, float oct2, float semi2, float cent2){
        osc1.setOctave(oct1, fs);
        osc1.setSemitone(semi1, fs);
        osc1.setCent(cent1, fs);
        
        osc2.setOctave(oct2, fs);
        osc2.setSemitone(semi2, fs);
        osc2.setCent(cent2, fs);
    }
    
    inline void setOscEnv(float osc1EnvAmnt, float osc2EnvAmnt){
        osc1.setEnvelopeModDepth(osc1EnvAmnt);
        osc2.setEnvelopeModDepth(osc2EnvAmnt);
    }
    
    inline void setOscPW(float pulseWidth1, float pwmAmnt1, float pulseWidth2, float pwmAmnt2){
        osc1.setPulseWidth(pulseWidth1);
        osc1.setPWMAmount(pwmAmnt1);
        osc2.setPulseWidth(pulseWidth2);
        osc2.setPWMAmount(pwmAmnt2);
    }
    
    inline void setOscShape(float sawShape1, float sawShape2){
        osc1.setShape(sawShape1);
        osc2.setShape(sawShape2);
    }
    
    inline void setOscSub(float sub1, float sub2){
        osc1.setSubOscGain(sub1);
        osc2.setSubOscGain(sub2);
    }
    
    inline void setOscWaveform(int wf1, int wf2){
        osc1.setWaveform(wf1);
        osc2.setWaveform(wf2);
    }
    
    inline void setOscGain(float g1, float g2){
        osc1.setGain(g1);
        osc2.setGain(g2);
    }
        
    inline void setFilter(float cutoff1, float q1, float drive1, float lfo1, float env1, float inv1){
        filter1.setCutoff(cutoff1);
        filter1.setQ(q1);
        filter1.setDrive(drive1);
        filter1.setLFOModDepth(lfo1);
        filter1.setEnvelopeModDepthAndInversion(env1, inv1);
    }
    
    inline void setFilterType(float filter1type){
        filter1.setFilterType(filter1type);
    }
    
    inline void setFilterModulationValue(float filterModInput){
        filterModValue = filterModInput;
    }
    
    inline void setADSR(float a1, float d1, float s1, float r1, float scale1){
        adsr1.setAttack(a1);
        adsr1.setDecay(d1);
        adsr1.setSustain(s1);
        adsr1.setRelease(r1);
        if(scale1 == 0.0){ //if button is OFF, scale by 1.0x
            adsr1.setScaleFactor(1.0);
        }
        else{ //if button is ON, scale by 10.0x
            adsr1.setScaleFactor(10.0);
        }
    }
        
    inline void setLFOValues(float lfo1, float lfo2){
        lfo1Value = lfo1;
        lfo2Value = lfo2;
    }
        
    inline void triggerNote(float p, float g){
        pitch = p;
        osc1.setPitch(pitch, fs);
        osc2.setPitch(pitch, fs);
        adsr1.setGate(g);
    }
    
    inline float getPitch(){
        return pitch;
    }
    
    inline float dsp(){
        float adsr1Val = adsr1.dsp();
        
        //If the voice's output is greater than -96dB, then do DSP on it
        if(adsr1Val > dB2lin(-96.0)){
            osc1.lfoFreqInput(lfo1Value);
            osc1.lfoPWMInput(lfo1Value);

            osc2.lfoFreqInput(lfo1Value);
            osc2.lfoPWMInput(lfo1Value);

            filter1.lfoInput(filterModValue);
                                    
            gainEnv1.SetGain(adsr1Val);
            
            float osc1Output = osc1.dsp();
            float osc2Output = osc2.dsp();
                                
            float filter1output = filter1.dsp(osc1Output+osc2Output);
            
            env1output = gainEnv1.dsp(filter1output);
        }
        return(env1output);
    }
    
private:
    float       fs = 44100.0f;
    float       pitch = -1; //Holds pitch set by MIDI
    float       filterModValue = 0.0f;
    float       lfo1Value = 0.0f;
    float       lfo2Value = 0.0f;
    float       env1output = 0.0f;
    
    Oscillator  osc1;
    Oscillator  osc2;
    ZDSVF       filter1;
    EnvGen      adsr1;
    Gain        gainEnv1; //envelope 1 modulates
};

