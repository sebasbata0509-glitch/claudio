#pragma once

#include "EditSession.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace tone::plugin
{
/**
    Adds (or writes) session audio into buffer samples [start, start + num) for the clip time
    range beginning at clipSeconds. Linear interpolation handles host/clip rate differences.
    Real-time safe. Returns a mask of whether any sample fell inside the clip.
*/
inline bool readClip (const PlaybackAudio& clip, double clipSeconds, double hostRate,
                      juce::AudioBuffer<float>& buffer, int start, int num, bool replace) noexcept
{
    const auto len = (int64_t) clip.samples.size();
    if (len < 2)
        return false;
    const double step = clip.sampleRate / hostRate;
    double pos = clipSeconds * clip.sampleRate;
    bool any = false;
    const int channels = buffer.getNumChannels();
    for (int i = 0; i < num; ++i, pos += step)
    {
        if (pos < 0.0 || pos >= (double) (len - 1))
            continue;
        const auto k = (int64_t) pos;
        const auto f = (float) (pos - (double) k);
        const float v = clip.samples[(size_t) k] + f * (clip.samples[(size_t) k + 1] - clip.samples[(size_t) k]);
        for (int c = 0; c < channels; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            d[start + i] = replace ? v : d[start + i] + v;
        }
        any = true;
    }
    return any;
}
} // namespace tone::plugin
