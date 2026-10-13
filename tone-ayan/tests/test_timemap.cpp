#include "engine/TimeMap.h"
#include "model/Edits.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace tone;
using Catch::Matchers::WithinAbs;

TEST_CASE ("Time map follows note edits", "[timemap]")
{
    Project p;
    p.sourceLength = 4.0;
    for (int i = 0; i < 2; ++i)
    {
        Note n;
        n.id = p.nextId++;
        n.srcStart = n.dstStart = 1.0 + i;
        n.srcEnd = n.dstEnd = 2.0 + i;
        p.notes.push_back (n);
    }

    CHECK (TimeMap::fromProject (p).isIdentity());

    edits::resizeEdge (p, 2, edits::Edge::End, 4.0); // second note 1 s -> 2 s
    const auto m = TimeMap::fromProject (p);
    CHECK_THAT (m.toSrc (0.5), WithinAbs (0.5, 1e-12));
    CHECK_THAT (m.toSrc (3.0), WithinAbs (2.5, 1e-12));
    CHECK_THAT (m.srcRate (3.0), WithinAbs (0.5, 1e-12));
    CHECK_THAT (m.toDst (2.5), WithinAbs (3.0, 1e-12));
    CHECK_THAT (m.toSrc (4.5), WithinAbs (3.5, 1e-12)); // tail keeps the offset
    CHECK_THAT (p.outputLength(), WithinAbs (5.0, 1e-12));

    for (double t = 0.0; t < 5.0; t += 0.01)
        CHECK (m.toSrc (t + 0.01) >= m.toSrc (t));
}
