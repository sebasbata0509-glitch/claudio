#include "PluginEditor.h"

#include "dsp/Presets.h"

using namespace nsw;
using namespace nsw::ui;

NutSwellerEditor::Canvas::Canvas (NutSwellerProcessor& p)
    : proc (p),
      state (p.getState()),
      range (state, ids::range, { "LOW", "HIGH" }, "Range"),
      mode (state, ids::mode, { "OFF", "POLY", "MONO" }, "MIDI"),
      pitch (state, ids::pitch, "Pitch", Knob::Size::Large, true),
      formant (state, ids::formant, "Formant", Knob::Size::Large, true),
      octave (state, ids::octave, { "-2", "-1", "0", "+1", "+2" }, "Octave"),
      snap (state, ids::snap, "SNAP"),
      mix (state, ids::mix),
      glide (state, ids::glide, "Glide", Knob::Size::Small),
      output (state, ids::output, "Output", Knob::Size::Small, true),
      harmonics (state, ids::harmonics, "Harmonics", Knob::Size::Medium, true),
      alternator (state, ids::alternator, "Alternator", Knob::Size::Medium),
      fm (state, ids::fm, "FM", Knob::Size::Medium),
      ratio (state, ids::ratio, "Ratio", Knob::Size::Medium),
      smear (state, ids::smear, "Smear", Knob::Size::Medium),
      stereo (state, ids::stereo, "Stereo", Knob::Size::Medium),
      detune (state, ids::detune, "Detune", Knob::Size::Medium)
{
    for (auto* c : std::initializer_list<juce::Component*> { &pitchSection, &centreSection, &formantSection, &textureSection })
        addAndMakeVisible (c);
    for (auto* c : std::initializer_list<juce::Component*> {
             &wordmark, &presetBox, &prevPreset, &nextPreset, &range, &mode, &pitch, &formant, &octave, &snap, &readout,
             &mix, &glide, &output, &harmonics, &alternator, &fm, &ratio, &smear, &stereo, &detune })
        addAndMakeVisible (c);

    refreshPresetList();
    presetBox.onChange = [this] {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0 && idx != proc.getCurrentProgram())
            proc.setCurrentProgram (idx);
        else if (idx >= 0)
            proc.setCurrentProgram (idx); // re-selecting reloads (undo tweaks)
    };
    auto step = [this] (int delta) {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + delta + n) % n);
        refresh();
    };
    prevPreset.onClick = [step] { step (-1); };
    nextPreset.onClick = [step] { step (1); };
    prevPreset.setTooltip ("Previous preset");
    nextPreset.setTooltip ("Next preset");
    presetBox.setTooltip ("Factory presets");

    ratio.setTooltip ("FM modulator ratio (x detected pitch)");
    harmonics.setTooltip ("Tilt: + boosts odd harmonics (hollow), - boosts even harmonics (bright)");
    alternator.setTooltip ("Alternates cycles between original pitch and an octave down: gritty sub / alien texture");
}

void NutSwellerEditor::Canvas::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    refresh();
}

void NutSwellerEditor::Canvas::refresh()
{
    const auto& d = proc.getEngine().display();
    const auto value = [this] (const char* id) { return state.getRawParameterValue (id)->load(); };
    const float p = value (ids::snap) > 0.5f ? std::round (value (ids::pitch)) : value (ids::pitch);
    const float shift = p + 12.0f * std::round (value (ids::octave));
    readout.update (d.frequency.load(), d.confidence.load(), d.voiced.load(), shift, value (ids::mode) < 0.5f);

    const int prog = proc.getCurrentProgram();
    if (prog != lastProgram)
    {
        lastProgram = prog;
        presetBox.setSelectedItemIndex (prog, juce::dontSendNotification);
    }
}

void NutSwellerEditor::Canvas::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    juce::ColourGradient bg (colours::backgroundTop, 0, 0, colours::background, 0, r.getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Soft ambient blooms behind the two hero knobs.
    auto bloom = [&g] (juce::Point<float> c, float rad, juce::Colour col) {
        juce::ColourGradient grad (col.withAlpha (0.10f), c.x, c.y, col.withAlpha (0.0f), c.x + rad, c.y, true);
        g.setGradientFill (grad);
        g.fillEllipse (juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c));
    };
    bloom (pitch.getBounds().toFloat().getCentre(), 260.0f, colours::peach);
    bloom (formant.getBounds().toFloat().getCentre(), 260.0f, colours::lilac);

    g.setColour (colours::panelEdge.withAlpha (0.6f));
    g.drawHorizontalLine (68, 20.0f, r.getWidth() - 20.0f);

    g.setFont (font (10.5f));
    g.setColour (colours::textFaint);
    g.drawText ("v" JucePlugin_VersionString, getLocalBounds().removeFromBottom (18).reduced (24, 0),
                juce::Justification::centredRight);
}

void NutSwellerEditor::Canvas::paintOverChildren (juce::Graphics& g)
{
    // Formant direction hints.
    const auto hint = formantSection.getBounds().reduced (22, 0).withTop (formant.getBottom() + 14).withHeight (20);
    g.setFont (font (11.0f, true).withExtraKerningFactor (0.15f));
    g.setColour (colours::lilac.withAlpha (0.7f));
    g.drawText ("< LARGER / DARKER", hint, juce::Justification::centredLeft);
    g.setColour (colours::peach.withAlpha (0.8f));
    g.drawText ("CUTE / BRIGHTER >", hint, juce::Justification::centredRight);
    g.setFont (font (11.0f));
    g.setColour (colours::textFaint);
    g.drawText ("0 st = natural formants", hint.translated (0, 22), juce::Justification::centred);

}

void NutSwellerEditor::Canvas::resized()
{
    // Header.
    wordmark.setBounds (22, 14, 250, 42);
    presetBox.setBounds (390, 20, 220, 32);
    prevPreset.setBounds (354, 22, 30, 28);
    nextPreset.setBounds (616, 22, 30, 28);
    range.setBounds (690, 6, 120, 52);
    mode.setBounds (826, 6, 152, 52);

    // Hero row.
    pitchSection.setBounds (20, 82, 310, 318);
    centreSection.setBounds (345, 82, 310, 318);
    formantSection.setBounds (670, 82, 310, 318);

    pitch.setBounds (pitchSection.getBounds().reduced (30, 0).withTrimmedTop (22).withHeight (232));
    octave.setBounds (pitchSection.getX() + 20, pitchSection.getBottom() - 62, 190, 50);
    snap.setBounds (pitchSection.getRight() - 94, pitchSection.getBottom() - 42, 74, 28);

    readout.setBounds (centreSection.getBounds().reduced (14).withHeight (126));
    mix.setBounds (centreSection.getX() + 20, readout.getBottom() + 14, centreSection.getWidth() - 40, 50);
    const int smallY = mix.getBottom() + 8;
    glide.setBounds (centreSection.getX() + 50, smallY, 90, centreSection.getBottom() - smallY - 8);
    output.setBounds (centreSection.getRight() - 140, smallY, 90, centreSection.getBottom() - smallY - 8);

    formant.setBounds (formantSection.getBounds().reduced (30, 0).withTrimmedTop (22).withHeight (232));

    // Texture row: TONE | MOTION | FM | SPACE.
    textureSection.setBounds (20, 414, 960, 160);
    const int w = 104, gap = 34;
    const int total = 7 * w + 3 * gap;
    const int y = textureSection.getY() + 44, h = 108;
    int x = textureSection.getX() + (textureSection.getWidth() - total) / 2;
    auto place = [&] (juce::Component& c) { c.setBounds (x, y, w, h); x += w; };
    const int toneX = x;
    place (harmonics);
    x += gap;
    const int motionX = x;
    place (alternator);
    x += gap;
    const int fmX = x;
    place (fm);
    place (ratio);
    x += gap;
    const int spaceX = x;
    place (smear);
    place (stereo);
    place (detune);

    auto cap = [&] (int from, int to) { return juce::Rectangle<int> (from - textureSection.getX(), 28, to - from, 14); };
    textureSection.setSubCaptions ({ { "Tone", cap (toneX, toneX + w) },
                                     { "Motion", cap (motionX, motionX + w) },
                                     { "FM", cap (fmX, fmX + 2 * w) },
                                     { "Space", cap (spaceX, spaceX + 3 * w) } });
}

//==============================================================================
NutSwellerEditor::NutSwellerEditor (NutSwellerProcessor& p)
    : AudioProcessorEditor (p), proc (p), canvas (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (canvas);
    canvas.setBounds (0, 0, kBaseWidth, kBaseHeight);

    // Free resizing; the canvas is letter-boxed so any aspect ratio works (some hosts and
    // the standalone wrapper add their own chrome, which fights a fixed aspect ratio).
    setResizable (true, true);
    setResizeLimits (kBaseWidth * 6 / 10, kBaseHeight * 6 / 10, kBaseWidth * 2, kBaseHeight * 2);

    const float scale = juce::jlimit (0.6f, 2.0f, proc.getUiScale());
    setSize ((int) std::round (kBaseWidth * scale), (int) std::round (kBaseHeight * scale));
    startTimerHz (30);
}

NutSwellerEditor::~NutSwellerEditor()
{
    setLookAndFeel (nullptr);
}

void NutSwellerEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void NutSwellerEditor::resized()
{
    const float scale = std::min ((float) getWidth() / (float) kBaseWidth, (float) getHeight() / (float) kBaseHeight);
    const float dx = ((float) getWidth() - kBaseWidth * scale) * 0.5f;
    const float dy = ((float) getHeight() - kBaseHeight * scale) * 0.5f;
    canvas.setTransform (juce::AffineTransform::scale (scale).translated (dx, dy));
    proc.setUiScale (scale);
}

void NutSwellerEditor::timerCallback()
{
    canvas.refresh();
}
