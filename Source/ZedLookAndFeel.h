/*
  ==============================================================================

    ZedLookAndFeel.h
    Created: 28 Dec 2020 7:54:33pm
    Author:  Jim Murphy

  ==============================================================================
*/

#pragma once

class ZedLookAndFeel : public LookAndFeel_V4{
public:
    ZedLookAndFeel(){
        Typeface::Ptr tface = Typeface::createSystemTypefaceFor(BinaryData::Galvji_ttc, BinaryData::Galvji_ttcSize);
        const Colour textColour (255, 255, 255);
        setColour(ComboBox::textColourId, textColour);
        setColour(Label::textColourId, textColour);

        LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypeface (tface);
    }
    
    ~ZedLookAndFeel(){};
    
    private:
};
