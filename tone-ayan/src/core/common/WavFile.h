#pragma once

#include "Audio.h"

#include <string>

namespace tone
{
/** Minimal RIFF/WAVE reader (PCM 16/24/32, float 32/64; multichannel mixed to mono). Throws on error. */
SourceAudio readWavMono (const std::string& path);

/** Writes 32-bit float mono WAV. Throws on error. */
void writeWavMono (const std::string& path, const float* samples, size_t numSamples, double sampleRate);
} // namespace tone
