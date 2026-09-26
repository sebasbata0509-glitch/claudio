#pragma once

// Minimal RIFF/WAVE reader/writer for tests and offline tools (not used by the plugin).
// Reads 16/24/32-bit PCM and 32-bit float; writes 24-bit PCM.

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace nsw
{

struct AudioFile
{
    double sampleRate = 48000.0;
    std::vector<std::vector<float>> channels;

    int numChannels() const { return (int) channels.size(); }
    int numSamples() const { return channels.empty() ? 0 : (int) channels[0].size(); }
};

inline bool readWav (const std::string& path, AudioFile& out)
{
    std::ifstream f (path, std::ios::binary);
    if (! f)
        return false;

    auto rd32 = [&f] { uint32_t v = 0; f.read ((char*) &v, 4); return v; };
    auto rd16 = [&f] { uint16_t v = 0; f.read ((char*) &v, 2); return v; };

    char id[4];
    f.read (id, 4);
    if (std::memcmp (id, "RIFF", 4) != 0)
        return false;
    rd32();
    f.read (id, 4);
    if (std::memcmp (id, "WAVE", 4) != 0)
        return false;

    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    std::vector<char> data;

    while (f.read (id, 4))
    {
        const uint32_t size = rd32();
        if (std::memcmp (id, "fmt ", 4) == 0)
        {
            format = rd16();
            channels = rd16();
            rate = rd32();
            rd32();
            rd16();
            bits = rd16();
            if (size > 16)
            {
                std::vector<char> extra (size - 16);
                f.read (extra.data(), (std::streamsize) extra.size());
                if (format == 0xFFFE && extra.size() >= 10)
                    std::memcpy (&format, extra.data() + 8, 2); // sub-format GUID starts with the format tag
            }
        }
        else if (std::memcmp (id, "data", 4) == 0)
        {
            data.resize (size);
            f.read (data.data(), size);
        }
        else
        {
            f.seekg (size + (size & 1), std::ios::cur);
        }
    }

    if (channels == 0 || data.empty())
        return false;

    const int bytes = bits / 8;
    const size_t frames = data.size() / ((size_t) bytes * channels);
    out.sampleRate = rate;
    out.channels.assign (channels, std::vector<float> (frames));

    for (size_t i = 0; i < frames; ++i)
    {
        for (int c = 0; c < channels; ++c)
        {
            const unsigned char* p = (const unsigned char*) data.data() + (i * channels + (size_t) c) * (size_t) bytes;
            float v = 0.0f;
            if (format == 3 && bits == 32)
                std::memcpy (&v, p, 4);
            else if (bits == 16)
                v = (float) (int16_t) (p[0] | (p[1] << 8)) / 32768.0f;
            else if (bits == 24)
                v = (float) ((int32_t) ((uint32_t) p[0] << 8 | (uint32_t) p[1] << 16 | (uint32_t) p[2] << 24) >> 8) / 8388608.0f;
            else if (bits == 32)
                v = (float) ((double) (int32_t) (p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t) p[3] << 24)) / 2147483648.0);
            out.channels[(size_t) c][i] = v;
        }
    }
    return true;
}

inline bool writeWav (const std::string& path, const AudioFile& in)
{
    std::ofstream f (path, std::ios::binary);
    if (! f)
        return false;

    const uint16_t channels = (uint16_t) in.numChannels();
    const uint32_t frames = (uint32_t) in.numSamples();
    const uint32_t rate = (uint32_t) in.sampleRate;
    const uint32_t dataSize = frames * channels * 3;

    auto wr32 = [&f] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto wr16 = [&f] (uint16_t v) { f.write ((const char*) &v, 2); };

    f.write ("RIFF", 4);
    wr32 (36 + dataSize);
    f.write ("WAVEfmt ", 8);
    wr32 (16);
    wr16 (1);
    wr16 (channels);
    wr32 (rate);
    wr32 (rate * channels * 3);
    wr16 ((uint16_t) (channels * 3));
    wr16 (24);
    f.write ("data", 4);
    wr32 (dataSize);

    for (uint32_t i = 0; i < frames; ++i)
    {
        for (uint16_t c = 0; c < channels; ++c)
        {
            const float v = std::max (-1.0f, std::min (1.0f, in.channels[c][i]));
            const auto s = (int32_t) std::lround (v * 8388607.0f);
            const unsigned char b[3] = { (unsigned char) (s & 0xFF), (unsigned char) ((s >> 8) & 0xFF),
                                         (unsigned char) ((s >> 16) & 0xFF) };
            f.write ((const char*) b, 3);
        }
    }
    return (bool) f;
}

} // namespace nsw
