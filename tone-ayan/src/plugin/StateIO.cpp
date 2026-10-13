#include "StateIO.h"

namespace tone::plugin
{
juce::MemoryBlock encodeFlac (const SourceAudio& src)
{
    juce::MemoryBlock block;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::MemoryOutputStream> (block, false);
    juce::FlacAudioFormat flac;
    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate (src.sampleRate)
                             .withNumChannels (1)
                             .withBitsPerSample (24)
                             .withQualityOptionIndex (5);
    if (auto writer = flac.createWriterFor (stream, options))
    {
        const float* channels[] = { src.samples.data() };
        writer->writeFromFloatArrays (channels, 1, (int) src.samples.size());
        writer.reset(); // flushes into block
    }
    return block;
}

std::shared_ptr<SourceAudio> decodeFlac (const void* data, size_t size)
{
    juce::FlacAudioFormat flac;
    std::unique_ptr<juce::AudioFormatReader> reader (
        flac.createReaderFor (new juce::MemoryInputStream (data, size, false), true));
    if (reader == nullptr)
        return nullptr;
    auto out = std::make_shared<SourceAudio>();
    out->sampleRate = reader->sampleRate;
    out->samples.resize ((size_t) reader->lengthInSamples);
    float* dest[] = { out->samples.data() };
    if (! reader->read (dest, 1, 0, (int) reader->lengthInSamples))
        return nullptr;
    return out;
}

std::shared_ptr<SourceAudio> loadAudioFile (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Unsupported or unreadable audio file: " + file.getFileName();
        return nullptr;
    }
    if (reader->lengthInSamples > (juce::int64) reader->sampleRate * 60 * 30)
    {
        error = "File is longer than 30 minutes.";
        return nullptr;
    }

    const auto len = (int) reader->lengthInSamples;
    const auto channels = (int) reader->numChannels;
    juce::AudioBuffer<float> buffer (channels, len);
    reader->read (&buffer, 0, len, 0, true, true);

    auto out = std::make_shared<SourceAudio>();
    out->sampleRate = reader->sampleRate;
    out->samples.assign ((size_t) len, 0.0f);
    for (int c = 0; c < channels; ++c)
        juce::FloatVectorOperations::addWithMultiply (out->samples.data(), buffer.getReadPointer (c), 1.0f / (float) channels, len);
    return out;
}

bool writeWav (const juce::File& file, const std::vector<float>& samples, double sampleRate)
{
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (stream == nullptr)
        return false;
    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (24);
    auto writer = wav.createWriterFor (stream, options);
    if (writer == nullptr)
        return false;
    const float* channels[] = { samples.data() };
    return writer->writeFromFloatArrays (channels, 1, (int) samples.size());
}
} // namespace tone::plugin
