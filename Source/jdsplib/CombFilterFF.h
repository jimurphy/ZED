/*
  ==============================================================================

    CombFilterFF.h
    Created: 4 Oct 2020 10:19:32am
    Author:  Jim Murphy
    Feed-forward comb filter, inspired by
    https://ccrma.stanford.edu/~jos/Delay/Feedforward_Comb_Filter.html
  ==============================================================================
*/

#pragma once
#include "DelayLin.h"

class CombFilterFF {
public:
    CombFilterFF(){
    }
    
    ~CombFilterFF(){};
    
    inline void init(int sr, float mt){
        delay.init(sr, mt);
    }
    
    inline void setTime(float t){
        delay.setTime(t);
    }
    
    inline void setComb(float del, float dir){
        bm = del;
        b0 = dir;
    }
    
    inline float dsp(float i){
        float y = (delay.read()*bm) + (i*b0);
        delay.write(i);
        return(y);
    }
            
private:
    DelayLin delay;
    float bm = 0.0f; //Delay output coeff
    float b0 = 0.0f; //Feedforward coeff
};
