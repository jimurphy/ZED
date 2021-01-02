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
    
    //Override the slider
    void drawLinearSlider (Graphics& g, int x, int y, int width, int height,
                                           float sliderPos,
                                           float minSliderPos,
                                           float maxSliderPos,
                                           const Slider::SliderStyle style, Slider& slider) override
    {
        if (slider.isBar())
        {
            g.setColour (slider.findColour (Slider::trackColourId));
            g.fillRect (slider.isHorizontal() ? Rectangle<float> (static_cast<float> (x), y + 0.5f, sliderPos - x, height - 1.0f)
                                              : Rectangle<float> (x + 0.5f, sliderPos, width - 1.0f, y + (height - sliderPos)));
        }
        else
        {
            auto isTwoVal   = (style == Slider::SliderStyle::TwoValueVertical   || style == Slider::SliderStyle::TwoValueHorizontal);
            auto isThreeVal = (style == Slider::SliderStyle::ThreeValueVertical || style == Slider::SliderStyle::ThreeValueHorizontal);

            auto trackWidth = jmin (6.0f, slider.isHorizontal() ? height * 0.15f : width * 0.15f);

            Point<float> startPoint (slider.isHorizontal() ? x : x + width * 0.5f,
                                     slider.isHorizontal() ? y + height * 0.5f : height + y);

            Point<float> endPoint (slider.isHorizontal() ? width + x : startPoint.x,
                                   slider.isHorizontal() ? startPoint.y : y);

            Path backgroundTrack;
            backgroundTrack.startNewSubPath (startPoint);
            backgroundTrack.lineTo (endPoint);
            
            g.setColour (slider.findColour (Slider::trackColourId));
            g.strokePath (backgroundTrack, { trackWidth, PathStrokeType::curved, PathStrokeType::rounded });

            Path valueTrack;
            Point<float> minPoint, maxPoint, thumbPoint;

            if (isTwoVal || isThreeVal)
            {
                minPoint = { slider.isHorizontal() ? minSliderPos : width * 0.5f,
                             slider.isHorizontal() ? height * 0.5f : minSliderPos };

                if (isThreeVal)
                    thumbPoint = { slider.isHorizontal() ? sliderPos : width * 0.5f,
                                   slider.isHorizontal() ? height * 0.5f : sliderPos };

                maxPoint = { slider.isHorizontal() ? maxSliderPos : width * 0.5f,
                             slider.isHorizontal() ? height * 0.5f : maxSliderPos };
            }
            else
            {
                auto kx = slider.isHorizontal() ? sliderPos : (x + width * 0.5f);
                auto ky = slider.isHorizontal() ? (y + height * 0.5f) : sliderPos;

                minPoint = startPoint;
                maxPoint = { kx, ky };
            }

            auto thumbWidth = getSliderThumbRadius (slider);

            if (! isTwoVal)
            {
                g.setColour (slider.findColour (Slider::trackColourId));
                if(slider.isHorizontal()){
                    g.fillRect (Rectangle<float> (static_cast<float> (thumbWidth*0.33), static_cast<float> (20.0)).withCentre (isThreeVal ? thumbPoint : maxPoint));
                }
                else{
                    g.fillRect (Rectangle<float> (static_cast<float> (20.0f), static_cast<float> (thumbWidth*0.33)).withCentre (isThreeVal ? thumbPoint : maxPoint));
                }
            }
        }
    }

    
    private:
};
