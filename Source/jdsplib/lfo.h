/*
  ==============================================================================

    lfo.h
    Created: 19 Jun 2020 8:28:22pm
    Author:  Jim Murphy

  ==============================================================================
*/

#pragma once
#include "DSPMath.h"

//Multimode LFO, bipolar (-1.0 to 1.0)
class Loscil {
public:
    Loscil(){
    }
    
    ~Loscil(){};
    
    //Set LFO phase (used for retriggering)
    inline void setPhase(float phase){
        modulo1 = phase; //TODO: limit range (clip minmax?)
        z = phase+0.5f; //for triOsc
    }
    
    inline void setFreq(float freq, float fs){
        fs = sr;
        inc = freq/sr;
    }

    inline void setWaveform(int waveform){
        waveformSelect = waveform; //1 = saw, 2 = sqr, 3 = tri, 4 = sin
    }
    
    inline void setPulsewidth(float pulsewidth){
        if(pulsewidth >= 100.0){
            pulsewidth = 99.9;
        }
        else if(pulsewidth <= 0.0){
            pulsewidth = 0.1;
        }
        pw = pulsewidth;
    }
    
    //sawtooth with no AA
    inline float sawDSP(){
        modulo1 += inc;

        if(modulo1 >= 1.0){
            modulo1 -= 1.0;
        }
        
        float sawtooth = 2.0*modulo1-1.0; //bipolar
        return sawtooth;
    }
    
    inline float sqrDSP(){
        modulo1 += inc;
        
        modulo2 = modulo1;

        //calculate first (normal) aa sawtooth
        if(modulo1 >= 1.0){
            modulo1 -= 1.0;
        }
        
        float square = 0.0;
        
        if(modulo1 > pw/100.0){
            square = 1.0;
        }
        else{
            square = 0.0;
        }
        
        square = 2.0*square-1.0; //bipolar

        return square;
    }
    
    inline float triDSP(){
        z += (inc*2.0);
        if (z > 1.0)
            z -= 2.0;
            
        triWave = ((2*abs(z))-1);
        return triWave;
    }
    
    //return triwave value when a phase (ph) from -1 to 1 is given
    inline float tapTri(float ph){
        float t = z + ph;
        if (t > 1.0)
            t -= 2.0;
            
        float triWaveTap = ((2*abs(t))-1);
        return triWaveTap;
    }
    
    //Parabolic sinewave approx (Martijn 2019)
    inline float sineDSP(){
        modulo1 += inc;
        if (modulo1 > 0.5)
        modulo1 -= 1;
        return(modulo1 * (8 - (16 * abs(modulo1))));
    }
    
    inline float dsp(){
        float output = 0.0;
        switch(waveformSelect){
            case 1:
                output = sawDSP();
                break;
            case 2:
                output = sqrDSP();
                break;
            case 3:
                output = triDSP();
                break;
            case 4:
                output = sineDSP();
                break;
            default:
                output = 0.0;
        }
        lfoValue = output;
        return output;
    }
    
    inline float tapLFO(){
        return lfoValue;
    }
    
private:
    float       inc = 0.0f;
    int         sr = 44100;
        
    int         waveformSelect = 2;
    
    //saw stuff
    float       modulo1 = 0.0f;
    float       sat = 1.0f; //saw shaper
    
    //sqr stuff
    float       modulo2 = 0.0f;
    float       pw = 0.0f;

    float       triWave = 0.0;
    float       phaseInc = 0.0;
    float       freq = 220.0;
    float       z = 0.0;
    
    float lfoValue = 0.0f; //value for getter
};
