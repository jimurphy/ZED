/*
  ==============================================================================

    ZDML.h
    Created: 25 Jan 2021 3:28:51pm
    Author:  Jim Murphy
    Implements Vadim Zavalishin's Zero Delay / TPT Moog Ladder Filter (4 stage)
    From Will Pirkle, https://www.willpirkle.com/706-2/

  ==============================================================================
*/

#pragma once
#include "ZDOnePole.h"
#include "DSPMath.h"

class ZDML{
public:
    
    ZDML(){
    }
    
    ~ZDML(){};

    inline void init(float samplerate){
        sr = samplerate;
        //init all four
        filter1.filterType = LPF1;
        filter2.filterType = LPF1;
        filter3.filterType = LPF1;
        filter4.filterType = LPF1;
        
        filter1.init(sr);
        filter2.init(sr);
        filter3.init(sr);
        filter4.init(sr);
    }
    
    //expects cutoff 0-127
    inline void setCutoff(float pitch){
        cutoff = p2f(pitch);
        updateFilters();
    }
    
    inline void setResonance(float res){
        k = map(res, 0.0f, 1.0f, 0.0f, 4.0f); //TODO: is this the right map range?
    }
    
    inline void setDrive(float d){
        driveGain = d;
    }
    
    inline void updateFilters(){
        filter1.setCutoff(cutoff);
        filter2.setCutoff(cutoff);
        filter3.setCutoff(cutoff);
        filter4.setCutoff(cutoff);
    }
    
    inline float dsp(float ip){
        ip = fasttanh(ip * driveGain);

        // calculate G
        float wd = 2.0f * MathConstants<float>::pi * cutoff;
        float T = 1.0f/sr;
        float wa = (2.0f/T)*tan(wd*T/2.0f);
        float g = wa * T / 2.0f;
        float G = g * g * g;
        
        float S = g * g * g * filter1.getZ() +
                  g * g * filter2.getZ() +
                  g * filter3.getZ() +
                  filter4.getZ();
        
        float u = (ip - k*S)/(1.0f + k*G);
        u = fasttanh(u * 1.0f  + 1e-18);
        float filterOut = filter4.dsp(filter3.dsp(filter2.dsp(filter1.dsp(u))));
        return filterOut;        
    }


private:
    enum{LPF1,HPF1}; //for child members

    ZDOnePole filter1;
    ZDOnePole filter2;
    ZDOnePole filter3;
    ZDOnePole filter4;
    
    float sr = 44100.0f;
    float k = 0.0f; //Resonance
    float cutoff = 0.0f;
    float driveGain = 0.0f;
};
