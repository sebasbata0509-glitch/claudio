#include "model/Edits.h"
#include "model/Serialize.h"
#include "model/UndoStack.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace tone;
using Catch::Matchers::WithinAbs;

namespace
{
/** Three contiguous notes (0-1, 1-2, 2-3 s), a gap, then a fourth note (4-5 s). */
Project makeProject()
{
    Project p;
    p.sourceLength = 6.0;
    const double ranges[4][2] = { { 0.5, 1.0 }, { 1.0, 2.0 }, { 2.0, 3.0 }, { 4.0, 5.0 } };
    const float centres[4] = { 60.2f, 62.0f, 63.7f, 67.1f };
    for (int i = 0; i < 4; ++i)
    {
        Note n;
        n.id = p.nextId++;
        n.srcStart = n.dstStart = ranges[i][0];
        n.srcEnd = n.dstEnd = ranges[i][1];
        n.centerMidi = centres[i];
        p.notes.push_back (n);
    }
    return p;
}

float centreOf (double s0, double s1) { return (float) (60.0 + s0 + s1); } // deterministic stand-in
} // namespace

TEST_CASE ("Transpose and centre correction", "[model]")
{
    auto p = makeProject();
    edits::transpose (p, { 1, 2 }, 2.0f);
    CHECK_THAT (p.notes[0].targetMidi(), WithinAbs (62.2, 1e-5));
    CHECK_THAT (p.notes[2].targetMidi(), WithinAbs (63.7, 1e-5));

    edits::correctPitchCenter (p, { 1 }, 1.0f);
    CHECK_THAT (p.notes[0].targetMidi(), WithinAbs (62.0, 1e-5));
    edits::correctPitchCenter (p, { 3 }, 0.5f); // 63.7 -> halfway to 64
    CHECK_THAT (p.notes[2].targetMidi(), WithinAbs (63.85, 1e-5));
}

TEST_CASE ("Scale snapping", "[model]")
{
    Scale s { 0, ScaleType::Major };
    CHECK (s.contains (60));
    CHECK_FALSE (s.contains (61));
    CHECK (s.nearest (61.4f) == 62.0f);
    CHECK (s.nearest (63.4f) == 64.0f);

    auto p = makeProject();
    p.scale = { 2, ScaleType::NaturalMinor }; // D minor: D E F G A Bb C
    edits::correctToScale (p, { 1, 2, 3, 4 }, 1.0f);
    CHECK_THAT (p.notes[0].targetMidi(), WithinAbs (60.0, 1e-5)); // C
    CHECK_THAT (p.notes[1].targetMidi(), WithinAbs (62.0, 1e-5)); // D
    CHECK_THAT (p.notes[2].targetMidi(), WithinAbs (64.0, 1e-5)); // E
    CHECK_THAT (p.notes[3].targetMidi(), WithinAbs (67.0, 1e-5)); // G
}

TEST_CASE ("Moving a note stretches attached neighbours and respects limits", "[model]")
{
    auto p = makeProject();
    const double dt = edits::moveTime (p, { 2 }, 0.25);
    CHECK_THAT (dt, WithinAbs (0.25, 1e-12));
    CHECK_THAT (p.notes[0].dstEnd, WithinAbs (1.25, 1e-12));   // previous note stretched
    CHECK_THAT (p.notes[1].dstStart, WithinAbs (1.25, 1e-12));
    CHECK_THAT (p.notes[1].dstEnd, WithinAbs (2.25, 1e-12));
    CHECK_THAT (p.notes[2].dstStart, WithinAbs (2.25, 1e-12)); // next note squeezed
    CHECK_THAT (p.notes[2].dstEnd, WithinAbs (3.0, 1e-12));

    // Cannot push past the next note's minimum length.
    const double big = edits::moveTime (p, { 2 }, 5.0);
    CHECK_THAT (p.notes[2].dstDuration(), WithinAbs (edits::kMinNoteSeconds, 1e-9));
    CHECK (big < 5.0);

    // The isolated note moves freely inside its gap but not before the previous note.
    auto q = makeProject();
    const double back = edits::moveTime (q, { 4 }, -3.0);
    CHECK_THAT (back, WithinAbs (-1.0, 1e-12));
    CHECK_THAT (q.notes[3].dstStart, WithinAbs (3.0, 1e-12));
}

TEST_CASE ("Edge stretching", "[model]")
{
    auto p = makeProject();
    edits::resizeEdge (p, 4, edits::Edge::End, 5.5);
    CHECK_THAT (p.notes[3].dstEnd, WithinAbs (5.5, 1e-12));
    CHECK_THAT (p.notes[3].stretch(), WithinAbs (1.5, 1e-12));

    edits::resizeEdge (p, 2, edits::Edge::End, 2.5);
    CHECK_THAT (p.notes[1].dstEnd, WithinAbs (2.5, 1e-12));
    CHECK_THAT (p.notes[2].dstStart, WithinAbs (2.5, 1e-12));

    edits::resizeEdge (p, 1, edits::Edge::Start, -1.0); // clamped at 0
    CHECK_THAT (p.notes[0].dstStart, WithinAbs (0.0, 1e-12));
}

TEST_CASE ("Split and merge", "[model]")
{
    auto p = makeProject();
    REQUIRE (edits::split (p, 2, 1.4, centreOf));
    REQUIRE (p.notes.size() == 5);
    CHECK_THAT (p.notes[1].srcEnd, WithinAbs (1.4, 1e-12));
    CHECK_THAT (p.notes[2].srcStart, WithinAbs (1.4, 1e-12));
    CHECK (Project::contiguous (p.notes[1], p.notes[2]));
    CHECK_FALSE (edits::split (p, 2, 1.005, centreOf)); // too short

    const auto newId = p.notes[2].id;
    REQUIRE (edits::merge (p, { 2, newId }, centreOf));
    REQUIRE (p.notes.size() == 4);
    CHECK_THAT (p.notes[1].srcEnd, WithinAbs (2.0, 1e-12));

    // Non-contiguous notes are not merged.
    CHECK_FALSE (edits::merge (p, { 3, 4 }, centreOf));
}

TEST_CASE ("Undo / redo restores exact states", "[model]")
{
    auto p = makeProject();
    UndoStack undo;
    const auto s0 = p;
    undo.push (p, "transpose");
    edits::transpose (p, { 1 }, 3.0f);
    const auto s1 = p;
    undo.push (p, "move");
    edits::moveTime (p, { 4 }, 0.3);

    REQUIRE (undo.undo (p));
    CHECK (p == s1);
    REQUIRE (undo.undo (p));
    CHECK (p == s0);
    CHECK_FALSE (undo.undo (p));
    REQUIRE (undo.redo (p));
    CHECK (p == s1);
    CHECK (undo.undoLabel() == "transpose");
}

TEST_CASE ("Project JSON round trip", "[model]")
{
    auto p = makeProject();
    p.scale = { 9, ScaleType::Dorian };
    edits::transpose (p, { 2 }, -1.25f);
    edits::setFormant (p, { 3 }, 2.0f);
    edits::setDrift (p, { 1 }, 0.25f);
    edits::setModulation (p, { 1 }, 1.5f);
    edits::moveTime (p, { 4 }, 0.2);
    const auto back = projectFromJson (projectToJson (p));
    CHECK (back == p);
    CHECK_THROWS (projectFromJson ("{ not json"));
}
