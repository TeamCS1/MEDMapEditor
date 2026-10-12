// GTA III's traffic and pedestrian paths: the IDE "path" data of road pieces, and how full the game's fixed path
// tables get when it builds its path network from the placed pieces at start-up (the "path budget").
//
// The game (gta3.exe 1.0) keeps the whole network in one object (CPathFind, 0x8F6754) with fixed tables: 4930 path
// nodes, 2076 car path links, 10260 connections, 1250 road objects, plus 4000 temporary nodes while loading. Rockstar's
// map uses almost all of it (4923 / 2071 / 10220 / 1213), so a few extra road pieces overflow it and the game crashes
// while loading (seen as 0x00429CAA). simulatePathBudget repeats the game's own algorithm (PreparePathData 0x00429610,
// PreparePathDataForType 0x00429C20; re3 PathFind.cpp) and was checked against the game's own numbers (Project Upstate,
// 2026-10-12: 1995 car nodes, 3000 ped nodes kept, 2102 car links, 10406 connections, 1240 road objects).
#pragma once
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace med {

// One node line of a path block. type: 0 none, 1 connection point (joins another piece's point), 2 path node.
// next: the node this one leads to (an entry index 0-11, -1 for none). Coordinates are the IDE's units / 16.
struct PathEntry {
    int type = 0;
    int next = -1;
    float x = 0, y = 0, z = 0;
};

struct PathModelData {
    std::array<PathEntry, 12> car{}, ped{};
    bool hasCar = false, hasPed = false;
};

// Read every "path" block of an IDE text into `out`. Like the game: the header "car, <id>, <name>" or
// "ped, <id>, <name>" is followed by exactly 12 node lines, and a later block of the same model and kind replaces
// an earlier one.
void collectIdePaths(const std::string& ideText, std::map<int, PathModelData>& out);

// The sizes of the game's path tables.
struct PathTableLimits {
    int nodes = 4930, carLinks = 2076, connections = 10260, objects = 1250, temp = 4000;
    bool plugin = false;  // true when they come from the III.PathLimits plugin's settings
    std::string source;   // that settings file, game-relative
};

// The sizes the III.PathLimits plugin uses for its two settings (the same formulas as the plugin).
PathTableLimits pathLimitsFromSettings(int carPathNodes, int pedPathNodes);

// When the III.PathLimits plugin (the user's own, not part of the editor) is in the game folder, the limits from its
// III.PathLimits.ini (next to gta3.exe, or in scripts/ or plugins/, with the .asi beside it). False: the original
// limits apply.
bool readPathLimitsPlugin(const std::string& gameRoot, PathTableLimits& out);

// A placement as the game loads it, in loading order (the IPL files of gta3.dat, each from top to bottom).
struct PathPlacement {
    int id = 0;
    double x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0, rw = 1;
};

struct PathBudget {
    int objects = 0;          // placed pieces that become road objects (treadables with paths)
    int carNodes = 0;         // car path nodes
    int pedNodesCreated = 0;  // ped path nodes while loading (the table must hold all of them)
    int pedNodesKept = 0;     // after the game removes pedestrian nodes without any connection
    int carLinks = 0;         // car path links
    int connections = 0;      // connections (both kinds)
    int carPoints = 0, pedPoints = 0;  // temporary connection points per pass (table of `temp`)
    int nodesPeak() const { return carNodes + pedNodesCreated; }
};

// The network itself, for drawing it: every node, every link (two node indices), and every connection point
// that found no partner (a dead end where traffic or pedestrians cannot continue). `car` tells the two networks apart.
struct PathNetwork {
    struct Node { float x, y, z; bool car; bool kept; };  // kept: false for pedestrian nodes the game drops (no link)
    struct Link { int a, b; bool car; bool betweenPieces; };
    struct Point { float x, y, z; bool car; };
    std::vector<Node> nodes;
    std::vector<Link> links;
    std::vector<Point> openPoints;
};

// Simulate the game's path preparation. A placement becomes a road object when its model has path data with a
// node or point in entry 0 of the car or ped block (the game's IsPathObject) and is not one of `dynamicIds` (models
// in data/object.dat, which become dynamic objects instead).
PathBudget simulatePathBudget(const std::map<int, PathModelData>& paths, const std::set<int>& dynamicIds, const std::vector<PathPlacement>& placements,
                              PathNetwork* network = nullptr);

// What one piece adds at most: its car and ped nodes and connection points (joints with neighbours share points).
struct PathCost {
    int carNodes = 0, pedNodes = 0, carPoints = 0, pedPoints = 0;
};
PathCost pathCostOf(const PathModelData& model);

}  // namespace med
