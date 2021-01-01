//
//  Adsr.h
//  Gain Setter - All
//
//  Created by Jim Murphy on 13/05/20.
//

#ifndef Adsr_h
#define Adsr_h

#include "DSPMath.h"

class EnvGen {
public:
    EnvGen(){
    }
    
    ~EnvGen(){};

    inline void init(float samplerate){
        sr = samplerate;
    }
    
    //**** SET ADSR coefficients. dB2Lin is used because we want more control over
    //small amounts of time but the option to have very slow coefficients
    inline void setAttack(float a){
        aCoeff = 1/max((dB2lin(a)*(sr*0.0005*sf)), 1); //max to prevent < 1 samp
        aCoeff = 1 - pow(0.5, aCoeff); //"how many times you have to multiply the coeff by itself to get 0.5 kinda..."
    }
            
    inline void setDecay(float d){
        dCoeff = 1/max((dB2lin(d)*(sr*0.0005*sf)), 1);
        dCoeff = 1 - pow(0.5, dCoeff); //see Martijn comment for aCoeff
    }
    
    inline void setSustain(float s){
        sLevel = s/127.0;
    }
    
    inline void setRelease(float r){
        rCoeff = 1/max((dB2lin(r)*(sr*0.0005*sf)), 1);
        rCoeff = 1 - pow(0.5, rCoeff); //see Martijn comment for aCoeff
    }
    
    //Scale the input to allow for -v-e-r-y- long envelopes
    inline void setScaleFactor(float scaler){
        if(scaler > 10){
            scaler = 10; //clip to max of 10x
        }
        if(scaler <= 0){
            scaler = 0.1; //clip to min of 0.1x
        }
        sf = scaler;
    }
    
    //Gate is the also trigger: 0-127 (sets the amplitude of the note)
    inline void setGate(int g){
        float floatG = float(g);
        floatG = floatG/127.0;
        gate = floatG;
        state = floatG;
    }

    inline float dsp(){
        //Determine if in attack or in D,S, or R
        if(gate > z){
            state = state;
        }
        else{
            state = 0;
        }

        float p = 0;
        //determine if linear or ext
        if(state > 0){
            p = aCoeff; //linear
        }
        else{
            p = 0; //exponential
        }

        float q = 0;
        //determine if in decay or release
        if(state == 0){
            if(gate != 0 ){
                q = dCoeff;
            }
            else{
                q = rCoeff;
            }         }
        else{
            q = 0; //we're attacking, so be linear
        }

        float l = gate * sLevel; //scale to sLevel or to 0 if no gate

        z += (q * (l - z)) + p;
        return(z);
    }
    
private:
    float       sr;
    float       aCoeff = 0.0;
    float       dCoeff = 0.0;
    float       sLevel = 0.0;
    float       rCoeff = 0.0;
    float       gate = 0.0;
    float       state = 0.0;
    float       z = 0.0;
    float       sf = 1; //time stretch scale factor
};

#endif /* Adsr_h */
