/*
  ==============================================================================

    FilterSpline.h
    Created: 28 Dec 2020 8:09:44pm
    Author:  Jim Murphy

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
*/
class FilterSpline  : public juce::Component
{
public:
    FilterSpline();
    ~FilterSpline() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void setPitch(float);
    void setRes(float);
    
    void mouseDrag (const MouseEvent& event) override
    {
        DBG("Drag at: " << event.getPosition().toString());
    }

    
private:
    //Uniform b-spline basis functions, precomputed from
    //http://www2.cs.uregina.ca/~anima/408/Notes/Interpolation/UniformBSpline.htm
    float basis0(float);
    float basis1(float);
    float basis2(float);
    float basis3(float);
        
    void drawSpline(juce::Graphics&);
    void drawControlPoints(juce::Graphics&);
    void connectControlPoints(juce::Graphics&);
    void calculateLowpassControlPoints(float, float);

    //LPF control points for cubic bspline
    std::vector<float> ctrlX{ 0,  0,  0,  60, 90,  110, 130, 160, 219, 219, 219};
    std::vector<float> ctrlY{ 55, 55, 55, 55, 55,  10,  110, 110, 110, 110, 110};
    
    float maxSteps = 100.0f;
    float pitch = 64.0f; //0-127
    float res = 0.7f;

    //COLOURS
    Colour backgroundColourGradient1 = juce::Colour(0xFF684A52);
    Colour backgroundColourGradient2 = juce::Colour(0xFF87A0B2);
    Colour splineColour              = juce::Colour(0xFFA4BEF3);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterSpline)
};
