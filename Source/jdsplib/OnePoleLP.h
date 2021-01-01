/*
  ==============================================================================

    OnePoleLP.h
    Created: 11 Oct 2020 9:02:55pm
    Author:  Jim Murphy
    Simple one-pole LPF filter class, inspired by
    https://www.musicdsp.org/en/latest/Filters/237-one-pole-filter-lp-and-hp.html
  ==============================================================================
*/

#pragma once

class OnePoleLP {
public:
    OnePoleLP(){
    }
    
    ~OnePoleLP(){};
    
    inline void setCutoff(float freq, float fs){
        float x = exp(-2.0f * M_PI * freq / fs);
        a0 = 1.0f - x;
        b1 = -1.0f * x;
    }

    inline float dsp(float input){
        float output = a0 * input - b1 * z1;
        z1 = output;
        return(output);
    }
    
private:
    float       a0 = 0.0f;;
    float       b1 = 0.0f;;
    float       z1 = 0.0f;
};

/*
 Process loop (lowpass):
out = a0*in - b1*tmp;
tmp = out;

Simple HP version: subtract lowpass output from the input (has strange behaviour towards nyquist):
out = a0*in - b1*tmp;
tmp = out;
hp = in-out;

Coefficient calculation:
x = exp(-2.0*pi*freq/samplerate);
a0 = 1.0-x;
b1 = -x;

 */
