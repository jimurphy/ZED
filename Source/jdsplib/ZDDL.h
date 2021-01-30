/*
  ==============================================================================

    ZDDL.h
    Created: 29 Jan 2021 3:47:25pm
    Author:  Jim Murphy
    Implements Zero Delay / TPT Diode Ladder Filter
    From Will Pirkle, http://www.willpirkle.com/Downloads/AN-6DiodeLadderFilter.pdf

  ==============================================================================
*/

#pragma once

#pragma once
#include "ZDOnePole.h"
#include "DSPMath.h"

class ZDML{
public:
    inline void init(float samplerate){
        sr = samplerate;
        //init all four
        lpf1.filterType = LPF1;
        lpf2.filterType = LPF1;
        lpf3.filterType = LPF1;
        lpf4.filterType = LPF1;
        
        lpf1.init(44100);
        lpf2.init(44100);
        lpf3.init(44100);
        lpf4.init(44100);
        
        lpf1.setFeedback(0.0);
        lpf2.setFeedback(0.0);
        lpf3.setFeedback(0.0);
        lpf4.setFeedback(0.0);
        
        lpf1.a0 = 1.0f;
        lpf2.a0 = 0.5f;
        lpf3.a0 = 0.5f;
        lpf4.a0 = 0.5f;
        
        lpf4.gamma = 1.0f;
        lpf4.delta = 0.0f;
        lpf4.epsilon = 0.0f;
        lpf4.setFeedback(0.0f);
    }
    
    //expects cutoff 0-127
    inline void setCutoff(float pitch){
        cutoff = p2f(pitch);
        updateFilters();
    }
    
    inline void setResonance(float res){
        k = map(res, 0.0f, 1.0f, 0.0f, 4.0f); //TODO: is this the right map range?
    }
    
    inline void updateFilters(){
        filter1.setCutoff(cutoff);
        filter2.setCutoff(cutoff);
        filter3.setCutoff(cutoff);
        filter4.setCutoff(cutoff);
    }
    
    inline float dsp(float ip){
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
        u = fasttanh(u * 1.0f);
        float filterOut = filter4.dsp(filter3.dsp(filter2.dsp(filter1.dsp(u))));
        return filterOut;
    }


private:
    enum{LPF1}; //for child members

    ZDOnePole lpf1;
    ZDOnePole lpf2;
    ZDOnePole lpf3;
    ZDOnePole lpf4;
    
    float gamma = 0.0f;
    float sg1 = 0.0f;
    float sg2 = 0.0f;
    float sg3 = 0.0f;
    float sg4 = 0.0f;
    
    float sr = 44100.0f;
    float k = 0.0f; //Resonance
    float cutoff = 0.0f;
};
