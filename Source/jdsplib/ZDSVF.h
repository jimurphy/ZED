/*
  ==============================================================================

    ZDSVF.h
    Created: 8 Jun 2020 8:12:47pm
    Author:  Jim Murphy, with 100% debt owed to Vadim Zavalishin
             Thanks to Will Pirkle's App Note 4 for clarifying things
  ==============================================================================
*/

#pragma once

#include "DSPMath.h"

class ZDSVF {
public:
    ZDSVF(){
    }
    
    ~ZDSVF(){};
    
    inline void init(float samplerate){
        sr = samplerate;
    }

    //In semitones (conversion happens in lfoinput)
    inline void setLFOModDepth(float lfomoddepth){
        lfoModDepth = lfomoddepth;
    }
    
    inline void setEnvelopeModDepthAndInversion(float envmoddepth, float inv){
        envelopeModDepth = envmoddepth;
        envelopeInverted = inv; //0 == normal, 1 == inverted
    }

    inline void lfoInput(float lfoIn){
        //from -1 to 1 TO +/- lfomoddepth (semitones)
        lfoModValue = map(lfoIn,-1.0,1.0,semi2freq(lfoModDepth*-1),semi2freq(lfoModDepth));
    }
    
    inline void envelopeInput(float envIn){
        if(envelopeInverted == 0){ //if not inverted, then normal envelope
            //from 0 to 1 TO 0 to LFOModDepth
            envModValue = map(envIn, 0.0,1.0,semi2freq(0),semi2freq(envelopeModDepth));
        }
        else if(envelopeInverted == 1){ //if inverted, flip envelope
            envIn = envIn * -1.0;
            envIn = envIn + 1.0;
            envModValue = map(envIn, 0.0,1.0,semi2freq(0),semi2freq(envelopeModDepth*-1));
        }
    }
    
    inline void setQ(float q){
        //r = 1.0/(2.0*q);
        r = 2.0 * (1.0 - q); //Martijn code, q -> 0.0 - 1.0
    }
    
    inline void setCutoff(float cutoffin){
        float cutoffFreq = p2f(cutoffin);
        cutoffFreq = cutoffFreq*lfoModValue*envModValue;
        if(cutoffFreq > sr/3.0){ //avoid > nyquist/3 for stability issues
            cutoffFreq = sr/3.0;
        }
        wd = 2 * M_PI * (cutoffFreq);
        t = 1/sr;
        wa = (2/t)*std::tan(wd*t/2);
        g = wa*t/2;
    }
    
    inline void setDrive(float drive){
        driveGain = drive; //range can exceed 1.0
    }
    
    inline void setFilterType(float filter){
        filterType = filter;
    }

    inline float dsp(float input){
        //process input through tanh waveshaper
        input = fasttanh(input * driveGain);
        
        hp = (input - (2.0*r+g)*z1-z2)/(1.0+2.0*r*g+g*g);
        bp = g*hp+z1;
        bp = fasttanh(1.0*bp + 1e-18); //Nonlinear processing from pg353 of pirkle plugin book. DNC for self-osc (via M.Z.)
        lp = g*bp+z2;
        lp = fasttanh(1.0*lp + 1e-18); //Martijn suggests adding second tanh saturator to second integrator
        br = hp + lp;
        
        z1 = g*hp + bp;
        z2 = g*bp + lp;
        
        if(filterType == 4.0){
            return br;
        }
        else if(filterType == 3.0){
            return lp;
        }
        else if (filterType == 2.0){
            return bp;
        }
        else{
            return hp;
        }
    }
    
private:
    float       filterType = 1.0;
    float       driveGain = 1.0;
    float       lfoModDepth = 12.0; //TODO: 0 this
    float       lfoModValue = 1.0;
    float       envelopeModDepth = 0.0;
    float       envModValue = 1.0;
    float       envelopeInverted = 0.0;

    float       sr = 44100;
    float       wd = 0.0;
    float       t = 0.0;
    float       wa = 0.0;
    float       g = 0.0;
    float       r = 0.0;
    float       lp = 0.0;
    float       hp = 0.0;
    float       bp = 0.0;
    float       br = 0.0; //band reject, notch
    float       z1 = 0.0;
    float       z2 = 0.0;
};
