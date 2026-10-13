#include "Project.h"

#include <algorithm>

namespace tone
{
const Note* Project::find (NoteId id) const
{
    for (const auto& n : notes)
        if (n.id == id)
            return &n;
    return nullptr;
}

Note* Project::find (NoteId id)
{
    for (auto& n : notes)
        if (n.id == id)
            return &n;
    return nullptr;
}

int Project::indexOf (NoteId id) const
{
    for (size_t i = 0; i < notes.size(); ++i)
        if (notes[i].id == id)
            return (int) i;
    return -1;
}

double Project::outputLength() const
{
    if (notes.empty())
        return sourceLength;
    const auto& last = notes.back();
    return std::max (last.dstEnd, sourceLength + (last.dstEnd - last.srcEnd));
}
} // namespace tone
