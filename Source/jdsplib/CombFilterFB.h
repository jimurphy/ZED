/*
  ==============================================================================

    CombFilterFB.h
    Created: 4 Oct 2020 3:56:24pm
    Author:  Jim Murphy
    Feedback comb filter, inspired by
    https://ccrma.stanford.edu/~jos/Delay/Feedback_Comb_Filter.html
  ==============================================================================
*/

#pragma once
#include "DelayLin.h"

class CombFilterFB {
public:
    CombFilterFB(){
    }
    
    ~CombFilterFB(){};
    
    inline void init(int sr, float mt){
        delay.init(sr, mt);
    }
    
    inline void setTime(float t){
        delay.setTime(t);
    }
    
    inline void setComb(float fb, float output){
        am = fb;
        b0 = output;
    }
    
    inline float dsp(float i){
        float y = (delay.read() * (am * -1.0f)) + i;
        delay.write(y);
        return(y*b0);
    }
            
private:
    DelayLin delay;
    float b0 = 0.0f; //Delay output coeff
    float am = 0.0f; //Feedback coeff
};
