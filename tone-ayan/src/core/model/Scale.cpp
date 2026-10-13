#include "Scale.h"

#include <cmath>
#include <initializer_list>

namespace tone
{
namespace
{
std::array<bool, 12> maskOf (std::initializer_list<int> steps)
{
    std::array<bool, 12> m {};
    for (int s : steps)
        m[(size_t) s] = true;
    return m;
}

const char* const kTypeNames[] = { "Chromatic", "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor",
                                   "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian",
                                   "Major Pentatonic", "Minor Pentatonic", "Blues" };
const char* const kRootNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
} // namespace

std::array<bool, 12> Scale::pitchClasses() const
{
    std::array<bool, 12> rel {};
    switch (type)
    {
        case ScaleType::Major:           rel = maskOf ({ 0, 2, 4, 5, 7, 9, 11 }); break;
        case ScaleType::NaturalMinor:    rel = maskOf ({ 0, 2, 3, 5, 7, 8, 10 }); break;
        case ScaleType::HarmonicMinor:   rel = maskOf ({ 0, 2, 3, 5, 7, 8, 11 }); break;
        case ScaleType::MelodicMinor:    rel = maskOf ({ 0, 2, 3, 5, 7, 9, 11 }); break;
        case ScaleType::Dorian:          rel = maskOf ({ 0, 2, 3, 5, 7, 9, 10 }); break;
        case ScaleType::Phrygian:        rel = maskOf ({ 0, 1, 3, 5, 7, 8, 10 }); break;
        case ScaleType::Lydian:          rel = maskOf ({ 0, 2, 4, 6, 7, 9, 11 }); break;
        case ScaleType::Mixolydian:      rel = maskOf ({ 0, 2, 4, 5, 7, 9, 10 }); break;
        case ScaleType::Locrian:         rel = maskOf ({ 0, 1, 3, 5, 6, 8, 10 }); break;
        case ScaleType::MajorPentatonic: rel = maskOf ({ 0, 2, 4, 7, 9 }); break;
        case ScaleType::MinorPentatonic: rel = maskOf ({ 0, 3, 5, 7, 10 }); break;
        case ScaleType::Blues:           rel = maskOf ({ 0, 3, 5, 6, 7, 10 }); break;
        case ScaleType::Chromatic:
        case ScaleType::NumTypes:
        default:                         rel.fill (true); break;
    }
    std::array<bool, 12> abs {};
    for (int i = 0; i < 12; ++i)
        abs[(size_t) ((i + root) % 12)] = rel[(size_t) i];
    return abs;
}

bool Scale::contains (int midiNote) const
{
    return pitchClasses()[(size_t) (((midiNote % 12) + 12) % 12)];
}

float Scale::nearest (float midi) const
{
    const auto pcs = pitchClasses();
    const int base = (int) std::lround (midi);
    float best = (float) base;
    float bestDist = 1.0e9f;
    for (int d = -6; d <= 6; ++d)
    {
        const int cand = base + d;
        if (! pcs[(size_t) (((cand % 12) + 12) % 12)])
            continue;
        const float dist = std::abs ((float) cand - midi);
        if (dist < bestDist) { bestDist = dist; best = (float) cand; }
    }
    return best;
}

const char* Scale::typeName (ScaleType t)
{
    const auto i = (int) t;
    return (i >= 0 && i < (int) ScaleType::NumTypes) ? kTypeNames[i] : kTypeNames[0];
}

const char* Scale::rootName (int pc) { return kRootNames[((pc % 12) + 12) % 12]; }

ScaleType Scale::typeFromName (const std::string& name)
{
    for (int i = 0; i < (int) ScaleType::NumTypes; ++i)
        if (name == kTypeNames[i])
            return (ScaleType) i;
    // tolerant aliases for hand-written JSON
    if (name == "major") return ScaleType::Major;
    if (name == "minor") return ScaleType::NaturalMinor;
    return ScaleType::Chromatic;
}

int Scale::rootFromName (const std::string& name)
{
    for (int i = 0; i < 12; ++i)
        if (name == kRootNames[i])
            return i;
    if (name == "Db") return 1;
    if (name == "Eb") return 3;
    if (name == "Gb") return 6;
    if (name == "Ab") return 8;
    if (name == "Bb") return 10;
    return 0;
}
} // namespace tone
