/*
  ==============================================================================

    ZDOnePole.h
    Created: 19 Jan 2021 2:52:32pm
    Author:  Jim Murphy
    Implements Vadim Zavalishin's 1 pole TPT / Zero Delay
    From Will Pirkle, https://www.willpirkle.com/app-notes/virtual-analog-korg35-lpf/
  ==============================================================================
*/

#pragma once
#include <math.h>

class ZDOnePole{

public:
    ZDOnePole(){
    }
    
    ~ZDOnePole(){};

    int filterType = LPF1;
    enum{LPF1, HPF1};
    float ff = 1.0f; //Feedforward coeff
    float fb = 1.0f; //Feedback coeff

    inline void init(float samplerate){
        sr = samplerate;
        updateFilter();
    }
    
    inline float getFeedbackOutput(){
        return(z * fb);
    }

    inline void updateFilter(){
        float wd = 2.0f * MathConstants<float>::pi * cutoff;
        float T = 1.0f/sr;
        float wa = (2/T)*tan(wd*T/2);
        float g  = wa*T/2;
        ff = g/(1.0 + g);
    }
    
    inline float dsp(float input){
        float vn = (input - z)*ff;

        float lpf = vn + z;

        z = vn + lpf;

        float hpf = input - lpf;

        if(filterType == LPF1)
            return lpf;
        else if(filterType == HPF1)
            return hpf;

        return lpf;

    }
    
private:
    float sr = 44100;
    float cutoff = 0.0f;
    float z = 0.0f;
};
