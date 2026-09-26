#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace nsw::ui
{
/** Dark, soft-glow palette with a warm peach accent and a lilac partner. */
namespace colours
{
    inline const juce::Colour background { 0xff131117 };
    inline const juce::Colour backgroundTop { 0xff1b1720 };
    inline const juce::Colour panel { 0xff1c1922 };
    inline const juce::Colour panelEdge { 0xff2d2835 };
    inline const juce::Colour knobTop { 0xff2c2733 };
    inline const juce::Colour knobBottom { 0xff17141c };
    inline const juce::Colour track { 0xff2f2a38 };
    inline const juce::Colour text { 0xffede7f3 };
    inline const juce::Colour textDim { 0xff9d95ab };
    inline const juce::Colour textFaint { 0xff5f586b };
    inline const juce::Colour peach { 0xffffb49a };
    inline const juce::Colour peachDeep { 0xffff8f7a };
    inline const juce::Colour lilac { 0xffc7b3ff };
    inline const juce::Colour mint { 0xff9ff2d2 };
} // namespace colours

juce::Font font (float height, bool bold = false);

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle,
                           juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (15.0f, true); }
    juce::Font getPopupMenuFont() override { return font (15.0f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (13.0f, true); }
    juce::Font getLabelFont (juce::Label&) override { return font (13.0f); }

    /** Soft glow + gradient arc used by knobs and meters. */
    static void drawGlowArc (juce::Graphics&, juce::Point<float> centre, float radius, float from, float to,
                             float thickness, float glow);
};
} // namespace nsw::ui
