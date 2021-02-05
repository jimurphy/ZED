/*
  ==============================================================================

    FilterSpline.cpp
    Created: 28 Dec 2020 8:09:44pm
    Author:  Jim Murphy

  ==============================================================================
*/

#include <JuceHeader.h>
#include "FilterSpline.h"
#include "DSPMath.h"

//==============================================================================
FilterSpline::FilterSpline(AudioProcessorValueTreeState& vts) : valueTreeState (vts)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.

}

FilterSpline::~FilterSpline()
{
}

void FilterSpline::paint (juce::Graphics& g)
{
    g.setGradientFill(juce::ColourGradient(backgroundColourGradient1, 0, 0, backgroundColourGradient2, getLocalBounds().getWidth(), getLocalBounds().getHeight(), false));
    
    g.fillRect (getLocalBounds()); //Draw rect the size of main frame to fill w/ gradient
    
    drawSpline(g);
}

void FilterSpline::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
}

void FilterSpline::setMode(int m){
    filtermode = m;
    repaint();
}

void FilterSpline::setPitch(float p){
    pitch = p;
    repaint();
}

void FilterSpline::setRes(float r){
    res = r;
    repaint();
}

float FilterSpline::basis0(float u){
    return (((1 - u) * (1 - u) * (1 - u)) / 6);
}

float FilterSpline::basis1(float u){
    return (((3 * (u * u * u)) - (6 * (u * u)) + 4) / 6);
}

float FilterSpline::basis2(float u){
    return ((-3 * (u * u * u) + 3 * (u * u) + (3 * u) + 1) / 6);
}

float FilterSpline::basis3(float u){
    return ((u * u * u) / 6);
}

void FilterSpline::drawSpline(juce::Graphics& g){
    
    //drawControlPoints(g);
    //connectControlPoints(g);
    
    //Check what mode of filter is selected, draw appropriate type
    switch(filtermode){
        case lpf:
            calculateLowpassControlPoints(pitch, res);
            break;
        case hpf:
            calculateHighpassControlPoints(pitch, res);
            break;
        case bpf:
            calculateBandpassControlPoints(pitch, res);
            break;
        case brf:
            calculateBandrejectControlPoints(pitch, res);
            break;
        default:
            calculateLowpassControlPoints(pitch, res);
            break;
    }
    
    g.setColour (splineColour);
    int m = (int) ctrlX.size();
    
    for(int i = 0; i < m - 3; ++i){
        for(int j = 0; j < maxSteps; ++j){
            float u = j / maxSteps;
            float qx = basis0(u) * ctrlX[i] +
                       basis1(u) * ctrlX[i + 1] +
                       basis2(u) * ctrlX[i + 2] +
                       basis3(u) * ctrlX[i + 3];
                
            float qy = basis0(u) * ctrlY[i] +
                       basis1(u) * ctrlY[i + 1] +
                       basis2(u) * ctrlY[i + 2] +
                       basis3(u) * ctrlY[i + 3];
            
            g.fillEllipse(qx, qy, 2, 2);
        }
    }
}

void FilterSpline::drawControlPoints(juce::Graphics& g){
    g.setColour (juce::Colours::lightgreen);
    for(int i = 0; i < ctrlX.size(); ++i)
        g.fillEllipse(ctrlX[i] - 3, ctrlY[i] - 3, 6, 6); //offset by radius to draw at centre
}

void FilterSpline::connectControlPoints(juce::Graphics& g){
    g.setColour (juce::Colours::white);
    
    Path myPath;
    myPath.startNewSubPath (ctrlX[0], ctrlY[0]);
    for(int i = 1; i < ctrlX.size() - 1; ++i){
        myPath.lineTo (ctrlX[i], ctrlY[i]);
    }
    g.strokePath (myPath, PathStrokeType (1.0f));
}

//Expects cutoff in MIDI range (0-127) and resonance values between 0-1
void FilterSpline::calculateLowpassControlPoints(float c, float q){
    auto area = getLocalBounds();
    
    float qScale = q / 2.0f;
    float cutoffFreqValue = map(c, 0.0f, 127.0f, 0.0f, area.getWidth());
    float resPeakValue = map(min(qScale, 1.0f), 0.0f, 1.0f, area.getHeight()/1.5f, -50.0f); //narrows band
    float peakWidth = map(min(q, 1.0f), 0.0f, 1.0f, area.getWidth()/10.0f, 1.0f);
    
    //repeat first 3 and last 3 values to clamp spline to control points
    ctrlX[0] = -10; //Clamp left
    ctrlX[1] = -10; //Clamp left
    ctrlX[2] = -10; //Clamp left
    
    ctrlX[3] = max(-10.0f, cutoffFreqValue - (peakWidth + (area.getWidth()/10.0f))); //left pb 2
    ctrlX[4] = max(-10.0f, cutoffFreqValue - peakWidth); //knee left pb
    ctrlX[5] = cutoffFreqValue; //midpoint
    ctrlX[6] = cutoffFreqValue + peakWidth; //knee right sb
    ctrlX[7] = cutoffFreqValue + peakWidth + (area.getWidth()/4.0f); //far right sb
    ctrlX[8] = area.getWidth(); //Clamp right
    ctrlX[9] = area.getWidth(); //Clamp right
    ctrlX[10] = area.getWidth();//Clamp right

    ctrlY[0] = area.getHeight()/2.0f; //Clamp left
    ctrlY[1] = area.getHeight()/2.0f; //Clamp left
    ctrlY[2] = area.getHeight()/2.0f; //Clamp left
    ctrlY[3] = area.getHeight()/2.0f; //left pb 2
    ctrlY[4] = area.getHeight()/2.0f; //knee left pb
    ctrlY[5] = resPeakValue; //midpoint
    ctrlY[6] = area.getHeight()/1.25f; //knee right sb
    ctrlY[7] = area.getHeight();  //far right sb
    ctrlY[8] = area.getHeight();  //Clamp right
    ctrlY[9] = area.getHeight();  //Clamp right
    ctrlY[10] = area.getHeight(); //Clamp right
}

//Expects cutoff in MIDI range (0-127) and resonance values between 0-1
void FilterSpline::calculateHighpassControlPoints(float c, float q){
    auto area = getLocalBounds();
    
    float qScale = q / 2.0f;
    float cutoffFreqValue = map(c, 0.0f, 127.0f, 0.0f, area.getWidth());
    float resPeakValue = map(min(qScale, 1.0f), 0.0f, 1.0f, area.getHeight()/1.5f, -50.0f); //narrows band
    float peakWidth = map(min(q, 1.0f), 0.0f, 1.0f, area.getWidth()/10.0f, 1.0f);
    
    //X values for LPF
    //repeat first 3 and last 3 values to clamp spline to control points
    ctrlX[0] = -10; //Clamp left
    ctrlX[1] = -10; //Clamp left
    ctrlX[2] = -10; //Clamp left
    
    //Far stopband
    ctrlX[3] = cutoffFreqValue - peakWidth - (area.getWidth()/4.0f); //far right sb

    //knee right sb
    ctrlX[4] = cutoffFreqValue - peakWidth;
    
    //midpoint
    ctrlX[5] = cutoffFreqValue; //midpoint

    //passband knee
    ctrlX[6] = cutoffFreqValue + peakWidth;
    
    //far passband knee
    ctrlX[7] = cutoffFreqValue + (peakWidth + (area.getWidth()/10.0f)); //left pb 2
    
    //clamp
    ctrlX[8] = area.getWidth(); //Clamp right
    ctrlX[9] = area.getWidth(); //Clamp right
    ctrlX[10] = area.getWidth();//Clamp right

    //Y Values for HPF
    ctrlY[0] = area.getHeight(); //Clamp left
    ctrlY[1] = area.getHeight(); //Clamp left
    ctrlY[2] = area.getHeight(); //Clamp left
    
    //Far stopband
    ctrlY[3] = area.getHeight();
    
    //knee stopband
    ctrlY[4] = area.getHeight()/1.25;
    
    //midpoint
    ctrlY[5] = resPeakValue;
    
    //passband knee
    ctrlY[6] = area.getHeight()/2.0f;
    
    //far passband knee
    ctrlY[7] = area.getHeight()/2.0f;
    
    //clamp right
    ctrlY[8] = area.getHeight()/2.0f;
    ctrlY[9] = area.getHeight()/2.0f;
    ctrlY[10] = area.getHeight()/2.0f;
}

//Expects cutoff in MIDI range (0-127) and resonance values between 0-1
void FilterSpline::calculateBandpassControlPoints(float c, float q){
    auto area = getLocalBounds();
    
    float qScale = q / 2.0f;
    
    float cutoffFreqValue = map(c, 0.0f, 127.0f, 0.0f, area.getWidth());
    
    float resPeakValue = map(min(qScale, 1.0f), 0.0f, 1.0f, area.getHeight()/2.0f, -50.0f); //narrows band
    
    float peakWidth = map(min(q, 1.0f), 0.0f, 1.0f, area.getWidth()/10.0f, 1.0f);
    
    ctrlX[0] = cutoffFreqValue - area.getWidth()/2.0f;
    ctrlX[1] = cutoffFreqValue - area.getWidth()/2.0f;
    ctrlX[2] = cutoffFreqValue - area.getWidth()/2.0f;

    ctrlX[3] = cutoffFreqValue - (4.0f * peakWidth);
    ctrlX[4] = cutoffFreqValue - peakWidth;
    
    ctrlX[5] = cutoffFreqValue;
    
    ctrlX[6] = cutoffFreqValue + peakWidth;
    ctrlX[7] = cutoffFreqValue + (4.0f * peakWidth);
    
    ctrlX[8] = cutoffFreqValue + area.getWidth()/2.0f;
    ctrlX[9] = cutoffFreqValue + area.getWidth()/2.0f;
    ctrlX[10] = cutoffFreqValue + area.getWidth()/2.0f;
    
    ctrlY[0] = area.getHeight();
    ctrlY[1] = area.getHeight();
    ctrlY[2] = area.getHeight();

    ctrlY[3] = area.getHeight()/1.25;
    ctrlY[4] = area.getHeight()/2.0f;
    
    ctrlY[5] = resPeakValue;
    
    ctrlY[6] = area.getHeight()/2.0f;
    ctrlY[7] = area.getHeight()/1.25f;

    ctrlY[8] = area.getHeight();
    ctrlY[9] = area.getHeight();
    ctrlY[10] = area.getHeight();
}

//Expects cutoff in MIDI range (0-127) and resonance values between 0-1
void FilterSpline::calculateBandrejectControlPoints(float c, float q){
    auto area = getLocalBounds();
    
    float qScale = q / 2.0f;
    
    float cutoffFreqValue = map(c, 0.0f, 127.0f, 0.0f, area.getWidth());
    
    float resPeakValue = map(min(qScale, 1.0f), 0.0f, 1.0f, area.getHeight(), area.getHeight() * 2.0f); //narrows band
    
    float peakWidth = map(min(q, 1.0f), 0.0f, 1.0f, area.getWidth()/10.0f, 1.0f);
    
    ctrlX[0] = cutoffFreqValue - area.getWidth();
    ctrlX[1] = cutoffFreqValue - area.getWidth();
    ctrlX[2] = cutoffFreqValue - area.getWidth();

    ctrlX[3] = cutoffFreqValue - (4.0f * peakWidth);
    ctrlX[4] = cutoffFreqValue - peakWidth;
    
    ctrlX[5] = cutoffFreqValue;
    
    ctrlX[6] = cutoffFreqValue + peakWidth;
    ctrlX[7] = cutoffFreqValue + (4.0f * peakWidth);
    
    ctrlX[8] = cutoffFreqValue + area.getWidth();
    ctrlX[9] = cutoffFreqValue + area.getWidth();
    ctrlX[10] = cutoffFreqValue + area.getWidth();
    
    ctrlY[0] = area.getHeight()/2.0f;
    ctrlY[1] = area.getHeight()/2.0f;
    ctrlY[2] = area.getHeight()/2.0f;

    ctrlY[3] = area.getHeight()/2.0f;
    ctrlY[4] = area.getHeight()/2.0f;
    
    ctrlY[5] = resPeakValue;
    
    ctrlY[6] = area.getHeight()/2.0f;
    ctrlY[7] = area.getHeight()/2.0f;

    ctrlY[8] = area.getHeight()/2.0f;
    ctrlY[9] = area.getHeight()/2.0f;
    ctrlY[10] = area.getHeight()/2.0f;
}


void FilterSpline::mouseDrag (const MouseEvent& event)
{
    auto area = getLocalBounds();
    auto cutoffParamValue = valueTreeState.getParameterAsValue("cutoff");
    auto resParamValue = valueTreeState.getParameterAsValue("resonance");

    float mouseXDragPos = event.getPosition().x;
    float mouseYDragPos = event.getPosition().y;

    cutoffParamValue.setValue(map(mouseXDragPos, 0.0f, area.getWidth(), 0.0f, 127.0f));
    resParamValue.setValue(map(mouseYDragPos, 0.0f, area.getHeight(), 1.1f, 0.0f));
}
