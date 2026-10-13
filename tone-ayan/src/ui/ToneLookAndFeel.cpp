#include "ToneLookAndFeel.h"

namespace tone::ui
{
ToneLookAndFeel::ToneLookAndFeel()
{
    using namespace colours;
    setColourScheme ({ background, panel, panelLight, gridStrong, text, accent, juce::Colours::black, panelLight, text });
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::TextButton::buttonColourId, panelLight);
    setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.85f));
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ComboBox::backgroundColourId, panelLight);
    setColour (juce::ComboBox::outlineColourId, gridStrong);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.4f));
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::backgroundColourId, panelLight);
    setColour (juce::Slider::thumbColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ProgressBar::foregroundColourId, accent);
    setColour (juce::ProgressBar::backgroundColourId, panelLight);
    setColour (juce::ScrollBar::thumbColourId, gridStrong.brighter (0.2f));
    setColour (juce::TooltipWindow::backgroundColourId, panelLight);
}

void ToneLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : bg;
    if (down)
        c = c.brighter (0.15f);
    else if (highlighted)
        c = c.brighter (0.07f);
    if (! b.isEnabled())
        c = c.withMultipliedAlpha (0.5f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::gridStrong);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void ToneLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                        juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    const float cy = (float) y + (float) h * 0.5f;
    const juce::Rectangle<float> track ((float) x, cy - 2.0f, (float) w, 4.0f);
    g.setColour (colours::panelLight.brighter (0.1f));
    g.fillRoundedRectangle (track, 2.0f);

    // fill from the neutral value (the double-click return value) to the thumb
    const float zeroX = (float) s.getPositionOfValue (s.getDoubleClickReturnValue());
    const float from = std::min (zeroX, pos), to = std::max (zeroX, pos);
    g.setColour (s.isEnabled() ? colours::accent : colours::gridStrong);
    g.fillRoundedRectangle ({ from, cy - 2.0f, std::max (1.0f, to - from), 4.0f }, 2.0f);
    g.setColour (s.isEnabled() ? colours::text : colours::textDim);
    g.fillEllipse (pos - 6.0f, cy - 6.0f, 12.0f, 12.0f);
}

juce::Font ToneLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions ((float) std::min (14, buttonHeight - 10)));
}
} // namespace tone::ui
