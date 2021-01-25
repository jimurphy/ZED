/*
  ==============================================================================

    ZDSK.h - Sallen-Key Zero Delay filter
    Created: 19 Jan 2021 10:01:25am
    Author:  Jim Murphy - implementing code from Will Pirkle's app notes
    Based on a Korg35 LPF (a la Korg MS10) - see
    http://www.willpirkle.com/Downloads/AN-5Korg35_V3.pdf

  ==============================================================================
*/

#pragma once
#include <math.h>
#include "ZDOnePole.h"
#include "DSPMath.h"

class ZDSK{

public:
    ZDSK(){
    }
    
    ~ZDSK(){};


    inline void init(float samplerate){
        sr = samplerate;
        lpfOP1.filterType = LPF1;
        lpfOP2.filterType = LPF1;
        hpfOP.filterType = HPF1;
        
        lpfOP1.init(44100);
        lpfOP2.init(44100);
        hpfOP.init(44100);

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
        
        lpfOP1.ff = G;
        lpfOP2.ff = G;
        hpfOP.ff = G;
        
        lpfOP2.fb = (k - k*G)/(1.0f + g);
        hpfOP.fb = -1.0f/(1.0f + g);
        alpha0 = 1.0f/(1.0f - k*G + k*G*G);
    }
    
    inline float dsp(float input){
        input = fasttanh(input * driveGain);
        float y1 = lpfOP1.dsp(input);
        float s35 = hpfOP.getFeedbackOutput() + lpfOP2.getFeedbackOutput();
        float u = alpha0 * (y1 + s35);
                
        //Bipolar shaping
        if(u >= 0.0f)
            u = tanh(saturationpos * u);
        else
            u = tanh(saturationneg * u);
            
        float y = k * lpfOP2.dsp(u);
        y = hpfOP.dsp(y);
        
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
    
    ZDOnePole lpfOP1; // Low Pass 1 pole portion
    ZDOnePole lpfOP2; // Low Pass 1 pole portion
    ZDOnePole hpfOP; // High Pass 1 pole portion
};
