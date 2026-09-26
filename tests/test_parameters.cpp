// One objective check per parameter: each control must do what it says and
// nothing else, without zipper noise.

#include "dsp/Engine.h"
#include "support/Check.h"
#include "support/Spectrum.h"
#include "support/TestSignals.h"

using namespace nsw;
using namespace nsw::test;

namespace
{
using Channels = std::vector<std::vector<float>>;
constexpr double fs = 48000.0;

struct Automation
{
    size_t atSample;
    Params params;
};

Channels render (const std::vector<float>& input, const Params& p, int numOut = 1,
                 const std::vector<MidiEvent>& midi = {}, const std::vector<Automation>& automation = {},
                 int block = 64)
{
    Engine e;
    e.setParams (p);
    e.prepare (fs, block, 1, numOut);
    Channels out ((size_t) numOut, std::vector<float> (input.size()));
    for (size_t pos = 0; pos < input.size(); pos += (size_t) block)
    {
        for (const auto& a : automation)
            if (a.atSample >= pos && a.atSample < pos + (size_t) block)
                e.setParams (a.params);
        const int n = (int) std::min ((size_t) block, input.size() - pos);
        std::vector<MidiEvent> ev;
        for (auto m : midi)
            if (m.sampleOffset >= (int) pos && m.sampleOffset < (int) pos + n)
            {
                m.sampleOffset -= (int) pos;
                ev.push_back (m);
            }
        const float* in[1] = { input.data() + pos };
        float* o[2] = { out[0].data() + pos, numOut > 1 ? out[1].data() + pos : nullptr };
        e.process (in, o, n, ev.data(), (int) ev.size());
    }
    return out;
}

int latency()
{
    Engine e;
    e.prepare (fs, 64, 1, 1);
    return e.getLatencySamples();
}

Params neutral()
{
    Params p;
    p.mix = 1.0f;
    return p;
}

Signal steadyVowel (double hz, double seconds)
{
    Signal s;
    s.sampleRate = fs;
    VowelSynth v (fs);
    v.setFormants (VowelSynth::vowelA());
    v.voiced (s, hz, hz, seconds, 0.4f, 0.0);
    return s;
}

/** Power in +-bw Hz around a frequency, from an LTAS. */
double bandPower (const std::vector<double>& psd, double hz, double bw)
{
    const double binHz = fs / (2.0 * (double) (psd.size() - 1));
    double s = 0;
    for (auto k = (long) ((hz - bw) / binHz); k <= (long) ((hz + bw) / binHz); ++k)
        if (k >= 0 && k < (long) psd.size())
            s += psd[(size_t) k];
    return s;
}

double oddEvenDb (const std::vector<float>& x, double f0)
{
    const auto psd = ltas (x, (size_t) (0.4 * fs), x.size() - 8192, 8192);
    double odd = 0, even = 0;
    for (int k = 1; k <= 12; ++k)
        (k % 2 ? odd : even) += bandPower (psd, k * f0, 12.0);
    return 10.0 * std::log10 (odd / even);
}

/** Fraction of power (dB) that is NOT within +-12 Hz of a harmonic of f0, over 50 Hz - 5 kHz. */
double inharmonicDb (const std::vector<float>& x, double f0)
{
    const auto psd = ltas (x, (size_t) (0.4 * fs), x.size() - 8192, 8192);
    const double binHz = fs / (2.0 * (double) (psd.size() - 1));
    double harm = 0, other = 0;
    for (size_t k = 0; k < psd.size(); ++k)
    {
        const double f = (double) k * binHz;
        if (f < 50.0 || f > 5000.0)
            continue;
        const double nearest = std::round (f / f0) * f0;
        (std::abs (f - nearest) <= 12.0 && nearest > 0 ? harm : other) += psd[k];
    }
    return 10.0 * std::log10 (other / (harm + other));
}

/** Output pitch track: (time in input timeline, Hz) for voiced output frames. */
std::vector<std::pair<double, float>> pitchTrack (const std::vector<float>& x, DetectionRange range)
{
    PitchDetector d;
    d.prepare (fs);
    d.setRange (range);
    std::vector<std::pair<double, float>> track;
    const int lat = latency();
    for (size_t i = 0; i < x.size(); ++i)
        if (d.process (x[i]) && d.latest().voiced)
            track.emplace_back ((double) (d.latest().time - lat) / fs, d.latest().frequency);
    return track;
}

float medianPitch (const std::vector<float>& x, double from, double to, DetectionRange range = DetectionRange::High)
{
    std::vector<float> v;
    for (auto& [t, hz] : pitchTrack (x, range))
        if (t >= from && t <= to)
            v.push_back (hz);
    return percentile (v, 0.5f);
}

double rmsDb (const std::vector<float>& x, size_t from, size_t to)
{
    double s = 0;
    for (size_t i = from; i < to; ++i)
        s += (double) x[i] * x[i];
    return 10.0 * std::log10 (s / (double) (to - from) + 1e-30);
}

void testHarmonics()
{
    const auto in = steadyVowel (200.0, 2.0);
    auto p = neutral();
    p.harmonics = 0.0f;
    const double base = oddEvenDb (render (in.samples, p)[0], 200.0);
    p.harmonics = 1.0f;
    const double odd = oddEvenDb (render (in.samples, p)[0], 200.0);
    p.harmonics = -1.0f;
    const double even = oddEvenDb (render (in.samples, p)[0], 200.0);
    check (odd > base + 8.0 && even < base - 8.0,
           fmt ("harmonics: odd/even balance %.1f dB at 0, %.1f dB at +1 (odd), %.1f dB at -1 (even)", base, odd, even));
}

void testAlternator()
{
    const auto in = steadyVowel (200.0, 2.0);
    auto p = neutral();
    const double clean = inharmonicDb (render (in.samples, p)[0], 200.0);
    p.alternator = 1.0f;
    const double alt = inharmonicDb (render (in.samples, p)[0], 200.0);
    p.alternator = 0.5f;
    const double half = inharmonicDb (render (in.samples, p)[0], 200.0);
    check (alt > clean + 15.0 && half > clean + 10.0,
           fmt ("alternator: sub-harmonic/inharmonic energy %.1f dB at 0%%, %.1f dB at 50%%, %.1f dB at 100%%", clean, half,
                alt));
}

void testFM()
{
    const auto in = steadyVowel (200.0, 2.0);
    auto p = neutral();
    const double clean = inharmonicDb (render (in.samples, p)[0], 200.0);
    p.fm = 0.6f;
    p.fmRatio = 1.5f;
    const double fm = inharmonicDb (render (in.samples, p)[0], 200.0);
    p.fmRatio = 2.0f; // integer ratio: sidebands land on harmonics
    const double fmInt = inharmonicDb (render (in.samples, p)[0], 200.0);
    check (fm > clean + 15.0 && fmInt < fm - 6.0,
           fmt ("FM: inharmonic energy %.1f dB off, %.1f dB at ratio 1.5, %.1f dB at ratio 2 (harmonic sidebands)", clean,
                fm, fmInt));
}

void testGlide()
{
    const auto in = steadyVowel (200.0, 2.5);
    for (float glide : { 0.0f, 200.0f })
    {
        auto p = neutral();
        p.glideMs = glide;
        auto q = p;
        q.pitch = 12.0f;
        const auto out = render (in.samples, p, 1, {}, { { (size_t) (1.0 * fs), q } });
        // Time until the output is within 1 semitone of the new pitch.
        double t90 = -1;
        for (auto& [t, hz] : pitchTrack (out[0], DetectionRange::High))
            if (t > 1.0 && t90 < 0 && hz > 200.0 * semitonesToRatio (11.0f))
                t90 = (t - 1.0) * 1000.0;
        const bool ok = glide == 0.0f ? (t90 >= 0 && t90 < 40.0) : (t90 > 120.0 && t90 < 260.0);
        check (ok, fmt ("glide %.0f ms: +12 st step settles (within 1 st) after %.0f ms", glide, t90));
    }
}

void testSmear()
{
    const auto in = steadyVowel (220.0, 2.0);
    auto p = neutral();
    p.pitch = 12.0f;
    PitchDetector d;
    auto periodicity = [&] (const std::vector<float>& x) {
        d.prepare (fs);
        double c = 0;
        int n = 0;
        for (size_t i = (size_t) (0.4 * fs); i < x.size(); ++i)
            if (d.process (x[i]))
            {
                c += d.latest().confidence;
                ++n;
            }
        return c / std::max (1, n);
    };
    const auto clean = render (in.samples, p)[0];
    p.smear = 1.0f;
    const auto smeared = render (in.samples, p)[0];
    const float hz = medianPitch (smeared, 0.4, 1.9);
    check (periodicity (smeared) < periodicity (clean) - 0.03 && std::abs (centsError (hz, 440.0f)) < 20.0f,
           fmt ("smear 100%%: periodicity %.3f -> %.3f (blurred), pitch still %.1f Hz", periodicity (clean),
                periodicity (smeared), hz));
}

void testStereoAndDetune()
{
    const auto in = steadyVowel (220.0, 2.0);
    auto correlation = [] (const Channels& c) {
        double lr = 0, ll = 0, rr = 0;
        for (size_t i = (size_t) (0.4 * fs); i < c[0].size(); ++i)
        {
            lr += (double) c[0][i] * c[1][i];
            ll += (double) c[0][i] * c[0][i];
            rr += (double) c[1][i] * c[1][i];
        }
        return lr / std::sqrt (ll * rr + 1e-30);
    };
    auto p = neutral();
    p.pitch = 12.0f;
    const double c0 = correlation (render (in.samples, p, 2));
    p.stereo = 1.0f;
    const double c1 = correlation (render (in.samples, p, 2));
    check (c0 > 0.9999 && c1 < 0.8, fmt ("stereo: L/R correlation %.4f at 0%%, %.3f at 100%%", c0, c1));

    p.stereo = 0.0f;
    p.detuneCents = 50.0f;
    const auto out = render (in.samples, p, 2);
    const float l = medianPitch (out[0], 0.4, 1.9), r = medianPitch (out[1], 0.4, 1.9);
    const float spread = centsError (r, l);
    check (std::abs (spread - 50.0f) < 5.0f, fmt ("detune 50 c: L %.2f Hz, R %.2f Hz, spread %.1f c", l, r, spread));
}

void testOutputGainAndZipper()
{
    const auto in = sine (fs, 220.0, 1.5, 0.25f);
    auto p = neutral();
    const auto ref = render (in.samples, p)[0];
    p.outputDb = -6.0f;
    const auto quiet = render (in.samples, p)[0];
    const size_t a = (size_t) (0.3 * fs), b = in.samples.size();
    const double diff = rmsDb (quiet, a, b) - rmsDb (ref, a, b);
    check (std::abs (diff + 6.0) < 0.05, fmt ("output gain -6 dB: measured %.2f dB", diff));

    // Slam output gain and mix every 64-sample block; smoothing must keep the waveform continuous.
    p = neutral();
    p.mix = 0.0f;
    std::vector<Automation> autos;
    Random r (5);
    for (size_t pos = 0; pos < in.samples.size(); pos += 64)
    {
        auto q = p;
        q.outputDb = r.uniform() < 0.5f ? -24.0f : 12.0f;
        q.mix = r.uniform() < 0.5f ? 0.0f : 0.001f;
        autos.push_back ({ pos, q });
    }
    const auto out = render (in.samples, p, 1, {}, autos)[0];
    // Clicks/zipper are broadband: energy above 3 kHz must stay far below the 220 Hz tone.
    Biquad h1, h2;
    h1.setHighpass (fs, 3000.0, 0.54);
    h2.setHighpass (fs, 3000.0, 1.31);
    double hf = 0, all = 0;
    for (size_t i = 0; i < out.size(); ++i)
    {
        const float y = h2.process (h1.process (out[i]));
        if (i > (size_t) (0.1 * fs))
        {
            hf += (double) y * y;
            all += (double) out[i] * out[i];
        }
    }
    const double ratio = 10.0 * std::log10 (hf / all);
    check (ratio < -70.0, fmt ("zipper: random gain/mix jumps every 64 samples leave %.1f dB of HF residue", ratio));
}

void testMidi()
{
    const auto in = steadyVowel (200.0, 2.0);
    auto p = neutral();
    p.midiMode = MidiMode::Mono;
    const auto mono = render (in.samples, p, 1, { { (int) (0.2 * fs), MidiEvent::Type::NoteOn, 69, 1.0f } })[0];
    const float hz = medianPitch (mono, 0.5, 1.8);
    check (std::abs (centsError (hz, 440.0f)) < 10.0f, fmt ("MIDI MONO: note A4 on a 200 Hz voice -> %.2f Hz", hz));

    p.midiMode = MidiMode::Poly;
    const auto poly = render (in.samples, p, 1,
                              { { (int) (0.2 * fs), MidiEvent::Type::NoteOn, 60, 1.0f },
                                { (int) (0.2 * fs), MidiEvent::Type::NoteOn, 67, 1.0f } })[0];
    const auto psd = ltas (poly, (size_t) (0.5 * fs), poly.size() - 8192, 8192);
    const double c4 = bandPower (psd, midiToFrequency (60), 6), g4 = bandPower (psd, midiToFrequency (67), 6);
    const double orig = bandPower (psd, 200.0, 6);
    check (c4 > orig * 30.0 && g4 > orig * 30.0,
           fmt ("MIDI POLY: C4 and G4 present (%.1f / %.1f dB above the original 200 Hz)", 10 * std::log10 (c4 / orig),
                10 * std::log10 (g4 / orig)));

    // Poly with no notes: wet path silent.
    const auto silent = render (in.samples, p)[0];
    check (rmsDb (silent, (size_t) (0.3 * fs), silent.size()) < -100.0, "MIDI POLY with no notes: wet output silent");

    // Velocity -> level.
    p.midiMode = MidiMode::Mono;
    p.velocityTarget = VelocityTarget::Level;
    p.velocityAmount = 1.0f;
    const auto loud = render (in.samples, p, 1, { { 0, MidiEvent::Type::NoteOn, 69, 1.0f } })[0];
    const auto soft = render (in.samples, p, 1, { { 0, MidiEvent::Type::NoteOn, 69, 0.5f } })[0];
    const double d = rmsDb (soft, (size_t) (0.4 * fs), soft.size()) - rmsDb (loud, (size_t) (0.4 * fs), loud.size());
    check (std::abs (d + 6.02) < 0.3, fmt ("velocity -> level: half velocity is %.2f dB", d));
}
} // namespace

int main()
{
    testHarmonics();
    testAlternator();
    testFM();
    testGlide();
    testSmear();
    testStereoAndDetune();
    testOutputGainAndZipper();
    testMidi();
    return finish ("test_parameters");
}
