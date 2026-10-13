#pragma once

#include "Project.h"

#include <functional>

namespace tone::edits
{
/** Shortest a note may become on the output timeline (seconds). */
inline constexpr double kMinNoteSeconds = 0.02;

/** Returns the analysed centre pitch (median MIDI) for a source range; used by split / merge. */
using CenterFn = std::function<float (double srcStart, double srcEnd)>;

enum class Edge { Start, End };

// Pitch -------------------------------------------------------------------------------------
void transpose (Project&, const NoteIdSet&, float semitones);
void setPitchShift (Project&, NoteId, float semitones);
/** Moves each note's centre toward the nearest semitone by amount (0..1). */
void correctPitchCenter (Project&, const NoteIdSet&, float amount);
/** Moves each note's centre toward the nearest degree of the project scale by amount (0..1). */
void correctToScale (Project&, const NoteIdSet&, float amount);
void setDrift (Project&, const NoteIdSet&, float amount);
void setModulation (Project&, const NoteIdSet&, float amount);
void setFormant (Project&, const NoteIdSet&, float semitones);
void setGain (Project&, const NoteIdSet&, float db);
/** Resets pitch, drift, modulation, formant and gain of the notes. */
void resetNotes (Project&, const NoteIdSet&);

// Time --------------------------------------------------------------------------------------
/** Largest |dt| (same sign) that keeps every note >= kMinNoteSeconds and gaps >= 0. */
double clampMove (const Project&, const NoteIdSet&, double dt);
/**
    Moves the notes in time. Contiguous unselected neighbours are stretched/squeezed so the
    shared boundary stays attached; gaps absorb the rest. Returns the dt actually applied.
*/
double moveTime (Project&, const NoteIdSet&, double dt);
/** Stretches a note by dragging one edge (an attached neighbour follows). Returns the new edge time. */
double resizeEdge (Project&, NoteId, Edge, double newDstTime);
/** Puts every note back at its analysed time. */
void resetTiming (Project&);

// Structure ---------------------------------------------------------------------------------
/** Splits a note at an output time. Both halves keep the edit parameters. Returns false if too short. */
bool split (Project&, NoteId, double dstTime, const CenterFn&);
/** Merges each run of contiguous selected notes into one note. Returns false if nothing merged. */
bool merge (Project&, const NoteIdSet&, const CenterFn&);
} // namespace tone::edits
