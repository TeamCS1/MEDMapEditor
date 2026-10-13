// "Check Map": looks for things in a GTA III install that the game itself rejects, skips or chokes on.
// Every rule here comes from the game's own code (gta3.exe 1.0, see CLAUDE.md "gta3.exe findings") or from
// measurements of the shipped game data; rules that are only experience or heuristics say so in their text.
//
// What the game does that matters here:
//  - It reads a line of at most 255 characters, turns control characters and commas into spaces and skips
//    leading blanks (0x004761d0). Longer lines are cut off.
//  - A placement line is parsed with sscanf("%d %s %f x10") and silently skipped unless all 12 values were
//    converted (0x004786b0). A placement whose model id has no model info is silently skipped too.
//  - The rotation is rebuilt from the quaternion with acos(w) computed as atan2(w, sqrt(1 - w*w)), which is
//    NaN for |w| > 1 and corrupts the entity's matrix.
//  - A model whose texture dictionary is missing falls back to "generic" ("txd has been set to generic").
//  - The game has fixed pools for placements: buildings 5500, treadables 1214, dummies 2802 (0x004a1770).
#pragma once
#include <map>
#include "paths.h"
#include "poollimits.h"
#include <string>
#include <vector>

namespace med {

enum class Severity { Info = 0, Warning = 1, Error = 2 };

struct CheckIssue {
    Severity severity = Severity::Info;
    std::string code;     // short stable id, e.g. "ipl-short-line"
    std::string message;  // what is wrong and what the game does about it
    std::string file;     // game-relative path, empty when not tied to a file
    int line = 0;         // 1-based, 0 when not tied to a line
    bool hasPosition = false;  // a place on the map to jump to
    double x = 0, y = 0, z = 0;
};

struct CheckInput {
    std::string root;  // GTA III folder
    // IPL files as they would be saved, by lower-case game-relative path with '/' (for example
    // "data/maps/industse/industse.ipl"). Files not listed here are read from disk. This lets the check run
    // on unsaved edits.
    std::map<std::string, std::string> iplText;
    // IDE files with unsaved definition edits, the same way (lower-case game-relative path with '/').
    std::map<std::string, std::string> ideText;
    // The pool sizes the game can really have. The editor never blocks going over the vanilla sizes; the check
    // says when a map is over them and only calls it an error when it is over these as well. Vanilla unless a
    // limit adjuster file was found.
    PoolLimits limits;
    // The game's path tables (nodes, car path links, connections, road objects): the original sizes unless the
    // III.PathLimits plugin was found in the game folder (readPathLimitsPlugin; the editor does not ship it).
    PathTableLimits pathLimits;
};

struct CheckSummary {
    int placements = 0, ipl = 0, ide = 0, col = 0, objectDefinitions = 0, modelsPlaced = 0, imgEntries = 0;
    int poolUsed[3] = {0, 0, 0};  // placements per pool: buildings, treadables, dummies (see pools.h)
    int lodWithoutBuilding = 0, buildingWithoutLod = 0, duplicates = 0;      // all of them, including the ones the original game has
    int newLodWithoutBuilding = 0, newBuildingWithoutLod = 0, newDuplicates = 0;  // those the original game does not have
    PathBudget paths;  // how full the game's path tables get while it loads this map
};

struct CheckResult {
    std::vector<CheckIssue> issues;  // errors first, then warnings, then information
    CheckSummary summary;
    // Keys of every LOD without a building, building without a LOD and duplicate placement ("name|x|y|z" with the
    // position in hundredths). Used to generate mapcheck_baseline.inc from the unmodified game.
    std::vector<std::string> lodAloneKeys, buildingAloneKeys, duplicateKeys;
    int count(Severity s) const {
        int n = 0;
        for (const CheckIssue& i : issues) n += i.severity == s;
        return n;
    }
};

// What the game reads from one placement line.
struct GamePlacement {
    int id = 0;
    std::string name;
    double x = 0, y = 0, z = 0, sx = 1, sy = 1, sz = 1, rx = 0, ry = 0, rz = 0, rw = 1;
};

// Parse a placement line the way the game does. Returns how many of the 12 values were converted (12 means
// the game accepts the line). `lineTruncated` is set when the line is longer than the game's 255 characters.
int gameParsePlacement(const std::string& rawLine, GamePlacement& out, bool* lineTruncated = nullptr);

// Reference numbers measured on the unmodified GTA III 1.0 data. Having MORE than these is the sign of an
// edit that broke something; the original game itself has this many.
constexpr int kOriginalLodWithoutBuilding = 221;  // LOD placements with no building of the same name tail within 5 units
constexpr int kOriginalBuildingWithoutLod = 42;   // buildings whose LOD model exists but no LOD placed
constexpr int kOriginalDuplicates = 6;            // placements with the same id and position as another
constexpr int kPlacementPoolCapacity = 5500 + 1214 + 2802;  // buildings + treadables + dummies
// The editor's Delete (move below the map) puts placements at z = -200; at or below this depth a placement counts as
// deleted for the LOD pairing checks (the original map has nothing below -100).
constexpr double kDeletedDepth = -199.0;

CheckResult checkMap(const CheckInput& input);

// The result as plain text, one block per issue, for the "Copy report" button and the command line.
std::string checkReportText(const CheckResult& result);

}  // namespace med
