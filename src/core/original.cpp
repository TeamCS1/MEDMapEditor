#include "original.h"
#include <cmath>
#include <map>
#include <set>
#include <unordered_map>
#include "textutil.h"

namespace med {

namespace {
#include "original_gta3.inc"
}  // namespace

std::string iplFileKey(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return toLower(slash == std::string::npos ? path : path.substr(slash + 1));
}

std::uint64_t placementHash(const std::string& file, int id, double x, double y, double z) {
    const std::string s = iplFileKey(file) + "|" + std::to_string(id) + "|" + std::to_string(std::llround(x * 100)) + "|" + std::to_string(std::llround(y * 100)) + "|" +
                          std::to_string(std::llround(z * 100));
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

std::vector<bool> classifyOriginal(const std::vector<IplInstance>& placements, const std::vector<std::string>& fileOf, const OriginalEntry* table,
                                   std::size_t tableSize) {
    std::vector<bool> original(placements.size(), true);
    if (!table || tableSize == 0 || fileOf.size() != placements.size()) return original;
    std::set<std::string> tableFiles;
    std::unordered_map<std::uint64_t, int> unused;  // hash -> table entries not matched yet
    std::map<int, int> idLeft;                      // model id -> original placements not accounted for yet
    for (std::size_t i = 0; i < tableSize; ++i) {
        tableFiles.insert(table[i].file);
        ++unused[table[i].hash];
        ++idLeft[table[i].id];
    }
    std::vector<std::string> keys(placements.size());
    bool known = false;
    for (std::size_t i = 0; i < placements.size(); ++i) {
        keys[i] = iplFileKey(fileOf[i]);
        known = known || tableFiles.count(keys[i]) > 0;
    }
    if (!known) return original;  // not the game the table is for: treat everything as original (the safe side)
    std::vector<bool> matched(placements.size(), false);
    for (std::size_t i = 0; i < placements.size(); ++i) {  // 1. exact matches
        const IplInstance& p = placements[i];
        auto u = unused.find(placementHash(keys[i], p.id, p.x, p.y, p.z));
        auto l = idLeft.find(p.id);
        if (u != unused.end() && u->second > 0 && l != idLeft.end() && l->second > 0) {
            --u->second;
            --l->second;
            matched[i] = true;
        }
    }
    for (std::size_t i = 0; i < placements.size(); ++i) {  // 2. moved originals, 3. the rest was added
        if (matched[i]) continue;
        auto l = idLeft.find(placements[i].id);
        if (l != idLeft.end() && l->second > 0) --l->second;
        else original[i] = false;
    }
    return original;
}

const OriginalEntry* gta3OriginalTable(std::size_t& size) {
    size = sizeof(kOriginalGta3) / sizeof(kOriginalGta3[0]);
    if (size > 0 && kOriginalGta3[size - 1].file == nullptr) --size;  // terminator
    return kOriginalGta3;
}

}  // namespace med
