// MEd's bookmarks.ini: saved camera positions, one per line,
//   <bookmark id=N>name</bookmark><location id=N>x:y:z:h:yaw:pitch:flag</location>
// with Windows line endings and an empty line at the end (MEd 0.32's own file, MEdv032/bookmarks.ini). The camera values
// are MEd's (CMapView this+0x1c..0x2c); the last number is a flag MEd writes as 0 (its meaning was not found).
// MEd's window (CBookMarks, 0x0040a870): Add... ("Please choose a name for this location."), Remove ("Are you sure you
// want to remove this item from your bookmarks list?"), Move Up, Move Down, Move to location, OK (save), Cancel.
#pragma once
#include <string>
#include <vector>

namespace med {

struct Bookmark {
    std::string name;
    double x = 0, y = 0, z = 0, h = 0, yaw = 0, pitch = 0;
    int flag = 0;
};

// Lines that are not a complete bookmark are skipped; ids are only used to pair a name with its location.
std::vector<Bookmark> parseBookmarks(const std::string& text);

// Numbered 0, 1, 2, ... in list order, numbers written like MEd (6 significant digits).
std::string writeBookmarks(const std::vector<Bookmark>& list);

// A name MEd's format can hold: not empty, no '<', '>', line breaks.
bool validBookmarkName(const std::string& name);

}  // namespace med
