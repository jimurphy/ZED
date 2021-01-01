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
FilterSpline::FilterSpline()
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.

}

FilterSpline::~FilterSpline()
{
}

void FilterSpline::paint (juce::Graphics& g)
{

    g.setGradientFill(juce::ColourGradient(backgroundColourGradient1, 0, 0, backgroundColourGradient2, 200, 200, false));
    
    g.fillRect (getLocalBounds()); //Draw rect the size of main frame to fill w/ gradient
    
    drawSpline(g);
}

void FilterSpline::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
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
    
    drawControlPoints(g);
    connectControlPoints(g);
    calculateLowpassControlPoints(80.0f, 0.7f);
    
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
            
            g.fillEllipse(qx, qy, 3, 3);
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
    
    float cutoffFreqValue = map(c, 0.0f, 127.0f, 0.0f, area.getWidth());
    float resPeakValue = map(q, 0.0f, 1.0f, area.getHeight(), 0.0f); //higher res = narrower band
    float peakWidth = map(q, 0.0f, 1.0f, 50.0f, 1.0f);
    
    //repeat first 3 and last 3 values to clamp spline to control points
    ctrlX[0] = 0;
    ctrlX[1] = 0;
    ctrlX[2] = 0;
    ctrlX[3] = cutoffFreqValue - (peakWidth*1.25); //scale peakWidth a bit for cosmetic symmetry
    ctrlX[4] = cutoffFreqValue;
    ctrlX[5] = cutoffFreqValue + peakWidth;
    ctrlX[6] = area.getWidth();
    ctrlX[7] = area.getWidth();
    ctrlX[8] = area.getWidth();
    
    ctrlY[0] = area.getHeight()/2.0f;
    ctrlY[1] = area.getHeight()/2.0f;
    ctrlY[2] = area.getHeight()/2.0f;
    ctrlY[3] = area.getHeight()/2.0f;
    ctrlY[4] = resPeakValue;
    ctrlY[5] = area.getHeight();
    ctrlY[6] = area.getHeight();
    ctrlY[7] = area.getHeight();
    ctrlY[8] = area.getHeight();

}
