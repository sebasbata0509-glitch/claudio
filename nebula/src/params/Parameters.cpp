#include "Parameters.h"

namespace nebula
{
//==============================================================================
const juce::StringArray& filterTypeNames()   { static const juce::StringArray a { "LP 12", "LP 24", "BP 12", "BP 24", "HP 12", "HP 24" }; return a; }
const juce::StringArray& wavetableNames()    { static const juce::StringArray a { "Saw", "Square", "Vocal", "Glass", "Metallic" }; return a; }
const juce::StringArray& granularModeNames() { static const juce::StringArray a { "Granular", "Texture", "Stretch" }; return a; }
const juce::StringArray& noiseTypeNames()    { static const juce::StringArray a { "White", "Pink", "Vinyl", "Wind" }; return a; }
const juce::StringArray& lfoShapeNames()     { static const juce::StringArray a { "Sine", "Triangle", "Saw", "Square", "Random", "Smooth Random" }; return a; }
const juce::StringArray& spectralModeNames() { static const juce::StringArray a { "Spectral Gate", "Freeze", "Smear", "MP3ify" }; return a; }
const juce::StringArray& chorusModeNames()   { static const juce::StringArray a { "Chorus", "Ensemble" }; return a; }
const juce::StringArray& reverbModeNames()   { static const juce::StringArray a { "Normal", "Big Stereo", "Reverse" }; return a; }
const juce::StringArray& fxSlotNames()       { static const juce::StringArray a { "Spectral", "Resonator", "Grain Delay", "Vocoder", "Chorus", "Shimmer" }; return a; }
const juce::StringArray& layerNames()        { static const juce::StringArray a { "Wavetable", "Additive", "Granular", "Sub/Noise" }; return a; }
const juce::StringArray& macroNames()        { static const juce::StringArray a { "SPACE", "TEXTURE", "MOVEMENT", "WIDTH" }; return a; }

namespace
{
    struct Division { const char* name; double beats; };

    constexpr Division divisions[] = {
        { "4 bars", 16.0 }, { "2 bars", 8.0 }, { "1 bar", 4.0 },
        { "1/2", 2.0 },  { "1/2 T", 4.0 / 3.0 },
        { "1/4 D", 1.5 }, { "1/4", 1.0 },  { "1/4 T", 2.0 / 3.0 },
        { "1/8 D", 0.75 }, { "1/8", 0.5 }, { "1/8 T", 1.0 / 3.0 },
        { "1/16 D", 0.375 }, { "1/16", 0.25 }, { "1/16 T", 1.0 / 6.0 },
        { "1/32", 0.125 }
    };
} // namespace

const juce::StringArray& syncDivisionNames()
{
    static const juce::StringArray a = [] {
        juce::StringArray s;
        for (auto& d : divisions)
            s.add (d.name);
        return s;
    }();
    return a;
}

double syncDivisionBeats (int index) noexcept
{
    return divisions[juce::jlimit (0, (int) std::size (divisions) - 1, index)].beats;
}

//==============================================================================
const juce::StringArray& modSourceNames()
{
    static const juce::StringArray a {
        "None",
        "LFO 1", "LFO 2", "LFO 3", "LFO 4",
        "Env 1", "Env 2", "Env 3",
        "Velocity", "Mod Wheel", "Aftertouch", "Note", "Random",
        "SPACE", "TEXTURE", "MOVEMENT", "WIDTH"
    };
    return a;
}

const std::vector<ModDestination>& modDestinations()
{
    static const std::vector<ModDestination> d {
        { "None",               "" },
        // Layer 1
        { "WT Volume",          "l1_vol" },
        { "WT Pan",             "l1_pan" },
        { "WT Osc A Position",  "l1_a_pos" },
        { "WT Osc B Position",  "l1_b_pos" },
        { "WT Osc A Level",     "l1_a_level" },
        { "WT Osc B Level",     "l1_b_level" },
        { "WT Fine",            "l1_fine" },
        { "WT Unison Detune",   "l1_uni_detune" },
        { "WT Unison Blend",    "l1_uni_blend" },
        { "WT Unison Spread",   "l1_uni_spread" },
        { "WT Cutoff",          "l1_cutoff" },
        { "WT Resonance",       "l1_reso" },
        { "WT Drive",           "l1_drive" },
        // Layer 2
        { "ADD Volume",         "l2_vol" },
        { "ADD Pan",            "l2_pan" },
        { "ADD Brightness",     "l2_bright" },
        { "ADD Inharmonicity",  "l2_inharm" },
        { "ADD Shimmer",        "l2_shimmer" },
        { "ADD Twinkle",        "l2_twinkle" },
        { "ADD Cutoff",         "l2_cutoff" },
        { "ADD Resonance",      "l2_reso" },
        // Layer 3
        { "GRN Volume",         "l3_vol" },
        { "GRN Pan",            "l3_pan" },
        { "GRN Position",       "l3_pos" },
        { "GRN Spray",          "l3_spray" },
        { "GRN Grain Size",     "l3_size" },
        { "GRN Density",        "l3_density" },
        { "GRN Pitch Random",   "l3_pitch_rand" },
        { "GRN Cutoff",         "l3_cutoff" },
        { "GRN Resonance",      "l3_reso" },
        // Layer 4
        { "SUB Level",          "l4_sub_level" },
        { "NOISE Level",        "l4_noise_level" },
        { "NOISE Cutoff",       "l4_cutoff" },
        // FX
        { "Spectral Amount",    "sp_amount" },
        { "Spectral Mix",       "sp_mix" },
        { "Resonator Tune",     "rs_tune" },
        { "Resonator Decay",    "rs_decay" },
        { "Resonator Bright",   "rs_bright" },
        { "Resonator Mix",      "rs_mix" },
        { "Grain Dly Spray",    "gd_spray" },
        { "Grain Dly Pitch",    "gd_pitch" },
        { "Grain Dly Feedback", "gd_feedback" },
        { "Grain Dly Mix",      "gd_mix" },
        { "Vocoder Formant",    "vc_formant" },
        { "Vocoder Mix",        "vc_mix" },
        { "Chorus Depth",       "ch_depth" },
        { "Chorus Mix",         "ch_mix" },
        { "Reverb Size",        "rv_size" },
        { "Reverb Decay",       "rv_decay" },
        { "Reverb Shimmer",     "rv_shimmer" },
        { "Reverb Mix",         "rv_mix" },
        // Global / modulators
        { "Master Width",       "master_width" },
        { "LFO 1 Rate",         "lfo1_rate" },
        { "LFO 2 Rate",         "lfo2_rate" },
        { "LFO 3 Rate",         "lfo3_rate" },
        { "LFO 4 Rate",         "lfo4_rate" },
    };
    return d;
}

const juce::StringArray& modDestinationNames()
{
    static const juce::StringArray a = [] {
        juce::StringArray s;
        for (auto& d : modDestinations())
            s.add (d.name);
        return s;
    }();
    return a;
}

//==============================================================================
namespace ids
{
    juce::String layer (int i, const char* name)   { return "l" + juce::String (i + 1) + "_" + name; }
    juce::String lfo (int i, const char* name)     { return "lfo" + juce::String (i + 1) + "_" + name; }
    juce::String modEnv (int i, const char* name)  { return "env" + juce::String (i + 1) + "_" + name; }
    juce::String modSlot (int i, const char* name) { return "mm" + juce::String (i + 1) + "_" + name; }
    juce::String macro (int i)                     { return "macro" + juce::String (i + 1); }
} // namespace ids

//==============================================================================
namespace
{
    using Group = juce::AudioProcessorParameterGroup;
    using Range = juce::NormalisableRange<float>;

    Range skewed (float lo, float hi, float centre)
    {
        Range r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    Range linear (float lo, float hi, float step = 0.0f) { return { lo, hi, step }; }

    // --- value formatters -------------------------------------------------------
    juce::String fmtHz (float v, int)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                            : juce::String (v, v < 10.0f ? 2 : (v < 100.0f ? 1 : 0)) + " Hz";
    }

    juce::String fmtMs (float v, int)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, v < 10.0f ? 1 : 0) + " ms";
    }

    juce::String fmtSec (float v, int) { return juce::String (v, v < 10.0f ? 2 : 1) + " s"; }
    juce::String fmtPct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

    juce::String fmtBipolarPct (float v, int)
    {
        const int p = juce::roundToInt (v * 100.0f);
        return (p > 0 ? "+" : "") + juce::String (p) + "%";
    }

    juce::String fmtDb (float v, int) { return v <= -59.9f ? juce::String ("-inf dB") : juce::String (v, 1) + " dB"; }

    juce::String fmtSemis (float v, int)
    {
        return (v > 0.005f ? "+" : "") + juce::String (v, std::abs (v - std::round (v)) < 0.005f ? 0 : 2) + " st";
    }

    juce::String fmtCents (float v, int) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " ct"; }

    juce::String fmtPan (float v, int)
    {
        const int p = juce::roundToInt (std::abs (v) * 100.0f);
        return p == 0 ? juce::String ("C") : juce::String (p) + (v < 0 ? "L" : "R");
    }

    using Fmt = juce::String (*) (float, int);

    // --- builders -----------------------------------------------------------
    struct Builder
    {
        std::unique_ptr<Group> group;

        Builder (const juce::String& id, const juce::String& name)
            : group (std::make_unique<Group> (id, name, "|")) {}

        void flt (const juce::String& id, const juce::String& name, Range range, float def, Fmt fmt, const juce::String& label = {})
        {
            group->addChild (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { id, 1 }, name, range, def,
                juce::AudioParameterFloatAttributes().withLabel (label).withStringFromValueFunction (fmt)));
        }

        void pct (const juce::String& id, const juce::String& name, float def) { flt (id, name, linear (0.0f, 1.0f), def, fmtPct); }
        void bipolar (const juce::String& id, const juce::String& name, float def) { flt (id, name, linear (-1.0f, 1.0f), def, fmtBipolarPct); }
        void hz (const juce::String& id, const juce::String& name, float lo, float hi, float centre, float def) { flt (id, name, skewed (lo, hi, centre), def, fmtHz, "Hz"); }
        void ms (const juce::String& id, const juce::String& name, float lo, float hi, float centre, float def) { flt (id, name, skewed (lo, hi, centre), def, fmtMs, "ms"); }
        void db (const juce::String& id, const juce::String& name, float lo, float hi, float def) { flt (id, name, linear (lo, hi), def, fmtDb, "dB"); }
        void semis (const juce::String& id, const juce::String& name, float range, float def, float step = 0.0f) { flt (id, name, linear (-range, range, step), def, fmtSemis, "st"); }

        void integer (const juce::String& id, const juce::String& name, int lo, int hi, int def)
        {
            group->addChild (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, lo, hi, def));
        }

        void choice (const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
        {
            group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def));
        }

        void toggle (const juce::String& id, const juce::String& name, bool def)
        {
            group->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
        }
    };

    //==========================================================================
    void addLayerCommon (Builder& b, int l, float defaultVolumeDb, float attackMs, float releaseMs, float cutoff)
    {
        const auto P = [l] (const char* n) { return ids::layer (l, n); };
        const juce::String prefix = layerNames()[l] + " ";

        b.db (P ("vol"), prefix + "Volume", -60.0f, 6.0f, defaultVolumeDb);
        b.flt (P ("pan"), prefix + "Pan", linear (-1.0f, 1.0f), 0.0f, fmtPan);
        b.toggle (P ("mute"), prefix + "Mute", false);
        b.toggle (P ("solo"), prefix + "Solo", false);
        b.integer (P ("octave"), prefix + "Octave", -3, 3, 0);

        b.ms (P ("attack"), prefix + "Attack", 0.5f, 20000.0f, 500.0f, attackMs);
        b.ms (P ("decay"), prefix + "Decay", 1.0f, 20000.0f, 800.0f, 1000.0f);
        b.pct (P ("sustain"), prefix + "Sustain", 0.8f);
        b.ms (P ("release"), prefix + "Release", 1.0f, 30000.0f, 1000.0f, releaseMs);

        b.toggle (P ("filter_on"), prefix + "Filter On", true);
        b.choice (P ("ftype"), prefix + "Filter Type", filterTypeNames(), (int) FilterType::LP12);
        b.hz (P ("cutoff"), prefix + "Cutoff", 20.0f, 20000.0f, 1000.0f, cutoff);
        b.pct (P ("reso"), prefix + "Resonance", 0.15f);
        b.pct (P ("drive"), prefix + "Drive", 0.0f);
        b.bipolar (P ("fenv"), prefix + "Filter Env", 0.0f);
        b.pct (P ("fkey"), prefix + "Filter Keytrack", 0.0f);
    }

    void addWavetableOsc (Builder& b, const char* osc, const juce::String& oscName, bool on, int table, float level)
    {
        const auto P = [osc] (const char* n) { return ids::layer (0, (juce::String (osc) + "_" + n).toRawUTF8()); };
        b.toggle (P ("on"), "WT " + oscName + " On", on);
        b.choice (P ("table"), "WT " + oscName + " Table", wavetableNames(), table);
        b.pct (P ("pos"), "WT " + oscName + " Position", 0.0f);
        b.pct (P ("level"), "WT " + oscName + " Level", level);
        b.semis (P ("semi"), "WT " + oscName + " Semi", 24.0f, 0.0f, 1.0f);
        b.flt (P ("fine"), "WT " + oscName + " Fine", linear (-100.0f, 100.0f), 0.0f, fmtCents, "ct");
    }
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ---- Global ------------------------------------------------------------
    {
        Builder b ("global", "Global");
        b.db (ids::masterGain, "Master Volume", -60.0f, 6.0f, -3.0f);
        b.flt (ids::masterWidth, "Master Width", linear (0.0f, 2.0f), 1.0f, fmtPct);
        b.integer (ids::bendRange, "Pitch Bend Range", 0, 24, 2);
        b.ms (ids::glide, "Glide", 0.0f, 2000.0f, 200.0f, 0.0f);
        layout.add (std::move (b.group));
    }

    // ---- Layer 1: Wavetable -----------------------------------------------
    {
        Builder b ("layer1", "Layer 1 - Wavetable");
        addLayerCommon (b, 0, -6.0f, 400.0f, 2500.0f, 4000.0f);
        addWavetableOsc (b, "a", "Osc A", true, (int) Wavetable::Saw, 0.8f);
        addWavetableOsc (b, "b", "Osc B", false, (int) Wavetable::Glass, 0.6f);
        b.flt (ids::layer (0, "fine"), "WT Fine", linear (-100.0f, 100.0f), 0.0f, fmtCents, "ct");
        b.integer (ids::layer (0, "uni_voices"), "WT Unison Voices", 1, kMaxUnison, 5);
        b.pct (ids::layer (0, "uni_detune"), "WT Unison Detune", 0.25f);
        b.pct (ids::layer (0, "uni_blend"), "WT Unison Blend", 0.7f);
        b.pct (ids::layer (0, "uni_spread"), "WT Unison Spread", 0.8f);
        b.toggle (ids::layer (0, "rand_phase"), "WT Random Phase", true);
        b.toggle (ids::layer (0, "reese"), "WT Reese", false);
        b.pct (ids::layer (0, "reese_detune"), "WT Reese Detune", 0.35f);
        b.hz (ids::layer (0, "reese_rate"), "WT Reese LFO Rate", 0.01f, 2.0f, 0.2f, 0.12f);
        layout.add (std::move (b.group));
    }

    // ---- Layer 2: Additive -------------------------------------------------
    {
        Builder b ("layer2", "Layer 2 - Additive");
        addLayerCommon (b, 1, -60.0f, 20.0f, 3000.0f, 20000.0f);
        b.integer (ids::layer (1, "partials"), "ADD Partials", 1, kMaxPartials, 64);
        b.pct (ids::layer (1, "bright"), "ADD Brightness", 0.5f);
        b.bipolar (ids::layer (1, "oddeven"), "ADD Odd/Even", 0.0f);
        b.pct (ids::layer (1, "inharm"), "ADD Inharmonicity", 0.0f);
        b.pct (ids::layer (1, "shimmer"), "ADD Shimmer", 0.3f);
        b.hz (ids::layer (1, "shimmer_rate"), "ADD Shimmer Rate", 0.1f, 20.0f, 3.0f, 4.0f);
        b.pct (ids::layer (1, "pluck"), "ADD Pluck", 0.0f);
        b.pct (ids::layer (1, "twinkle"), "ADD Twinkle", 0.0f);
        b.hz (ids::layer (1, "twinkle_rate"), "ADD Twinkle Rate", 0.5f, 20.0f, 4.0f, 6.0f);
        layout.add (std::move (b.group));
    }

    // ---- Layer 3: Granular sampler -----------------------------------------
    {
        Builder b ("layer3", "Layer 3 - Granular");
        addLayerCommon (b, 2, -60.0f, 300.0f, 2500.0f, 20000.0f);
        b.choice (ids::layer (2, "mode"), "GRN Mode", granularModeNames(), (int) GranularMode::Granular);
        b.pct (ids::layer (2, "pos"), "GRN Position", 0.3f);
        b.flt (ids::layer (2, "scan"), "GRN Scan Speed", linear (-2.0f, 2.0f), 0.0f, fmtBipolarPct);
        b.pct (ids::layer (2, "spray"), "GRN Spray", 0.1f);
        b.ms (ids::layer (2, "size"), "GRN Grain Size", 5.0f, 500.0f, 80.0f, 90.0f);
        b.flt (ids::layer (2, "density"), "GRN Density", skewed (1.0f, 100.0f, 15.0f), 20.0f,
               [] (float v, int) { return juce::String (v, 1) + " /s"; }, "/s");
        b.flt (ids::layer (2, "pitch_rand"), "GRN Pitch Random", linear (0.0f, 12.0f), 0.0f, fmtSemis, "st");
        b.pct (ids::layer (2, "reverse"), "GRN Reverse Prob", 0.0f);
        b.flt (ids::layer (2, "stretch"), "GRN Stretch Factor", skewed (1.0f, 50.0f, 8.0f), 8.0f,
               [] (float v, int) { return juce::String (v, 1) + "x"; }, "x");
        b.toggle (ids::layer (2, "keytrack"), "GRN Keytrack", true);
        b.integer (ids::layer (2, "root"), "GRN Root Note", 0, 127, 60);
        layout.add (std::move (b.group));
    }

    // ---- Layer 4: Sub / Noise ----------------------------------------------
    {
        Builder b ("layer4", "Layer 4 - Sub/Noise");
        addLayerCommon (b, 3, -60.0f, 5.0f, 800.0f, 8000.0f);
        b.pct (ids::layer (3, "sub_level"), "SUB Level", 0.8f);
        b.pct (ids::layer (3, "noise_level"), "NOISE Level", 0.0f);
        b.choice (ids::layer (3, "noise_type"), "NOISE Type", noiseTypeNames(), (int) NoiseType::Pink);
        layout.add (std::move (b.group));
    }

    // ---- FX: Spectral ------------------------------------------------------
    {
        Builder b ("fx_spectral", "FX - Spectral");
        b.toggle ("sp_on", "Spectral On", false);
        b.choice ("sp_mode", "Spectral Mode", spectralModeNames(), (int) SpectralMode::Smear);
        b.pct ("sp_amount", "Spectral Amount", 0.5f);
        b.toggle ("sp_freeze", "Spectral Freeze Hold", false);
        b.pct ("sp_mix", "Spectral Mix", 0.5f);
        layout.add (std::move (b.group));
    }

    // ---- FX: Resonator -----------------------------------------------------
    {
        Builder b ("fx_resonator", "FX - Resonator");
        b.toggle ("rs_on", "Resonator On", false);
        b.semis ("rs_tune", "Resonator Tune", 24.0f, 0.0f);
        b.flt ("rs_decay", "Resonator Decay", skewed (0.05f, 20.0f, 2.0f), 1.5f, fmtSec, "s");
        b.pct ("rs_material", "Resonator Material", 0.5f);
        b.pct ("rs_bright", "Resonator Brightness", 0.6f);
        b.pct ("rs_inharm", "Resonator Inharmonicity", 0.2f);
        b.toggle ("rs_midi", "Resonator MIDI Tune", false);
        b.pct ("rs_lfo_amt", "Resonator LFO Amount", 0.15f);
        b.hz ("rs_lfo_rate", "Resonator LFO Rate", 0.01f, 5.0f, 0.3f, 0.15f);
        b.pct ("rs_mix", "Resonator Mix", 0.35f);
        layout.add (std::move (b.group));
    }

    // ---- FX: Grain delay ---------------------------------------------------
    {
        Builder b ("fx_graindelay", "FX - Grain Delay");
        b.toggle ("gd_on", "Grain Delay On", false);
        b.ms ("gd_time", "Grain Delay Time", 1.0f, 2000.0f, 250.0f, 300.0f);
        b.toggle ("gd_sync", "Grain Delay Sync", false);
        b.choice ("gd_div", "Grain Delay Division", syncDivisionNames(), 8);
        b.ms ("gd_spray", "Grain Delay Spray", 0.0f, 500.0f, 50.0f, 20.0f);
        b.hz ("gd_freq", "Grain Delay Frequency", 1.0f, 150.0f, 20.0f, 25.0f);
        b.semis ("gd_pitch", "Grain Delay Pitch", 24.0f, 12.0f);
        b.pct ("gd_randpitch", "Grain Delay Rand Pitch", 0.0f);
        b.flt ("gd_feedback", "Grain Delay Feedback", linear (0.0f, 0.95f), 0.4f, fmtPct);
        b.pct ("gd_mix", "Grain Delay Mix", 0.3f);
        layout.add (std::move (b.group));
    }

    // ---- FX: Vocoder -------------------------------------------------------
    {
        Builder b ("fx_vocoder", "FX - Vocoder");
        b.toggle ("vc_on", "Vocoder On", false);
        b.db ("vc_gate", "Vocoder Gate", -80.0f, 0.0f, -60.0f);
        b.pct ("vc_bandwidth", "Vocoder Bandwidth", 0.5f);
        b.ms ("vc_attack", "Vocoder Attack", 0.5f, 200.0f, 10.0f, 5.0f);
        b.ms ("vc_release", "Vocoder Release", 5.0f, 2000.0f, 100.0f, 80.0f);
        b.semis ("vc_formant", "Vocoder Formant", 12.0f, 0.0f);
        b.pct ("vc_mix", "Vocoder Mix", 1.0f);
        layout.add (std::move (b.group));
    }

    // ---- FX: Chorus --------------------------------------------------------
    {
        Builder b ("fx_chorus", "FX - Chorus");
        b.toggle ("ch_on", "Chorus On", true);
        b.choice ("ch_mode", "Chorus Mode", chorusModeNames(), (int) ChorusMode::Ensemble);
        b.hz ("ch_rate", "Chorus Rate", 0.02f, 8.0f, 0.8f, 0.35f);
        b.pct ("ch_depth", "Chorus Depth", 0.4f);
        b.pct ("ch_mix", "Chorus Mix", 0.3f);
        layout.add (std::move (b.group));
    }

    // ---- FX: Shimmer reverb ------------------------------------------------
    {
        Builder b ("fx_shimmer", "FX - Shimmer Reverb");
        b.toggle ("rv_on", "Reverb On", true);
        b.choice ("rv_mode", "Reverb Mode", reverbModeNames(), (int) ReverbMode::Normal);
        b.flt ("rv_decay", "Reverb Decay", skewed (0.2f, 60.0f, 6.0f), 8.0f, fmtSec, "s");
        b.pct ("rv_size", "Reverb Size", 0.7f);
        b.pct ("rv_diffusion", "Reverb Diffusion", 0.75f);
        b.ms ("rv_predelay", "Reverb Pre-Delay", 0.0f, 500.0f, 80.0f, 20.0f);
        b.pct ("rv_shimmer", "Reverb Shimmer", 0.3f);
        b.semis ("rv_pitch", "Reverb Shimmer Pitch", 24.0f, 12.0f, 1.0f);
        b.hz ("rv_lowcut", "Reverb Low Cut", 20.0f, 2000.0f, 200.0f, 120.0f);
        b.hz ("rv_highcut", "Reverb High Cut", 1000.0f, 20000.0f, 6000.0f, 9000.0f);
        b.hz ("rv_modrate", "Reverb Mod Rate", 0.05f, 5.0f, 0.8f, 0.5f);
        b.pct ("rv_moddepth", "Reverb Mod Depth", 0.35f);
        b.pct ("rv_mix", "Reverb Mix", 0.35f);
        layout.add (std::move (b.group));
    }

    // ---- FX: Output dynamics (always last in the chain) -------------------
    {
        Builder b ("fx_dynamics", "FX - Compressor/Limiter");
        b.toggle ("cp_on", "Compressor On", true);
        b.db ("cp_thresh", "Compressor Threshold", -40.0f, 0.0f, -12.0f);
        b.flt ("cp_ratio", "Compressor Ratio", skewed (1.0f, 20.0f, 4.0f), 2.0f,
               [] (float v, int) { return juce::String (v, 1) + ":1"; });
        b.ms ("cp_attack", "Compressor Attack", 0.1f, 200.0f, 20.0f, 15.0f);
        b.ms ("cp_release", "Compressor Release", 10.0f, 2000.0f, 200.0f, 250.0f);
        b.db ("cp_makeup", "Compressor Makeup", 0.0f, 24.0f, 0.0f);
        b.db ("lim_ceiling", "Limiter Ceiling", -12.0f, 0.0f, -0.3f);
        layout.add (std::move (b.group));
    }

    // ---- LFOs --------------------------------------------------------------
    {
        Builder b ("lfos", "LFOs");
        for (int i = 0; i < kNumLfos; ++i)
        {
            const juce::String n = "LFO " + juce::String (i + 1) + " ";
            b.choice (ids::lfo (i, "shape"), n + "Shape", lfoShapeNames(), i == 3 ? (int) LfoShape::SmoothRandom : (int) LfoShape::Sine);
            b.hz (ids::lfo (i, "rate"), n + "Rate", 0.01f, 20.0f, 1.0f, i == 0 ? 0.2f : 1.0f);
            b.toggle (ids::lfo (i, "sync"), n + "Sync", false);
            b.choice (ids::lfo (i, "div"), n + "Division", syncDivisionNames(), 2);
            b.toggle (ids::lfo (i, "retrig"), n + "Retrigger", false);
        }
        layout.add (std::move (b.group));
    }

    // ---- Modulation envelopes ---------------------------------------------
    {
        Builder b ("modenvs", "Mod Envelopes");
        for (int i = 0; i < kNumModEnvs; ++i)
        {
            const juce::String n = "Env " + juce::String (i + 1) + " ";
            b.ms (ids::modEnv (i, "attack"), n + "Attack", 0.5f, 20000.0f, 500.0f, 10.0f);
            b.ms (ids::modEnv (i, "decay"), n + "Decay", 1.0f, 20000.0f, 800.0f, 600.0f);
            b.pct (ids::modEnv (i, "sustain"), n + "Sustain", 0.0f);
            b.ms (ids::modEnv (i, "release"), n + "Release", 1.0f, 30000.0f, 1000.0f, 600.0f);
        }
        layout.add (std::move (b.group));
    }

    // ---- Macros ------------------------------------------------------------
    {
        Builder b ("macros", "Macros");
        for (int i = 0; i < kNumMacros; ++i)
            b.pct (ids::macro (i), macroNames()[i], 0.0f);
        layout.add (std::move (b.group));
    }

    // ---- Modulation matrix -------------------------------------------------
    {
        Builder b ("modmatrix", "Mod Matrix");
        for (int i = 0; i < kNumModSlots; ++i)
        {
            const juce::String n = "Mod " + juce::String (i + 1) + " ";
            b.choice (ids::modSlot (i, "src"), n + "Source", modSourceNames(), 0);
            b.choice (ids::modSlot (i, "dst"), n + "Destination", modDestinationNames(), 0);
            b.bipolar (ids::modSlot (i, "amt"), n + "Amount", 0.0f);
        }
        layout.add (std::move (b.group));
    }

    return layout;
}

} // namespace nebula
