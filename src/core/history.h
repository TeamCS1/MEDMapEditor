// Undo / redo history, modelled on MEd v0.32 (CMapList undo at 0x00413350, redo at 0x00412600,
// snapshot at 0x004116a0 and 0x00411e50).
//
// MEd keeps whole-document snapshots on two stacks:
//  - before the first change of an "edit session" it pushes a snapshot of the document on the undo
//    stack and trims the stack to 5 entries;
//  - the session lasts until the selection changes or is cancelled (CMapView, flag at +0xb0), so
//    moving one selection around any number of times is a single undo step;
//  - Undo pushes the current state on the redo stack and restores the last undo snapshot; Redo is the
//    mirror image; both end by cancelling the selection.
//
// Deliberate differences from MEd (see CLAUDE.md, "Original bugs fixed" and "Deliberate changes"):
//  - the limit is 20 actions, not 5 (the user asked for 20);
//  - a new edit clears the redo stack. MEd leaves stale redo entries, so a Redo after a new edit
//    jumped the document to an unrelated older state.
#pragma once
#include <cstddef>
#include <utility>
#include <vector>

namespace med {

constexpr size_t kUndoLimit = 20;  // MEd: 5

template <class State>
class UndoHistory {
public:
    explicit UndoHistory(size_t limit = kUndoLimit) : limit_(limit) {}

    // True when the next change must be preceded by snapshot() (no edit session is open).
    bool needsSnapshot() const { return !sessionOpen_; }

    // Call with the document as it is BEFORE the first change of a session. Opens the session,
    // drops the redo stack and trims the oldest undo entries beyond the limit.
    void snapshot(State state) {
        undo_.push_back(std::move(state));
        trim(undo_);
        redo_.clear();
        sessionOpen_ = true;
    }

    // The selection changed or was cancelled: the next change starts a new undo step.
    void endSession() { sessionOpen_ = false; }

    // Replace `current` with the previous state. Returns false when there is nothing to undo.
    bool undo(State& current) {
        if (undo_.empty()) return false;
        redo_.push_back(std::move(current));
        trim(redo_);
        current = std::move(undo_.back());
        undo_.pop_back();
        sessionOpen_ = false;
        return true;
    }

    bool redo(State& current) {
        if (redo_.empty()) return false;
        undo_.push_back(std::move(current));
        trim(undo_);
        current = std::move(redo_.back());
        redo_.pop_back();
        sessionOpen_ = false;
        return true;
    }

    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    size_t undoCount() const { return undo_.size(); }
    size_t redoCount() const { return redo_.size(); }
    size_t limit() const { return limit_; }

    void clear() {
        undo_.clear();
        redo_.clear();
        sessionOpen_ = false;
    }

private:
    void trim(std::vector<State>& v) {
        while (v.size() > limit_) v.erase(v.begin());  // drop the oldest
    }

    size_t limit_;
    std::vector<State> undo_, redo_;
    bool sessionOpen_ = false;
};

}  // namespace med
