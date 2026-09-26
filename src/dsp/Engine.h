#pragma once

#include "Modulation.h"
#include "Synth.h"

#include <atomic>

namespace nsw
{

enum class MidiMode
{
    Off,
    Poly,
    Mono
};

enum class VelocityTarget
{
    None,
    Level,
    Formant,
    Harmonics,
    FM,
    Smear
};

/** Plain parameter values (percentages as 0..1). */
struct Params
{
    float pitch = 0.0f;       // semitones, -24..24
    bool snap = false;
    float octave = 0.0f;      // -2..2
    float formant = 0.0f;     // semitones, -12..12
    float harmonics = 0.0f;   // -1..1
    float alternator = 0.0f;  // 0..1
    float fm = 0.0f;          // 0..1
    float fmRatio = 1.0f;     // 0.25..8
    float glideMs = 0.0f;     // 0..500
    float smear = 0.0f;       // 0..1
    float stereo = 0.0f;      // 0..1
    float detuneCents = 0.0f; // 0..50 (L/R spread)
    float mix = 1.0f;         // 0..1
    float outputDb = 0.0f;    // -24..12
    MidiMode midiMode = MidiMode::Off;
    DetectionRange range = DetectionRange::High;
    VelocityTarget velocityTarget = VelocityTarget::None;
    float velocityAmount = 0.5f; // -1..1
    ModParams mod;
};

struct MidiEvent
{
    enum class Type { NoteOn, NoteOff, AllNotesOff };
    int sampleOffset = 0;
    Type type = Type::NoteOn;
    int note = 60;
    float velocity = 1.0f;
};

/**
    The complete NutSweller signal path, independent of any plugin framework:
    analysis -> PSOLA voices (with formant resampling) -> harmonics / FM ->
    latency-compensated dry/wet -> output gain.
*/
class Engine
{
public:
    static constexpr int kMaxVoices = 8;
    static constexpr int kMaxChannels = 2;
    static constexpr int kControlInterval = 32;

    void prepare (double sampleRate, int maxBlockSize, int numInputChannels, int numOutputChannels);
    void reset();

    /** Set parameter values; picked up (and smoothed) from the next sample. */
    void setParams (const Params& p) noexcept;

    /** Host tempo / position for the sequencer; call before each process(). */
    void setTransport (const Transport& t) noexcept { transport = t; }

    /**
        Process a block. `in` / `out` may alias. MIDI events must be sorted by
        sampleOffset.
    */
    void process (const float* const* in, float* const* out, int numSamples,
                  const MidiEvent* events = nullptr, int numEvents = 0) noexcept;

    /** Run only the latency-matched dry path (for host bypass). */
    void processBypassed (const float* const* in, float* const* out, int numSamples) noexcept;

    /** Total input -> output delay, identical for the dry and wet paths. */
    int getLatencySamples() const noexcept { return latencyTotal; }
    static int computeLatencySamples (double sampleRate, DetectionRange range);

    // Real-time readouts for the UI (written on the audio thread).
    struct Display
    {
        std::atomic<float> frequency { 0.0f };
        std::atomic<float> confidence { 0.0f };
        std::atomic<bool> voiced { false };
        std::atomic<float> level { -120.0f };
        std::atomic<int> seqStep { 0 };
        std::atomic<float> envelope { 0.0f };
    };
    const Display& display() const noexcept { return disp; }

    const Analyzer& analyzer() const noexcept { return an; }

private:
    struct Voice
    {
        bool active = false;
        bool held = false;
        int note = -1;
        float velocity = 1.0f;
        uint64_t age = 0;
        float harmonics = 0.0f, fm = 0.0f;
        Smoother gain;        // note on/off fades
        Smoother targetNote;  // glide in MIDI modes
        Smoother shiftSemis;  // glide in follow mode
        Smoother midiWeight;  // follow <-> MIDI handover
        SynthChannel ch[kMaxChannels];
        PostStage post[kMaxChannels];
        GrainSettings gs[kMaxChannels];
    };

    void configureTiming();
    void startVoice (Voice& v, int note, float velocity, bool glideFromCurrent);
    void handleEvent (const MidiEvent& e) noexcept;
    void updateControl (int sampleInBlock) noexcept;
    Params applyModulation (const Params& base) const noexcept;
    float currentShift() const noexcept;
    Voice* findVoiceForNote (int note) noexcept;

    double fs = 48000.0;
    int numIn = 2, numOut = 2;
    Analyzer an;
    SynthTiming timing;
    int fmBase = 72;
    int latencyTotal = 0;

    Params params, effective;
    Transport transport;
    Modulators mods;
    float controlPeak = 0.0f;
    DetectionRange activeRange = DetectionRange::High;
    MidiMode activeMode = MidiMode::Off;
    Voice voices[kMaxVoices];
    uint64_t voiceCounter = 0;
    int controlCounter = 0;

    // Mono-mode note stack (most recent last).
    int heldNotes[16] {};
    int numHeld = 0;
    float monoVelocity = 1.0f;

    Smoother mixSmooth, gainSmooth;
    Display disp;
};

} // namespace nsw
