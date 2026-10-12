// Tests for core/paths.h: reading IDE path blocks, the path budget simulation and the III.PathLimits settings.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "paths.h"

using namespace med;

static int failures = 0;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) {                                                    \
            ++failures;                                                \
            std::printf("FAIL line %d: %s\n", __LINE__, #c);           \
        }                                                              \
    } while (0)

// A 50-long straight like rd_Road2A50 (its real path data from generic.ide): one car node in the middle with two
// connection points at the ends; two ped nodes at the sides with four points.
static const char* kStraight =
    "path\n"
    "ped, 100, straight\n"
    "\t2, -1, 0, -200, 0, 2, 160, 1, 1\n"
    "\t2, -1, 0, 200, 0, 2, 160, 1, 1\n"
    "\t1, 0, 0, -200, 400, 2, 160, 1, 1\n"
    "\t1, 0, 0, -200, -400, 2, 160, 1, 1\n"
    "\t1, 1, 0, 200, -400, 2, 160, 1, 1\n"
    "\t1, 1, 0, 200, 400, 2, 160, 1, 1\n"
    "\t0, -1, 0, 0, 0, 0, 0, 1, 1\n\t0, -1, 0, 0, 0, 0, 0, 1, 1\n\t0, -1, 0, 0, 0, 0, 0, 1, 1\n"
    "\t0, -1, 0, 0, 0, 0, 0, 1, 1\n\t0, -1, 0, 0, 0, 0, 0, 1, 1\n\t0, -1, 0, 0, 0, 0, 0, 1, 1\n"
    "car, 100, straight\n"
    "\t2, -1, 0, 0, 0, 0, 160, 1, 1\n"
    "\t1, 0, 0, 0, -400, 0, 160, 2, 2\n"
    "\t1, 0, 0, 0, 400, 0, 160, 2, 2\n"
    "\t0, -1, 0, 0, 0, 0, 0, 1, 1\n\t0, -1, 0, 0, 0, 0, 0, 1, 1\n\t0, -1, 0, 0, 0, 0, 0, 1, 1\n"
    "\t0, -1, 0, 0, 0, 0, 0, 1, 1\n\t0, -1, 0, 0, 0, 0, 0, 1, 1\n\t0, -1, 0, 0, 0, 0, 0, 1, 1\n"
    "\t0, -1, 0, 0, 0, 0, 0, 1, 1\n\t0, -1, 0, 0, 0, 0, 0, 1, 1\n\t0, -1, 0, 0, 0, 0, 0, 1, 1\n"
    "end\n";

static std::string block(const char* kind, int id, const std::string& lines12) { return std::string(kind) + ", " + std::to_string(id) + ", m\n" + lines12; }
static std::string zeros(int n) {
    std::string s;
    for (int i = 0; i < n; ++i) s += "0, -1, 0, 0, 0, 0, 0, 1, 1\n";
    return s;
}

int main() {
    // ---- reading path blocks ----
    std::map<int, PathModelData> paths;
    collectIdePaths(std::string("objs\n100, straight, generic, 1, 220, 0\nend\n") + kStraight, paths);
    CHECK(paths.size() == 1 && paths.count(100));
    const PathModelData& s = paths[100];
    CHECK(s.hasCar && s.hasPed && s.car[0].type == 2 && s.car[1].type == 1 && s.car[2].next == 0);
    CHECK(std::fabs(s.car[2].y - 25.0f) < 1e-6f && std::fabs(s.ped[0].x + 12.5f) < 1e-6f && std::fabs(s.ped[0].z - 0.125f) < 1e-6f);  // IDE units / 16
    CHECK(s.car[3].type == 0 && s.ped[11].type == 0);
    {
        const PathCost c = pathCostOf(s);
        CHECK(c.carNodes == 1 && c.carPoints == 2 && c.pedNodes == 2 && c.pedPoints == 4);
    }
    {  // a later block of the same model and kind replaces the earlier one (Rockstar's IDE files have such copies)
        std::map<int, PathModelData> p2;
        collectIdePaths("path\n" + block("car", 7, "2, -1, 0, 0, 0, 0, 0, 1, 1\n" + zeros(11)) + block("car", 7, "1, -1, 0, 0, 0, 0, 0, 1, 1\n" + zeros(11)) + "end\n", p2);
        CHECK(p2[7].car[0].type == 1);
        // the header word is compared case-sensitively by the game: "Car" is not a car block
        std::map<int, PathModelData> p3;
        collectIdePaths("path\n" + block("Car", 8, "2, -1, 0, 0, 0, 0, 0, 1, 1\n" + zeros(11)) + "end\n", p3);
        CHECK(!p3.count(8) || (!p3[8].hasCar && !p3[8].hasPed));
        // comments and blank lines do not count as node lines; exactly 12 node lines per block
        std::map<int, PathModelData> p4;
        collectIdePaths("# x\npath\n\nped, 9, m\n# comment\n2, -1, 0, 16, 0, 0, 0, 1, 1\n\n" + zeros(11) + "car, 9, m\n2, -1, 0, 0, 0, 0, 0, 1, 1\n" + zeros(11) + "end\n", p4);
        CHECK(p4[9].ped[0].type == 2 && std::fabs(p4[9].ped[0].x - 1.0f) < 1e-6f && p4[9].car[0].type == 2);
    }

    // ---- the budget ----
    // two straights end to end along y (50 apart): their middle car points meet, both ped sides meet
    std::vector<PathPlacement> two = {{100, 0, 0, 0, 0, 0, 0, 1}, {100, 0, 50, 0, 0, 0, 0, 1}};
    PathBudget b = simulatePathBudget(paths, {}, two);
    CHECK(b.objects == 2);
    CHECK(b.carNodes == 2 && b.carPoints == 3);    // 4 car points, one pair joins
    CHECK(b.carLinks == 1);                        // the joint links the two car nodes
    CHECK(b.pedNodesCreated == 4 && b.pedPoints == 6);
    CHECK(b.pedNodesKept == 4);                    // every ped node has a joint
    CHECK(b.connections == 2 * 1 + 2 * 2);         // a joint is a connection at each end
    CHECK(b.nodesPeak() == 6);
    {  // the network for drawing: the same counts, the joint as a link between pieces, the outer ends open
        PathNetwork net;
        const PathBudget bn = simulatePathBudget(paths, {}, two, &net);
        CHECK(bn.carLinks == b.carLinks && bn.connections == b.connections);
        int carNodes = 0, pedNodes = 0, carLinks = 0, pedLinks = 0, openCar = 0, openPed = 0;
        for (const PathNetwork::Node& n : net.nodes) (n.car ? carNodes : pedNodes) += 1;
        for (const PathNetwork::Link& l : net.links) {
            (l.car ? carLinks : pedLinks) += 1;
            CHECK(l.betweenPieces && l.a >= 0 && l.b >= 0 && l.a < int(net.nodes.size()) && l.b < int(net.nodes.size()) && net.nodes[size_t(l.a)].car == l.car);
        }
        for (const PathNetwork::Point& p : net.openPoints) (p.car ? openCar : openPed) += 1;
        CHECK(carNodes == 2 && pedNodes == 4 && carLinks == 1 && pedLinks == 2 && openCar == 2 && openPed == 4);
        for (const PathNetwork::Point& p : net.openPoints)
            if (p.car) CHECK(std::fabs(p.y + 25.0f) < 0.01f || std::fabs(p.y - 75.0f) < 0.01f);  // y -25 and y 75: the outer ends
        PathNetwork na;
        const std::vector<PathPlacement> far = {{100, 0, 0, 0, 0, 0, 0, 1}, {100, 0, 51.5, 0, 0, 0, 0, 1}};
        simulatePathBudget(paths, {}, far, &na);
        int dropped = 0;
        for (const PathNetwork::Node& n : na.nodes) dropped += !n.kept;
        CHECK(dropped == 4 && na.links.empty() && na.openPoints.size() == 12);  // the game drops the 4 lonely ped nodes
    }
    // the second turned half round: its ends are swapped but it still joins (the points are symmetric)
    std::vector<PathPlacement> turned = {{100, 0, 0, 0, 0, 0, 0, 1}, {100, 0, 50, 0, 0, 0, 1, 0}};
    PathBudget bt = simulatePathBudget(paths, {}, turned);
    CHECK(bt.carLinks == 1 && bt.pedNodesKept == 4);
    // apart (more than 1.0 in x or y): nothing joins, the ped nodes are dropped after loading, the car nodes stay
    std::vector<PathPlacement> apart = {{100, 0, 0, 0, 0, 0, 0, 1}, {100, 0, 51.5, 0, 0, 0, 0, 1}};
    PathBudget ba = simulatePathBudget(paths, {}, apart);
    CHECK(ba.carLinks == 0 && ba.carPoints == 4 && ba.pedNodesCreated == 4 && ba.pedNodesKept == 0 && ba.connections == 0);
    // within 1.0 still joins (the game's test is |dx| < 1 and |dy| < 1)
    std::vector<PathPlacement> close = {{100, 0, 0, 0, 0, 0, 0, 1}, {100, 0.5, 50.9, 0, 0, 0, 0, 1}};
    CHECK(simulatePathBudget(paths, {}, close).carLinks == 1);
    // a model listed in object.dat becomes a dynamic object, not a road object
    CHECK(simulatePathBudget(paths, {100}, two).objects == 0);
    // unknown models and models without paths are ignored
    std::vector<PathPlacement> other = {{555, 0, 0, 0, 0, 0, 0, 1}};
    CHECK(simulatePathBudget(paths, {}, other).objects == 0);
    // the game's IsPathObject looks at entry 0 only: data that starts with an empty entry does not count
    {
        std::map<int, PathModelData> p5;
        collectIdePaths("path\n" + block("car", 11, zeros(1) + "2, -1, 0, 0, 0, 0, 0, 1, 1\n" + zeros(10)) + "end\n", p5);
        std::vector<PathPlacement> one = {{11, 0, 0, 0, 0, 0, 0, 1}};
        CHECK(simulatePathBudget(p5, {}, one).objects == 0);
    }
    // links inside a piece: a node whose "next" is another node of the same piece; a pair named both ways is one link
    {
        std::map<int, PathModelData> p6;
        collectIdePaths("path\n" + block("car", 12, "2, 1, 0, 0, 0, 0, 0, 1, 1\n2, 0, 0, 0, 160, 0, 0, 1, 1\n" + zeros(10)) + "end\n", p6);
        std::vector<PathPlacement> one = {{12, 0, 0, 0, 0, 0, 0, 1}};
        const PathBudget bi = simulatePathBudget(p6, {}, one);
        CHECK(bi.carNodes == 2 && bi.carLinks == 1 && bi.connections == 2);
    }
    // a connection point with next -1 belongs to the node whose "next" names it (0x00429C20)
    {
        std::map<int, PathModelData> p7;
        collectIdePaths("path\n" + block("ped", 13, "2, 1, 0, 0, 0, 0, 0, 1, 1\n1, -1, 0, 0, 400, 0, 0, 1, 1\n" + zeros(10)) + "end\n", p7);
        std::vector<PathPlacement> pair = {{13, 0, 0, 0, 0, 0, 0, 1}, {13, 0, 50, 0, 0, 0, 1, 0}};  // the second turned: the points meet at y 25
        const PathBudget bn = simulatePathBudget(p7, {}, pair);
        CHECK(bn.pedNodesCreated == 2 && bn.pedNodesKept == 2);
    }

    // ---- the III.PathLimits plugin's settings ----
    const PathTableLimits def = pathLimitsFromSettings(6000, 8000);
    CHECK(def.nodes == 14002 && def.carLinks == 7300 && def.connections == 32204 && def.objects == 4667 && def.temp == 14002 && def.plugin);  // the plugin's own log
    const PathTableLimits low = pathLimitsFromSettings(0, 0);
    CHECK(low.nodes == 4930 && low.carLinks == 2076 && low.connections == 11339 && low.objects == 1643 && low.temp == 4930);  // never below the game's sizes
    const PathTableLimits high = pathLimitsFromSettings(100000, 100000);
    CHECK(high.nodes == 32762 && high.carLinks == 32767 && high.connections == 32767 && (high.nodes - 2) % 8 == 0);
    for (int car : {0, 1999, 4000, 6000, 12345})
        for (int ped : {0, 3001, 8000, 20000}) {
            const PathTableLimits l = pathLimitsFromSettings(car, ped);
            CHECK((l.nodes - 2) % 8 == 0 && l.nodes >= 4930 && l.nodes <= 32762 && l.connections <= 32767 && l.carLinks <= 32767);
        }
    const PathTableLimits orig;
    CHECK(orig.nodes == 4930 && orig.carLinks == 2076 && orig.connections == 10260 && orig.objects == 1250 && orig.temp == 4000 && !orig.plugin);

    // ---- finding the plugin in a game folder ----
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "med_paths_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "scripts");
    PathTableLimits found;
    CHECK(!readPathLimitsPlugin(root.string(), found));  // nothing there
    std::ofstream(root / "III.PathLimits.ini") << "[PathLimits]\nCarPathNodes = 3000\nPedPathNodes = 4000\n";
    CHECK(!readPathLimitsPlugin(root.string(), found));  // an .ini without the plugin does nothing
    std::ofstream(root / "III.PathLimits.asi") << "x";
    CHECK(readPathLimitsPlugin(root.string(), found) && found.plugin && found.source == "III.PathLimits.ini");
    CHECK(found.nodes == pathLimitsFromSettings(3000, 4000).nodes && found.carLinks == pathLimitsFromSettings(3000, 4000).carLinks);
    std::ofstream(root / "III.PathLimits.ini") << "; comment\n[Other]\nCarPathNodes = 1\n[pathlimits]\n  carpathnodes=9000 ; note\nPedPathNodes = 9000\n";
    CHECK(readPathLimitsPlugin(root.string(), found) && found.nodes == pathLimitsFromSettings(9000, 9000).nodes);  // keys and section in any case
    fs::remove(root / "III.PathLimits.ini");
    CHECK(readPathLimitsPlugin(root.string(), found) && found.nodes == 14002);  // the plugin's defaults without its .ini
    fs::remove(root / "III.PathLimits.asi");
    std::ofstream(root / "scripts" / "III.PathLimits.asi") << "x";
    std::ofstream(root / "scripts" / "III.PathLimits.ini") << "[PathLimits]\nCarPathNodes = 2500\nPedPathNodes = 3500\n";
    CHECK(readPathLimitsPlugin(root.string(), found) && found.source == "scripts/III.PathLimits.ini" && found.nodes == pathLimitsFromSettings(2500, 3500).nodes);
    fs::remove_all(root, ec);

    if (failures == 0) std::printf("all path tests passed\n");
    return failures ? 1 : 0;
}
