/*
  ==============================================================================

    RevMini.h
    Created: 6 Oct 2020 10:01:06am
    Author:  Jim Murphy
 
  ==============================================================================
*/

#pragma once
#include "CombFilterFB.h"
#include "CombFilterFBLP.h"
#include "DiffusionDelayLin.h"
#include "lfo.h"
#include "DelayLin.h"
#include "ZDSVF.h"

class RevMini {
public:
    RevMini(){
    }
    
    ~RevMini(){};
    
    inline void init(int sr){
        sampleRate = sr;
        
        lpf.init(sr);
        hpf.init(sr);
        
        lpf.setQ(0.7f);
        hpf.setQ(0.7f);
        
        hpf.setFilterType(1.0f);
        lpf.setFilterType(3.0f);

        lpf.setCutoff(40.0f);
        hpf.setCutoff(40.0f);
        
        predelay.init(sr, 2.0f); //pre-delay
        predelay.setTime(0.0f);
        
        comb1.init(sr, 2.0f);
        comb2.init(sr, 2.0f);
        comb3.init(sr, 2.0f);
        comb4.init(sr, 2.0f);

        apf1.init(sr, 2.0f);
        apf2.init(sr, 2.0f);
        apf3.init(sr, 2.0f);
        
        comb1.setTime(0.03825f);
        comb1.setComb(0.873f, 1.0f); //direct, delayed //0.873, 0.902, 0.853, 0.833
        
        comb2.setTime(0.03530f);
        comb2.setComb(0.902f, 1.0f); //direct, delayed

        comb3.setTime(0.04655f);
        comb3.setComb(0.853f, 1.0f); //direct, delayed

        comb4.setTime(0.05104f);
        comb4.setComb(0.833f, 1.0f); //direct, delayed
        
        apf1.setTime(0.00786f);
        apf2.setTime(0.00256f);
        apf3.setTime(0.00084f);
        
        apf1.setDiffusion(0.7f);
        apf2.setDiffusion(0.7f);
        apf3.setDiffusion(0.7f);
        
        lfo1.setFreq(0.5, sr);
        lfo1.setWaveform(3); //Triangle
    }
    
    inline void setWetDry(float i){
        wetDry = i;
    }
    
    inline void setLfoFreq(float i){
        lfo1.setFreq(i, sampleRate);
    }
    
    inline void setApfDiff(float i){
        apf1.setDiffusion(i);
        apf2.setDiffusion(i);
        apf3.setDiffusion(i);
    }
    
    inline void setApfDelayTime(float i){
        apf1basetime = i;
        apf2basetime = i * 0.33;
        apf3basetime = apf2basetime * 0.33;
    }
    
    inline void setCombDelaytime(float i){
        comb1basetime = i;
        comb2basetime = i * 1.33;
        comb3basetime = i * 2.17;
        comb4basetime = i * 3.19;
    }
    
    inline void setCombFeedback(float i){        
        comb1.setComb(i, 1.0f);
        comb2.setComb(i, 1.0f);
        comb3.setComb(i, 1.0f);
        comb4.setComb(i, 1.0f);
    }
    
    inline void setApfMod(float i){
        apfModDepth = i;
    }
    
    inline void setPredelayTime(float i){
        predelay.setTime(i);
    }
    
    inline void setFilterGains(float i, float j){
        lowpassgain = i;
        highpassgain = j;
    }
    
    inline float getL(){
        return lVal;
    }
    
    inline float getR(){
        return rVal;
    }

    
    inline void dsp(float input){
        //std::vector<float> channels;

        //calculate and scale modulation LFO
        float mod1 = lfo1.dsp();
        float mod2 = lfo1.tapTri( 0.25f);
        float mod3 = lfo1.tapTri( 0.375f);
        float mod4 = lfo1.tapTri( 0.5f);

        float fs = sampleRate;
        float apfmod1 = (mod1 * apfModDepth)/fs; //+/- 10 samples
        float apfmod2 = (mod2 * apfModDepth)/fs;
        float apfmod3 = (mod3 * apfModDepth)/fs;

        //vary apf sizes
        apf1.setTime(apf1basetime + apfmod1);
        apf2.setTime(apf2basetime + apfmod2);
        apf3.setTime(apf3basetime + apfmod3);

        float combmod1 = (mod1 * 4.0f)/fs; //+/- 10 samples
        float combmod2 = (mod2 * 4.0f)/fs;
        float combmod3 = (mod3 * 4.0f)/fs;
        float combmod4 = (mod4 * 4.0f)/fs;

        //vary comb sizes
        comb1.setTime(comb1basetime + combmod1);
        comb2.setTime(comb2basetime + combmod2);
        comb3.setTime(comb3basetime + combmod3);
        comb4.setTime(comb4basetime + combmod4);
        
        float i = 0.0f;
        
        i = predelay.dsp(input);
        
        float j = lpf.dsp(i) * lowpassgain;
        float k = hpf.dsp(i) * highpassgain;
        
        i = j + k;
        
        i = apf1.dsp(i + lateFeedback);
        i = apf2.dsp(i);
        i = apf3.dsp(i);
        lateFeedback = i * lateFBGain;
        float combSum = comb1.dsp(i) + comb2.dsp(i) + comb3.dsp(i) + comb4.dsp(i);
        
        float dryVal = input;
    
        float wetGain = sqrt(0.5f*(1.0f + wetDry));
        float dryGain = sqrt(0.5f*(1.0f - wetDry));
        
        lVal = (combSum * wetGain) + (dryVal * dryGain);
        rVal = ((combSum * -1.0f) * wetGain) + (dryVal * dryGain);
        
//      channels.push_back((combSum * wetGain) + (dryVal * dryGain));
//      channels.push_back(((combSum * -1.0f) * wetGain) + (dryVal * dryGain));
//      return channels;
    }
            
private:
    CombFilterFBLP comb1;
    CombFilterFBLP comb2;
    CombFilterFBLP comb3;
    CombFilterFBLP comb4;
    
    DiffusionDelayLin apf1;
    DiffusionDelayLin apf2;
    DiffusionDelayLin apf3;
    
    DelayLin predelay; //pre-delay
    
    Loscil lfo1;
    
    ZDSVF lpf;
    ZDSVF hpf;

    float sampleRate = 0.0f;
    float lateFeedback = 0.0f;
    float lateFBGain = 0.0f;
    
    float lowpassgain = 1.0f;
    float highpassgain = 1.0f;
    
    float apfModDepth = 5.0f;
    float apf1basetime = 0.00786f;
    float apf2basetime = 0.00256f;
    float apf3basetime = 0.00084f;
    
    float comb1basetime = 0.03825f;
    float comb2basetime = 0.03530f;
    float comb3basetime = 0.04655f;
    float comb4basetime = 0.05104f;
    
    float comb1fb = 0.873f;
    float comb2fb = 0.902f;
    float comb3fb = 0.853f;
    float comb4fb = 0.833f;

    float wetDry = -1.0f; //-1 to 1
    
    float lVal = 0.0f;
    float rVal = 0.0f;
};
