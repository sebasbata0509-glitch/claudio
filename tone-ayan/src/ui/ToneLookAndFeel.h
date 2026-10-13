#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace tone::ui
{
namespace colours
{
inline const juce::Colour background { 0xff14161b };
inline const juce::Colour panel      { 0xff1c1f26 };
inline const juce::Colour panelLight { 0xff252932 };
inline const juce::Colour rowWhite   { 0xff1f232a };
inline const juce::Colour rowBlack   { 0xff191c22 };
inline const juce::Colour rowInScale { 0xff22303d };
inline const juce::Colour gridLine   { 0xff2a2f39 };
inline const juce::Colour gridStrong { 0xff39404c };
inline const juce::Colour text       { 0xffd8dbe2 };
inline const juce::Colour textDim    { 0xff8a909c };
inline const juce::Colour note       { 0xffe39b3a };
inline const juce::Colour noteEdited { 0xfff0b860 };
inline const juce::Colour selected   { 0xff3fc6b8 };
inline const juce::Colour curve      { 0xeeffffff };
inline const juce::Colour curveOrig  { 0x66ffffff };
inline const juce::Colour playhead   { 0xffff5a5f };
inline const juce::Colour accent     { 0xff3fc6b8 };
inline const juce::Colour record     { 0xffe5484d };
} // namespace colours

class ToneLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ToneLookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};
} // namespace tone::ui
