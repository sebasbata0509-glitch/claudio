#include "WavFile.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace tone
{
namespace
{
uint32_t u32 (const uint8_t* p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24); }
uint16_t u16 (const uint8_t* p) { return (uint16_t) (p[0] | (p[1] << 8)); }

void put32 (std::ofstream& f, uint32_t v) { uint8_t b[4] = { (uint8_t) v, (uint8_t) (v >> 8), (uint8_t) (v >> 16), (uint8_t) (v >> 24) }; f.write ((const char*) b, 4); }
void put16 (std::ofstream& f, uint16_t v) { uint8_t b[2] = { (uint8_t) v, (uint8_t) (v >> 8) }; f.write ((const char*) b, 2); }
} // namespace

SourceAudio readWavMono (const std::string& path)
{
    std::ifstream f (path, std::ios::binary);
    if (! f)
        throw std::runtime_error ("cannot open " + path);
    std::vector<uint8_t> d ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
    if (d.size() < 12 || std::memcmp (d.data(), "RIFF", 4) != 0 || std::memcmp (d.data() + 8, "WAVE", 4) != 0)
        throw std::runtime_error (path + " is not a RIFF/WAVE file");

    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t* data = nullptr;
    size_t dataSize = 0;

    for (size_t pos = 12; pos + 8 <= d.size();)
    {
        const uint32_t size = u32 (&d[pos + 4]);
        const uint8_t* body = &d[pos + 8];
        const size_t avail = std::min<size_t> (size, d.size() - pos - 8);
        if (std::memcmp (&d[pos], "fmt ", 4) == 0 && avail >= 16)
        {
            format = u16 (body);
            channels = u16 (body + 2);
            rate = u32 (body + 4);
            bits = u16 (body + 14);
            if (format == 0xFFFE && avail >= 26)
                format = u16 (body + 24); // WAVE_FORMAT_EXTENSIBLE sub-format
        }
        else if (std::memcmp (&d[pos], "data", 4) == 0)
        {
            data = body;
            dataSize = avail;
        }
        pos += 8 + size + (size & 1);
    }

    if (data == nullptr || channels == 0 || rate == 0)
        throw std::runtime_error (path + ": missing fmt or data chunk");

    const size_t bytes = bits / 8;
    const size_t frames = dataSize / (bytes * channels);
    SourceAudio out;
    out.sampleRate = rate;
    out.samples.resize (frames);

    for (size_t i = 0; i < frames; ++i)
    {
        double sum = 0.0;
        for (size_t c = 0; c < channels; ++c)
        {
            const uint8_t* p = data + (i * channels + c) * bytes;
            double v = 0.0;
            if (format == 3 && bits == 32) { float x; std::memcpy (&x, p, 4); v = x; }
            else if (format == 3 && bits == 64) { double x; std::memcpy (&x, p, 8); v = x; }
            else if (format == 1 && bits == 16) v = (int16_t) u16 (p) / 32768.0;
            else if (format == 1 && bits == 24) v = ((int32_t) ((uint32_t) p[0] << 8 | (uint32_t) p[1] << 16 | (uint32_t) p[2] << 24) >> 8) / 8388608.0;
            else if (format == 1 && bits == 32) v = (int32_t) u32 (p) / 2147483648.0;
            else if (format == 1 && bits == 8) v = (p[0] - 128) / 128.0;
            else throw std::runtime_error (path + ": unsupported WAV encoding");
            sum += v;
        }
        out.samples[i] = (float) (sum / channels);
    }
    return out;
}

void writeWavMono (const std::string& path, const float* x, size_t n, double sampleRate)
{
    std::ofstream f (path, std::ios::binary);
    if (! f)
        throw std::runtime_error ("cannot write " + path);
    const auto dataBytes = (uint32_t) (n * 4);
    f.write ("RIFF", 4);
    put32 (f, 36 + dataBytes);
    f.write ("WAVEfmt ", 8);
    put32 (f, 16);
    put16 (f, 3);
    put16 (f, 1);
    put32 (f, (uint32_t) sampleRate);
    put32 (f, (uint32_t) sampleRate * 4);
    put16 (f, 4);
    put16 (f, 32);
    f.write ("data", 4);
    put32 (f, dataBytes);
    f.write ((const char*) x, (std::streamsize) dataBytes);
    if (! f)
        throw std::runtime_error ("write failed: " + path);
}
} // namespace tone
