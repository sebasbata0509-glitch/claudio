#pragma once

#include <cstdint>
#include <vector>

namespace tone
{
/** One analysis frame. Frame i is centred on sample i * hop. */
struct PitchFrame
{
    float f0Hz = 0.0f;        // 0 when unvoiced
    float midi = 0.0f;        // continuous MIDI pitch (valid when voiced)
    float voicedProb = 0.0f;  // pYIN voicing probability 0..1
    float rmsDb = -120.0f;    // frame energy
    float onset = 0.0f;       // spectral-flux onset strength
    bool voiced = false;
};

struct PitchTrack
{
    double sampleRate = 44100.0;
    int hop = 220;
    std::vector<PitchFrame> frames;

    double frameSeconds() const noexcept { return hop / sampleRate; }
    double timeOfFrame (int i) const noexcept { return i * frameSeconds(); }

    /** Nearest frame index for a time in seconds, clamped to the track. */
    int frameAt (double seconds) const noexcept
    {
        if (frames.empty())
            return 0;
        const auto i = (long long) (seconds / frameSeconds() + 0.5);
        return (int) (i < 0 ? 0 : (i >= (long long) frames.size() ? (long long) frames.size() - 1 : i));
    }

    /** First frame whose span starts at or after the given time (frame i spans [(i-0.5)h, (i+0.5)h)). */
    int frameBoundaryAt (double seconds) const noexcept
    {
        const auto i = (long long) (seconds / frameSeconds() + 0.5 + 1.0e-9);
        return (int) (i < 0 ? 0 : (i > (long long) frames.size() ? (long long) frames.size() : i));
    }

    /** Time of the boundary preceding frame i. */
    double boundaryTime (int i) const noexcept
    {
        const double t = (i - 0.5) * frameSeconds();
        return t < 0.0 ? 0.0 : t;
    }
};

/** A glottal-period mark used by PSOLA: sample position and local period, both in samples. */
struct PitchMark
{
    double position = 0.0;
    float period = 0.0f;
};
} // namespace tone
