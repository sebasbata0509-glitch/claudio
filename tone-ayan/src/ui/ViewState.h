#pragma once

namespace tone::ui
{
enum class Tool
{
    Main,  // move pitch or time (axis picked by the first drag direction), stretch from edges
    Pitch, // vertical moves only
    Time,  // horizontal moves and edge stretching only
    Split  // click to split a note
};

/** Zoom / scroll and tool state shared by the editor widgets. */
struct ViewState
{
    double startSeconds = 0.0;
    double pixelsPerSecond = 140.0;
    float topMidi = 79.0f;
    float pixelsPerSemitone = 16.0f;

    Tool tool = Tool::Main;
    bool snapToSemitone = true;
    bool showOriginalCurve = true;
    bool followPlayhead = true;
};
} // namespace tone::ui
