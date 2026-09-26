#pragma once

#include "Engine.h"

#include <array>

namespace nsw
{

struct FactoryPreset
{
    const char* name;
    Params params;
};

/** Neutral starting point that every factory preset is built from. */
inline Params neutralParams()
{
    Params p;
    p.pitch = 0.0f;
    p.snap = true;
    p.octave = 0.0f;
    p.formant = 0.0f;
    p.mix = 1.0f;

    // Macro ships with musical ranges so turning it up "opens" the sound;
    // the knob itself starts at 0 so presets are unaffected until it is used.
    auto depth = [&p] (ModDest d, float v) { p.mod.macroDepth[(size_t) d - 1] = v; };
    depth (ModDest::Formant, 0.25f);
    depth (ModDest::Smear, 0.4f);
    depth (ModDest::Stereo, 0.5f);
    depth (ModDest::Detune, 0.2f);

    // A ready-to-use arpeggio for the sequencer (off by default): root, fifth, octave...
    p.mod.seqValue = { 0.0f, 0.0f, 7.0f / 12, 0.0f, 1.0f, 0.0f, 7.0f / 12, 5.0f / 12,
                       0.0f, 0.0f, 7.0f / 12, 0.0f, 1.0f, 7.0f / 12, 5.0f / 12, 3.0f / 12 };
    return p;
}

inline std::array<FactoryPreset, 6> factoryPresets()
{
    std::array<FactoryPreset, 6> list {};

    // Bright, pitched-up-an-octave vocal layer. Formant +4 keeps it cute without chipmunking.
    Params porter = neutralParams();
    porter.pitch = 12.0f;
    porter.formant = 4.0f;
    porter.glideMs = 20.0f;
    porter.smear = 0.10f;
    porter.stereo = 0.30f;
    porter.detuneCents = 8.0f;
    porter.mix = 0.60f;
    list[0] = { "Porter Up", porter };

    Params octave = neutralParams();
    octave.pitch = 12.0f;
    list[1] = { "Pure Octave", octave };

    Params chip = neutralParams();
    chip.pitch = 12.0f;
    chip.formant = 12.0f;
    list[2] = { "Chipmunk", chip };

    Params giant = neutralParams();
    giant.octave = -1.0f;
    giant.formant = -6.0f;
    list[3] = { "Giant", giant };

    Params choir = neutralParams();
    choir.alternator = 0.70f;
    choir.smear = 0.50f;
    choir.stereo = 1.0f;
    list[4] = { "Glitch Choir", choir };

    list[5] = { "Init", neutralParams() };
    return list;
}

} // namespace nsw
