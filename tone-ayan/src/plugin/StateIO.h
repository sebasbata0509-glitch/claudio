#pragma once

#include "common/Audio.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>

namespace tone::plugin
{
/** Lossless 24-bit FLAC encoding of mono source audio for the plugin state. */
juce::MemoryBlock encodeFlac (const SourceAudio&);
std::shared_ptr<SourceAudio> decodeFlac (const void* data, size_t size);

/** Reads any format JUCE knows (WAV, AIFF, FLAC, MP3, OGG...) and mixes it to mono. */
std::shared_ptr<SourceAudio> loadAudioFile (const juce::File&, juce::String& error);

/** Writes a 24-bit WAV. */
bool writeWav (const juce::File&, const std::vector<float>& samples, double sampleRate);
} // namespace tone::plugin
