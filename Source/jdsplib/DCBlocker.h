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
    DCBlocker(){
    }
    
    ~DCBlocker(){};
        
    inline float dsp(float i){
        float y = i - xm1 + 0.8f * ym1;
        xm1 = i;
        ym1 = y;
        return y;
    }
            
private:
    float xm1 = 0.0f; //Delay output coeff
    float ym1 = 0.0f; //Feedforward coeff
};
