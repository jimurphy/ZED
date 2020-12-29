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
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterSpline)
};
