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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterSpline)
};
