/*
  ==============================================================================

    ZDDL.h
    Created: 29 Jan 2021 3:47:25pm
    Author:  Jim Murphy
    Implements Zero Delay / TPT Diode Ladder Filter
    From Will Pirkle, http://www.willpirkle.com/Downloads/AN-6DiodeLadderFilter.pdf

  ==============================================================================
*/

#pragma once

#pragma once
#include "ZDOnePoleEx.h"
#include "DSPMath.h"

class ZDDL{
public:
    ZDDL(){
    }
    
    ~ZDDL(){};

    inline void init(float samplerate){
        sr = samplerate;
        //init all four
        lpf1.filterType = LPF1;
        lpf2.filterType = LPF1;
        lpf3.filterType = LPF1;
        lpf4.filterType = LPF1;
        
        lpf1.init(sr);
        lpf2.init(sr);
        lpf3.init(sr);
        lpf4.init(sr);
        
        lpf1.setFeedback(0.0f);
        lpf2.setFeedback(0.0f);
        lpf3.setFeedback(0.0f);
        lpf4.setFeedback(0.0f);
        
        lpf1.a0 = 1.0f;
        lpf2.a0 = 0.5f;
        lpf3.a0 = 0.5f;
        lpf4.a0 = 0.5f;
        
        lpf4.gamma = 1.0f;
        lpf4.delta = 0.0f;
        lpf4.epsilon = 0.0f;
        lpf4.setFeedback(0.0f);
        
        updateFilter();
    }
    
    //expects cutoff 0-127
    inline void setCutoff(float pitch){
        cutoff = p2f(pitch);
        updateFilter();
    }
    
    inline void setResonance(float res){
        k = map(res, 0.0f, 1.0f, 0.0f, 17.0f);
        updateFilter();
    }
    
    inline void setDrive(float d){
        driveGain = d;
    }

    inline void updateFilter(){
        // calculate G
        float wd = 2.0f * MathConstants<float>::pi * cutoff;
        float T = 1.0f/sr;
        float wa = (2.0f/T)*tan(wd*T/2.0f);
        float g = wa * T / 2.0f;

        float G4 = (0.5f * g) / (1.0f + g);
        float G3 = (0.5f * g) / (1.0f + g - (0.5f*g*G4));
        float G2 = (0.5f * g) / (1.0f + g - (0.5f*g*G3));
        float G1 = g / (1.0f + g - (g*G2));
        gamma = G4 * G3 * G2 * G1;
        sg1 = G4 * G3 * G2;
        sg2 = G4 * G3;
        sg3 = G4;
        sg4 = 1.0f;
        
        lpf1.ff = g/(1.0f + g);
        lpf2.ff = g/(1.0f + g);
        lpf3.ff = g/(1.0f + g);
        lpf4.ff = g/(1.0f + g);
        
        lpf1.fb = 1.0f/(1.0f + g - (g*G2));
        lpf2.fb = 1.0f/(1.0f + g - (0.5f*g*G3));
        lpf3.fb = 1.0f/(1.0f + g - (0.5f*g*G4));
        lpf4.fb = 1.0f/(1.0f + g);
        
        lpf1.gamma = 1.0f + G1*G2;
        lpf2.gamma = 1.0f + G2*G3;
        lpf3.gamma = 1.0f + G3*G4;
        
        lpf1.delta = g;
        lpf2.delta = 0.5f * g;
        lpf3.delta = 0.5f * g;
        
        lpf1.epsilon = G2;
        lpf2.epsilon = G3;
        lpf3.epsilon = G4;
    }
    
    inline float dsp(float ip){
        ip = tanh(ip * driveGain);

        lpf3.setFeedback(lpf4.getFeedbackOutput());
        lpf2.setFeedback(lpf3.getFeedbackOutput());
        lpf1.setFeedback(lpf2.getFeedbackOutput());
        
        float sigma =  (sg1 * lpf1.getFeedbackOutput()) +
                       (sg2 * lpf2.getFeedbackOutput()) +
                       (sg3 * lpf3.getFeedbackOutput()) +
                       (sg4 * lpf4.getFeedbackOutput());
                
        float un = tanh((ip - k*sigma)/(1 + k * gamma)  + 1e-18);
        return(lpf4.dsp(lpf3.dsp(lpf2.dsp(lpf1.dsp(un)))));
    }


private:
    enum{LPF1}; //for child members

    ZDOnePoleEx lpf1;
    ZDOnePoleEx lpf2;
    ZDOnePoleEx lpf3;
    ZDOnePoleEx lpf4;
    
    float gamma = 0.0f;
    float sg1 = 0.0f;
    float sg2 = 0.0f;
    float sg3 = 0.0f;
    float sg4 = 0.0f;
    
    float sr = 44100.0f;
    float k = 0.0f; //Resonance
    float cutoff = 0.0f;
    float driveGain = 0.0f;
};
