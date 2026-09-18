/*
  ==============================================================================

    ZDSKmm.h
    Created: 26 Jan 2021 12:20:36pm
    Author:  Jim Murphy
    Zero Delay Sallen-Key Multimode Filter
    Implements a dual-mode Sallen-Key filter (Korg MS-10 style), bringing together
    both lpf and hpf Sallen Key implementations into one abstraction
  ==============================================================================
*/

#pragma once

#include "ZDSK.h"
#include "ZDSKHPF.h"

class ZDSKmm{
public:
    // Clear audio history while retaining coefficients and controls.
    inline void reset() noexcept { lpf.reset(); hpf.reset(); }

    ZDSKmm(){
    }

    ~ZDSKmm(){};

    //init
    inline void init(double samplerate){
        sr = samplerate;
        lpf.init(sr);
        hpf.init(sr);
    }

    //set filter type
    inline void setFilterType(int ft){
        filterType = ft;
    }

    //set cutoff
    inline void setCutoff(float pitch){
        lpf.setCutoff(pitch);
        hpf.setCutoff(pitch);
    }

    //set resonance
    inline void setResonance(float res){
        lpf.setResonance(res);
        hpf.setResonance(res);
    }

    //set drive
    inline void setDrive(float d){
        lpf.setDrive(d);
        hpf.setDrive(d);
    }

    //dsp
    inline float dsp(float ip){
        if(filterType == LPF )
            return lpf.dsp(ip);
        else if(filterType == HPF)
            return hpf.dsp(ip);
        else
            return 0.0f;
    }

private:
    friend struct ZedLifecycleTestAccess;
    ZDSK lpf;
    ZDSKHPF hpf;

    enum{LPF, HPF};

    int filterType = LPF; //0 == LPF, 1 == HPF
    double sr = 0.0; // Set by init before processing.
};
