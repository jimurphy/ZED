/*
  ==============================================================================

    DCBlocker.h
    Created: 5 Feb 2021 5:44:27pm
    Author:  Jim Murphy
    Simple DC blocker, as per https://ccrma.stanford.edu/~jos/fp/DC_Blocker_Software_Implementations.html
  ==============================================================================
*/

#pragma once

class DCBlocker {
public:
    // Clear audio history while retaining coefficients and controls.
    inline void reset() noexcept { xm1 = ym1 = 0.0f; }

    DCBlocker(){
    }

    ~DCBlocker(){};

    inline float dsp(float i){
        float y = i - xm1 + 0.9 * ym1; //Bigger values = lower freq cutoff (default = 0.995f)
        xm1 = i;
        ym1 = y;
        return y;
    }

private:
    friend struct ZedLifecycleTestAccess;
    float xm1 = 0.0f; //Delay output coeff
    float ym1 = 0.0f; //Feedforward coeff
};
