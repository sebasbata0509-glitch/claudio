#include "PianoRoll.h"

#include "ToneLookAndFeel.h"

#include "common/MathUtil.h"
#include "model/Edits.h"

namespace tone::ui
{
namespace
{
constexpr float kLowestMidi = 24.0f;  // C1
constexpr float kHighestMidi = 108.0f; // C8

const char* const kNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

juce::String noteName (float midi, bool withCents)
{
    const int m = (int) std::lround (midi);
    juce::String s = juce::String (kNames[((m % 12) + 12) % 12]) + juce::String (m / 12 - 1);
    const int cents = (int) std::lround ((midi - (float) m) * 100.0f);
    if (withCents && cents != 0)
        s << (cents > 0 ? " +" : " ") << cents;
    return s;
}

bool isBlackKey (int midi)
{
    switch (((midi % 12) + 12) % 12)
    {
        case 1: case 3: case 6: case 8: case 10: return true;
        default: return false;
    }
}

juce::String formatTime (double t, double step)
{
    const int minutes = (int) (t / 60.0);
    const double secs = t - minutes * 60.0;
    const int decimals = step >= 1.0 ? 0 : (step >= 0.1 && std::abs (step - 0.25) > 1e-9 ? 1 : 2);
    const auto secText = decimals == 0 ? juce::String ((int) std::lround (secs)) : juce::String (secs, decimals);
    return juce::String (minutes) + ":" + secText.paddedLeft ('0', decimals == 0 ? 2 : 3 + decimals);
}
} // namespace

PianoRoll::PianoRoll (ViewState& v) : view (v)
{
    setWantsKeyboardFocus (true);
    setOpaque (true);
    addAndMakeVisible (hScroll);
    addAndMakeVisible (vScroll);
    hScroll.addListener (this);
    vScroll.addListener (this);
    hScroll.setAutoHide (false);
    vScroll.setAutoHide (false);
}

PianoRoll::~PianoRoll()
{
    setSession (nullptr);
}

void PianoRoll::setSession (plugin::EditSession* s)
{
    if (s == session)
        return;
    if (session != nullptr)
    {
        if (session->isInGesture())
            session->endGesture ("Edit");
        session->removeListener (this);
    }
    session = s;
    drag = DragMode::None;
    if (session != nullptr)
    {
        session->addListener (this);
        lastNoteCount = session->getProject().notes.size();
        zoomToFit();
    }
    updateScrollBars();
    repaint();
}

void PianoRoll::sessionChanged (plugin::EditSession& s)
{
    const auto count = s.getProject().notes.size();
    if (lastNoteCount == 0 && count > 0)
        zoomToFit();
    lastNoteCount = count;
    updateScrollBars();
    repaint();
}

void PianoRoll::setPlayhead (double seconds, bool isPlaying)
{
    const auto area = noteArea();
    const double visible = area.getWidth() / view.pixelsPerSecond;
    if (isPlaying && view.followPlayhead && seconds >= 0.0
        && (seconds < view.startSeconds || seconds > view.startSeconds + 0.92 * visible))
    {
        view.startSeconds = std::max (0.0, seconds - 0.08 * visible);
        userAdjustedView = true;
        updateScrollBars();
        repaint();
    }

    if (std::abs (seconds - playhead) * view.pixelsPerSecond < 0.5 && isPlaying == playing)
        return;
    const float oldX = timeToX (playhead), newX = timeToX (seconds);
    playhead = seconds;
    playing = isPlaying;
    repaint (juce::Rectangle<float> (std::min (oldX, newX) - 6.0f, 0.0f, std::abs (newX - oldX) + 12.0f, (float) getHeight()).toNearestInt());
}

//==============================================================================
juce::Rectangle<int> PianoRoll::noteArea() const
{
    return getLocalBounds().withTrimmedLeft (keysWidth).withTrimmedTop (rulerHeight)
                           .withTrimmedRight (scrollSize).withTrimmedBottom (scrollSize);
}

float PianoRoll::timeToX (double t) const noexcept
{
    return (float) keysWidth + (float) ((t - view.startSeconds) * view.pixelsPerSecond);
}

double PianoRoll::xToTime (float x) const noexcept
{
    return view.startSeconds + (x - (float) keysWidth) / view.pixelsPerSecond;
}

float PianoRoll::midiToY (float midi) const noexcept
{
    return (float) rulerHeight + (view.topMidi - midi) * view.pixelsPerSemitone;
}

float PianoRoll::yToMidi (float y) const noexcept
{
    return view.topMidi - (y - (float) rulerHeight) / view.pixelsPerSemitone;
}

juce::Rectangle<float> PianoRoll::noteBounds (const Note& n) const noexcept
{
    const float x0 = timeToX (n.dstStart), x1 = timeToX (n.dstEnd);
    const float h = std::max (8.0f, view.pixelsPerSemitone * 1.1f);
    return { x0, midiToY (n.targetMidi()) - h * 0.5f, std::max (2.0f, x1 - x0), h };
}

PianoRoll::Hit PianoRoll::noteHitTest (juce::Point<float> p) const
{
    if (session == nullptr || ! noteArea().toFloat().contains (p))
        return {};
    const auto& notes = session->getProject().notes;
    for (int i = (int) notes.size() - 1; i >= 0; --i)
    {
        const auto b = noteBounds (notes[(size_t) i]).expanded (0.0f, 3.0f);
        if (! b.contains (p))
            continue;
        const float edge = b.getWidth() > 24.0f ? 6.0f : 0.0f;
        if (p.x < b.getX() + edge)
            return { i, Zone::StartEdge };
        if (p.x > b.getRight() - edge)
            return { i, Zone::EndEdge };
        return { i, Zone::Body };
    }
    return {};
}

//==============================================================================
void PianoRoll::resized()
{
    const auto b = getLocalBounds();
    hScroll.setBounds (keysWidth, b.getHeight() - scrollSize, b.getWidth() - keysWidth - scrollSize, scrollSize);
    vScroll.setBounds (b.getWidth() - scrollSize, rulerHeight, scrollSize, b.getHeight() - rulerHeight - scrollSize);
    if (needsFit || ! userAdjustedView)
        zoomToFit();
    clampView();
    updateScrollBars();
}

void PianoRoll::clampView()
{
    view.pixelsPerSecond = juce::jlimit (8.0, 4000.0, view.pixelsPerSecond);
    view.pixelsPerSemitone = juce::jlimit (5.0f, 60.0f, view.pixelsPerSemitone);
    const float visibleSemis = (float) noteArea().getHeight() / view.pixelsPerSemitone;
    view.topMidi = juce::jlimit (std::min (kHighestMidi, kLowestMidi + visibleSemis), kHighestMidi, view.topMidi);
    view.startSeconds = std::max (0.0, view.startSeconds);
}

void PianoRoll::updateScrollBars()
{
    const auto area = noteArea();
    const double length = session != nullptr ? std::max (session->getProject().outputLength(),
                                                         session->hasSource() ? session->getSource()->lengthSeconds() : 0.0) : 0.0;
    const double visible = area.getWidth() / view.pixelsPerSecond;
    hScroll.setRangeLimits (0.0, std::max (length + 1.0, view.startSeconds + visible), juce::dontSendNotification);
    hScroll.setCurrentRange (view.startSeconds, visible, juce::dontSendNotification);

    const double visibleSemis = area.getHeight() / view.pixelsPerSemitone;
    vScroll.setRangeLimits (0.0, kHighestMidi - kLowestMidi, juce::dontSendNotification);
    vScroll.setCurrentRange (kHighestMidi - view.topMidi, visibleSemis, juce::dontSendNotification);
}

void PianoRoll::scrollBarMoved (juce::ScrollBar* bar, double start)
{
    userAdjustedView = true;
    if (bar == &hScroll)
        view.startSeconds = start;
    else
        view.topMidi = kHighestMidi - (float) start;
    clampView();
    repaint();
}

void PianoRoll::zoomToFit()
{
    const auto area = noteArea();
    if (area.getWidth() < 50 || area.getHeight() < 50)
    {
        needsFit = true; // apply once we have a size
        return;
    }
    needsFit = false;
    userAdjustedView = false;
    if (session == nullptr || session->getProject().notes.empty())
    {
        const double len = session != nullptr && session->hasSource() ? session->getSource()->lengthSeconds() : 10.0;
        view.startSeconds = 0.0;
        view.pixelsPerSecond = area.getWidth() / std::max (1.0, len + 0.5);
        clampView();
        updateScrollBars();
        repaint();
        return;
    }
    const auto& p = session->getProject();
    float lo = 200.0f, hi = -200.0f;
    for (const auto& n : p.notes)
    {
        lo = std::min (lo, n.targetMidi());
        hi = std::max (hi, n.targetMidi());
    }
    view.startSeconds = 0.0;
    view.pixelsPerSecond = area.getWidth() / std::max (1.0, p.outputLength() + 0.3);
    const float range = std::max (12.0f, hi - lo + 6.0f);
    view.pixelsPerSemitone = (float) area.getHeight() / range;
    view.topMidi = hi + (range - (hi - lo)) * 0.5f;
    clampView();
    updateScrollBars();
    repaint();
}

void PianoRoll::zoomTime (double factor, float anchorX)
{
    userAdjustedView = true;
    const double anchorT = xToTime (anchorX);
    view.pixelsPerSecond *= factor;
    clampView();
    view.startSeconds = std::max (0.0, anchorT - (anchorX - (float) keysWidth) / view.pixelsPerSecond);
    updateScrollBars();
    repaint();
}

void PianoRoll::zoomPitch (float factor, float anchorY)
{
    userAdjustedView = true;
    const float anchorM = yToMidi (anchorY);
    view.pixelsPerSemitone *= factor;
    view.pixelsPerSemitone = juce::jlimit (5.0f, 60.0f, view.pixelsPerSemitone);
    view.topMidi = anchorM + (anchorY - (float) rulerHeight) / view.pixelsPerSemitone;
    clampView();
    updateScrollBars();
    repaint();
}

//==============================================================================
void PianoRoll::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    const auto area = noteArea();

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (area);
        drawGrid (g, area);
        drawNotes (g, area);

        if (drag == DragMode::Rubber && ! rubber.isEmpty())
        {
            g.setColour (colours::selected.withAlpha (0.15f));
            g.fillRect (rubber);
            g.setColour (colours::selected.withAlpha (0.7f));
            g.drawRect (rubber, 1.0f);
        }

        if (playhead >= 0.0)
        {
            g.setColour (colours::playhead);
            g.fillRect (timeToX (playhead) - 0.75f, (float) area.getY(), 1.5f, (float) area.getHeight());
        }
    }

    drawRuler (g);
    drawKeys (g);

    g.setColour (colours::panel);
    g.fillRect (0, 0, keysWidth, rulerHeight);
    g.fillRect (getWidth() - scrollSize, getHeight() - scrollSize, scrollSize, scrollSize);

    // Status overlay
    juce::String message;
    if (session == nullptr || ! session->hasSource())
        message = "Pulsa \"Capturar\" y reproduce tu DAW, o abre un archivo de audio (Abrir...)";
    else if (session->isAnalysing())
        message = "Analizando... " + juce::String ((int) (session->getAnalysisProgress() * 100.0f)) + " %";
    else if (session->hasAnalysis() && session->getProject().notes.empty())
        message = "No se detectaron notas con altura definida en esta grabacion";
    if (message.isNotEmpty())
    {
        g.setColour (colours::panel.withAlpha (0.85f));
        const auto box = area.withSizeKeepingCentre (std::min (area.getWidth() - 40, 560), 54);
        g.fillRoundedRectangle (box.toFloat(), 8.0f);
        g.setColour (colours::text);
        g.setFont (juce::FontOptions (15.0f));
        g.drawFittedText (message, box.reduced (12, 4), juce::Justification::centred, 2);
    }
}

void PianoRoll::drawGrid (juce::Graphics& g, juce::Rectangle<int> area) const
{
    std::array<bool, 12> inScale {};
    inScale.fill (false);
    bool showScale = false;
    if (session != nullptr && session->getProject().scale.type != ScaleType::Chromatic)
    {
        inScale = session->getProject().scale.pitchClasses();
        showScale = true;
    }

    const int lo = (int) std::floor (yToMidi ((float) area.getBottom())) - 1;
    const int hi = (int) std::ceil (yToMidi ((float) area.getY())) + 1;
    for (int m = lo; m <= hi; ++m)
    {
        const float y0 = midiToY ((float) m + 0.5f), y1 = midiToY ((float) m - 0.5f);
        juce::Colour c = isBlackKey (m) ? colours::rowBlack : colours::rowWhite;
        if (showScale && inScale[(size_t) (((m % 12) + 12) % 12)])
            c = colours::rowInScale;
        g.setColour (c);
        g.fillRect ((float) area.getX(), y0, (float) area.getWidth(), y1 - y0);
        g.setColour (((m % 12) + 12) % 12 == 0 ? colours::gridStrong : colours::gridLine);
        g.fillRect ((float) area.getX(), y1 - 0.5f, (float) area.getWidth(), ((m % 12) + 12) % 12 == 0 ? 1.0f : 0.5f);
    }

    // vertical time grid
    static constexpr double steps[] = { 0.05, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 60.0 };
    double step = 60.0;
    for (double s : steps)
        if (s * view.pixelsPerSecond >= 80.0) { step = s; break; }
    const double t0 = std::floor (view.startSeconds / step) * step;
    for (double t = t0; timeToX (t) < (float) area.getRight(); t += step)
    {
        g.setColour (colours::gridLine);
        g.fillRect (timeToX (t), (float) area.getY(), 1.0f, (float) area.getHeight());
    }
}

void PianoRoll::drawNotes (juce::Graphics& g, juce::Rectangle<int> area) const
{
    if (session == nullptr || session->getEditModel() == nullptr)
        return;
    const auto* model = session->getEditModel();
    const auto& track = model->track();
    const auto& map = model->timeMap();
    const auto& notes = session->getProject().notes;
    const auto& sel = session->getSelection();

    float maxDb = -120.0f;
    for (const auto& f : track.frames)
        maxDb = std::max (maxDb, f.rmsDb);
    const float floorDb = maxDb - 42.0f;

    std::vector<std::pair<double, float>> curve;
    const float row = view.pixelsPerSemitone;

    for (int i = 0; i < (int) notes.size(); ++i)
    {
        const auto& n = notes[(size_t) i];
        const float x0 = timeToX (n.dstStart), x1 = timeToX (n.dstEnd);
        if (x1 < (float) area.getX() || x0 > (float) area.getRight())
            continue;

        const bool isSel = sel.count (n.id) != 0;
        auto differs = [] (float a, float b) { return std::abs (a - b) > 1.0e-6f; };
        const bool edited = differs (n.pitchShift, 0.0f) || differs (n.driftAmount, 1.0f) || differs (n.modulationAmount, 1.0f)
                            || differs (n.formantShift, 0.0f) || differs (n.gainDb, 0.0f)
                            || std::abs (n.dstStart - n.srcStart) > 1e-6 || std::abs (n.dstEnd - n.srcEnd) > 1e-6;
        const float yc = midiToY (n.targetMidi());

        // Ghost of the original position.
        if (differs (n.pitchShift, 0.0f))
        {
            const float yo = midiToY (n.centerMidi);
            juce::Path ghost;
            ghost.addRoundedRectangle (timeToX (n.srcStart), yo - row * 0.4f, timeToX (n.srcEnd) - timeToX (n.srcStart), row * 0.8f, 3.0f);
            const float dashes[] = { 4.0f, 3.0f };
            juce::Path dashed;
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, ghost, dashes, 2);
            g.setColour (colours::textDim.withAlpha (0.5f));
            g.fillPath (dashed);
        }

        // Amplitude-shaped blob.
        const auto [f0, f1] = model->frameRange (i);
        juce::Path blob;
        std::vector<juce::Point<float>> top, bottom;
        const float gainScale = dbToGain (n.gainDb * 0.5f);
        auto thickness = [&] (int f)
        {
            const float db = track.frames[(size_t) juce::jlimit (0, (int) track.frames.size() - 1, f)].rmsDb;
            const float a = juce::jlimit (0.0f, 1.0f, (db - floorDb) / (maxDb - floorDb));
            return juce::jlimit (3.0f, row * 2.2f, row * (0.25f + 1.35f * a) * gainScale);
        };
        float lastX = -1.0e9f;
        for (int f = f0; f < f1; ++f)
        {
            float x = timeToX (map.toDst (track.timeOfFrame (f)));
            x = juce::jlimit (x0, x1, x);
            if (x - lastX < 2.0f && f != f1 - 1)
                continue;
            lastX = x;
            const float h = thickness (f) * 0.5f;
            top.push_back ({ x, yc - h });
            bottom.push_back ({ x, yc + h });
        }
        if (top.empty() || top.front().x > x0 + 1.0f)
        {
            const float h = thickness (f0) * 0.5f;
            top.insert (top.begin(), { x0, yc - h });
            bottom.insert (bottom.begin(), { x0, yc + h });
        }
        if (top.back().x < x1 - 1.0f)
        {
            const float h = thickness (f1 - 1) * 0.5f;
            top.push_back ({ x1, yc - h });
            bottom.push_back ({ x1, yc + h });
        }
        blob.startNewSubPath (top.front());
        for (size_t k = 1; k < top.size(); ++k)
            blob.lineTo (top[k]);
        for (size_t k = bottom.size(); k-- > 0;)
            blob.lineTo (bottom[k]);
        blob.closeSubPath();
        blob = blob.createPathWithRoundedCorners (3.0f);

        const auto base = isSel ? colours::selected : (edited ? colours::noteEdited : colours::note);
        g.setColour (base.withAlpha (0.78f));
        g.fillPath (blob);
        g.setColour (base.brighter (0.4f));
        g.strokePath (blob, juce::PathStrokeType (1.0f));

        // Original and edited pitch curves.
        auto drawCurve = [&] (const std::vector<std::pair<double, float>>& pts, juce::Colour c, float width)
        {
            juce::Path p;
            double lastT = -1.0;
            for (const auto& [t, m] : pts)
            {
                const juce::Point<float> pt { timeToX (t), midiToY (m) };
                if (lastT < 0.0 || t - lastT > 4.0 * track.frameSeconds() * std::max (1.0, n.stretch()))
                    p.startNewSubPath (pt);
                else
                    p.lineTo (pt);
                lastT = t;
            }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        if (view.showOriginalCurve && edited)
        {
            model->originalCurve (i, curve);
            drawCurve (curve, colours::curveOrig, 1.0f);
        }
        model->editedCurve (i, curve);
        drawCurve (curve, colours::curve, 1.4f);

        if (x1 - x0 > 40.0f && row >= 10.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.75f));
            g.setFont (juce::FontOptions (std::min (12.0f, row * 0.8f), juce::Font::bold));
            g.drawText (noteName (n.targetMidi(), true), juce::Rectangle<float> (x0 + 4.0f, yc - row * 0.5f, x1 - x0 - 6.0f, row),
                        juce::Justification::centredLeft, false);
        }
    }
}

void PianoRoll::drawRuler (juce::Graphics& g) const
{
    const juce::Rectangle<int> r (keysWidth, 0, getWidth() - keysWidth, rulerHeight);
    g.setColour (colours::panel);
    g.fillRect (r);
    g.setColour (colours::gridStrong);
    g.fillRect (r.getX(), r.getBottom() - 1, r.getWidth(), 1);

    static constexpr double steps[] = { 0.05, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 60.0 };
    double step = 60.0;
    for (double s : steps)
        if (s * view.pixelsPerSecond >= 80.0) { step = s; break; }

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (r);
    g.setFont (juce::FontOptions (11.0f));
    const double t0 = std::floor (view.startSeconds / step) * step;
    for (double t = t0; timeToX (t) < (float) r.getRight(); t += step)
    {
        const float x = timeToX (t);
        g.setColour (colours::textDim);
        g.fillRect (x, (float) r.getBottom() - 8.0f, 1.0f, 7.0f);
        g.drawText (formatTime (t, step), juce::Rectangle<float> (x + 3.0f, 2.0f, 70.0f, (float) rulerHeight - 10.0f),
                    juce::Justification::centredLeft, false);
    }
    if (playhead >= 0.0)
    {
        const float x = timeToX (playhead);
        juce::Path tri;
        tri.addTriangle (x - 6.0f, (float) rulerHeight - 10.0f, x + 6.0f, (float) rulerHeight - 10.0f, x, (float) rulerHeight);
        g.setColour (colours::playhead);
        g.fillPath (tri);
    }
}

void PianoRoll::drawKeys (juce::Graphics& g) const
{
    const juce::Rectangle<int> r (0, rulerHeight, keysWidth, getHeight() - rulerHeight - scrollSize);
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (r);
    g.setColour (colours::panel);
    g.fillRect (r);

    const int lo = (int) std::floor (yToMidi ((float) r.getBottom())) - 1;
    const int hi = (int) std::ceil (yToMidi ((float) r.getY())) + 1;
    g.setFont (juce::FontOptions (std::min (11.0f, view.pixelsPerSemitone * 0.85f)));
    for (int m = lo; m <= hi; ++m)
    {
        const float y0 = midiToY ((float) m + 0.5f), y1 = midiToY ((float) m - 0.5f);
        const bool black = isBlackKey (m);
        g.setColour (black ? juce::Colour (0xff2a2d33) : juce::Colour (0xffc9ccd2));
        g.fillRect (black ? 0.0f : 0.0f, y0, black ? (float) keysWidth * 0.62f : (float) keysWidth - 1.0f, y1 - y0);
        g.setColour (juce::Colour (0xff6c717b));
        g.fillRect (0.0f, y1 - 0.5f, (float) keysWidth - 1.0f, 0.5f);
        if (((m % 12) + 12) % 12 == 0 && view.pixelsPerSemitone >= 8.0f)
        {
            g.setColour (juce::Colours::black);
            g.drawText (noteName ((float) m, false), juce::Rectangle<float> (2.0f, y0, (float) keysWidth - 6.0f, y1 - y0),
                        juce::Justification::centredRight, false);
        }
    }
    g.setColour (colours::gridStrong);
    g.fillRect (keysWidth - 1, r.getY(), 1, r.getHeight());
}

//==============================================================================
void PianoRoll::mouseMove (const juce::MouseEvent& e)
{
    const auto hit = noteHitTest (e.position);
    auto mouseCursor = juce::MouseCursor::NormalCursor;
    if (e.position.y < (float) rulerHeight && e.position.x > (float) keysWidth)
        mouseCursor = juce::MouseCursor::IBeamCursor;
    else if (hit.index >= 0)
    {
        if (view.tool == Tool::Split)
            mouseCursor = juce::MouseCursor::CrosshairCursor;
        else if (hit.zone != Zone::Body && view.tool != Tool::Pitch)
            mouseCursor = juce::MouseCursor::LeftRightResizeCursor;
        else if (view.tool == Tool::Pitch)
            mouseCursor = juce::MouseCursor::UpDownResizeCursor;
        else if (view.tool == Tool::Time)
            mouseCursor = juce::MouseCursor::LeftRightResizeCursor;
        else
            mouseCursor = juce::MouseCursor::DraggingHandCursor;

        if (onHint && session != nullptr)
        {
            const auto& n = session->getProject().notes[(size_t) hit.index];
            onHint (noteName (n.targetMidi(), true) + "   " + juce::String (n.dstDuration(), 3) + " s");
        }
    }
    setMouseCursor (mouseCursor);
}

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    dragStart = e.position;
    deselectOnUp = false;

    if (e.position.y < (float) rulerHeight && e.position.x > (float) keysWidth)
    {
        drag = DragMode::Seek;
        if (onSeek)
            onSeek (std::max (0.0, xToTime (e.position.x)));
        return;
    }
    if (session == nullptr || session->getEditModel() == nullptr || ! noteArea().toFloat().contains (e.position))
        return;

    const auto hit = noteHitTest (e.position);
    auto sel = session->getSelection();
    const bool additive = e.mods.isShiftDown() || e.mods.isCommandDown();

    if (hit.index < 0)
    {
        drag = DragMode::Rubber;
        rubberBase = additive ? sel : NoteIdSet {};
        if (! additive)
            session->setSelection ({});
        rubber = {};
        return;
    }

    const auto& n = session->getProject().notes[(size_t) hit.index];
    if (view.tool == Tool::Split)
    {
        splitAt (hit.index, e.position.x);
        return;
    }

    if (sel.count (n.id) == 0)
    {
        if (! additive)
            sel.clear();
        sel.insert (n.id);
        session->setSelection (sel);
    }
    else if (additive)
        deselectOnUp = true;

    dragNote = n.id;
    dragBaseTarget = n.targetMidi();
    if (hit.zone == Zone::StartEdge && view.tool != Tool::Pitch)
        drag = DragMode::StretchStart;
    else if (hit.zone == Zone::EndEdge && view.tool != Tool::Pitch)
        drag = DragMode::StretchEnd;
    else if (view.tool == Tool::Pitch)
        drag = DragMode::Pitch;
    else if (view.tool == Tool::Time)
        drag = DragMode::Time;
    else
        drag = DragMode::Undecided;
    session->beginGesture();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == DragMode::Seek)
    {
        if (onSeek)
            onSeek (std::max (0.0, xToTime (e.position.x)));
        return;
    }
    if (session == nullptr)
        return;

    if (drag == DragMode::Rubber)
    {
        rubber = juce::Rectangle<float> (dragStart, e.position);
        auto sel = rubberBase;
        for (const auto& n : session->getProject().notes)
            if (noteBounds (n).intersects (rubber))
                sel.insert (n.id);
        session->setSelection (sel);
        repaint();
        return;
    }

    const float dx = e.position.x - dragStart.x, dy = e.position.y - dragStart.y;
    if (drag == DragMode::Undecided)
    {
        if (std::max (std::abs (dx), std::abs (dy)) < 4.0f)
            return;
        drag = e.mods.isCommandDown() ? DragMode::Free : (std::abs (dx) > std::abs (dy) ? DragMode::Time : DragMode::Pitch);
    }
    if (std::max (std::abs (dx), std::abs (dy)) >= 2.0f)
        deselectOnUp = false;

    const auto sel = session->getSelection();
    const float raw = -dy / view.pixelsPerSemitone;
    const bool snap = view.snapToSemitone && ! e.mods.isAltDown();
    const float delta = snap ? std::round (dragBaseTarget + raw) - dragBaseTarget : raw;
    const double dt = dx / view.pixelsPerSecond;
    const auto id = dragNote;

    switch (drag)
    {
        case DragMode::Pitch:
            session->updateGesture ([&] (Project& p) { edits::transpose (p, sel, delta); });
            break;
        case DragMode::Time:
            session->updateGesture ([&] (Project& p) { edits::moveTime (p, sel, dt); });
            break;
        case DragMode::Free:
            session->updateGesture ([&] (Project& p) { edits::transpose (p, sel, delta); edits::moveTime (p, sel, dt); });
            break;
        case DragMode::StretchStart:
        case DragMode::StretchEnd:
        {
            const double t = xToTime (e.position.x);
            const auto edge = drag == DragMode::StretchStart ? edits::Edge::Start : edits::Edge::End;
            session->updateGesture ([&] (Project& p) { edits::resizeEdge (p, id, edge, t); });
            break;
        }
        case DragMode::None: case DragMode::Undecided: case DragMode::Rubber: case DragMode::Seek:
        default:
            break;
    }

    if (onHint)
        if (const auto* n = session->getProject().find (id))
            onHint (noteName (n->targetMidi(), true) + "   " + juce::String (n->dstDuration(), 3) + " s  (x"
                    + juce::String (n->stretch(), 2) + ")");
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    if (session != nullptr && session->isInGesture())
    {
        static const char* labels[] = { "", "Edit", "Move pitch", "Move time", "Move note", "Stretch note", "Stretch note", "", "" };
        session->endGesture (labels[(int) drag]);
        if (deselectOnUp)
        {
            auto sel = session->getSelection();
            sel.erase (dragNote);
            session->setSelection (sel);
        }
    }
    drag = DragMode::None;
    rubber = {};
    repaint();
}

void PianoRoll::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (session == nullptr || view.tool == Tool::Split)
        return;
    const auto hit = noteHitTest (e.position);
    if (hit.index >= 0 && hit.zone == Zone::Body)
    {
        if (session->isInGesture())
            session->endGesture ("Edit");
        drag = DragMode::None;
        splitAt (hit.index, e.position.x);
    }
}

void PianoRoll::splitAt (int index, float x)
{
    if (session == nullptr)
        return;
    const auto id = session->getProject().notes[(size_t) index].id;
    const double t = xToTime (x);
    const auto centre = session->centreFunction();
    session->perform ("Split note", [&] (Project& p) { edits::split (p, id, t, centre); });
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown())
        zoomTime (std::pow (2.0, (double) w.deltaY * 1.5), e.position.x);
    else if (e.mods.isAltDown())
        zoomPitch (std::pow (2.0f, w.deltaY * 1.5f), e.position.y);
    else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        userAdjustedView = true;
        const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
        view.startSeconds -= d * 300.0 / view.pixelsPerSecond;
        clampView();
        updateScrollBars();
        repaint();
    }
    else
    {
        userAdjustedView = true;
        view.topMidi += w.deltaY * 12.0f;
        clampView();
        updateScrollBars();
        repaint();
    }
}

bool PianoRoll::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const int code = key.getKeyCode();

    if (code == juce::KeyPress::spaceKey)
    {
        if (onTogglePlay)
            onTogglePlay();
        return true;
    }
    if (session == nullptr)
        return false;

    if (mods.isCommandDown() && (code == 'Z' || code == 'z'))
    {
        if (mods.isShiftDown())
            session->redo();
        else
            session->undo();
        return true;
    }
    if (mods.isCommandDown() && (code == 'Y' || code == 'y'))
    {
        session->redo();
        return true;
    }
    if (mods.isCommandDown() && (code == 'A' || code == 'a'))
    {
        NoteIdSet all;
        for (const auto& n : session->getProject().notes)
            all.insert (n.id);
        session->setSelection (all);
        return true;
    }
    if (code == juce::KeyPress::escapeKey)
    {
        session->setSelection ({});
        return true;
    }

    const auto sel = session->getSelection();
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    {
        const float step = mods.isShiftDown() ? 12.0f : (mods.isAltDown() ? 0.1f : 1.0f);
        const float d = code == juce::KeyPress::upKey ? step : -step;
        session->perform ("Transpose", [&] (Project& p) { edits::transpose (p, sel, d); });
        return true;
    }
    if (! mods.isCommandDown() && (code == 'M' || code == 'm'))
    {
        const auto centre = session->centreFunction();
        session->perform ("Merge notes", [&] (Project& p) { edits::merge (p, sel, centre); });
        return true;
    }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        session->perform ("Reset notes", [&] (Project& p) { edits::resetNotes (p, sel); });
        return true;
    }
    if (! mods.isCommandDown() && (code == 'F' || code == 'f'))
    {
        zoomToFit();
        return true;
    }
    return false;
}
} // namespace tone::ui
