// Placement pool limits of a game install: the vanilla ones, or the ones set by the III.VC.SA Limit Adjuster
// (https://github.com/GTAmodding/III.VC.SA.LimitAdjuster), whose settings file is read, never written.
//
// Its GTA III settings are in the [GTA3LIMITS] section as "Key = value" lines; ';' starts a comment and the
// value "unlimited" removes the cap. The names that matter here are Buildings, Treadables and Dummys (the
// game's three placement pools, see pools.h). Names the adjuster has but this editor does not use are ignored.
// Only the text format was checked (from the adjuster's example .ini); how the adjuster behaves in the game was not.
#pragma once
#include <string>
#include "pools.h"

namespace med {

constexpr long long kUnlimited = 2000000000LL;

struct PoolLimits {
    long long cap[kPoolCount] = {kPoolCapacity[0], kPoolCapacity[1], kPoolCapacity[2]};
    bool adjusted = false;   // true when the numbers come from a limit adjuster file
    std::string source;      // that file, game-relative, empty for vanilla
    long long total() const { return cap[0] + cap[1] + cap[2]; }
};

// Read the GTA III limits from the text of a limit adjuster .ini. Returns false when it has no [GTA3LIMITS] section.
// Settings the file does not mention keep their vanilla value (the adjuster leaves those alone too).
bool parseLimitAdjusterIni(const std::string& text, PoolLimits& out);

// Look for the adjuster's settings file in a game folder (next to gta3.exe, then in scripts/ and plugins/).
// Returns the game-relative path, or an empty string when there is none.
std::string findLimitAdjusterIni(const std::string& gameRoot);

// "Vanilla" or "Limit adjuster (<file>)", for the window and the reports.
std::string describeLimits(const PoolLimits& l);

}  // namespace med
