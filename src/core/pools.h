// The game's placement pools (CPools::Initialise, gta3.exe 0x004a1770) and the rule that sorts a placement into one.
//
// CFileLoader::LoadObjectInstance (0x004786b0) creates, per placement line:
//   - a dummy (pool of 2802) when the model is listed in data/object.dat (OBJECT.DAT loader 0x004bc0e0 gives those
//     models an object index in the model info at +0x24; every other model has -1 there),
//   - otherwise a treadable building (pool of 1214) when the model has ped or car path data (a "path" block for
//     its id in an IDE file),
//   - otherwise a plain building (pool of 5500).
// Checked on the shipped GTA III 1.0 data: 5140 buildings, 1213 treadables (of 1214!), 2336 dummies.
#pragma once
#include <set>
#include <string>
#include "ide.h"

namespace med {

enum class Pool { Building = 0, Treadable = 1, Dummy = 2 };
constexpr int kPoolCount = 3;
constexpr int kPoolCapacity[kPoolCount] = {5500, 1214, 2802};
const char* poolName(Pool p);

// The model names listed in object.dat (lower case). Lines are "name, mass, ..." with ';' comments, ended by '*'.
std::set<std::string> parseObjectDatNames(const std::string& text);

// The model ids that have path data: the header lines "ped, <id>, <name>" or "car, <id>, <name>" in an IDE "path" section.
void collectPathModelIds(const IdeFile& ide, std::set<int>& ids);

inline Pool poolFor(bool listedInObjectDat, bool hasPathData) {
    return listedInObjectDat ? Pool::Dummy : hasPathData ? Pool::Treadable : Pool::Building;
}

}  // namespace med
