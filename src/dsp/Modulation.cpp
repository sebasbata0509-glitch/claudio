#include "Modulation.h"

#include <algorithm>
#include <cmath>

namespace nsw
{

float modSpan (ModDest d)
{
    switch (d)
    {
        case ModDest::Pitch:      return 24.0f; // semitones
        case ModDest::Formant:    return 12.0f; // semitones
        case ModDest::Harmonics:  return 2.0f;
        case ModDest::Alternator: return 1.0f;
        case ModDest::FM:         return 1.0f;
        case ModDest::Ratio:      return 5.0f;  // octaves of ratio
        case ModDest::Smear:      return 1.0f;
        case ModDest::Stereo:     return 1.0f;
        case ModDest::Detune:     return 50.0f; // cents
        case ModDest::Mix:        return 1.0f;
        case ModDest::Output:     return 24.0f; // dB
        case ModDest::None:
        case ModDest::Count:      break;
    }
    return 0.0f;
}

void Modulators::prepare (double sampleRate)
{
    fs = sampleRate;
    reset();
}

void Modulators::reset()
{
    freePpq = 0.0;
    seqOut = 0.0f;
    step = 0;
    stage = Stage::Idle;
    env = 0.0f;
    lastGate = retrig = false;
    follower = inputPeak = 0.0f;
    levelOpen = false;
}

bool Modulators::levelGate (float thresholdDb) noexcept
{
    // Peak follower: fast attack, ~80 ms release; 6 dB hysteresis.
    follower = std::max (inputPeak, follower * 0.985f);
    const float db = 20.0f * std::log10 (std::max (follower, 1.0e-6f));
    if (! levelOpen && db > thresholdDb)
        levelOpen = true;
    else if (levelOpen && db < thresholdDb - 6.0f)
        levelOpen = false;
    return levelOpen;
}

void Modulators::advance (const ModParams& m, int samples, double ppq, double bpm, bool gate) noexcept
{
    const double dt = (double) samples / fs;

    // ---- Sequencer
    if (ppq < 0.0)
    {
        freePpq += dt * bpm / 60.0;
        ppq = freePpq;
    }
    else
    {
        freePpq = ppq;
    }

    if (m.seqOn)
    {
        const int len = std::clamp (m.seqLength, 1, 16);
        const double stepBeats = seqRateBeats (m.seqRate);
        const double pos = ppq / stepBeats;
        const double whole = std::floor (pos);
        const auto phase = (float) (pos - whole);
        step = (int) (((long long) whole % len + len) % len);
        const int prev = (step + len - 1) % len;

        const float cur = std::clamp (m.seqValue[(size_t) step], -1.0f, 1.0f);
        const float before = std::clamp (m.seqValue[(size_t) prev], -1.0f, 1.0f);
        const float glide = std::clamp (m.seqGlide[(size_t) step], 0.0f, 1.0f);
        float v = cur;
        if (glide > 0.0f && phase < glide)
        {
            const float x = phase / glide;
            v = before + (cur - before) * x * x * (3.0f - 2.0f * x); // smoothstep
        }
        seqOut = v * m.seqDepth;
    }
    else
    {
        seqOut = 0.0f;
    }

    // ---- ADSR (linear attack, exponential decay/release)
    if ((gate && ! lastGate) || (gate && retrig))
        stage = Stage::Attack;
    else if (! gate && lastGate)
        stage = Stage::Release;
    lastGate = gate;
    retrig = false;

    const auto ms = (float) (dt * 1000.0);
    switch (stage)
    {
        case Stage::Attack:
            env += ms / std::max (1.0f, m.attackMs);
            if (env >= 1.0f)
            {
                env = 1.0f;
                stage = Stage::Decay;
            }
            break;
        case Stage::Decay:
        {
            const float k = std::exp (-ms / std::max (1.0f, m.decayMs / 4.0f));
            env = m.sustain + (env - m.sustain) * k;
            if (std::abs (env - m.sustain) < 1.0e-3f)
                stage = Stage::Sustain;
            break;
        }
        case Stage::Sustain:
            env = m.sustain;
            break;
        case Stage::Release:
        {
            const float k = std::exp (-ms / std::max (1.0f, m.releaseMs / 4.0f));
            env *= k;
            if (env < 1.0e-4f)
            {
                env = 0.0f;
                stage = Stage::Idle;
            }
            break;
        }
        case Stage::Idle:
            env = 0.0f;
            break;
    }
}

} // namespace nsw
