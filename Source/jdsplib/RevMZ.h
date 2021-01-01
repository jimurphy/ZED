/*
  ==============================================================================
    RevMZ.h
    Created: 26 Oct 2020 8:49:45am
    Author:  Jim Murphy
    Reverb based on Martijn Zwartjes's javascript reverb R6
  ==============================================================================
*/

#pragma once
#include "DSPMath.h"
#include "lfo.h"
#include "DiffusionDelayLin.h"
#include "DelayLin.h"
#include "OnePoleLP.h"

class RevMZ {
public:
//    RevMZ(int x){
//        DBG(x);
//    }
    
    RevMZ(){
    }

    
    ~RevMZ(){};
    
    inline void init(int sr){
        sampleRate = sr;
                
        //init predelay
        predelayl.init(sr, 2.0f);
        predelayl.setTime(0.0f);
        predelayr.init(sr, 2.0f);
        predelayr.setTime(0.0f);

        //init early reflections' delays
        mDelayDFFL0E.init(sr, 2.0f);
        mDelayDFFL1E.init(sr, 2.0f);
        mDelayDFFL2E.init(sr, 2.0f);
        mDelayDFFL3E.init(sr, 2.0f);

        mDelayDFFR0E.init(sr, 2.0f);
        mDelayDFFR1E.init(sr, 2.0f);
        mDelayDFFR2E.init(sr, 2.0f);
        mDelayDFFR3E.init(sr, 2.0f);

        //init late reverb right and left delays
        mDelayL.init(sr, 2.0f);
        mDelayR.init(sr, 2.0f);
        
        mDelayDFFL0.init(sr, 2.0f);
        mDelayDFFL1.init(sr, 2.0f);
        mDelayDFFL2.init(sr, 2.0f);
        mDelayDFFR0.init(sr, 2.0f);
        mDelayDFFR1.init(sr, 2.0f);
        mDelayDFFR2.init(sr, 2.0f);
        
        setSpin(1.0f);
        setSize(-12.0f);
        setRT60(80.0f);
        setDiffusion(0.667f);
        setDamping(440.0f);
        setEarlyTuning(24.0f);
    }
        
    inline void setSpin(float spin){
        lfo.setWaveform(3); //Triangle
        lfo.setFreq(spin, sampleRate);
        modAmount = spin * 0.001f;
        earlyModAmount = spin * 0.0001f;
    }
    
    inline void set(){
        dTL0 = 1.0f / p2f(size);
        dTL1 = dTL0 * 1.18921f; //minor 3rd
        dTL2 = dTL1 * 1.18921f;
        dTL3 = dTL2 * 1.18921f;
        
        dTR0 = dTL0 * 1.09051f;
        dTR1 = dTR0 * 1.18921f;
        dTR2 = dTR1 * 1.18921f;
        dTR3 = dTR2 * 1.18921f;

        fb = dB2lin((dTR3 / (dB2lin(rt60) * 0.0005f)) * -60.0f);
        
        //set early reflections
        dTL0e = 1.0f/p2f(size+earlyScale);
        dTL1e = dTL0e * 1.18921f; //minor 3rd
        dTL2e = dTL1e * 1.18921f;
        dTL3e = dTL2e * 1.18921f;

        dTR0e = dTL0e * 1.09051f;
        dTR1e = dTR0e * 1.18921f;
        dTR2e = dTR1e * 1.18921f;
        dTR3e = dTR2e * 1.18921f;
    }

    inline void setSize(float sz){
        size = sz;
        set();
    }
    
    inline void setEarlyTuning(float et){
        earlyScale = et;
        set();
    }
    
    inline void setRT60(float rt){
        rt60 = rt;
        set();
    }
    
    inline void setPredelayTime(float i){
        predelayl.setTime(i);
        predelayr.setTime(i);
    }
    
    inline void setDiffusion(float dff){
        mDelayDFFR0E.setDiffusion(dff);
        mDelayDFFR1E.setDiffusion(dff);
        mDelayDFFR2E.setDiffusion(dff);
        mDelayDFFL0E.setDiffusion(dff);
        mDelayDFFL1E.setDiffusion(dff);
        mDelayDFFL2E.setDiffusion(dff);
        
        mDelayDFFR0.setDiffusion(dff);
        mDelayDFFR1.setDiffusion(dff);
        mDelayDFFR2.setDiffusion(dff);
        mDelayDFFL0.setDiffusion(dff);
        mDelayDFFL1.setDiffusion(dff);
        mDelayDFFL2.setDiffusion(dff);
    }
    
    inline void setWetDry(float i){
        wetDry = i;
    }
    
    inline void setDryGain(float i){
        dryGain = i;
    }
    
    inline void setEarlyGain(float i){
        earlyGain = i;
    }
    
    inline void setLateGain(float i){
        lateGain = i;
    }
    
    inline void setDamping(float f){
        dampR.setCutoff(f, sampleRate);
        dampL.setCutoff(f, sampleRate);
    }
    
    inline void setCalcEarly(bool ce){
        calcEarly = ce;
    }
    
    inline void setHold(bool h){
        isHolding = h;
    }
    
    inline void calculateEarly(float inL, float inR, float& outL, float& outR){
        float pdL = 0.0f;
        float pdR = 0.0f;
        
        predelayl.dsp(inL, pdL);
        predelayr.dsp(inR, pdR);

        //calculate early reflections
        float le = 0.0f;
        mDelayDFFL0E.dsp(pdL, le);
        mDelayDFFL1E.dsp(le, le);
        mDelayDFFL2E.dsp(le, le);
        mDelayDFFL3E.dsp(le, le);
        
        float re = 0.0f;
        mDelayDFFL0E.dsp(pdR, re);
        mDelayDFFL1E.dsp(re, re);
        mDelayDFFL2E.dsp(re, re);
        mDelayDFFL3E.dsp(re, re);
        
        outL = le;
        outR = re;
    }
    
    inline void dsp(float inL, float inR, float& outL, float& outR){
        
        //Calculate mod values
        float mod0 = lfo.triDSP() * modAmount;
        float mod1 = lfo.tapTri(0.25f) * modAmount;
        float mod2 = lfo.tapTri(0.5f) * modAmount;
        float mod3 = lfo.tapTri(0.75f) * modAmount;

        float mod0e = lfo.triDSP() * earlyModAmount;
        float mod1e = lfo.tapTri(0.25f) * earlyModAmount;
        float mod2e = lfo.tapTri(0.5f) * earlyModAmount;
        float mod3e = lfo.tapTri(0.75f) * earlyModAmount;
        
        //set early reflections' times
        mDelayDFFL0E.setTime(dTL0e + mod0e);
        mDelayDFFL1E.setTime(dTL1e + mod1e);
        mDelayDFFL2E.setTime(dTL2e + mod2e);
        mDelayDFFL3E.setTime(dTL3e + mod3e);

        mDelayDFFR0E.setTime(dTR0e - mod0e);
        mDelayDFFR1E.setTime(dTR1e - mod1e);
        mDelayDFFR2E.setTime(dTR2e - mod2e);
        mDelayDFFR3E.setTime(dTR3e - mod3e);
        
        //adjust late reverb's delaylines' times
        mDelayDFFL0.setTime(dTL0 + mod0);
        mDelayDFFL1.setTime(dTL1 + mod1);
        mDelayDFFL2.setTime(dTL2 + mod2);
        mDelayL.setTime(dTL3 + mod3);
        
        mDelayDFFR0.setTime(dTR0 - mod0);
        mDelayDFFR1.setTime(dTR1 - mod1);
        mDelayDFFR2.setTime(dTR2 - mod2);
        mDelayR.setTime(dTR3 - mod3);
        
        //calculate early reflections
        float re = 0.0f;
        float le = 0.0f;
        
        if(calcEarly == true)
           calculateEarly(inL, inR, le, re);
        else{
            le = inL;
            re = inR;
        }

        //calculate late reverb
        float l = 0.0f;

        //if hold (freeze), then don't feed audio into late, set fb to 1.0
        if(isHolding){
            le = 0.0f;
            re = 0.0f;
            fb = 1.0f;
        }
        
        mDelayDFFL0.dsp(le + (zr * fb) + 1e-150, l);
        mDelayDFFL1.dsp(l, l);
        mDelayDFFL2.dsp(l, l);
        mDelayL.dsp(l, l);
        if(!isHolding)
            l = dampL.dsp(l);
        zl = l;
        
        float r = 0.0f;
        mDelayDFFR0.dsp(re + (zl * fb) + 1e-150, r);
        mDelayDFFR1.dsp(r, r);
        mDelayDFFR2.dsp(r, r);
        mDelayR.dsp(r, r);
        if(!isHolding)
            r = dampR.dsp(r);
        zr = r;

        r = r + re;
        l = l + le;
        
        //calculate dry gain
        inL = inL * dryGain;
        inR = inR * dryGain;
        
        //Calculate early gain
        le = le * earlyGain;
        re = re * earlyGain;

        //Calculate late gain
        l = l * lateGain;
        r = r * lateGain;

        //output mix of dry, early, and late
        outL = inL + le + l;
        outR = inR + re + r;
    }
            
private:
    Loscil lfo;
    
    //Pre-delay (l+r)
    DelayLin predelayl;
    DelayLin predelayr;

    //Early reflections
    DiffusionDelayLin mDelayDFFR0E;
    DiffusionDelayLin mDelayDFFR1E;
    DiffusionDelayLin mDelayDFFR2E;
    DiffusionDelayLin mDelayDFFR3E;

    DiffusionDelayLin mDelayDFFL0E;
    DiffusionDelayLin mDelayDFFL1E;
    DiffusionDelayLin mDelayDFFL2E;
    DiffusionDelayLin mDelayDFFL3E;

    //Late reverb
    DiffusionDelayLin mDelayDFFR0;
    DiffusionDelayLin mDelayDFFR1;
    DiffusionDelayLin mDelayDFFR2;
    DelayLin mDelayR;
    OnePoleLP dampR;

    DiffusionDelayLin mDelayDFFL0;
    DiffusionDelayLin mDelayDFFL1;
    DiffusionDelayLin mDelayDFFL2;
    DelayLin mDelayL;
    OnePoleLP dampL;

    float sampleRate = 44100.0f;
    bool calcEarly = true;
    bool isHolding = false;
    
    float wetDry = -1.0f;
    float dryGain = 0.0f;
    float earlyGain = 0.0f;
    float lateGain = 0.0f;
    
    float modAmount = 0.0f;
    float earlyModAmount = 0.0f;
    float earlyScale = 0.0f;
    float rt60 = 60.0f;
    float size = -12.0f;
    float fb = 0.0f;
    float zr = 0.0f;
    float zl = 0.0f;

    float dTR0 = 0.0f;
    float dTR1 = 0.0f;
    float dTR2 = 0.0f;
    float dTR3 = 0.0f;
    
    float dTL0 = 0.0f;
    float dTL1 = 0.0f;
    float dTL2 = 0.0f;
    float dTL3 = 0.0f;
    
    float dTR0e = 0.0f;
    float dTR1e = 0.0f;
    float dTR2e = 0.0f;
    float dTR3e = 0.0f;

    float dTL0e = 0.0f;
    float dTL1e = 0.0f;
    float dTL2e = 0.0f;
    float dTL3e = 0.0f;
};
