//
//  DSPMath.h
//  Sonora
//

#include <JuceHeader.h>

#ifndef DSPMath_h
#define DSPMath_h

//Decibel to Linear (0.0 -> 1.0)
inline float dB2lin(float dB){
    return(pow(1.122018454301963, dB));
}

//semitone to frequency (same as martijn's dP2ff)
inline float semi2freq(float semi){
    //return(pow(2,semi/12));
    return(exp2f(semi*0.0833333f));
}

//midi pitch to freq
inline float p2f(float pitch){
    //return(8.1757989*pow(1.0594631, pitch));
    return(semi2freq(pitch)*8.1757989);
}

//Frequency to MIDI
//From https://gist.github.com/mazbox/3742083
inline float ftom(float f){
    return (17.3123405046 * log(.12231220585 * f));
}


//Map - scales from an input range to an output range
//from https://www.arduino.cc/reference/en/language/functions/math/map/
inline float map(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

//From Martijn Zwartjes
inline float max(float a, float b){
    return(a > b ? a : b);
}

inline float min(float a, float b){
    return(a < b ? a : b);
}

//experimental fast tanh from Aleksey Vaneev
inline double fasttanh( const double x )
{
    const double ax = fabs( x );
    const double x2 = x * x;

    return( x * ( 2.45550750702956 + 2.45550750702956 * ax +
        ( 0.893229853513558 + 0.821226666969744 * ax ) * x2 ) /
        ( 2.44506634652299 + ( 2.44506634652299 + x2 ) *
        fabs( x + 0.814642734961073 * x * ax )));
}

inline float lerp(float low, float high, float t){
    return(low*(1.0f-t) + high*t);
}

//linear crossfade from Martijn Zwartjes
inline float xFadeL(float i0, float i1, float x){
    return(((i1-i0)*x)+i0);
}

#endif /* DSPMath_h */
