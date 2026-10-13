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

// The lights of the "2dfx" sections, read as MEd reads them (IDE_loadFile 0x00418230, assembly 0x004187c0): a line with
// more than 16 fields is a light; id, x y z (relative to the model), r g b from fields 0-6, the corona size from field 13
// and the flash type from field 16. GTA III's flash types (re3's names): 0 always on, 1 on at night, 2 / 3 flicker,
// 4-9 flash (odd = at night), 10 / 11 random flicker, 12 traffic light.
struct Ide2dfxLight {
    int id = 0;
    float x = 0, y = 0, z = 0;
    unsigned char r = 255, g = 255, b = 255;
    float size = 1;
    int flash = 0;
};
std::vector<Ide2dfxLight> ide2dfxLights(const IdeFile& ide);
// Does MEd draw a light of this flash type at this hour of "Time in scene" (0x00416a70)? Even types before 21:00, odd
// (night) types after 19:00, type 12 always, anything else never.
bool ide2dfxLightShown(int flash, int hour);
std::string writeIde(const IdeFile& ide);

}  // namespace med
