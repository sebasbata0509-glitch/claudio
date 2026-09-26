#include "Widgets.h"

namespace nsw::ui
{
juce::String noteName (float midi)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int n = (int) std::lround (midi);
    return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
}

//==============================================================================
Knob::Knob (APVTS& state, const juce::String& paramId, const juce::String& n, Size s, bool bipolar)
    : name (n), size (s)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.getProperties().set ("bipolar", bipolar);
    slider.setMouseDragSensitivity (s == Size::Large ? 320 : 220);
    slider.setVelocityBasedMode (false);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);

    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64) + " - double-click to reset");
    }
    slider.onValueChange = [this] { repaint(); };
}

void Knob::resized()
{
    auto r = getLocalBounds();
    const int labelH = size == Size::Large ? 26 : 18;
    const int valueH = size == Size::Large ? 24 : 18;
    r.removeFromTop (labelH);
    r.removeFromBottom (valueH);
    const int d = std::min (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (d, d));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    const bool large = size == Size::Large;
    const int labelH = large ? 26 : 18;
    const int valueH = large ? 24 : 18;

    g.setColour (colours::textDim);
    g.setFont (font (large ? 15.0f : 11.5f, true).withExtraKerningFactor (0.18f));
    g.drawText (name.toUpperCase(), r.removeFromTop (labelH), juce::Justification::centred);

    g.setColour (slider.isMouseOverOrDragging() ? colours::peach : colours::text);
    g.setFont (font (large ? 17.0f : 12.5f, large));
    g.drawText (slider.getTextFromValue (slider.getValue()), r.removeFromBottom (valueH), juce::Justification::centred);
}

//==============================================================================
MixSlider::MixSlider (APVTS& state, const juce::String& paramId)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
}

void MixSlider::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedTop (22).withTrimmedLeft (44).withTrimmedRight (44));
}

void MixSlider::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (22);
    g.setFont (font (13.0f, true).withExtraKerningFactor (0.18f));
    g.setColour (colours::textDim);
    g.drawText ("DRY / WET", top, juce::Justification::centredLeft);
    g.setColour (colours::text);
    g.setFont (font (15.0f, true));
    g.drawText (slider.getTextFromValue (slider.getValue()), top, juce::Justification::centredRight);

    g.setFont (font (11.0f, true).withExtraKerningFactor (0.15f));
    g.setColour (colours::textFaint);
    g.drawText ("DRY", r.removeFromLeft (40), juce::Justification::centredLeft);
    g.drawText ("WET", r.removeFromRight (40), juce::Justification::centredRight);
}

//==============================================================================
SegmentedChoice::SegmentedChoice (APVTS& state, const juce::String& paramId, juce::StringArray l, juce::String c)
    : param (*state.getParameter (paramId)),
      attachment (param, [this] (float v) { value = v; repaint(); }, state.undoManager),
      labels (std::move (l)),
      caption (std::move (c))
{
    attachment.sendInitialUpdate();
    setTooltip (param.getName (64));
}

juce::Rectangle<float> SegmentedChoice::segmentArea() const
{
    auto r = getLocalBounds().toFloat();
    if (caption.isNotEmpty())
        r.removeFromTop (18.0f);
    return r.reduced (1.0f);
}

int SegmentedChoice::segmentAt (juce::Point<int> p) const
{
    const auto r = segmentArea();
    if (! r.contains (p.toFloat()))
        return -1;
    return juce::jlimit (0, labels.size() - 1, (int) ((p.x - r.getX()) / (r.getWidth() / (float) labels.size())));
}

void SegmentedChoice::paint (juce::Graphics& g)
{
    if (caption.isNotEmpty())
    {
        g.setFont (font (11.5f, true).withExtraKerningFactor (0.18f));
        g.setColour (colours::textDim);
        g.drawText (caption.toUpperCase(), getLocalBounds().removeFromTop (18), juce::Justification::centred);
    }
    const auto r = segmentArea();
    const float corner = r.getHeight() * 0.5f;
    g.setColour (colours::panel.darker (0.25f));
    g.fillRoundedRectangle (r, corner);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r, corner, 1.0f);

    // The value callback gives the denormalised value; map it to a segment index.
    const auto& range = param.getNormalisableRange();
    const int selected = juce::jlimit (0, labels.size() - 1, (int) std::lround (value - range.start));
    const float w = r.getWidth() / (float) labels.size();
    for (int i = 0; i < labels.size(); ++i)
    {
        const auto seg = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (2.0f);
        if (i == selected)
        {
            juce::DropShadow (colours::peach.withAlpha (0.35f), 10, {}).drawForRectangle (g, seg.toNearestInt());
            juce::ColourGradient grad (colours::peach, seg.getX(), seg.getY(), colours::peachDeep, seg.getRight(),
                                       seg.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (seg, seg.getHeight() * 0.5f);
        }
        else if (i == hover)
        {
            g.setColour (colours::panelEdge.withAlpha (0.8f));
            g.fillRoundedRectangle (seg, seg.getHeight() * 0.5f);
        }
        g.setColour (i == selected ? colours::background : colours::textDim);
        g.setFont (font (12.5f, true));
        g.drawText (labels[i], seg, juce::Justification::centred);
    }
}

void SegmentedChoice::mouseDown (const juce::MouseEvent& e)
{
    const int i = segmentAt (e.getPosition());
    if (i >= 0)
        attachment.setValueAsCompleteGesture (param.getNormalisableRange().start + (float) i);
}

void SegmentedChoice::mouseMove (const juce::MouseEvent& e)
{
    const int h = segmentAt (e.getPosition());
    if (h != hover)
    {
        hover = h;
        repaint();
    }
}

void SegmentedChoice::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

//==============================================================================
ParamToggle::ParamToggle (APVTS& state, const juce::String& paramId, const juce::String& text)
{
    button.setButtonText (text);
    addAndMakeVisible (button);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, button);
    if (auto* p = state.getParameter (paramId))
        button.setTooltip (p->getName (64));
}

//==============================================================================
void Section::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    juce::ColourGradient bg (colours::panel.brighter (0.03f), r.getX(), r.getY(), colours::panel.darker (0.12f), r.getX(),
                             r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r, 14.0f, 1.0f);

    if (caption.isNotEmpty())
    {
        g.setFont (font (12.0f, true).withExtraKerningFactor (0.3f));
        g.setColour (colours::peach.withAlpha (0.85f));
        g.drawText (caption.toUpperCase(), r.toNearestInt().reduced (18, 10).removeFromTop (16),
                    juce::Justification::centredLeft);
    }
    for (const auto& [text, area] : subCaptions)
    {
        g.setFont (font (10.5f, true).withExtraKerningFactor (0.3f));
        g.setColour (colours::textFaint);
        g.drawText (text.toUpperCase(), area, juce::Justification::centred);
        const float y = (float) area.getCentreY();
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (font (10.5f, true).withExtraKerningFactor (0.3f), text.toUpperCase());
        g.setColour (colours::panelEdge);
        const float cx = (float) area.getCentreX();
        g.drawLine ((float) area.getX() + 6.0f, y, cx - tw * 0.5f - 8.0f, y, 1.0f);
        g.drawLine (cx + tw * 0.5f + 8.0f, y, (float) area.getRight() - 6.0f, y, 1.0f);
    }
}

//==============================================================================
void Wordmark::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float h = r.getHeight();

    // Emblem: a rounded seed with two "swell" ripples.
    const float s = h * 0.78f;
    const auto e = juce::Rectangle<float> (s * 0.62f, s).withCentre ({ r.getX() + s * 0.42f, r.getCentreY() });
    juce::Path seed;
    seed.startNewSubPath (e.getCentreX(), e.getY());
    seed.cubicTo (e.getRight() + s * 0.05f, e.getY() + s * 0.30f, e.getRight(), e.getBottom() - s * 0.08f, e.getCentreX(), e.getBottom());
    seed.cubicTo (e.getX(), e.getBottom() - s * 0.08f, e.getX() - s * 0.05f, e.getY() + s * 0.30f, e.getCentreX(), e.getY());
    seed.closeSubPath();
    juce::DropShadow (colours::peach.withAlpha (0.55f), (int) (h * 0.35f), {}).drawForPath (g, seed);
    juce::ColourGradient seedFill (colours::peach, e.getX(), e.getY(), colours::lilac, e.getRight(), e.getBottom(), false);
    g.setGradientFill (seedFill);
    g.fillPath (seed);
    // Cap line across the seed.
    g.setColour (colours::background.withAlpha (0.55f));
    g.drawLine (e.getX() + e.getWidth() * 0.18f, e.getY() + e.getHeight() * 0.36f, e.getRight() - e.getWidth() * 0.18f,
                e.getY() + e.getHeight() * 0.36f, std::max (1.5f, h * 0.05f));

    for (int i = 1; i <= 2; ++i)
    {
        juce::Path ripple;
        const float rr = s * (0.42f + 0.2f * (float) i);
        ripple.addCentredArc (e.getCentreX(), e.getCentreY() + s * 0.06f, rr, rr, 0.0f, juce::degreesToRadians (50.0f),
                              juce::degreesToRadians (130.0f), true);
        g.setColour (colours::lilac.withAlpha (0.75f / (float) i));
        g.strokePath (ripple, juce::PathStrokeType (std::max (1.5f, h * 0.06f), juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // Lettering: "Nut" in peach, "Sweller" in lilac, glowing.
    const float textX = r.getX() + s * 1.25f;
    const auto f = font (h * 0.62f, true).withExtraKerningFactor (0.02f);
    juce::GlyphArrangement nut, sweller;
    nut.addLineOfText (f, "Nut", textX, r.getCentreY() + h * 0.21f);
    const float nutW = nut.getBoundingBox (0, -1, true).getWidth();
    sweller.addLineOfText (f, "Sweller", textX + nutW + h * 0.03f, r.getCentreY() + h * 0.21f);
    juce::Path pn, ps;
    nut.createPath (pn);
    sweller.createPath (ps);
    juce::DropShadow (colours::peach.withAlpha (0.45f), (int) (h * 0.3f), {}).drawForPath (g, pn);
    juce::DropShadow (colours::lilac.withAlpha (0.35f), (int) (h * 0.3f), {}).drawForPath (g, ps);
    g.setColour (colours::peach);
    g.fillPath (pn);
    juce::ColourGradient sw (colours::text, textX + nutW, r.getY(), colours::lilac, r.getRight(), r.getBottom(), false);
    g.setGradientFill (sw);
    g.fillPath (ps);
}

//==============================================================================
void PitchReadout::update (float frequency, float confidence, bool isVoiced, float outputShiftSemis, bool showOutput)
{
    shownConfidence += (confidence - shownConfidence) * 0.35f;
    shift = outputShiftSemis;
    showOut = showOutput;
    if (isVoiced && frequency > 0.0f)
    {
        // Light smoothing in the log domain so the readout is legible.
        shownHz = shownHz > 0.0f ? std::exp2 (std::log2 (shownHz) + 0.5f * (std::log2 (frequency) - std::log2 (shownHz)))
                                 : frequency;
        voiced = true;
        holdFrames = 8;
    }
    else if (holdFrames > 0)
    {
        --holdFrames;
    }
    else
    {
        voiced = false;
    }
    repaint();
}

void PitchReadout::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::background.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
    r = r.reduced (14.0f, 10.0f);

    auto header = r.removeFromTop (16.0f);
    g.setFont (font (11.0f, true).withExtraKerningFactor (0.25f));
    g.setColour (colours::textDim);
    g.drawText ("INPUT PITCH", header, juce::Justification::centredLeft);
    // Voiced LED.
    const auto led = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ header.getRight() - 4.0f, header.getCentreY() });
    if (voiced)
    {
        g.setColour (colours::mint.withAlpha (0.3f));
        g.fillEllipse (led.expanded (4.0f));
    }
    g.setColour (voiced ? colours::mint : colours::textFaint);
    g.fillEllipse (led);
    g.setColour (colours::textDim);
    g.drawText (voiced ? "VOICED" : "UNVOICED", header.withTrimmedRight (14.0f), juce::Justification::centredRight);

    auto meterArea = r.removeFromBottom (26.0f);
    auto big = r;

    const float midi = voiced ? 69.0f + 12.0f * std::log2 (shownHz / 440.0f) : 0.0f;
    const float cents = voiced ? (midi - std::round (midi)) * 100.0f : 0.0f;

    auto left = big.removeFromLeft (big.getWidth() * 0.55f);
    g.setFont (font (left.getHeight() * 0.62f, true));
    g.setColour (voiced ? colours::text : colours::textFaint);
    g.drawText (voiced ? noteName (midi) : "--", left, juce::Justification::centredLeft);

    // Cents + Hz.
    auto c1 = big.removeFromTop (big.getHeight() * 0.5f);
    g.setFont (font (18.0f, true));
    g.setColour (voiced ? (std::abs (cents) < 10.0f ? colours::mint : colours::peach) : colours::textFaint);
    g.drawText (voiced ? ((cents >= 0 ? "+" : "") + juce::String (cents, 0) + " ct") : "-- ct", c1,
                juce::Justification::centredRight);
    g.setFont (font (13.0f));
    g.setColour (colours::textDim);
    juce::String sub = voiced ? juce::String (shownHz, 1) + " Hz" : "no pitch";
    if (voiced && showOut)
        sub << "  >  " << noteName (midi + shift);
    g.drawText (sub, big, juce::Justification::centredRight);

    // Confidence meter.
    auto label = meterArea.removeFromLeft (84.0f);
    g.setFont (font (10.5f, true).withExtraKerningFactor (0.2f));
    g.setColour (colours::textDim);
    g.drawText ("CONFIDENCE", label, juce::Justification::centredLeft);
    const auto bar = meterArea.withSizeKeepingCentre (meterArea.getWidth(), 8.0f);
    g.setColour (colours::track);
    g.fillRoundedRectangle (bar, 4.0f);
    const float conf = juce::jlimit (0.0f, 1.0f, shownConfidence);
    // Emphasise the useful top end of the aperiodicity scale.
    const float shown = juce::jlimit (0.0f, 1.0f, (conf - 0.4f) / 0.6f);
    if (shown > 0.0f)
    {
        const auto fill = bar.withWidth (bar.getWidth() * shown);
        g.setColour (colours::mint.withAlpha (0.18f));
        g.fillRoundedRectangle (fill.expanded (3.0f), 7.0f);
        juce::ColourGradient grad (colours::peach, bar.getX(), 0, colours::mint, bar.getRight(), 0, false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 4.0f);
    }
}
} // namespace nsw::ui
