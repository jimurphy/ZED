/*
  ==============================================================================

    CombFilterFBLP.h
    Created: 12 Oct 2020 11:05:43am
    Author:  Jim Murphy
    Feedback Comb filter with lowpass filter in feedback loop
    Inspired by Freeverb's comb filters
  ==============================================================================
*/

#pragma once
#include "DelayLin.h"
#include "OnePoleLP.h"

class CombFilterFBLP {
public:
    CombFilterFBLP(){
    }
    
    ~CombFilterFBLP(){};
    
    inline void init(int sr, float mt){
        delay.init(sr, mt);
        lpf.setCutoff(2000.0f, sr);
    }
    
    inline void setTime(float t){
        delay.setTime(t);
    }
    
    inline void setComb(float fb, float output){
        am = fb;
        b0 = output;
    }
    
    inline void setLPF(int sr, float freq){
        lpf.setCutoff(freq, sr);
    }
    
    inline float dsp(float i){
        float z = lpf.dsp(delay.read());
        float y = (z * (am * -1.0f)) + i;
        delay.write(y);
        return(y*b0);
    }
            
private:
    DelayLin delay;
    OnePoleLP lpf;
    
    float b0 = 0.0f; //Delay output coeff
    float am = 0.0f; //Feedback coeff
};
