/*
  ==============================================================================

    CustomTooltip.h
    Created: 28 Jan 2021 10:19:58am
    Author:  Jim Murphy

  ==============================================================================
*/

#pragma once

class CustomTooltip : public TooltipWindow{
public:
    
    //sets tooltip to be transparent (/juce_gui_basics/windows/TooltipWindow)
    CustomTooltip(){
        setOpaque (false);
    }
    ~CustomTooltip(){};

private:
};
