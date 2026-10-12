// IDE (item definition) files. MEd edits the objs and tobj sections; all other sections
// (cars, peds, 2dfx, path, ...) are preserved verbatim.
#pragma once
#include <string>
#include <vector>
#include "gtatypes.h"

namespace med {

struct IdeObject {
    int id = 0;
    std::string model;                 // DFF name without extension
    std::string txd;                   // TXD name without extension
    std::vector<double> drawDistances; // GTA III/VC: one per mesh. San Andreas: exactly one
    int flags = 0;
    bool timed = false;                // true for tobj entries
    int timeOn = 0, timeOff = 24;      // tobj only
};

struct IdeRawSection {
    std::string name;
    std::vector<std::string> lines;
};

struct IdeFile {
    GameVersion version = GameVersion::GTA3;
    std::vector<IdeObject> objects;           // objs and tobj entries, in file order
    std::vector<IdeRawSection> otherSections;
    std::vector<std::string> warnings;
};

IdeFile parseIde(const std::string& text, GameVersion version);
std::string writeIde(const IdeFile& ide);

}  // namespace med
