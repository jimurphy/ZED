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
    // Clear audio history while retaining coefficients and controls.
    inline void reset() noexcept { z = feedback = 0.0f; }

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

    inline void init(double samplerate){
        sr = samplerate;
    }

    inline float getFeedbackOutput(){
        return(fb * (z+feedback*delta));
    }

    inline void setFeedback(float fbin){
        feedback = fbin;
    }

    inline void setCutoff(float co){
        cutoff = co;
    }

    inline float dsp(float input){

        float xn = (input*gamma + feedback + epsilon*getFeedbackOutput());

        float vn = (a0 * xn - z)*ff;

        float lpf = tanh((vn + z) * 1.0f);

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
    friend struct ZedLifecycleTestAccess;
    double sr = 0.0; // Set by init before processing.
    float cutoff = 0.0f;
    float z = 0.0f;
};
