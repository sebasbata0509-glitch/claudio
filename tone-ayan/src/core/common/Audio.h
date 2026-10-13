#pragma once

#include <vector>

namespace tone
{
/** Mono source audio the editor works on (immutable once shared). */
struct SourceAudio
{
    std::vector<float> samples;
    double sampleRate = 44100.0;

    double lengthSeconds() const noexcept { return sampleRate > 0 ? (double) samples.size() / sampleRate : 0.0; }
};
} // namespace tone
