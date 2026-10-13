#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace tone
{
/** Little-endian binary writer for compact persistence blobs. */
class BinaryWriter
{
public:
    template <typename T>
    void put (T v)
    {
        static_assert (std::is_trivially_copyable_v<T>);
        const auto* p = reinterpret_cast<const uint8_t*> (&v);
        data.insert (data.end(), p, p + sizeof (T));
    }

    template <typename T>
    void putArray (const std::vector<T>& v)
    {
        put<uint64_t> (v.size());
        const auto* p = reinterpret_cast<const uint8_t*> (v.data());
        data.insert (data.end(), p, p + v.size() * sizeof (T));
    }

    std::vector<uint8_t> data;
};

class BinaryReader
{
public:
    BinaryReader (const uint8_t* d, size_t n) : ptr (d), size (n) {}

    template <typename T>
    T get()
    {
        T v;
        need (sizeof (T));
        std::memcpy (&v, ptr + pos, sizeof (T));
        pos += sizeof (T);
        return v;
    }

    template <typename T>
    std::vector<T> getArray()
    {
        const auto count = get<uint64_t>();
        if (count > (size - pos) / sizeof (T))
            throw std::runtime_error ("corrupt blob: array too large");
        std::vector<T> v ((size_t) count);
        std::memcpy (v.data(), ptr + pos, (size_t) count * sizeof (T));
        pos += (size_t) count * sizeof (T);
        return v;
    }

private:
    void need (size_t n) const
    {
        if (pos + n > size)
            throw std::runtime_error ("corrupt blob: unexpected end");
    }

    const uint8_t* ptr;
    size_t size;
    size_t pos = 0;
};
} // namespace tone
