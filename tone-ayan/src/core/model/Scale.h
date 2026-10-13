#pragma once

#include <array>
#include <string>

namespace tone
{
enum class ScaleType
{
    Chromatic, Major, NaturalMinor, HarmonicMinor, MelodicMinor,
    Dorian, Phrygian, Lydian, Mixolydian, Locrian,
    MajorPentatonic, MinorPentatonic, Blues,
    NumTypes
};

struct Scale
{
    int root = 0; // pitch class, 0 = C
    ScaleType type = ScaleType::Chromatic;

    std::array<bool, 12> pitchClasses() const;
    bool contains (int midiNote) const;

    /** Nearest MIDI pitch (integer) that belongs to the scale. */
    float nearest (float midi) const;

    bool operator== (const Scale&) const = default;

    static const char* typeName (ScaleType t);
    static const char* rootName (int pitchClass);
    static ScaleType typeFromName (const std::string& name); // unknown -> Chromatic
    static int rootFromName (const std::string& name);       // unknown -> 0
};
} // namespace tone
