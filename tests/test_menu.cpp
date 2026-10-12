#include <cstdio>
#include "../src/viewer/menu.h"

using namespace med;

static int failures = 0;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) {                                                    \
            std::printf("FAIL line %d: %s\n", __LINE__, #c);           \
            ++failures;                                                \
        }                                                              \
    } while (0)

static const MenuItem* find(const std::vector<MenuItem>& m, int cmd) {
    for (const MenuItem& i : m) if (i.command == cmd) return &i;
    return nullptr;
}

int main() {
    // both menus offer the two kinds of delete, enabled, with the safe one as the default
    for (bool multiple : {false, true}) {
        auto m = objectMenu(multiple);
        const MenuItem* relocate = find(m, kCmdDeleteRelocate);
        const MenuItem* hard = find(m, kCmdDeleteHard);
        CHECK(relocate && relocate->enabled && relocate->isDefault);
        CHECK(hard && hard->enabled && !hard->isDefault);
        CHECK(relocate->text.find("below the map") != std::string::npos);
        CHECK(hard->text.find("permanently") != std::string::npos);
        const MenuItem* copy = find(m, kCmdCopy);
        CHECK(copy && copy->enabled);  // Copy is live
    }
    // the single-object menu has MEd's entries; only Centre on Object is live
    auto single = objectMenu(false);
    CHECK(find(single, kCmdCentre) && find(single, kCmdCentre)->enabled);
    CHECK(find(single, kCmdExtract) && !find(single, kCmdExtract)->enabled);
    CHECK(find(single, kCmdDuplicate) && find(single, kCmdDuplicate)->enabled);
    CHECK(find(single, kCmdReplace) && !find(single, kCmdReplace)->enabled);
    CHECK(find(single, kCmdProperties) && !find(single, kCmdProperties)->enabled);
    // the multiple-objects menu is the "Selected Items" one and has no per-object entries
    auto multi = objectMenu(true);
    CHECK(multi[0].text == "Selected Items" && !multi[0].enabled);
    CHECK(!find(multi, kCmdCentre) && !find(multi, kCmdExtract));

    // which menu: Shift always, or several selected and the click is on one of them
    CHECK(useMultipleMenu(true, 0, false));
    CHECK(useMultipleMenu(false, 3, true));
    CHECK(!useMultipleMenu(false, 3, false));
    CHECK(!useMultipleMenu(false, 1, true));
    CHECK(kSunkZ == -200.0);
    {  // right-click on empty ground: New Object (live) and Paste Object (not built yet)
        const std::vector<MenuItem> g = emptyGroundMenu(false), g2 = emptyGroundMenu(true);
        CHECK(g.size() == 2 && g[0].command == kCmdNewObject && g[0].enabled && g[1].command == kCmdPaste && !g[1].enabled);
        CHECK(g2[1].enabled);  // Paste needs something on the clipboard
    }

    if (failures == 0) std::printf("all menu tests passed\n");
    return failures ? 1 : 0;
}
