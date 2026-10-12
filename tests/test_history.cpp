#include <cstdio>
#include <string>
#include "../src/core/history.h"

using namespace med;

static int failures = 0;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) {                                                    \
            std::printf("FAIL line %d: %s\n", __LINE__, #c);           \
            ++failures;                                                \
        }                                                              \
    } while (0)

// A change helper in the way the editor uses the history: snapshot once per session, then edit.
static void change(UndoHistory<int>& h, int& doc, int newValue) {
    if (h.needsSnapshot()) h.snapshot(doc);
    doc = newValue;
}

int main() {
    CHECK(kUndoLimit == 20);

    // basic undo and redo
    UndoHistory<int> h;
    int doc = 0;
    CHECK(!h.canUndo() && !h.canRedo());
    change(h, doc, 1);
    h.endSession();
    change(h, doc, 2);
    h.endSession();
    change(h, doc, 3);
    CHECK(doc == 3 && h.undoCount() == 3);
    CHECK(h.undo(doc) && doc == 2);
    CHECK(h.undo(doc) && doc == 1);
    CHECK(h.undo(doc) && doc == 0);
    CHECK(!h.undo(doc) && doc == 0);  // nothing left
    CHECK(h.redo(doc) && doc == 1);
    CHECK(h.redo(doc) && doc == 2);
    CHECK(h.redo(doc) && doc == 3);
    CHECK(!h.redo(doc));

    // many changes with the same selection are ONE undo step (the session stays open)
    h = UndoHistory<int>();
    doc = 0;
    change(h, doc, 1);
    change(h, doc, 2);
    change(h, doc, 3);
    CHECK(h.undoCount() == 1);
    CHECK(h.undo(doc) && doc == 0);

    // undo and redo end the session: the next change is a new step
    change(h, doc, 7);
    CHECK(h.undoCount() == 1);
    CHECK(h.undo(doc) && doc == 0);
    CHECK(h.needsSnapshot());

    // a new edit clears the redo stack (MEd kept stale redo entries; fixed)
    h = UndoHistory<int>();
    doc = 0;
    change(h, doc, 1);
    h.endSession();
    change(h, doc, 2);
    h.endSession();
    CHECK(h.undo(doc) && doc == 1 && h.canRedo());
    change(h, doc, 10);
    CHECK(!h.canRedo());

    // the limit is 20: the 21st oldest snapshot is dropped
    h = UndoHistory<int>();
    doc = 0;
    for (int i = 1; i <= 30; ++i) {
        change(h, doc, i);
        h.endSession();
    }
    CHECK(h.undoCount() == 20);
    int steps = 0;
    while (h.undo(doc)) ++steps;
    CHECK(steps == 20);
    CHECK(doc == 10);  // 30 changes, 20 undone: the oldest ten cannot be undone
    steps = 0;
    while (h.redo(doc)) ++steps;
    CHECK(steps == 20 && doc == 30);

    // the redo stack is limited too
    h = UndoHistory<int>(3);
    doc = 0;
    for (int i = 1; i <= 6; ++i) {
        change(h, doc, i);
        h.endSession();
    }
    while (h.undo(doc)) {}
    CHECK(h.redoCount() == 3);

    // works with a non-trivial state type
    UndoHistory<std::string> hs;
    std::string text = "a";
    hs.snapshot(text);
    text = "b";
    CHECK(hs.undo(text) && text == "a");
    CHECK(hs.redo(text) && text == "b");

    if (failures == 0) std::printf("all history tests passed\n");
    return failures ? 1 : 0;
}
