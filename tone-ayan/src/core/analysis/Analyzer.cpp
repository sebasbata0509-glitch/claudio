#include "Analyzer.h"

#include "../common/BinaryIO.h"
#include "Onsets.h"

namespace tone
{
namespace
{
constexpr uint32_t kMagic = 0x544f4e41; // 'TONA'
constexpr uint32_t kVersion = 1;
} // namespace

AnalysisResult analyse (const float* x, size_t n, double sr, const AnalysisParams& params, const JobControl& job)
{
    AnalysisResult r;
    r.sourceLengthSeconds = (double) n / sr;

    JobControl pyinJob;
    pyinJob.cancel = job.cancel;
    pyinJob.onProgress = [&job] (float p) { job.progress (0.85f * p); };
    r.track = runPYin (x, n, sr, params.pyin, pyinJob);
    if (job.cancelled())
        return r;

    cleanVoicing (r.track, params.segmenter);
    computeOnsetStrength (x, n, r.track);
    job.progress (0.92f);
    if (job.cancelled())
        return r;

    r.notes = segmentNotes (r.track, params.segmenter);

    // Only frames that belong to a note are treated as voiced from here on.
    std::vector<bool> inNote (r.track.frames.size(), false);
    for (const auto& note : r.notes)
        for (int i = note.startFrame; i < note.endFrame; ++i)
            inNote[(size_t) i] = true;
    for (size_t i = 0; i < inNote.size(); ++i)
        if (! inNote[i])
            r.track.frames[i].voiced = false;

    r.marks = findPitchMarks (x, n, r.track);
    job.progress (1.0f);
    return r;
}

std::vector<uint8_t> AnalysisResult::serialise() const
{
    BinaryWriter w;
    w.put (kMagic);
    w.put (kVersion);
    w.put (sourceLengthSeconds);
    w.put (track.sampleRate);
    w.put<int32_t> (track.hop);
    w.putArray (track.frames);
    w.putArray (marks);
    w.putArray (notes);
    return std::move (w.data);
}

AnalysisResult AnalysisResult::deserialise (const uint8_t* data, size_t size)
{
    BinaryReader rd (data, size);
    if (rd.get<uint32_t>() != kMagic)
        throw std::runtime_error ("not a Tone-ayan analysis blob");
    if (rd.get<uint32_t>() != kVersion)
        throw std::runtime_error ("unsupported analysis version");
    AnalysisResult r;
    r.sourceLengthSeconds = rd.get<double>();
    r.track.sampleRate = rd.get<double>();
    r.track.hop = rd.get<int32_t>();
    r.track.frames = rd.getArray<PitchFrame>();
    r.marks = rd.getArray<PitchMark>();
    r.notes = rd.getArray<NoteSegment>();
    return r;
}
} // namespace tone
