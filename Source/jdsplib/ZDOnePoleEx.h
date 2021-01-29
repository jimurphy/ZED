/*
  ==============================================================================

    ZDOnePoleEx.h
    Created: 29 Jan 2021 10:18:09pm
    Author:  Jim Murphy
    Implements Vadim Zavalishin's 1 pole TPT / Zero Delay, (with extensions for Diode Ladder Filter)
    From Will Pirkle, http://www.willpirkle.com/Downloads/AN-6DiodeLadderFilter.pdf
 
  ==============================================================================
*/

#pragma once
#include <math.h>

class ZDOnePoleEx{

public:
    ZDOnePoleEx(){
    }
    
    ~ZDOnePoleEx(){};

    int filterType = LPF1;
    enum{LPF1, HPF1};
    float ff = 1.0f; //Feedforward coeff
    float fb = -1.0f; //Feedback coeff
    
    //Extended functionality variables
    //As used in the Diode Ladder filter (ZDDL.h)
    //(see http://www.willpirkle.com/Downloads/AN-6DiodeLadderFilter.pdf)
    float gamma = 1.0f; //pre-gain
    float delta = 0.0f; //FB_IN coeff
    float epsilon = 1.0f; //extra factor for local feedback
    float a0 = 1.0f; //filter gain
    float feedback = 0.0f; //Feedback storage register (not a delay register)

    inline void init(float samplerate){
        sr = samplerate;
        updateFilter();
    }
    
    inline float getFeedbackOutput(){
        return(fb * (z+feedback*delta));
    }
    
    inline void setFeedback(float fbin){
        feedback = fbin;
    }
    
    inline void setCutoff(float co){
        cutoff = co;
        updateFilter();
    }

    inline void updateFilter(){
        float wd = 2.0f * MathConstants<float>::pi * cutoff;
        float T = 1.0f/sr;
        float wa = (2/T)*tan(wd*T/2);
        float g  = wa*T/2;
        ff = g/(1.0 + g);
    }
    
    inline float dsp(float input){
        float xn = (input*gamma + feedback + epsilon*getFeedbackOutput());

        float vn = (a0 * input - z)*ff;

        float lpf = vn + z;

        z = vn + lpf;

        float hpf = input - lpf;

        if(filterType == LPF1)
            return lpf;
        else if(filterType == HPF1)
            return hpf;

        return lpf;
    }
    
    inline float getZ(){
        return z;
    }
    
private:
    float sr = 44100;
    float cutoff = 0.0f;
    float z = 0.0f;
};
