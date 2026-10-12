// IPL (item placement) files: text and binary ("bnry") formats.
#pragma once
#include <string>
#include <vector>
#include "gtatypes.h"

namespace med {

// One placed object. MEd keeps all numbers as double.
struct IplInstance {
    int id = 0;
    std::string model;                 // empty for San Andreas (model comes from the IDE)
    int interior = 0;                  // VC / SA
    double x = 0, y = 0, z = 0;
    double scaleX = 1, scaleY = 1, scaleZ = 1;  // GTA III / VC only
    double rotX = 0, rotY = 0, rotZ = 0, rotW = 1;
    short lod = kNoLod;                // SA only; index of the LOD instance, 0xFFFF if none
};

// A section other than "inst" (cull, zone, pick, path, ...) is kept verbatim so
// that saving does not lose data MEd cannot edit.
struct IplRawSection {
    std::string name;
    std::vector<std::string> lines;
};

struct IplFile {
    GameVersion version = GameVersion::GTA3;
    std::vector<IplInstance> instances;
    std::vector<IplRawSection> otherSections;
    std::vector<std::string> warnings;   // lines that were skipped, with line numbers
};

IplFile parseTextIpl(const std::string& text, GameVersion version);
std::string writeTextIpl(const IplFile& ipl);

// Remove one instance from the instance list of a single IPL file, as MEd does (IPL file remove,
// 0x00424490). The list entry is really erased, and the LOD links between the instances of that file
// are repaired: an instance whose LOD pointed at the removed one gets "no LOD" (0xFFFF), and every
// LOD index above the removed position moves down by one. Returns false for a bad index.
bool removeIplInstance(std::vector<IplInstance>& fileInstances, size_t index);

// GTA III and Vice City pair a building with its LOD stand-in by name and position: a LOD placement is
// named "LOD" + X and its building "<three letters>" + X (for example LODSEbuilding1 and comSEbuilding1,
// the same tail after the first three characters), and the two sit at the same spot. Deleting only one of
// them is what crashes the game (see CLAUDE.md). Of the 968 LOD placements in GTA III 1.0, 759 have such a
// partner, 737 of them within 1 unit and all but 12 within 5. Returns the indices (nearest first) of the
// placements in `all` that are the partner(s) of `all[index]`: the opposite kind (LOD or building), same
// tail, within `maxDistance` units. Empty when there is none.
std::vector<size_t> findLodPartners(const std::vector<IplInstance>& all, size_t index, double maxDistance = 5.0);

// One "inst" line (no line ending), in the format MEd writes (IPL save, 0x00423080): fields
// separated by ", ", numbers as QString::number(x, 'g', 12) i.e. %.12g.
std::string formatIplInstanceLine(const IplInstance& inst, GameVersion version);

// Replace the "inst" block of an existing IPL file, the way MEd saves (IPL save, 0x00423730):
// everything up to and including the "inst" line is kept byte for byte, then one line per
// instance, then everything from the first following "end" line to the end of the file is kept
// byte for byte (so cull, zone, pick and other sections survive). The line ending of the "inst"
// line (CRLF or LF) is used for the new lines. Returns false with `error` if the file has no
// inst block.
bool replaceIplInstSection(const std::string& originalText, const std::vector<IplInstance>& instances,
                           GameVersion version, std::string& result, std::string& error);

// Binary IPL (San Andreas): "bnry", then counts, then 40-byte instances.
// Only the instance table is decoded; the rest is not supported yet.
bool isBinaryIpl(const std::vector<unsigned char>& data);
IplFile parseBinaryIpl(const std::vector<unsigned char>& data);
std::vector<unsigned char> writeBinaryIpl(const IplFile& ipl);

}  // namespace med
