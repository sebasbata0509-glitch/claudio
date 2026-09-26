#include "Engine.h"

namespace nsw
{

namespace
{
    int maxPeriodFor (double fs, DetectionRange r)
    {
        return (int) std::ceil (fs / (r == DetectionRange::Low ? 50.0 : 100.0));
    }
    int fmBaseDelayFor (double fs) { return (int) std::lround (0.0015 * fs); }
} // namespace

int Engine::computeLatencySamples (double sampleRate, DetectionRange range)
{
    return (int) std::ceil (2.25 * maxPeriodFor (sampleRate, range)) + 8 + fmBaseDelayFor (sampleRate);
}

void Engine::prepare (double sampleRate, int /*maxBlockSize*/, int numInputChannels, int numOutputChannels)
{
    fs = sampleRate;
    numIn = std::clamp (numInputChannels, 1, kMaxChannels);
    numOut = std::clamp (numOutputChannels, 1, kMaxChannels);
    fmBase = fmBaseDelayFor (fs);

    // Everything is sized for the LOW range so switching never allocates.
    const int maxPeriodLow = maxPeriodFor (fs, DetectionRange::Low) + 16;
    an.prepare (fs, numIn, 12 * maxPeriodLow + (int) (0.05 * fs));
    an.setRange (params.range);
    activeRange = params.range;

    uint32_t seed = 0x1234567u;
    for (auto& v : voices)
    {
        for (int c = 0; c < kMaxChannels; ++c)
        {
            v.ch[c].prepare (8 * maxPeriodLow, seed += 0x9E3779B9u);
            v.post[c].prepare (fs, maxPeriodLow, fmBase);
        }
        v.gain.prepare (fs, 8.0);
        v.targetNote.prepare (fs, 1.0);
        v.shiftSemis.prepare (fs, 1.0);
        v.midiWeight.prepare (fs, 20.0);
    }
    mixSmooth.prepare (fs, 20.0);
    gainSmooth.prepare (fs, 20.0);

    configureTiming();
    reset();
}

void Engine::configureTiming()
{
    const int maxPeriod = an.getMaxPeriod();
    timing.maxPeriod = maxPeriod;
    timing.maxHalf = (int) std::ceil (1.25 * maxPeriod) + 2;
    timing.unvoicedHalf = std::min ((int) std::lround (0.005 * fs), maxPeriod / 2);
    timing.latency = (int) std::ceil (2.25 * maxPeriod) + 8;
    timing.sampleRate = fs;
    latencyTotal = timing.latency + fmBase;
}

void Engine::reset()
{
    an.reset();
    for (auto& v : voices)
    {
        v.active = v.held = false;
        v.note = -1;
        for (int c = 0; c < kMaxChannels; ++c)
        {
            v.ch[c].reset (an.now(), timing);
            v.post[c].reset();
        }
        v.gain.reset (0.0f);
    }
    numHeld = 0;
    controlCounter = 0;
    activeMode = params.midiMode;

    const float shift = (params.snap ? std::round (params.pitch) : params.pitch) + 12.0f * params.octave;
    if (activeMode != MidiMode::Poly)
    {
        auto& v = voices[0];
        v.active = true;
        v.gain.reset (1.0f);
        v.shiftSemis.reset (shift);
        v.targetNote.reset (60.0f);
        v.midiWeight.reset (0.0f);
    }
    mixSmooth.reset (params.mix);
    gainSmooth.reset (dbToGain (params.outputDb));
}

void Engine::setParams (const Params& p) noexcept
{
    params = p;
}

Engine::Voice* Engine::findVoiceForNote (int note) noexcept
{
    for (auto& v : voices)
        if (v.active && v.held && v.note == note)
            return &v;
    return nullptr;
}

void Engine::startVoice (Voice& v, int note, float velocity, bool glideFromCurrent)
{
    const float shift = (params.snap ? std::round (params.pitch) : params.pitch) + 12.0f * params.octave;
    if (! v.active || ! glideFromCurrent)
    {
        for (int c = 0; c < kMaxChannels; ++c)
        {
            if (! v.active)
            {
                v.ch[c].reset (an.now(), timing);
                v.post[c].reset();
            }
        }
        if (! v.active)
            v.gain.reset (0.0f);
        v.targetNote.reset ((float) note + shift);
        v.shiftSemis.reset (shift);
        v.midiWeight.reset (1.0f);
    }
    v.active = true;
    v.held = true;
    v.note = note;
    v.velocity = velocity;
    v.age = ++voiceCounter;
    v.gain.setTarget (1.0f);
}

void Engine::handleEvent (const MidiEvent& e) noexcept
{
    if (e.type == MidiEvent::Type::AllNotesOff)
    {
        numHeld = 0;
        for (auto& v : voices)
            if (activeMode == MidiMode::Poly)
            {
                v.held = false;
                v.gain.setTarget (0.0f);
            }
        return;
    }

    const bool on = e.type == MidiEvent::Type::NoteOn && e.velocity > 0.0f;

    // Mono note stack is kept in every mode so switching to MONO mid-note works.
    for (int i = 0; i < numHeld; ++i)
        if (heldNotes[i] == e.note)
        {
            std::copy (heldNotes + i + 1, heldNotes + numHeld, heldNotes + i);
            --numHeld;
            break;
        }
    if (on)
    {
        if (numHeld == 16)
        {
            std::copy (heldNotes + 1, heldNotes + 16, heldNotes);
            --numHeld;
        }
        heldNotes[numHeld++] = e.note;
        monoVelocity = e.velocity;
    }

    if (activeMode == MidiMode::Mono)
    {
        auto& v = voices[0];
        if (on)
        {
            const bool fromFollow = v.midiWeight.getCurrent() < 0.01f;
            const float shift = (params.snap ? std::round (params.pitch) : params.pitch) + 12.0f * params.octave;
            if (fromFollow)
                v.targetNote.reset ((float) e.note + shift);
            v.velocity = e.velocity;
        }
        return;
    }

    if (activeMode != MidiMode::Poly)
        return;

    if (on)
    {
        if (auto* v = findVoiceForNote (e.note))
        {
            startVoice (*v, e.note, e.velocity, true);
            return;
        }
        Voice* chosen = nullptr;
        for (auto& v : voices)
            if (! v.active)
            {
                chosen = &v;
                break;
            }
        if (chosen != nullptr)
        {
            startVoice (*chosen, e.note, e.velocity, false);
            return;
        }
        // Steal: prefer released voices, then the oldest; glide it to the new note.
        for (auto& v : voices)
            if (chosen == nullptr || (! v.held && chosen->held) || (v.held == chosen->held && v.age < chosen->age))
                chosen = &v;
        startVoice (*chosen, e.note, e.velocity, true);
    }
    else
    {
        for (auto& v : voices)
            if (v.active && v.held && v.note == e.note)
            {
                v.held = false;
                v.gain.setTarget (0.0f);
            }
    }
}

void Engine::updateControl() noexcept
{
    const Params& p = params;

    if (p.range != activeRange)
    {
        activeRange = p.range;
        an.setRange (activeRange);
        configureTiming();
        for (auto& v : voices)
            for (int c = 0; c < kMaxChannels; ++c)
            {
                v.ch[c].reset (an.now(), timing);
                v.post[c].reset();
            }
    }

    if (p.midiMode != activeMode)
    {
        activeMode = p.midiMode;
        for (int i = 0; i < kMaxVoices; ++i)
        {
            auto& v = voices[i];
            const bool keep = activeMode != MidiMode::Poly && i == 0;
            if (keep)
            {
                if (! v.active)
                {
                    for (int c = 0; c < kMaxChannels; ++c)
                    {
                        v.ch[c].reset (an.now(), timing);
                        v.post[c].reset();
                    }
                    v.gain.reset (0.0f);
                    v.midiWeight.reset (0.0f);
                }
                v.active = true;
                v.held = false;
                v.velocity = 1.0f;
                v.gain.setTarget (1.0f);
            }
            else if (v.active)
            {
                v.held = false;
                v.gain.setTarget (0.0f);
            }
        }
    }

    const float shift = (p.snap ? std::round (p.pitch) : p.pitch) + 12.0f * p.octave;
    const float glide = std::max (1.0f, p.glideMs / 3.0f);
    const bool midiNotes = activeMode != MidiMode::Off;
    int numActive = 0;
    for (auto& v : voices)
        numActive += v.active ? 1 : 0;

    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        v.targetNote.setTime (glide);
        v.shiftSemis.setTime (glide);
        v.shiftSemis.setTarget (shift);

        if (activeMode == MidiMode::Mono)
        {
            if (numHeld > 0)
            {
                v.targetNote.setTarget ((float) heldNotes[numHeld - 1] + shift);
                v.midiWeight.setTarget (1.0f);
            }
            else
            {
                v.midiWeight.setTarget (0.0f);
            }
        }
        else if (activeMode == MidiMode::Poly)
        {
            v.targetNote.setTarget ((float) v.note + shift);
            v.midiWeight.setTarget (1.0f);
        }
        else
        {
            v.midiWeight.setTarget (0.0f);
        }

        const float targetNote = v.targetNote.advance (kControlInterval);
        const float shiftNow = v.shiftSemis.advance (kControlInterval);
        const float weight = v.midiWeight.advance (kControlInterval);

        // Velocity mapping (MIDI modes only).
        float formant = p.formant, harmonics = p.harmonics, fm = p.fm, smear = p.smear, level = 1.0f;
        if (midiNotes && p.velocityTarget != VelocityTarget::None)
        {
            const float amt = p.velocityAmount, vel = v.velocity;
            switch (p.velocityTarget)
            {
                case VelocityTarget::Level:
                    level = amt >= 0.0f ? 1.0f - amt * (1.0f - vel) : 1.0f + amt * vel;
                    break;
                case VelocityTarget::Formant:   formant += amt * vel * 12.0f; break;
                case VelocityTarget::Harmonics: harmonics += amt * vel * 2.0f; break;
                case VelocityTarget::FM:        fm += amt * vel; break;
                case VelocityTarget::Smear:     smear += amt * vel; break;
                case VelocityTarget::None:      break;
            }
        }
        v.harmonics = std::clamp (harmonics, -1.0f, 1.0f);
        v.fm = std::clamp (fm, 0.0f, 1.0f);

        for (int c = 0; c < kMaxChannels; ++c)
        {
            auto& gs = v.gs[c];
            gs.shiftSemis = shiftNow;
            gs.targetNote = targetNote;
            gs.midiWeight = weight;
            gs.detuneSemis = numOut > 1 ? (c == 0 ? -0.5f : 0.5f) * p.detuneCents * 0.01f : 0.0f;
            gs.formant = semitonesToRatio (std::clamp (formant, -12.0f, 12.0f));
            gs.alternator = std::clamp (p.alternator, 0.0f, 1.0f);
            gs.smear = std::clamp (smear, 0.0f, 1.0f);
            gs.stereo = numOut > 1 ? std::clamp (p.stereo, 0.0f, 1.0f) : 0.0f;
            gs.voicedGain = level;
            gs.unvoicedGain = level / (float) std::max (1, activeMode == MidiMode::Poly ? numActive : 1);
        }
    }

    mixSmooth.setTarget (std::clamp (p.mix, 0.0f, 1.0f));
    gainSmooth.setTarget (dbToGain (std::clamp (p.outputDb, -24.0f, 12.0f)));
}

void Engine::process (const float* const* in, float* const* out, int numSamples, const MidiEvent* events,
                      int numEvents) noexcept
{
    int ev = 0;
    float frame[kMaxChannels] {};

    for (int i = 0; i < numSamples; ++i)
    {
        while (ev < numEvents && events[ev].sampleOffset <= i)
            handleEvent (events[ev++]);

        if (controlCounter == 0)
            updateControl();
        if (++controlCounter == kControlInterval)
            controlCounter = 0;

        for (int c = 0; c < numIn; ++c)
            frame[c] = in[c][i];
        an.push (frame);
        const int64_t n = an.now();

        float wet[kMaxChannels] {};
        for (auto& v : voices)
        {
            if (! v.active)
                continue;
            const float g = v.gain.next();
            for (int c = 0; c < numOut; ++c)
            {
                float y = v.ch[c].process (n, an, an.input (c), v.gs[c], timing, c == 1);
                y = v.post[c].process (y, v.ch[c].outputPeriod(), v.ch[c].outputVoiced(), v.harmonics, v.fm,
                                       params.fmRatio);
                wet[c] += g * y;
            }
            if (! v.held && v.gain.getTarget() == 0.0f && v.gain.getCurrent() < 1.0e-4f)
                v.active = false;
        }

        const float mix = mixSmooth.next();
        const float gain = gainSmooth.next();
        for (int c = 0; c < numOut; ++c)
        {
            const float dry = an.input (c).at (n - latencyTotal);
            out[c][i] = (dry + mix * (wet[c] - dry)) * gain;
        }
    }

    for (; ev < numEvents; ++ev)
        handleEvent (events[ev]);

    const auto& e = an.detector().latest();
    disp.frequency.store (e.voiced ? e.frequency : 0.0f, std::memory_order_relaxed);
    disp.confidence.store (e.confidence, std::memory_order_relaxed);
    disp.voiced.store (e.voiced, std::memory_order_relaxed);
    disp.level.store (e.levelDb, std::memory_order_relaxed);
}

void Engine::processBypassed (const float* const* in, float* const* out, int numSamples) noexcept
{
    float frame[kMaxChannels] {};
    for (int i = 0; i < numSamples; ++i)
    {
        for (int c = 0; c < numIn; ++c)
            frame[c] = in[c][i];
        an.push (frame);
        const int64_t n = an.now();
        for (int c = 0; c < numOut; ++c)
            out[c][i] = an.input (c).at (n - latencyTotal);
    }
}

} // namespace nsw
