#pragma once

// Tiny dependency-free test helpers.

#include <cstdio>
#include <string>

namespace nsw::test
{
inline int& failures() { static int f = 0; return f; }

inline void check (bool ok, const std::string& what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what.c_str());
    if (! ok)
        ++failures();
}

inline int finish (const char* suite)
{
    if (failures() == 0)
        std::printf ("%s: all checks passed\n", suite);
    else
        std::printf ("%s: %d check(s) FAILED\n", suite, failures());
    return failures() == 0 ? 0 : 1;
}

template <typename... Args>
std::string fmt (const char* f, Args... args)
{
    char buf[512];
    std::snprintf (buf, sizeof (buf), f, args...);
    return buf;
}
} // namespace nsw::test
