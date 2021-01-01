//
//  Gain.h
//  Gain Setter
//
//  Created by Jim Murphy on 9/05/20.
//

#ifndef Gain_h
#define Gain_h

class Gain {
public:
    Gain(){
    }
    
    ~Gain(){};
    
    inline void SetGain(float gain){
        gainVal = gain;
    }

    inline float dsp(float input){
        float output = input * gainVal;
        return(output);
    }
    
private:
    float       gainVal;
};


#endif /* Gain_h */
