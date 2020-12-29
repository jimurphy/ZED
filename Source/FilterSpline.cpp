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

    g.setColour (juce::Colours::white);
    g.setFont (14.0f);
    g.drawText ("FilterSpline", getLocalBounds(),
                juce::Justification::centred, true);   // draw some placeholder text
    
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
    g.setColour (juce::Colours::orange);
    juce::Rectangle<int> area (juce::Point<int> (10, 10),
                               juce::Point<int> (50, 50));
 
    g.fillRect (area);
}
