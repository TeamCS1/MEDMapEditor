// Which placements belong to the original game and which were added (Duplicate, Paste, New Object, a LOD stand-in
// added by Replace, or any later edit that added a line). Worked out from the placements themselves, so it holds after
// saving and reopening too.
//
// The original GTA III 1.0 map is known only as a fingerprint table (original_gta3.inc, generated from the unmodified
// game by `validate_gta3 <folder> --original-table <file>`): per placement its IPL file name, model id and a 64-bit hash
// of file, id and position (rounded to 0.01). The positions themselves are not stored.
//
// Rules, in this order (every doubtful case comes out as "original", the safe side):
//   1. a placement whose file, id and position match an unused table entry is original;
//   2. of the rest, in file order, as many placements of a model id as the original game has (counted over all files,
//      minus those already matched) are original too: those are moved originals (also moved to another region);
//   3. everything else was added. Copies are always appended after the originals, so they are the ones left over.
//   If none of the loaded IPL file names appears in the table (another game), everything counts as original.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "ipl.h"

namespace med {

struct OriginalEntry {
    const char* file;    // IPL file name, lower case, without folder (e.g. "industse.ipl")
    int id;              // model id
    std::uint64_t hash;  // placementHash(file, id, x, y, z)
};

// FNV-1a over "file|id|x|y|z" with the coordinates rounded to hundredths. `file` is lower-cased and stripped of folders.
std::uint64_t placementHash(const std::string& file, int id, double x, double y, double z);

// "DATA/MAPS/INDUSTSE/INDUSTSE.IPL" -> "industse.ipl"
std::string iplFileKey(const std::string& path);

// One flag per placement: true = original, false = added. fileOf[i] is the IPL path (or key) of placement i.
std::vector<bool> classifyOriginal(const std::vector<IplInstance>& placements, const std::vector<std::string>& fileOf, const OriginalEntry* table,
                                   std::size_t tableSize);

// The GTA III 1.0 table (original_gta3.inc).
const OriginalEntry* gta3OriginalTable(std::size_t& size);

}  // namespace med
