#pragma once

#include "../common/Parallel.h"
#include "PitchTrack.h"

#include <cstddef>

namespace tone
{
struct PYinParams
{
    double minHz = 55.0;          // A1
    double maxHz = 1760.0;        // A6
    double hopSeconds = 0.005;    // 5 ms
    float silenceDb = -60.0f;     // frames quieter than this are unvoiced
    float betaA = 2.0f, betaB = 18.0f;   // threshold prior (mean 0.1), as in the pYIN paper
    int numThresholds = 100;
    float voicingSwitchProb = 0.02f;     // per-frame probability of a voiced/unvoiced change
    float maxJumpSemitones = 2.5f;       // max pitch change between consecutive frames
    float binCents = 10.0f;              // HMM pitch-state resolution
};

/**
    Probabilistic YIN (Mauch & Dixon, 2014), implemented from the paper.

    Stage 1: for every frame, the cumulative-mean-normalised difference function is thresholded
    with a beta-distributed range of thresholds, producing a set of pitch candidates with
    probabilities. Stage 2: a two-state voicing HMM decides voiced/unvoiced, and a pitch HMM
    (Viterbi) picks the smoothest candidate path inside each voiced run.
*/
PitchTrack runPYin (const float* samples, size_t numSamples, double sampleRate,
                    const PYinParams& params = {}, const JobControl& job = {});
} // namespace tone
