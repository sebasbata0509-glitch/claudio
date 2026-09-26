#include "Parameters.h"

#include "dsp/Presets.h"

namespace nsw
{
namespace ids
{
    juce::String macroDepth (ModDest d) { return "macroDepth_" + juce::String (modDestName (d)).toLowerCase(); }
    juce::String seqStep (int index) { return "seqStep" + juce::String (index + 1); }
    juce::String seqGlide (int index) { return "seqGlide" + juce::String (index + 1); }
} // namespace ids

namespace
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;
    using API = juce::AudioParameterInt;

    juce::ParameterID pid (const juce::String& id) { return { id, 1 }; }

    juce::String fmtMs (float v, int) { return v < 1000.0f ? juce::String (v, v < 10.0f ? 1 : 0) + " ms" : juce::String (v / 1000.0f, 2) + " s"; }
    juce::String fmtSignedPercent (float v, int) { return (v > 0.05f ? "+" : "") + juce::String (v, 0) + "%"; }

    juce::String fmtSemis (float v, int)
    {
        return (v > 0.004f ? "+" : "") + juce::String (v, std::abs (v - std::round (v)) < 0.005f ? 0 : 2) + " st";
    }

    juce::String fmtPercent (float v, int) { return juce::String (v, 0) + "%"; }

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre, float step = 0.0f)
    {
        juce::NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    const Params d = factoryPresets()[0].params; // "Porter Up" is the default state
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto attrs = [] (juce::String label = {}) { return juce::AudioParameterFloatAttributes().withLabel (label); };

    layout.add (std::make_unique<APF> (pid (ids::pitch), "Pitch", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
                                       d.pitch, attrs ("st").withStringFromValueFunction (fmtSemis)));
    layout.add (std::make_unique<APB> (pid (ids::snap), "Snap", d.snap));
    layout.add (std::make_unique<API> (pid (ids::octave), "Octave", -2, 2, (int) d.octave));
    layout.add (std::make_unique<APF> (pid (ids::formant), "Formant", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f),
                                       d.formant, attrs ("st").withStringFromValueFunction (fmtSemis)));
    layout.add (std::make_unique<APF> (pid (ids::harmonics), "Harmonics", juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f),
                                       d.harmonics,
                                       attrs().withStringFromValueFunction ([] (float v, int) {
                                           if (std::abs (v) < 0.005f)
                                               return juce::String ("neutral");
                                           return juce::String (std::abs (v) * 100.0f, 0) + (v > 0 ? "% odd" : "% even");
                                       })));
    layout.add (std::make_unique<APF> (pid (ids::alternator), "Alternator", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
                                       d.alternator * 100.0f, attrs ("%").withStringFromValueFunction (fmtPercent)));
    layout.add (std::make_unique<APF> (pid (ids::fm), "FM", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
                                       d.fm * 100.0f, attrs().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0); })));
    layout.add (std::make_unique<APF> (pid (ids::ratio), "Ratio", skewed (0.25f, 8.0f, 1.0f, 0.001f), d.fmRatio,
                                       attrs().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + "x"; })));
    layout.add (std::make_unique<APF> (pid (ids::glide), "Glide", skewed (0.0f, 500.0f, 60.0f, 0.1f), d.glideMs,
                                       attrs ("ms").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0) + " ms"; })));
    layout.add (std::make_unique<APF> (pid (ids::smear), "Smear", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
                                       d.smear * 100.0f, attrs ("%").withStringFromValueFunction (fmtPercent)));
    layout.add (std::make_unique<APF> (pid (ids::stereo), "Stereo", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
                                       d.stereo * 100.0f, attrs ("%").withStringFromValueFunction (fmtPercent)));
    layout.add (std::make_unique<APF> (pid (ids::detune), "Detune", juce::NormalisableRange<float> (0.0f, 50.0f, 0.1f),
                                       d.detuneCents,
                                       attrs ("ct").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " ct"; })));
    layout.add (std::make_unique<APF> (pid (ids::mix), "Dry/Wet", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
                                       d.mix * 100.0f, attrs ("%").withStringFromValueFunction (fmtPercent)));
    layout.add (std::make_unique<APF> (pid (ids::output), "Output", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f),
                                       d.outputDb,
                                       attrs ("dB").withStringFromValueFunction ([] (float v, int) {
                                           return (v > 0.005f ? "+" : "") + juce::String (v, 1) + " dB";
                                       })));
    layout.add (std::make_unique<APC> (pid (ids::mode), "MIDI Mode", juce::StringArray { "Off", "Poly", "Mono" },
                                       (int) d.midiMode));
    layout.add (std::make_unique<APC> (pid (ids::range), "Range", juce::StringArray { "Low", "High" },
                                       d.range == DetectionRange::Low ? 0 : 1));
    layout.add (std::make_unique<APC> (pid (ids::velTarget), "Velocity Target",
                                       juce::StringArray { "None", "Level", "Formant", "Harmonics", "FM", "Smear" },
                                       (int) d.velocityTarget));
    layout.add (std::make_unique<APF> (pid (ids::velAmount), "Velocity Amount",
                                       juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), d.velocityAmount * 100.0f,
                                       attrs ("%").withStringFromValueFunction (fmtPercent)));

    // ---- Modulation
    const auto& m = d.mod;
    layout.add (std::make_unique<APF> (pid (ids::macro), "Macro", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
                                       m.macro * 100.0f, attrs ("%").withStringFromValueFunction (fmtPercent)));
    for (int i = 1; i <= kNumModDests; ++i)
    {
        const auto dest = (ModDest) i;
        layout.add (std::make_unique<APF> (pid (ids::macroDepth (dest)), juce::String ("Macro > ") + modDestName (dest),
                                           juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f),
                                           m.macroDepth[(size_t) i - 1] * 100.0f,
                                           attrs ("%").withStringFromValueFunction (fmtSignedPercent)));
    }

    layout.add (std::make_unique<APB> (pid (ids::seqOn), "Seq On", m.seqOn));
    layout.add (std::make_unique<APC> (pid (ids::seqRate), "Seq Rate",
                                       juce::StringArray { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }, (int) m.seqRate));
    layout.add (std::make_unique<APC> (pid (ids::seqDest), "Seq Target", juce::StringArray { "Pitch", "Formant" },
                                       m.seqToFormant ? 1 : 0));
    layout.add (std::make_unique<APF> (pid (ids::seqDepth), "Seq Depth", juce::NormalisableRange<float> (0.0f, 24.0f, 0.01f),
                                       m.seqDepth, attrs ("st").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " st"; })));
    layout.add (std::make_unique<API> (pid (ids::seqLength), "Seq Length", 1, 16, m.seqLength));
    for (int i = 0; i < 16; ++i)
    {
        layout.add (std::make_unique<APF> (pid (ids::seqStep (i)), "Seq Step " + juce::String (i + 1),
                                           juce::NormalisableRange<float> (-100.0f, 100.0f, 0.01f), m.seqValue[(size_t) i] * 100.0f,
                                           attrs ("%").withStringFromValueFunction (fmtSignedPercent)));
        layout.add (std::make_unique<APF> (pid (ids::seqGlide (i)), "Seq Glide " + juce::String (i + 1),
                                           juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), m.seqGlide[(size_t) i] * 100.0f,
                                           attrs ("%").withStringFromValueFunction (fmtPercent)));
    }

    juce::StringArray dests;
    for (int i = 0; i <= kNumModDests; ++i)
        dests.add (modDestName ((ModDest) i));
    layout.add (std::make_unique<APC> (pid (ids::envDest), "Env Target", dests, (int) m.envDest));
    layout.add (std::make_unique<APF> (pid (ids::envDepth), "Env Depth", juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f),
                                       m.envDepth * 100.0f, attrs ("%").withStringFromValueFunction (fmtSignedPercent)));
    layout.add (std::make_unique<APF> (pid (ids::envAttack), "Env Attack", skewed (0.5f, 2000.0f, 60.0f, 0.1f), m.attackMs,
                                       attrs ("ms").withStringFromValueFunction (fmtMs)));
    layout.add (std::make_unique<APF> (pid (ids::envDecay), "Env Decay", skewed (1.0f, 4000.0f, 250.0f, 0.1f), m.decayMs,
                                       attrs ("ms").withStringFromValueFunction (fmtMs)));
    layout.add (std::make_unique<APF> (pid (ids::envSustain), "Env Sustain", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
                                       m.sustain * 100.0f, attrs ("%").withStringFromValueFunction (fmtPercent)));
    layout.add (std::make_unique<APF> (pid (ids::envRelease), "Env Release", skewed (1.0f, 5000.0f, 300.0f, 0.1f), m.releaseMs,
                                       attrs ("ms").withStringFromValueFunction (fmtMs)));
    layout.add (std::make_unique<APC> (pid (ids::envTrigger), "Env Trigger", juce::StringArray { "MIDI", "Input" },
                                       (int) m.envTrigger));
    layout.add (std::make_unique<APF> (pid (ids::envThreshold), "Env Threshold", juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f),
                                       m.envThresholdDb,
                                       attrs ("dB").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })));
    return layout;
}

ParameterReader::ParameterReader (juce::AudioProcessorValueTreeState& s)
{
    auto get = [&s] (const juce::String& id) {
        auto* p = s.getRawParameterValue (id);
        jassert (p != nullptr);
        return Raw { p };
    };
    pitch = get (ids::pitch);
    snap = get (ids::snap);
    octave = get (ids::octave);
    formant = get (ids::formant);
    harmonics = get (ids::harmonics);
    alternator = get (ids::alternator);
    fm = get (ids::fm);
    ratio = get (ids::ratio);
    glide = get (ids::glide);
    smear = get (ids::smear);
    stereo = get (ids::stereo);
    detune = get (ids::detune);
    mix = get (ids::mix);
    output = get (ids::output);
    mode = get (ids::mode);
    range = get (ids::range);
    velTarget = get (ids::velTarget);
    velAmount = get (ids::velAmount);

    macro = get (ids::macro);
    for (int i = 1; i <= kNumModDests; ++i)
        macroDepth[(size_t) i - 1] = get (ids::macroDepth ((ModDest) i));
    seqOn = get (ids::seqOn);
    seqRate = get (ids::seqRate);
    seqDest = get (ids::seqDest);
    seqDepth = get (ids::seqDepth);
    seqLength = get (ids::seqLength);
    for (int i = 0; i < 16; ++i)
    {
        seqStep[(size_t) i] = get (ids::seqStep (i));
        seqGlide[(size_t) i] = get (ids::seqGlide (i));
    }
    envDest = get (ids::envDest);
    envDepth = get (ids::envDepth);
    envAttack = get (ids::envAttack);
    envDecay = get (ids::envDecay);
    envSustain = get (ids::envSustain);
    envRelease = get (ids::envRelease);
    envTrigger = get (ids::envTrigger);
    envThreshold = get (ids::envThreshold);
}

Params ParameterReader::read() const noexcept
{
    Params p;
    p.pitch = pitch.get();
    p.snap = snap.get() > 0.5f;
    p.octave = std::round (octave.get());
    p.formant = formant.get();
    p.harmonics = harmonics.get();
    p.alternator = alternator.get() * 0.01f;
    p.fm = fm.get() * 0.01f;
    p.fmRatio = ratio.get();
    p.glideMs = glide.get();
    p.smear = smear.get() * 0.01f;
    p.stereo = stereo.get() * 0.01f;
    p.detuneCents = detune.get();
    p.mix = mix.get() * 0.01f;
    p.outputDb = output.get();
    p.midiMode = (MidiMode) juce::jlimit (0, 2, (int) std::round (mode.get()));
    p.range = std::round (range.get()) < 0.5f ? DetectionRange::Low : DetectionRange::High;
    p.velocityTarget = (VelocityTarget) juce::jlimit (0, 5, (int) std::round (velTarget.get()));
    p.velocityAmount = velAmount.get() * 0.01f;

    auto& m = p.mod;
    m.macro = macro.get() * 0.01f;
    for (size_t i = 0; i < macroDepth.size(); ++i)
        m.macroDepth[i] = macroDepth[i].get() * 0.01f;
    m.seqOn = seqOn.get() > 0.5f;
    m.seqRate = (SeqRate) juce::jlimit (0, 5, (int) std::round (seqRate.get()));
    m.seqToFormant = seqDest.get() > 0.5f;
    m.seqDepth = seqDepth.get();
    m.seqLength = juce::jlimit (1, 16, (int) std::round (seqLength.get()));
    for (size_t i = 0; i < 16; ++i)
    {
        m.seqValue[i] = seqStep[i].get() * 0.01f;
        m.seqGlide[i] = seqGlide[i].get() * 0.01f;
    }
    m.envDest = (ModDest) juce::jlimit (0, kNumModDests, (int) std::round (envDest.get()));
    m.envDepth = envDepth.get() * 0.01f;
    m.attackMs = envAttack.get();
    m.decayMs = envDecay.get();
    m.sustain = envSustain.get() * 0.01f;
    m.releaseMs = envRelease.get();
    m.envTrigger = envTrigger.get() > 0.5f ? EnvTrigger::InputLevel : EnvTrigger::Midi;
    m.envThresholdDb = envThreshold.get();
    return p;
}

void applyParams (juce::AudioProcessorValueTreeState& s, const Params& p)
{
    auto set = [&s] (const juce::String& id, float value) {
        if (auto* param = s.getParameter (id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (value));
            param->endChangeGesture();
        }
    };
    set (ids::pitch, p.pitch);
    set (ids::snap, p.snap ? 1.0f : 0.0f);
    set (ids::octave, p.octave);
    set (ids::formant, p.formant);
    set (ids::harmonics, p.harmonics);
    set (ids::alternator, p.alternator * 100.0f);
    set (ids::fm, p.fm * 100.0f);
    set (ids::ratio, p.fmRatio);
    set (ids::glide, p.glideMs);
    set (ids::smear, p.smear * 100.0f);
    set (ids::stereo, p.stereo * 100.0f);
    set (ids::detune, p.detuneCents);
    set (ids::mix, p.mix * 100.0f);
    set (ids::output, p.outputDb);
    set (ids::mode, (float) (int) p.midiMode);
    set (ids::range, p.range == DetectionRange::Low ? 0.0f : 1.0f);
    set (ids::velTarget, (float) (int) p.velocityTarget);
    set (ids::velAmount, p.velocityAmount * 100.0f);

    const auto& m = p.mod;
    set (ids::macro, m.macro * 100.0f);
    for (int i = 1; i <= kNumModDests; ++i)
        set (ids::macroDepth ((ModDest) i), m.macroDepth[(size_t) i - 1] * 100.0f);
    set (ids::seqOn, m.seqOn ? 1.0f : 0.0f);
    set (ids::seqRate, (float) (int) m.seqRate);
    set (ids::seqDest, m.seqToFormant ? 1.0f : 0.0f);
    set (ids::seqDepth, m.seqDepth);
    set (ids::seqLength, (float) m.seqLength);
    for (int i = 0; i < 16; ++i)
    {
        set (ids::seqStep (i), m.seqValue[(size_t) i] * 100.0f);
        set (ids::seqGlide (i), m.seqGlide[(size_t) i] * 100.0f);
    }
    set (ids::envDest, (float) (int) m.envDest);
    set (ids::envDepth, m.envDepth * 100.0f);
    set (ids::envAttack, m.attackMs);
    set (ids::envDecay, m.decayMs);
    set (ids::envSustain, m.sustain * 100.0f);
    set (ids::envRelease, m.releaseMs);
    set (ids::envTrigger, (float) (int) m.envTrigger);
    set (ids::envThreshold, m.envThresholdDb);
}
} // namespace nsw
