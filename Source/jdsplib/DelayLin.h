/*
  ==============================================================================

    DelayLin.h
    Created: 3 Oct 2020 9:41:34am
    Author:  Jim Murphy
    
    Fractional delay with linear interpolation
    Intended as building block in more complex delay-based effects
    Based on code by Martijn Zwartjes
  ==============================================================================
*/

#pragma once
#include "DSPMath.h"

class DelayLin {
public:
    DelayLin(){
    }
    
    ~DelayLin(){};
    
    inline void init(int sr, float mt){
        maxTime = mt;
        size = sr * mt;
        maxPos = size - 2;
        //clear buf
        buf.clear();
        //resize buf to sr*maxTime samples, fill w/ zeros
        buf.resize(size, 0.0f);
    }
        
    inline void setTime(float t){
        dt = t*sampleRate;
        if(dt < 0)
            dt = 0;
        if(dt > maxPos)
            dt = maxPos;
    }
    
    inline void write(float i){
        idx++;
        if(idx >= size)
            idx -= size;
        buf[idx] = i;
    }
    
    inline float read(){
        float readPos = idx - dt; //fractional value
        float r0 = floor(readPos);
        float r1 = r0 + 1.0;
        float rf = readPos - r0; //distance between samples
        if(r0 < 0)
            r0 += size;
        if(r1 < 0)
            r1 += size;
        return(xFadeL(buf[r0], buf[r1], rf)); //lerp between buf positions
    }
    
    inline void dsp(float i, float& o){
        write(i);
        o = read();
    }
    
    
private:
    int size = 0;
    int sampleRate = 44100;
    float maxTime = 2.0f; //2 seconds max time
    int maxPos = 0;
    std::vector<float> buf;
    int idx = 0;
    float dt = 0.0f;
};
