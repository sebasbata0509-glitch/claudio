#pragma once

#include "ViewState.h"

#include "../plugin/EditSession.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace tone::ui
{
/**
    The main editing surface: a time ruler, a piano keyboard and the note area where every note is
    drawn as an amplitude-shaped blob at its (edited) pitch, with the pitch curve on top.

    Mouse: drag notes vertically (pitch; snapped to semitones unless Alt is held) or horizontally
    (time); drag a note's edge to stretch it; double-click (or the Split tool) to split; drag on
    empty space to rubber-band select; click the ruler to move the playhead.
    Wheel: scroll; Shift = horizontal; Ctrl/Cmd = time zoom; Alt = pitch zoom.
*/
class PianoRoll : public juce::Component,
                  private plugin::EditSession::Listener,
                  private juce::ScrollBar::Listener
{
public:
    explicit PianoRoll (ViewState&);
    ~PianoRoll() override;

    void setSession (plugin::EditSession*);
    plugin::EditSession* getSession() const noexcept { return session; }

    /** Playhead in session seconds (negative hides it). */
    void setPlayhead (double seconds, bool playing);

    void zoomToFit();
    void zoomTime (double factor, float anchorX);
    void zoomPitch (float factor, float anchorY);

    std::function<void (double)> onSeek;        // ruler click (session seconds)
    std::function<void()> onTogglePlay;         // space bar
    std::function<void (const juce::String&)> onHint;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int rulerHeight = 24;
    static constexpr int keysWidth = 52;
    static constexpr int scrollSize = 12;

private:
    enum class DragMode { None, Undecided, Pitch, Time, Free, StretchStart, StretchEnd, Rubber, Seek };
    enum class Zone { None, Body, StartEdge, EndEdge };

    struct Hit
    {
        int index = -1;
        Zone zone = Zone::None;
    };

    void sessionChanged (plugin::EditSession&) override;
    void scrollBarMoved (juce::ScrollBar*, double newStart) override;

    juce::Rectangle<int> noteArea() const;
    float timeToX (double seconds) const noexcept;
    double xToTime (float x) const noexcept;
    float midiToY (float midi) const noexcept;
    float yToMidi (float y) const noexcept;
    juce::Rectangle<float> noteBounds (const Note&) const noexcept;

    Hit noteHitTest (juce::Point<float>) const;
    void updateScrollBars();
    void clampView();
    void splitAt (int noteIndex, float x);

    void drawGrid (juce::Graphics&, juce::Rectangle<int>) const;
    void drawNotes (juce::Graphics&, juce::Rectangle<int>) const;
    void drawRuler (juce::Graphics&) const;
    void drawKeys (juce::Graphics&) const;

    ViewState& view;
    plugin::EditSession* session = nullptr;
    juce::ScrollBar hScroll { false }, vScroll { true };

    size_t lastNoteCount = 0;
    bool needsFit = true;
    bool userAdjustedView = false; // stop auto-fitting on resize once the user zooms / scrolls
    double playhead = -1.0;
    bool playing = false;

    // current gesture
    DragMode drag = DragMode::None;
    NoteId dragNote = 0;
    float dragBaseTarget = 0.0f;
    juce::Point<float> dragStart;
    juce::Rectangle<float> rubber;
    NoteIdSet rubberBase;
    bool deselectOnUp = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRoll)
};
} // namespace tone::ui
