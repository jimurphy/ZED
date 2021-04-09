/*
  ==============================================================================

    ZDSKHPF.h
    Created: 21 Jan 2021 9:23:55am
    Author:  Jim Murphy
    Author:  Jim Murphy - implementing code from Will Pirkle's app notes
    Based on a Korg35 HPF (a la Korg MS10) - see
    http://www.willpirkle.com/Downloads/AN-7Korg35HPF_V2.pdf
 
  ==============================================================================
*/

#pragma once
#include <math.h>
#include "ZDOnePole.h"
#include "DSPMath.h"

class ZDSKHPF{

public:
    ZDSKHPF(){
    }
    
    ~ZDSKHPF(){};


    inline void init(float samplerate){
        sr = samplerate;
        hpfOP1.filterType = HPF1;
        hpfOP2.filterType = HPF1;
        lpfOP1.filterType = LPF1;
        
        hpfOP1.init(44100);
        hpfOP2.init(44100);
        lpfOP1.init(44100);

        updateFilters();
    }
    
    inline void setCutoff(float pitch){
        cutoff = p2f(pitch);
        updateFilters();
    }
    
    inline void setDrive(float d){
        driveGain = d;
    }

    inline void setResonance(float res){
        k = map(res, 0.0f, 1.0f, 0.0f, 2.0f);
    }
    
    inline void updateFilters(){
        float wd = 2.0f * MathConstants<float>::pi * cutoff;
        float T = 1.0 / sr;
        float wa = (2.0/T) * tan(wd * T/2.0f);
        float g = wa * T/2.0f;
        
        float G = g / (1.0f + g);
        
        hpfOP1.ff = G;
        hpfOP2.ff = G;
        lpfOP1.ff = G;
        
        hpfOP2.fb = -1.0f*G/(1.0f + g);
        lpfOP1.fb = 1.0f/(1.0f + g);
        
        alpha0 = 1.0f/(1.0f - k*G + k*G*G);
    }
    
    inline float dsp(float input){
        input = fasttanh(input * driveGain);
        float y1 = hpfOP1.dsp(input);
        
        float s35 = hpfOP2.getFeedbackOutput() + lpfOP1.getFeedbackOutput();
        float u = alpha0 * (y1 + s35);
                
        float y = k * u;

        //Bipolar shaping
        if(y >= 0.0f)
            y = tanh(saturationpos * y  + 1e-18);
        else
            y = tanh(saturationneg * y  + 1e-18);
            
        y = lpfOP1.dsp(hpfOP2.dsp(y));
        
        if(k > 0)
            y *= 1.0f/k;
        
        return y;
    }
    
private:
    float sr = 44100;
    float driveGain = 0.0f;
    float alpha0 = 0.0f;
    float cutoff = 0.0f;
    float k = 1.0f;
    float saturationpos = 1.0f;
    float saturationneg = 1.5f;

    enum{LPF1,HPF1}; //for child members
    
    ZDOnePole hpfOP1; // High Pass 1 pole portion
    ZDOnePole hpfOP2; // High Pass 1 pole portion
    ZDOnePole lpfOP1; // Low Pass 1 pole portion
};
