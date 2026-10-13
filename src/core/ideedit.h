// Editing one object definition (an objs or tobj line) inside an IDE file, the way MEd's "Item Definition Entry"
// window does (IDEView, 0x0042ddc0 updateText / 0x00432410 undo / 0x0042d3c0 save). Only that one line changes; every
// other byte of the file stays as it was. No window code: everything here is tested on its own.
#pragma once
#include <string>
#include "ide.h"

namespace med {

// The flag bits of an objs / tobj line as GTA III 1.0 reads them. The game's loaders (0x00477040 objs, 0x004772b0 tobj,
// re/gta3/out/ide_loader.c) copy only bits 1 to 64 into the model info (+0x4a); the field names are re3's
// (CSimpleModelInfo), whose bit layout agrees with that code. MEd shows eleven boxes with Vice City's names; bits 128 to
// 1024 do nothing in GTA III.
struct IdeFlagInfo {
    int bit;
    const char* name;      // the box label: re3's field name, then what it does in GTA III ("drawLast - Draw last, ...")
    const char* medName;   // MEd's label for the same box
    bool usedByGta3;
};
constexpr int kIdeFlagCount = 11;
const IdeFlagInfo& ideFlag(int index);  // 0 .. kIdeFlagCount-1, bit = 1 << index

constexpr double kMaxDrawDistance = 10000;  // Rockstar's largest is 3000 (the island LODs)

// The GTA III / Vice City line of a definition: "id, model, txd, count, distance..., flags[, on, off]". Numbers as the
// shortest exact text ("%.9g": 152.25 stays 152.25, 300 stays 300).
std::string formatIdeEntry(const IdeObject& o);

// Where the objs / tobj line of model `id` is: [begin, end) without its line ending. Sections are read as parseIde
// does. False when the file has no such line (or it does not parse).
struct IdeEntryLoc {
    size_t begin = 0, end = 0;
    bool timed = false;
    IdeObject object;  // the line as parsed
};
bool findIdeEntry(const std::string& text, int id, IdeEntryLoc& loc);

// Is `edited` a valid change of `original`? The id, the model name and the number of draw distances (the model's
// atomics) cannot change; every distance is 0 to kMaxDrawDistance (Rockstar ships one 0: LOD_subroads13 in LANDsw.ide); flags 0 to 2047 (MEd's boxes); a
// texture dictionary name of 1 to 19 characters ("name.txd" in a 23-character archive entry) without spaces or commas;
// tobj hours 0 to 23 (MEd's lists) and not equal (the game would never show it).
bool validIdeEdit(const IdeObject& original, const IdeObject& edited, std::string& why);

// The same definition (times compared only for tobj entries, the only ones that write them).
bool sameIdeEntry(const IdeObject& a, const IdeObject& b);

// Replace the definition of edited.id in `text`. An objs entry that became timed (or the other way round) moves to the
// end of the other section, which is added at the end of the file when the file has none. The file's own line ending is
// used. False with `why` (text unchanged) when the line is missing or the edit is not valid.
bool setIdeEntry(std::string& text, const IdeObject& edited, std::string& why);

}  // namespace med
