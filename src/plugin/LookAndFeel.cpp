#include "LookAndFeel.h"

namespace nsw::ui
{
juce::Font font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

LookAndFeel::LookAndFeel()
{
    using namespace colours;
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::arrowColourId, peach);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff201c26));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, peach.withAlpha (0.18f));
    setColour (juce::PopupMenu::highlightedTextColourId, peach);
    setColour (juce::TextButton::buttonColourId, panel);
    setColour (juce::TextButton::buttonOnColourId, peach);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, background);
    setColour (juce::ToggleButton::textColourId, textDim);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff221e29));
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, panelEdge);
}

void LookAndFeel::drawGlowArc (juce::Graphics& g, juce::Point<float> c, float r, float from, float to, float thickness,
                               float glow)
{
    if (std::abs (to - from) < 1.0e-4f)
        return;
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, r, r, 0.0f, from, to, true);

    // Glow: progressively wider, fainter strokes.
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (colours::peach.withAlpha (0.07f * glow));
        g.strokePath (arc, juce::PathStrokeType (thickness + (float) i * thickness * 0.9f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }
    juce::ColourGradient grad (colours::peach, c.x - r, c.y + r, colours::lilac, c.x + r, c.y - r, false);
    g.setGradientFill (grad);
    g.strokePath (arc, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle,
                                    float endAngle, juce::Slider& s)
{
    using namespace colours;
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float thickness = std::max (3.0f, radius * 0.085f);
    const float arcR = radius - thickness * 1.6f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool enabled = s.isEnabled();

    // Ambient glow under the knob, stronger with more "amount".
    const float amount = bipolar ? std::abs (pos - 0.5f) * 2.0f : pos;
    {
        juce::ColourGradient halo (peach.withAlpha (enabled ? 0.05f + 0.10f * amount : 0.0f), c.x, c.y,
                                   peach.withAlpha (0.0f), c.x + radius, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (bounds.expanded (2.0f));
    }

    // Track.
    juce::Path trackPath;
    trackPath.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::track);
    g.strokePath (trackPath, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc.
    if (enabled)
    {
        const float origin = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        drawGlowArc (g, c, arcR, std::min (origin, angle), std::max (origin, angle), thickness, 0.6f + 0.4f * amount);
    }

    // Body.
    const float bodyR = arcR - thickness * 1.5f;
    {
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), (int) (bodyR * 0.35f), { 0, (int) (bodyR * 0.08f) })
            .drawForPath (g, [&] { juce::Path p; p.addEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2); return p; }());
        juce::ColourGradient body (knobTop, c.x, c.y - bodyR, knobBottom, c.x, c.y + bodyR, false);
        g.setGradientFill (body);
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);
    }

    // Pointer: a soft glowing dot near the rim, plus a short line.
    const auto tip = c.getPointOnCircumference (bodyR * 0.72f, angle);
    const auto inner = c.getPointOnCircumference (bodyR * 0.38f, angle);
    const float dot = std::max (2.5f, bodyR * 0.09f);
    g.setColour (peach.withAlpha (enabled ? 0.25f : 0.1f));
    g.fillEllipse (juce::Rectangle<float> (dot * 4.0f, dot * 4.0f).withCentre (tip));
    g.setColour (enabled ? peach : textFaint);
    g.drawLine ({ inner, tip }, std::max (1.5f, dot * 0.7f));
    g.fillEllipse (juce::Rectangle<float> (dot * 2.0f, dot * 2.0f).withCentre (tip));
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    juce::Slider::SliderStyle, juce::Slider& s)
{
    using namespace colours;
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float trackH = std::min (12.0f, b.getHeight() * 0.35f);
    const auto trackR = b.withSizeKeepingCentre (b.getWidth(), trackH);

    g.setColour (track);
    g.fillRoundedRectangle (trackR, trackH * 0.5f);

    const auto fill = trackR.withRight (pos);
    if (s.isEnabled() && fill.getWidth() > 0.5f)
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (peach.withAlpha (0.06f));
            g.fillRoundedRectangle (fill.expanded ((float) i * 3.0f), (trackH + (float) i * 6.0f) * 0.5f);
        }
        juce::ColourGradient grad (peach, fill.getX(), 0, lilac, trackR.getRight(), 0, false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, trackH * 0.5f);
    }

    // Thumb: glowing pill.
    const auto thumb = juce::Rectangle<float> (trackH * 1.6f, b.getHeight() * 0.8f).withCentre ({ pos, b.getCentreY() });
    juce::DropShadow (peach.withAlpha (0.45f), 14, {}).drawForRectangle (g, thumb.toNearestInt());
    juce::ColourGradient body (juce::Colour (0xfffff1ea), thumb.getX(), thumb.getY(), peach, thumb.getX(),
                               thumb.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (thumb, thumb.getWidth() * 0.5f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted,
                                        bool down)
{
    using namespace colours;
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const float corner = r.getHeight() * 0.5f;
    if (on)
    {
        juce::DropShadow (peach.withAlpha (0.35f), 10, {}).drawForRectangle (g, r.toNearestInt());
        juce::ColourGradient grad (peach, r.getX(), r.getY(), peachDeep, r.getRight(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, corner);
    }
    else
    {
        g.setColour (panel.brighter (highlighted ? 0.12f : 0.0f).darker (down ? 0.2f : 0.0f));
        g.fillRoundedRectangle (r, corner);
        g.setColour (panelEdge);
        g.drawRoundedRectangle (r, corner, 1.0f);
    }
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool highlighted, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? colours::background : (highlighted ? colours::text : colours::textDim));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    using namespace colours;
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const float corner = r.getHeight() * 0.5f;
    g.setColour (panel.brighter (highlighted ? 0.1f : 0.0f));
    g.fillRoundedRectangle (r, corner);
    g.setColour (on ? peach.withAlpha (0.7f) : panelEdge);
    g.drawRoundedRectangle (r, corner, 1.0f);

    // LED
    const float d = r.getHeight() * 0.36f;
    const auto led = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + corner, r.getCentreY() });
    if (on)
    {
        g.setColour (peach.withAlpha (0.3f));
        g.fillEllipse (led.expanded (d * 0.6f));
    }
    g.setColour (on ? peach : textFaint);
    g.fillEllipse (led);

    g.setFont (font (12.5f, true));
    g.setColour (on ? text : textDim);
    g.drawText (b.getButtonText(), r.withTrimmedLeft (corner * 1.7f).withTrimmedRight (corner * 0.5f),
                juce::Justification::centred);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace colours;
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    g.setColour (panel.brighter (box.isMouseOver (true) ? 0.08f : 0.0f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    juce::Path arrow;
    const float ax = r.getRight() - 18.0f, ay = r.getCentreY();
    arrow.addTriangle (ax - 5.0f, ay - 2.5f, ax + 5.0f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (peach);
    g.fillPath (arrow);
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (box.getLocalBounds().withTrimmedLeft (10).withTrimmedRight (28));
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (colours::panelEdge);
    g.drawRect (0, 0, w, h);
}
} // namespace nsw::ui
