/*
  ==============================================================================

    FilterSpline.cpp
    Created: 28 Dec 2020 8:09:44pm
    Author:  Jim Murphy

  ==============================================================================
*/

#include <JuceHeader.h>
#include "FilterSpline.h"

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
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));   // clear the background

    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);   // draw an outline around the component
    
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

    g.setColour (juce::Colours::orange);
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
            
            g.fillEllipse(qx, qy, 1, 1);
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

