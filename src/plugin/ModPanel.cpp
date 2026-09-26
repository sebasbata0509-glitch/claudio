#include "ModPanel.h"

#include "Parameters.h"

namespace nsw::ui
{
//==============================================================================
ParamCombo::ParamCombo (APVTS& state, const juce::String& paramId, const juce::String& c) : caption (c)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramId, box);
}

void ParamCombo::paint (juce::Graphics& g)
{
    g.setFont (font (11.5f, true).withExtraKerningFactor (0.18f));
    g.setColour (colours::textDim);
    g.drawText (caption.toUpperCase(), getLocalBounds().removeFromTop (18), juce::Justification::centred);
}

void ParamCombo::resized()
{
    box.setBounds (getLocalBounds().withTrimmedTop (18).withHeight (30));
}

//==============================================================================
StepEditor::StepEditor (APVTS& s) : state (s)
{
    for (int i = 0; i < 16; ++i)
    {
        values[(size_t) i] = state.getParameter (ids::seqStep (i));
        glides[(size_t) i] = state.getParameter (ids::seqGlide (i));
    }
    depth = state.getParameter (ids::seqDepth);
    length = state.getParameter (ids::seqLength);
    dest = state.getParameter (ids::seqDest);
    setTooltip ("Drag to draw steps (snapped to semitones), drag the lower strip to set per-step glide, double-click to reset");
}

juce::Rectangle<float> StepEditor::barArea() const
{
    return getLocalBounds().toFloat().withTrimmedBottom (22.0f);
}

juce::Rectangle<float> StepEditor::glideArea() const
{
    return getLocalBounds().toFloat().removeFromBottom (16.0f);
}

void StepEditor::setPlayingStep (int step, bool isRunning)
{
    if (step != playing || isRunning != running)
    {
        playing = step;
        running = isRunning;
        repaint();
    }
}

void StepEditor::paint (juce::Graphics& g)
{
    const auto bars = barArea();
    g.setColour (colours::background.withAlpha (0.55f));
    g.fillRoundedRectangle (bars, 10.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (bars.reduced (0.5f), 10.0f, 1.0f);

    const float midY = bars.getCentreY();
    g.setColour (colours::panelEdge);
    g.drawHorizontalLine ((int) midY, bars.getX() + 6.0f, bars.getRight() - 6.0f);

    const int len = (int) std::round (length->convertFrom0to1 (length->getValue()));
    const float depthSemis = depth->convertFrom0to1 (depth->getValue());
    const bool formant = dest->getValue() > 0.5f;
    const float colW = bars.getWidth() / 16.0f;
    const float halfH = bars.getHeight() * 0.5f - 8.0f;

    for (int i = 0; i < 16; ++i)
    {
        const float v = values[(size_t) i]->convertFrom0to1 (values[(size_t) i]->getValue()) * 0.01f;
        const auto col = juce::Rectangle<float> (bars.getX() + colW * (float) i, bars.getY(), colW, bars.getHeight()).reduced (3.0f, 6.0f);
        const bool active = i < len;
        const bool isPlaying = running && i == playing;
        const float alpha = active ? 1.0f : 0.25f;

        if (isPlaying)
        {
            g.setColour (colours::peach.withAlpha (0.10f));
            g.fillRoundedRectangle (col.expanded (1.0f, 4.0f), 6.0f);
        }

        const float top = midY - std::max (0.0f, v) * halfH;
        const float bottom = midY - std::min (0.0f, v) * halfH;
        auto bar = juce::Rectangle<float> (col.getX() + 2.0f, top, col.getWidth() - 4.0f, std::max (2.0f, bottom - top));
        if (std::abs (v) < 0.005f)
            bar = bar.withHeight (2.0f).withY (midY - 1.0f);
        const auto c = (v >= 0.0f ? colours::peach : colours::lilac).withAlpha (alpha * (isPlaying ? 1.0f : 0.8f));
        if (isPlaying)
        {
            g.setColour (c.withAlpha (0.3f));
            g.fillRoundedRectangle (bar.expanded (3.0f), 5.0f);
        }
        g.setColour (c);
        g.fillRoundedRectangle (bar, 3.0f);

        if (active && std::abs (v) > 0.005f)
        {
            const float semis = v * depthSemis;
            g.setFont (font (10.0f, true));
            g.setColour (colours::text.withAlpha (0.8f));
            const bool whole = std::abs (semis - std::round (semis)) < 0.05f;
            const auto label = (semis > 0 ? "+" : "") + (whole ? juce::String (juce::roundToInt (semis)) : juce::String (semis, 1));
            const auto textArea = v > 0 ? col.withTop (midY + 2.0f).withHeight (14.0f) : col.withBottom (midY - 2.0f).withTrimmedTop (col.getHeight() * 0.5f - 16.0f);
            g.drawText (label, textArea, juce::Justification::centred);
        }

        // Glide strip.
        const float gl = glides[(size_t) i]->getValue();
        const auto strip = juce::Rectangle<float> (col.getX() + 2.0f, glideArea().getY(), col.getWidth() - 4.0f, glideArea().getHeight()).reduced (0, 4.0f);
        g.setColour (colours::track.withAlpha (alpha));
        g.fillRoundedRectangle (strip, 3.0f);
        if (gl > 0.001f)
        {
            g.setColour (colours::mint.withAlpha (0.8f * alpha));
            g.fillRoundedRectangle (strip.withWidth (strip.getWidth() * gl), 3.0f);
        }
    }

    g.setFont (font (10.0f, true).withExtraKerningFactor (0.2f));
    g.setColour (colours::textFaint);
    g.drawText (formant ? "FORMANT" : "PITCH", bars.reduced (8.0f, 4.0f), juce::Justification::topLeft);
}

void StepEditor::edit (const juce::MouseEvent& e, bool reset)
{
    const auto bars = barArea();
    const float colW = bars.getWidth() / 16.0f;
    const int i = juce::jlimit (0, 15, (int) ((e.position.x - bars.getX()) / colW));

    auto set = [] (juce::RangedAudioParameter* p, float value) {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    };

    if (editingGlide)
    {
        const float x0 = bars.getX() + colW * (float) i;
        set (glides[(size_t) i], reset ? 0.0f : juce::jlimit (0.0f, 100.0f, 100.0f * (e.position.x - x0) / colW));
    }
    else
    {
        const float halfH = bars.getHeight() * 0.5f - 8.0f;
        float v = reset ? 0.0f : juce::jlimit (-1.0f, 1.0f, (bars.getCentreY() - e.position.y) / halfH);
        const float depthSemis = depth->convertFrom0to1 (depth->getValue());
        if (depthSemis >= 1.0f && ! e.mods.isAltDown())
            v = std::round (v * depthSemis) / depthSemis; // semitone grid (hold Alt for free values)
        set (values[(size_t) i], v * 100.0f);
    }
    repaint();
}

void StepEditor::mouseDown (const juce::MouseEvent& e)
{
    editingGlide = e.position.y >= glideArea().getY() - 3.0f;
    edit (e, false);
}

void StepEditor::mouseDrag (const juce::MouseEvent& e)
{
    edit (e, false);
}

void StepEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    edit (e, true);
}

//==============================================================================
EnvelopeView::EnvelopeView (APVTS& state)
    : a (state.getRawParameterValue (ids::envAttack)),
      d (state.getRawParameterValue (ids::envDecay)),
      s (state.getRawParameterValue (ids::envSustain)),
      r (state.getRawParameterValue (ids::envRelease))
{
}

void EnvelopeView::setLevel (float l)
{
    if (std::abs (l - level) > 0.002f)
    {
        level = l;
        repaint();
    }
}

void EnvelopeView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (colours::background.withAlpha (0.55f));
    g.fillRoundedRectangle (area, 10.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (area.reduced (0.5f), 10.0f, 1.0f);
    area = area.reduced (12.0f, 14.0f);

    // Log-ish time axis so short and long settings both read well.
    const float wa = std::log1p (a->load() / 10.0f), wd = std::log1p (d->load() / 10.0f), wr = std::log1p (r->load() / 10.0f);
    const float hold = 0.6f * (wa + wd + wr) / 3.0f + 0.4f;
    const float total = wa + wd + hold + wr;
    const float sus = s->load() * 0.01f;
    auto X = [&] (float t) { return area.getX() + area.getWidth() * t / total; };
    auto Y = [&] (float v) { return area.getBottom() - area.getHeight() * v; };

    juce::Path p;
    p.startNewSubPath (X (0), Y (0));
    p.lineTo (X (wa), Y (1));
    p.quadraticTo (X (wa + wd * 0.3f), Y (sus), X (wa + wd), Y (sus));
    p.lineTo (X (wa + wd + hold), Y (sus));
    p.quadraticTo (X (wa + wd + hold + wr * 0.3f), Y (0), X (total), Y (0));

    juce::Path fill (p);
    fill.lineTo (X (0), Y (0));
    fill.closeSubPath();
    juce::ColourGradient grad (colours::peach.withAlpha (0.25f), 0, area.getY(), colours::peach.withAlpha (0.02f), 0,
                               area.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (fill);
    g.setColour (colours::peach);
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Live level.
    const float y = Y (juce::jlimit (0.0f, 1.0f, level));
    g.setColour (colours::mint.withAlpha (0.25f + 0.5f * level));
    g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
    g.setColour (colours::mint);
    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ area.getRight() - 4.0f, y }));
}

//==============================================================================
ModPanel::ModPanel (APVTS& s)
    : state (s),
      seqOn (s, ids::seqOn, "ON"),
      seqRate (s, ids::seqRate, "Rate"),
      seqDest (s, ids::seqDest, { "PITCH", "FORMANT" }, "Target"),
      seqDepth (s, ids::seqDepth, "Depth", Knob::Size::Small),
      seqLength (s, ids::seqLength, "Steps", Knob::Size::Small),
      steps (s),
      attack (s, ids::envAttack, "Attack", Knob::Size::Small),
      decay (s, ids::envDecay, "Decay", Knob::Size::Small),
      sustain (s, ids::envSustain, "Sustain", Knob::Size::Small),
      release (s, ids::envRelease, "Release", Knob::Size::Small),
      envDepth (s, ids::envDepth, "Depth", Knob::Size::Small, true),
      threshold (s, ids::envThreshold, "Threshold", Knob::Size::Small),
      trigger (s, ids::envTrigger, { "MIDI", "INPUT" }, "Trigger"),
      envDest (s, ids::envDest, "Target"),
      envView (s),
      midiMode (s, ids::mode, { "OFF", "POLY", "MONO" }, "MIDI Mode"),
      velTarget (s, ids::velTarget, "Velocity to"),
      velAmount (s, ids::velAmount, "Vel Amount", Knob::Size::Small, true)
{
    for (auto name : { "MACRO", "SEQUENCER", "ENVELOPE", "MIDI" })
    {
        auto* b = tabs.add (new juce::TextButton (name));
        b->setClickingTogglesState (false);
        const int index = tabs.size() - 1;
        b->onClick = [this, index] { showTab (index); };
        addAndMakeVisible (b);
    }

    // Macro page.
    macro = std::make_unique<Knob> (s, ids::macro, "Macro", Knob::Size::Medium);
    macro->setTooltip ("Sweeps every destination below by its own range");
    macroPage.addAndMakeVisible (*macro);
    for (int i = 1; i <= kNumModDests; ++i)
    {
        const auto d = (ModDest) i;
        static const char* shortNames[] = { "", "Pitch", "Formant", "Harm", "Alt", "FM", "Ratio",
                                            "Smear", "Stereo", "Detune", "Mix", "Out" };
        auto* k = depths.add (new Knob (s, ids::macroDepth (d), shortNames[i], Knob::Size::Small, true));
        k->setTooltip (juce::String ("How far the Macro moves ") + modDestName (d));
        macroPage.addAndMakeVisible (k);
    }

    // Sequencer page.
    for (auto* c : std::initializer_list<juce::Component*> { &seqOn, &seqRate, &seqDest, &seqDepth, &seqLength, &steps })
        seqPage.addAndMakeVisible (c);

    // Envelope page.
    for (auto* c : std::initializer_list<juce::Component*> { &attack, &decay, &sustain, &release, &envView, &envDest,
                                                              &trigger, &envDepth, &threshold })
        envPage.addAndMakeVisible (c);
    threshold.setTooltip ("Input level that opens the envelope (Input trigger)");

    // MIDI page.
    midiHelp.setText ("OFF: the Pitch knob transposes what you sing.\n"
                      "MONO: the latest held note sets the target pitch (hard-tune); Pitch and Octave offset it.\n"
                      "POLY: one voice per held note (up to 8) for harmonies and vocoder-style chords.",
                      juce::dontSendNotification);
    midiHelp.setFont (font (12.5f));
    midiHelp.setColour (juce::Label::textColourId, colours::textDim);
    midiHelp.setJustificationType (juce::Justification::centredLeft);
    for (auto* c : std::initializer_list<juce::Component*> { &midiMode, &velTarget, &velAmount, &midiHelp })
        midiPage.addAndMakeVisible (c);

    for (auto* page : { &macroPage, &seqPage, &envPage, &midiPage })
        addChildComponent (page);
    showTab (0);
}

void ModPanel::showTab (int index)
{
    current = index;
    juce::Component* pages[] = { &macroPage, &seqPage, &envPage, &midiPage };
    for (int i = 0; i < 4; ++i)
    {
        pages[i]->setVisible (i == index);
        tabs[i]->setToggleState (i == index, juce::dontSendNotification);
    }
}

void ModPanel::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    juce::ColourGradient bg (colours::panel.brighter (0.03f), r.getX(), r.getY(), colours::panel.darker (0.12f), r.getX(),
                             r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r, 14.0f, 1.0f);
    g.setFont (font (12.0f, true).withExtraKerningFactor (0.3f));
    g.setColour (colours::peach.withAlpha (0.85f));
    g.drawText ("MODULATION", getLocalBounds().reduced (18, 10).removeFromTop (26), juce::Justification::centredLeft);
}

void ModPanel::resized()
{
    int x = 160;
    for (auto* t : tabs)
    {
        t->setBounds (x, 10, 108, 26);
        x += 114;
    }

    const auto content = getLocalBounds().reduced (16, 0).withTrimmedTop (46).withTrimmedBottom (12);
    for (auto* page : { &macroPage, &seqPage, &envPage, &midiPage })
        page->setBounds (content);
    const int h = content.getHeight();

    // Macro: big knob + one range knob per destination.
    macro->setBounds (0, 0, 130, h);
    const int kw = (content.getWidth() - 150) / kNumModDests;
    for (int i = 0; i < depths.size(); ++i)
        depths[i]->setBounds (150 + i * kw, 10, kw, h - 10);

    // Sequencer.
    seqOn.setBounds (0, 20, 64, 28);
    seqRate.setBounds (74, 0, 92, 50);
    seqDest.setBounds (174, 0, 158, 50);
    seqDepth.setBounds (40, 58, 90, h - 58);
    seqLength.setBounds (190, 58, 90, h - 58);
    steps.setBounds (348, 0, content.getWidth() - 348, h);

    // Envelope.
    const int ew = 78;
    attack.setBounds (0, 6, ew, h - 6);
    decay.setBounds (ew, 6, ew, h - 6);
    sustain.setBounds (2 * ew, 6, ew, h - 6);
    release.setBounds (3 * ew, 6, ew, h - 6);
    envView.setBounds (4 * ew + 12, 6, 230, h - 12);
    const int cx = envView.getRight() + 18;
    envDest.setBounds (cx, 0, 150, 50);
    trigger.setBounds (cx, 64, 150, 50);
    envDepth.setBounds (cx + 168, 6, 90, h - 6);
    threshold.setBounds (cx + 262, 6, 90, h - 6);

    // MIDI.
    midiMode.setBounds (0, 0, 220, 52);
    velTarget.setBounds (0, 70, 220, 50);
    velAmount.setBounds (240, 6, 100, h - 6);
    midiHelp.setBounds (370, 0, content.getWidth() - 370, h);
}

void ModPanel::updateFromEngine (const Engine::Display& d)
{
    steps.setPlayingStep (d.seqStep.load(), state.getRawParameterValue (ids::seqOn)->load() > 0.5f);
    envView.setLevel (d.envelope.load());
    // Cheap; keeps the step bars and envelope shape in sync with automation and presets.
    if (seqPage.isVisible())
        steps.repaint();
    if (envPage.isVisible())
        envView.repaint();
}
} // namespace nsw::ui
