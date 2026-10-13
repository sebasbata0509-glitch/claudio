#include "Serialize.h"

#include <nlohmann/json.hpp>

namespace tone
{
using nlohmann::json;

std::string projectToJson (const Project& p, int indent)
{
    json j;
    j["format"] = "tone-ayan-project";
    j["version"] = 1;
    j["sourceLength"] = p.sourceLength;
    j["nextId"] = p.nextId;
    j["scale"] = { { "root", Scale::rootName (p.scale.root) }, { "type", Scale::typeName (p.scale.type) } };
    auto& notes = j["notes"] = json::array();
    for (const auto& n : p.notes)
        notes.push_back ({ { "id", n.id },
                           { "srcStart", n.srcStart }, { "srcEnd", n.srcEnd }, { "centerMidi", n.centerMidi },
                           { "dstStart", n.dstStart }, { "dstEnd", n.dstEnd },
                           { "pitchShift", n.pitchShift }, { "drift", n.driftAmount },
                           { "modulation", n.modulationAmount }, { "formant", n.formantShift },
                           { "gainDb", n.gainDb } });
    return j.dump (indent);
}

Project projectFromJson (const std::string& text)
{
    const auto j = json::parse (text);
    Project p;
    p.sourceLength = j.value ("sourceLength", 0.0);
    p.nextId = j.value ("nextId", 1u);
    if (j.contains ("scale"))
    {
        p.scale.root = Scale::rootFromName (j["scale"].value ("root", std::string ("C")));
        p.scale.type = Scale::typeFromName (j["scale"].value ("type", std::string ("Chromatic")));
    }
    for (const auto& jn : j.at ("notes"))
    {
        Note n;
        n.id = jn.at ("id").get<NoteId>();
        n.srcStart = jn.at ("srcStart").get<double>();
        n.srcEnd = jn.at ("srcEnd").get<double>();
        n.centerMidi = jn.at ("centerMidi").get<float>();
        n.dstStart = jn.value ("dstStart", n.srcStart);
        n.dstEnd = jn.value ("dstEnd", n.srcEnd);
        n.pitchShift = jn.value ("pitchShift", 0.0f);
        n.driftAmount = jn.value ("drift", 1.0f);
        n.modulationAmount = jn.value ("modulation", 1.0f);
        n.formantShift = jn.value ("formant", 0.0f);
        n.gainDb = jn.value ("gainDb", 0.0f);
        p.nextId = std::max (p.nextId, n.id + 1);
        p.notes.push_back (n);
    }
    std::sort (p.notes.begin(), p.notes.end(), [] (const Note& a, const Note& b) { return a.srcStart < b.srcStart; });
    return p;
}
} // namespace tone
