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

private:
    
    //Uniform b-spline basis functions, precomputed from
    //http://www2.cs.uregina.ca/~anima/408/Notes/Interpolation/UniformBSpline.htm
    float basis0(float);
    float basis1(float);
    float basis2(float);
    float basis3(float);
    
    void drawSpline(juce::Graphics&);
    
    std::vector<float> ctrlX{ 10, 10, 10, 50, 60, 80, 130, 200, 200, 200};
    std::vector<float> ctrlY{ 50, 50, 50, 40, 70, 77, 10, 100, 100, 100};
    
    float maxSteps = 100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterSpline)
};
