// Pitch detector accuracy tests.
//
//   test_pitch_detector [vocal.wav]
//
// Sine tones, exponential sweeps and a synthetic sung phrase (known ground
// truth) at 44.1/48/96 kHz, in both detection ranges. If a WAV path is given
// (or NUTSWELLER_VOCAL_WAV is set), a real recording is also analysed and
// compared against an offline full-rate reference YIN.

#include "dsp/PitchDetector.h"
#include "dsp/WavFile.h"
#include "support/Check.h"
#include "support/TestSignals.h"

#include <cstdlib>
#include <map>

using namespace nsw;
using namespace nsw::test;

namespace
{
struct Stats
{
    std::vector<float> absCents;
    int estimates = 0, voiced = 0, gross = 0;
    float median() const { return percentile (absCents, 0.5f); }
    float p95() const { return percentile (absCents, 0.95f); }
    float voicedFraction() const { return estimates ? (float) voiced / (float) estimates : 0.0f; }
};

/** Run the detector; score estimates whose window is entirely voiced (or unvoiced) in the truth. */
Stats run (const Signal& s, DetectionRange range, bool expectVoiced, double skipSeconds = 0.1)
{
    PitchDetector det;
    det.prepare (s.sampleRate);
    det.setRange (range);
    Stats st;
    const int guard = det.getMaxPeriodSamples();
    const auto skip = (int64_t) (skipSeconds * s.sampleRate);

    for (size_t i = 0; i < s.samples.size(); ++i)
    {
        if (! det.process (s.samples[i]))
            continue;
        const auto& e = det.latest();
        if (e.time < skip || e.time + guard >= (int64_t) s.samples.size() || e.time - guard < 0)
            continue;

        // Truth must be consistently voiced/unvoiced across the analysis window.
        bool allVoiced = true, allUnvoiced = true;
        for (int64_t k = e.time - guard; k <= e.time + guard; k += 16)
        {
            allVoiced &= s.f0[(size_t) k] > 0.0f;
            allUnvoiced &= s.f0[(size_t) k] <= 0.0f;
        }
        if (expectVoiced ? ! allVoiced : ! allUnvoiced)
            continue;

        ++st.estimates;
        if (e.voiced)
        {
            ++st.voiced;
            if (expectVoiced)
            {
                const float err = std::abs (centsError (e.frequency, s.f0[(size_t) e.time]));
                st.absCents.push_back (err);
                if (err > 50.0f)
                    ++st.gross;
            }
        }
    }
    return st;
}

const char* rangeName (DetectionRange r) { return r == DetectionRange::Low ? "LOW" : "HIGH"; }

void testSines (double fs)
{
    const std::map<DetectionRange, std::vector<double>> freqs = {
        { DetectionRange::Low, { 55, 82.4, 110, 196, 311, 440 } },
        { DetectionRange::High, { 110, 164.8, 220, 329.6, 523.3, 659.3, 880, 987.8 } },
    };
    for (const auto& [range, list] : freqs)
    {
        float worstMedian = 0, worstP95 = 0, worstVoiced = 1;
        for (double hz : list)
        {
            const auto st = run (sine (fs, hz, 0.6), range, true);
            worstMedian = std::max (worstMedian, st.median());
            worstP95 = std::max (worstP95, st.p95());
            worstVoiced = std::min (worstVoiced, st.voicedFraction());
        }
        check (worstMedian < 1.0f && worstP95 < 3.0f && worstVoiced > 0.98f,
               fmt ("sines %s @ %.0f Hz: worst median %.3f c, worst p95 %.3f c, min voiced %.1f%%",
                    rangeName (range), fs, worstMedian, worstP95, 100.0f * worstVoiced));
    }
}

void testSweeps (double fs)
{
    {
        const auto st = run (chirp (fs, 110.0, 950.0, 4.0), DetectionRange::High, true);
        check (st.median() < 5.0f && st.p95() < 20.0f && st.voicedFraction() > 0.97f && st.gross == 0,
               fmt ("sweep HIGH 110-950 Hz @ %.0f: median %.2f c, p95 %.2f c, voiced %.1f%%, gross %d", fs,
                    st.median(), st.p95(), 100.0f * st.voicedFraction(), st.gross));
    }
    {
        const auto st = run (chirp (fs, 55.0, 480.0, 4.0), DetectionRange::Low, true);
        check (st.median() < 5.0f && st.p95() < 20.0f && st.voicedFraction() > 0.97f && st.gross == 0,
               fmt ("sweep LOW 55-480 Hz @ %.0f: median %.2f c, p95 %.2f c, voiced %.1f%%, gross %d", fs,
                    st.median(), st.p95(), 100.0f * st.voicedFraction(), st.gross));
    }
}

void testVocalModel (double fs)
{
    const auto phrase = vocalPhrase (fs);
    for (auto range : { DetectionRange::High, DetectionRange::Low })
    {
        const auto v = run (phrase, range, true, 0.0);
        check (v.median() < 5.0f && v.p95() < 25.0f && v.gross <= v.estimates / 50 && v.voicedFraction() > 0.95f,
               fmt ("sung phrase %s @ %.0f: median %.2f c, p95 %.2f c, gross %d/%d, voiced %.1f%%", rangeName (range),
                    fs, v.median(), v.p95(), v.gross, v.estimates, 100.0f * v.voicedFraction()));
        const auto u = run (phrase, range, false, 0.0);
        check (u.voicedFraction() < 0.05f,
               fmt ("phrase breaths/fricatives %s @ %.0f: %.1f%% falsely voiced (%d frames)", rangeName (range), fs,
                    100.0f * u.voicedFraction(), u.estimates));
    }
}

void testNoiseAndSilence (double fs)
{
    const auto n = run (noise (fs, 1.0, 0.3f), DetectionRange::High, false);
    check (n.voicedFraction() < 0.02f, fmt ("white noise @ %.0f: %.1f%% falsely voiced", fs, 100.0f * n.voicedFraction()));
    const auto z = run (Signal { fs, std::vector<float> ((size_t) fs, 0.0f), std::vector<float> ((size_t) fs, 0.0f) },
                        DetectionRange::Low, false);
    check (z.voiced == 0, fmt ("silence @ %.0f: %d voiced frames", fs, z.voiced));
}

/** Offline reference: plain full-rate YIN with a long window, no decimation. */
float referencePitch (const std::vector<float>& x, int64_t centre, double fs, float fmin, float fmax, float& aperiodicity)
{
    const int tauMax = (int) (fs / fmin), tauMin = (int) (fs / fmax), w = tauMax;
    const int64_t start = centre - (w + tauMax) / 2;
    if (start < 0 || start + w + tauMax + 2 >= (int64_t) x.size())
        return 0.0f;
    std::vector<double> d ((size_t) tauMax + 2), c ((size_t) tauMax + 2);
    double run = 0;
    c[0] = 1;
    for (int tau = 1; tau <= tauMax + 1; ++tau)
    {
        double s = 0;
        for (int j = 0; j < w; ++j)
        {
            const double diff = x[(size_t) (start + j)] - x[(size_t) (start + j + tau)];
            s += diff * diff;
        }
        d[(size_t) tau] = s;
        run += s;
        c[(size_t) tau] = run > 0 ? s * tau / run : 1;
    }
    int best = -1;
    for (int tau = tauMin; tau <= tauMax; ++tau)
        if (c[(size_t) tau] < 0.1)
        {
            while (tau + 1 <= tauMax && c[(size_t) tau + 1] < c[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    if (best < 0)
    {
        aperiodicity = 1;
        return 0;
    }
    aperiodicity = (float) c[(size_t) best];
    const double a = c[(size_t) best - 1], b = c[(size_t) best], cc = c[(size_t) best + 1];
    const double off = 0.5 * (a - cc) / (a - 2 * b + cc);
    return (float) (fs / (best + std::clamp (off, -0.5, 0.5)));
}

void testRecording (const std::string& path)
{
    AudioFile f;
    if (! readWav (path, f))
    {
        check (false, "could not read " + path);
        return;
    }
    std::vector<float> mono ((size_t) f.numSamples());
    for (auto& ch : f.channels)
        for (size_t i = 0; i < mono.size(); ++i)
            mono[i] += ch[i] / (float) f.numChannels();

    for (auto range : { DetectionRange::High, DetectionRange::Low })
    {
        PitchDetector det;
        det.prepare (f.sampleRate);
        det.setRange (range);
        std::vector<float> errs;
        int compared = 0, gross = 0, missed = 0, refVoiced = 0;
        for (size_t i = 0; i < mono.size(); ++i)
        {
            if (! det.process (mono[i]))
                continue;
            const auto& e = det.latest();
            float ap = 1;
            const float ref = referencePitch (mono, e.time, f.sampleRate, det.getMinFrequency(), det.getMaxFrequency(), ap);
            if (ref <= 0 || ap > 0.08f || e.levelDb < -45.0f)
                continue;
            ++refVoiced;
            if (! e.voiced)
            {
                ++missed;
                continue;
            }
            const float err = std::abs (centsError (e.frequency, ref));
            errs.push_back (err);
            ++compared;
            if (err > 50)
                ++gross;
        }
        check (compared > 0 && percentile (errs, 0.5f) < 10.0f && gross <= compared / 20,
               fmt ("recording %s (%s): %d frames vs reference, median %.2f c, p95 %.2f c, gross %d, missed %d/%d",
                    path.c_str(), rangeName (range), compared, percentile (errs, 0.5f), percentile (errs, 0.95f), gross,
                    missed, refVoiced));
    }
}
} // namespace

int main (int argc, char** argv)
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("-- %.0f Hz\n", fs);
        testSines (fs);
        testSweeps (fs);
        testVocalModel (fs);
        testNoiseAndSilence (fs);
    }

    std::string wav = argc > 1 ? argv[1] : "";
    if (wav.empty())
        if (const char* env = std::getenv ("NUTSWELLER_VOCAL_WAV"))
            wav = env;
    if (! wav.empty())
    {
        std::printf ("-- recording\n");
        testRecording (wav);
    }
    else
    {
        std::printf ("-- no recording given (pass a WAV path or set NUTSWELLER_VOCAL_WAV)\n");
    }

    return finish ("test_pitch_detector");
}
