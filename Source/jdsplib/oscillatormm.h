/*
  ==============================================================================

    oscillatormm.h
    Created: 18 Jun 2020 1:16:13pm
    Author:  Jim Murphy
    MULTIMODE oscillator
 
  ==============================================================================
*/

#pragma once
#include "DSPMath.h"

class Oscillator {
public:
    Oscillator(){
    }
    
    ~Oscillator(){};
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //Set fundamental frequencies for oscillators.
    //Individual oscillators' phase compensation is
    //handled in their own dsp methods
    inline void setPitch(int pitch, double fs){
        sr = fs;
        if(jportamento == false){
            pitchFreq = p2f(pitch);
            setOscillatorFreq();
            //currentPitch = pitch; //TODO: add small offset
        }
        else{
            targetPitch = pitch;
            float pitchInterval = targetPitch - currentPitch;
            pitchInc = (pitchInterval)/((sr*glideTime)+1);
        }
    }
    
    inline void setOctave(int octave, double fs){
        sr = fs;
        octFreq = semi2freq(octave*12);
        setOscillatorFreq();
    }
    
    inline void setSemitone(int semitone, double fs){
        sr = fs;
        semiFreq = semi2freq(semitone);
        setOscillatorFreq();
    }

    inline void setCent(float cent, double fs){
        sr = fs;
        centFreq = semi2freq(cent/100);
        setOscillatorFreq();
    }
    
    inline void setPitchbend(float pb){
        pitchbendFreq = semi2freq(pb);
    }
    
    //In semitones (conversion happens in lfoinput)
    inline void setLFOModDepth(float lfomoddepth){
        lfoModDepth = lfomoddepth;
    }
    
    inline void setEnvelopeModDepth(float envmoddepth){
        envelopeModDepth = envmoddepth;
    }
    
    inline void lfoFreqInput(float lfoIn){
        //from -1 to 1 TO +/- lfomoddepth (semitones)
        lfoModValue = map(lfoIn,-1.0,1.0,semi2freq(lfoModDepth*-1),semi2freq(lfoModDepth));
        setOscillatorFreq();
        
        //process PWM
        //applyPWM(lfoIn);
    }
    
    inline void lfoPWMInput(float lfoIn){        
        //process PWM
        //applyPWM(lfoIn);
    }

    inline void envelopeInput(float envIn){
        //map from 0 to 1 to semi2freq(0) to semi2freq(envelopeModDepth)
        envelopeModValue = map(envIn,0,1,semi2freq(0),semi2freq(envelopeModDepth));
        setOscillatorFreq();
    }
    
    inline void setOscillatorFreq(){
        
        float freqProduct = 0.0f;
        
        if(jportamento == false){
            //final freq = product of oct, semi, cent, lfo, env
            freqProduct =       pitchFreq*
                                octFreq*
                                semiFreq*
                                centFreq*
                                lfoModValue*
                                envelopeModValue*
                                pitchbendFreq;
            
            float subFreq = freqProduct/2;
            
            //Check to make sure it doesn't exceed Nyquist ever
            if(freqProduct > (sr/2.0)){
                freqProduct = sr/2.0;
                subFreq = sr/2.0;
            }
            
            inc = freqProduct/sr;
            subInc = subFreq/sr;
        }
        else{
            //final freq = product of oct, semi, cent, lfo, env
            freqProduct =       currentPortamentoFreq*
                                octFreq*
                                semiFreq*
                                centFreq*
                                lfoModValue*
                                envelopeModValue;
            
            float subFreq = freqProduct/2;
            
            //Check to make sure it doesn't exceed Nyquist ever
            if(freqProduct > (sr/2.0)){
                freqProduct = sr/2.0;
                subFreq = sr/2.0;
            }
            
            inc = freqProduct/sr;
            subInc = subFreq/sr;
        }
    }
    
    inline void setPortamento(bool porta, float glide){
        jportamento = porta;
        glideTime = glide;
    }
    
    inline void setPulseWidth(float pulsewidth){
        pwCenter = pulsewidth; //0 to 100
    }
    
    inline void setPWMAmount(float pwmamount){
        float mapPwm = (pwmamount + 1.0f) * 0.5;
        pw = mapPwm * 100.0f;
        if(pw > 98.0){pw = 98.0;}; //clip if > 100
        if(pw < 2.0){pw = 2.0;}; //clip if < 0
    }
    
    inline void applyPWM(float lfoIn){
        //-1 to 1 to 0 to 1
        float lfoVal = (lfoIn + 1.0)*0.5;
        //scale between 0 and 50 (for full PWM at PW of 50)
        lfoVal = lfoVal * 50.0;
        
        //apply pwm amount and add to current pwCenter
        lfoVal = lfoVal * pwmAmount;
        pw = pwCenter + lfoVal;
        if(pw > 98.0){pw = 98.0;}; //clip if > 100
        if(pw < 2.0){pw = 2.0;}; //clip if < 0
    }
    
    inline void setShape(float s){
        sat = s; //1.0 -> 14.0
    }

    
    inline void setGain(float g){
        gain = g*g*g; //cubic approx. of log gain
    }
    
    inline void setSubOscGain(float subgain){
        subGain = subgain*subgain*subgain; //cubic approx of log gain
        if(subGain > 1.0f){
            subGain = 1.0f;
        }
        else if(subGain < 0.0f){
            subGain = 0.0f;
        }
    }
    
    inline void setWaveform(int waveform){
        waveformSelect = waveform; //0 = saw, 1 = sqr, 2 = tri
    }
    
    inline float sawDSP(){
        modulo1 += inc;

        if(modulo1 >= 1.0){
            modulo1 -= 1.0;
        }
        
        float sawtooth = 2.0*modulo1-1.0; //bipolar
        
        //apply waveshaping (sat = any pos value != 0.0)
        //from Pirkle pg 220
        sawtooth = fasttanh(sat*sawtooth)/fasttanh(sat);

        float polyblep = 0.0;

        if(modulo1 > 1.0-inc){
            float t = (modulo1-1.0)/inc;
            polyblep = 1.0*(t*t + 2.0*t + 1.0);
        }
        
        else if(modulo1 < inc){
            float t = modulo1/inc;
            polyblep = 1.0*(2.0*t - t*t - 1.0);
        }

        polyblep *= -1.0;
        sawtooth = sawtooth + polyblep;
        sawtooth = sawtooth * gain * 0.5;
        return sawtooth;
    }
    
    inline float sqrDSP(){
        modulo1 += inc;
        modulo2 = modulo1;

        //calculate first (normal) aa sawtooth
        if(modulo1 >= 1.0){
            modulo1 -= 1.0;
        }
        float sawtooth1 = 2.0*modulo1-1.0; //bipolar

        float polyblep1 = 0.0;

        if(modulo1 > 1.0-inc){
            float t = (modulo1-1.0)/inc;
            polyblep1 = 1.0*(t*t + 2.0*t + 1.0);
        }
        else if(modulo1 < inc){
            float t = modulo1/inc;
            polyblep1 = 1.0*(2.0*t - t*t - 1.0);
        }

        polyblep1 *= -1.0;
        sawtooth1 = sawtooth1 + polyblep1;

        //phase shift second modulo
        modulo2 += (pw/100.0);

        //decrement modulo
        if(modulo2 >= 1.0){
            modulo2 -= 1.0;
        }

        float sawtooth2 = 2.0*modulo2-1.0; //bipolar
        
        //calculate second (inv, shifted) sawtooth
        float polyblep2 = 0.0;

        if(modulo2 > 1.0-inc){
            float t = (modulo2-1.0)/inc;
            polyblep2 = 1.0*(t*t + 2.0*t + 1.0);
        }
        else if(modulo2 < inc){
            float t = modulo2/inc;
            polyblep2 = 1.0*(2.0*t - t*t - 1.0);
        }

        polyblep2 *= -1.0;
        sawtooth2 = sawtooth2 + polyblep2;

        //subtract saw1 - saw2 (0.5 gain?)
        float squareOut = 0.5*sawtooth1 - 0.5*sawtooth2;
        //return
        
        float pwCorrect = 1;

        //shift sawtooth after PWM-related DSP offset
        if(pw >= 50){
            pwCorrect = 1/(pw/100);
        }
        else{
            pwCorrect = 1/(1-(pw/100));
        }

        squareOut = squareOut * pwCorrect * 0.7;

        squareOut = squareOut * gain;
        return squareOut;
    }
    
    inline float triDSP(){
        z += (inc*2); //hack to make
        if (z > 1.0)
            z -= 2.0;
            
        triWave = ((2*abs(z))-1);
        triWave = triWave * gain * 0.5;
        return triWave;
    }
    
    inline float subDSP(){
        float twoPi = 2*M_PI;

        subPhase += subInc;

        if(subPhase >= 1){
            subPhase -= 1;
        }
        float sinOut = sin(twoPi*subPhase);
        return(sinOut);
    }
    
    inline float dsp(){
        float output = 0.0;
        
        if(jportamento == true){
            if((pitchInc < 0.0 && targetPitch <= currentPitch) || (pitchInc > 0.0 && targetPitch >= currentPitch)){
                currentPitch += pitchInc;
                currentPortamentoFreq = p2f(currentPitch);
            }
        setOscillatorFreq();
        }
        
        switch(waveformSelect){
            case 1:
                output = sawDSP();
                break;
            case 2:
                output = sqrDSP();
                break;
            case 3:
                output = triDSP();
                break;
            default:
                output = 0.0;
        }
        float subOut = 0.0f;
        if(subGain != 0.0f){ //only calculate subosc if it's turned up
            subOut = subDSP();
        }
        subOut = subOut * subGain;
        output = gain*(output + subOut);
        return output;
    }
    
private:
    float       pitchFreq = 1.0f;
    float       octFreq = 1.0f;
    float       semiFreq = 1.0f;
    float       centFreq = 1.0f;
    float       pitchbendFreq = 1.0f;
    
    float       targetPitch = 0.0f;
    float       currentPitch = 0.0f;
    float       pitchInc = 0.0f;
    float       glideTime = 0.25f;
    float       currentPortamentoFreq = 0.0;
    bool        jportamento = false;
    
    float       lfoModDepth = 1.0f;
    float       envelopeModDepth = 0.0f;
    float       lfoModValue = 1.0f;
    float       envelopeModValue = 1.0f;
    float       inc = 0.0f;
    float       subInc = 0.0f;
    int         sr = 44100;
    
    float       pw = 50.0f;
    float       pwCenter = 0.0f;
    float       pwmAmount = 0.0f;
    float       pwmLFO = 0.0f;
    
    float       gain = 1.0f;
    float       subGain = 1.0f;
    
    int         waveformSelect = 2; //1 = saw, 2 = sqr, 3 = tri
    
    //saw stuff
    float       modulo1 = 0.0f;
    float       sat = 1.0f; //saw shaper
    
    //sqr stuff
    float       modulo2 = 0.0;
    
    //tri stuff (TODO: replace with 0.0->1.0?)
    float       triWave = 0.0;
    float       phaseInc = 0.0;
    float       freq = 220.0;
    float       z = 0.0;
    
    //sub Stuff
    float       subPhase = 0.0;
};
