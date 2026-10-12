// The right-click object menu, after MEd v0.32 (CMapView::mousePressEvent, 0x0045f1e0). MEd shows one
// menu for a single object and a "Selected Items" menu when several objects are selected or Shift is held.
// Its labels and slots are: Extract Object (extractObject), Duplicate Object (duplicateObject),
// Properties, Centre on Object (centreObject), Replace Object (replaceObject), Delete Object
// (deleteObject), Copy (copyObject); and for several objects Copy (copyObjects) and Delete Objects
// (deleteObjects).
//
// Deliberate change: MEd has one Delete entry that removes the placement from the IPL file, which is
// what makes the game crash (a building and its LOD stand-in must stay together, see CLAUDE.md). Delete is
// split in two here: "move below the map" (the default, sinks the object and its LOD partner to
// z = -200, as the user has done by hand in the game) and "remove permanently" (removes the object and
// its LOD partner from the file). Entries that are not ported yet are shown greyed out.
#pragma once
#include <string>
#include <vector>

namespace med {

constexpr double kSunkZ = -200.0;  // tried in the game: -200 is safe, -2000 crashed it

enum MenuCommand {
    kCmdNone = 0,
    kCmdCentre = 1,
    kCmdProperties,
    kCmdExtract,
    kCmdDuplicate,
    kCmdReplace,
    kCmdCopy,
    kCmdDeleteRelocate,  // default Delete
    kCmdDeleteHard,
    kCmdNewObject,
    kCmdPaste,
};

struct MenuItem {
    std::string text;
    int command = kCmdNone;  // kCmdNone with an empty text is a separator, with text a disabled heading
    bool enabled = true;
    bool isDefault = false;  // drawn in bold
};

inline std::vector<MenuItem> objectMenu(bool multiple) {
    std::vector<MenuItem> m;
    if (multiple) {
        m.push_back({"Selected Items", kCmdNone, false, false});
        m.push_back({"Duplicate Objects", kCmdDuplicate, true, false});
        m.push_back({"Copy", kCmdCopy, true, false});
        m.push_back({"Replace Objects...", kCmdReplace, true, false});
        m.push_back({"Extract Objects...", kCmdExtract, true, false});
        m.push_back({"", kCmdNone, false, false});
        m.push_back({"Delete Objects (move below the map)", kCmdDeleteRelocate, true, true});
        m.push_back({"Delete Objects permanently (remove from file)", kCmdDeleteHard, true, false});
        return m;
    }
    m.push_back({"Centre on Object", kCmdCentre, true, false});
    m.push_back({"Properties", kCmdProperties, false, false});
    m.push_back({"Extract Object...", kCmdExtract, true, false});
    m.push_back({"Duplicate Object", kCmdDuplicate, true, false});
    m.push_back({"Replace Object...", kCmdReplace, true, false});
    m.push_back({"Copy", kCmdCopy, true, false});
    m.push_back({"", kCmdNone, false, false});
    m.push_back({"Delete Object (move below the map)", kCmdDeleteRelocate, true, true});
    m.push_back({"Delete Object permanently (remove from file)", kCmdDeleteHard, true, false});
    return m;
}

// MEd's menu for a right-click on empty ground: New Object and Paste Object (Paste only when something was copied).
inline std::vector<MenuItem> emptyGroundMenu(bool canPaste) {
    return {{"New Object...", kCmdNewObject, true, false}, {"Paste Object", kCmdPaste, canPaste, false}};
}

// Which menu MEd shows for a right-click: the multiple-objects menu with Shift held, or when more than one
// object is selected and the clicked object is one of them.
inline bool useMultipleMenu(bool shiftHeld, size_t selectedCount, bool clickedIsSelected) {
    return shiftHeld || (selectedCount > 1 && clickedIsSelected);
}

}  // namespace med
