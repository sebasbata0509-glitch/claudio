#pragma once

#include "Project.h"

#include <string>
#include <vector>

namespace tone
{
/** Snapshot-based undo/redo: every step stores the full (small) project before the change. */
class UndoStack
{
public:
    explicit UndoStack (size_t maxSteps = 500) : limit (maxSteps) {}

    void push (const Project& before, std::string label)
    {
        undoSteps.push_back ({ before, std::move (label) });
        if (undoSteps.size() > limit)
            undoSteps.erase (undoSteps.begin());
        redoSteps.clear();
    }

    bool canUndo() const noexcept { return ! undoSteps.empty(); }
    bool canRedo() const noexcept { return ! redoSteps.empty(); }
    std::string undoLabel() const { return canUndo() ? undoSteps.back().label : std::string(); }
    std::string redoLabel() const { return canRedo() ? redoSteps.back().label : std::string(); }

    /** Replaces current with the previous state. Returns false when there is nothing to undo. */
    bool undo (Project& current)
    {
        if (! canUndo())
            return false;
        auto step = std::move (undoSteps.back());
        undoSteps.pop_back();
        redoSteps.push_back ({ current, step.label });
        current = std::move (step.state);
        return true;
    }

    bool redo (Project& current)
    {
        if (! canRedo())
            return false;
        auto step = std::move (redoSteps.back());
        redoSteps.pop_back();
        undoSteps.push_back ({ current, step.label });
        current = std::move (step.state);
        return true;
    }

    void clear()
    {
        undoSteps.clear();
        redoSteps.clear();
    }

private:
    struct Step
    {
        Project state;
        std::string label;
    };

    size_t limit;
    std::vector<Step> undoSteps, redoSteps;
};
} // namespace tone
