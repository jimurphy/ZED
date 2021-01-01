/*
  ==============================================================================

    DiffusionDelayLin.h
    Created: 3 Oct 2020 7:08:26pm
    Author:  Jim Murphy
    Diffusion delay (AKA Schroeder Allpass Filter)
    Linear interpolation
    Based on code from Martijn Zwartjes
  ==============================================================================
*/

#pragma once
#include "DelayLin.h"

class DiffusionDelayLin {
public:
    DiffusionDelayLin(){
    }
    
    ~DiffusionDelayLin(){};
    
    inline void init(int sr, float mt){
        delay.init(sr, mt);
        maxTime = mt;
    }
    
    inline void setTime(float t){
        delay.setTime(t);
    }
    
    inline void setDiffusion(float diff){
        a = diff;
        ma = -1.0f * diff;
    }
    
    inline void dsp(float i, float& op){
        o = delay.read() + (i * ma) + 1.0e-150;
        delay.write(i + (o * a));
        op = o;
    }
            
private:
    DelayLin delay;
    float a = 0.0f;
    float ma = 0.0f;
    float o = 0.0f;
    float maxTime = 0.0f;
};
