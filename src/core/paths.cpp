#include "paths.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "roadkit.h"
#include "textutil.h"

namespace med {

namespace fs = std::filesystem;

namespace {

std::vector<std::string> gameFields(const std::string& line) {
    std::vector<std::string> f;
    std::string cur;
    for (char c : line) {
        if (c == ',' || c == ' ' || c == '\t') {  // the game's line reader turns commas into spaces
            if (!cur.empty()) f.push_back(cur), cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) f.push_back(cur);
    return f;
}

bool readText(const fs::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::stringstream s;
    s << f.rdbuf();
    out = s.str();
    return true;
}

}  // namespace

void collectIdePaths(const std::string& ideText, std::map<int, PathModelData>& out) {
    std::istringstream in(ideText);
    std::string raw, section;
    PathModelData* cur = nullptr;
    bool isPed = false;
    int index = -1;  // node lines still to read for the current header
    while (std::getline(in, raw)) {
        std::string l = trim(raw.substr(0, raw.find('#')));
        if (l.empty()) continue;
        if (section.empty()) {
            section = toLower(l.substr(0, 4));
            continue;
        }
        if (toLower(l.substr(0, 3)) == "end" && index < 0) {
            section.clear();
            continue;
        }
        if (section != "path") continue;
        const std::vector<std::string> f = gameFields(l);
        if (f.empty()) continue;
        if (index < 0) {  // a header: "ped, id, name" or "car, id, name" (the game compares the word case-sensitively)
            if (f.size() < 2 || (f[0] != "ped" && f[0] != "car")) { cur = nullptr; index = 0; continue; }
            isPed = f[0] == "ped";
            cur = &out[std::atoi(f[1].c_str())];
            (isPed ? cur->hasPed : cur->hasCar) = true;
            index = 0;
            continue;
        }
        if (cur && f.size() >= 6) {
            PathEntry& e = (isPed ? cur->ped : cur->car)[size_t(index)];
            e.type = std::atoi(f[0].c_str());
            e.next = std::atoi(f[1].c_str());
            e.x = float(std::atof(f[3].c_str()) / 16.0);
            e.y = float(std::atof(f[4].c_str()) / 16.0);
            e.z = float(std::atof(f[5].c_str()) / 16.0);
        }
        if (++index == 12) index = -1;
    }
}

PathTableLimits pathLimitsFromSettings(int car, int ped) {
    auto clampi = [](int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; };
    PathTableLimits l;
    int nodes = clampi(car, 1964, 32767) + clampi(ped, 2959, 32767);
    nodes = clampi(nodes, 4930, 32762);
    nodes = ((nodes - 2 + 7) / 8) * 8 + 2;  // the game's clearing loops: 8 at a time plus 2
    if (nodes > 32762) nodes = 32762;
    l.nodes = nodes;
    l.carLinks = clampi(car * 6 / 5 + 100, 2076, 32767);
    l.connections = clampi(nodes * 23 / 10, 10260, 32767);
    l.objects = clampi(nodes / 3, 1250, 32767);
    l.temp = clampi(nodes, 4000, 32767);
    l.plugin = true;
    return l;
}

bool readPathLimitsPlugin(const std::string& gameRoot, PathTableLimits& out) {
    for (const char* dir : {"", "scripts/", "plugins/"}) {
        std::error_code ec;
        const fs::path base = fs::path(gameRoot) / dir;
        if (!fs::exists(base / "III.PathLimits.asi", ec)) continue;
        // its settings: [PathLimits] CarPathNodes / PedPathNodes, the plugin's defaults when missing
        int car = 6000, ped = 8000;
        std::string text;
        if (readText(base / "III.PathLimits.ini", text)) {
            std::istringstream in(text);
            std::string raw, section;
            while (std::getline(in, raw)) {
                std::string l = trim(raw.substr(0, raw.find(';')));
                if (l.empty()) continue;
                if (l[0] == '[') { section = toLower(l); continue; }
                if (section != "[pathlimits]") continue;
                const size_t eq = l.find('=');
                if (eq == std::string::npos) continue;
                const std::string key = toLower(trim(l.substr(0, eq)));
                const int v = std::atoi(trim(l.substr(eq + 1)).c_str());
                if (key == "carpathnodes") car = v;
                if (key == "pedpathnodes") ped = v;
            }
        }
        out = pathLimitsFromSettings(car, ped);
        out.source = std::string(dir) + "III.PathLimits.ini";
        return true;
    }
    return false;
}

PathCost pathCostOf(const PathModelData& m) {
    PathCost c;
    for (const PathEntry& e : m.car) { c.carNodes += e.type == 2; c.carPoints += e.type == 1; }
    for (const PathEntry& e : m.ped) { c.pedNodes += e.type == 2; c.pedPoints += e.type == 1; }
    return c;
}

PathBudget simulatePathBudget(const std::map<int, PathModelData>& paths, const std::set<int>& dynamicIds, const std::vector<PathPlacement>& placements,
                              PathNetwork* network) {
    PathBudget b;
    if (network) *network = PathNetwork();
    // the road objects (CPathFind::RegisterMapObject), in loading order
    std::vector<const PathPlacement*> objs;
    std::vector<const PathModelData*> objModel;
    for (const PathPlacement& p : placements) {
        auto f = paths.find(p.id);
        if (f == paths.end() || dynamicIds.count(p.id)) continue;
        if (f->second.car[0].type == 0 && f->second.ped[0].type == 0) continue;  // IsPathObject looks at entry 0
        objs.push_back(&p);
        objModel.push_back(&f->second);
    }
    b.objects = int(objs.size());
    auto world = [](const PathPlacement& p, const PathEntry& e, double w[3]) {
        IplInstance o;
        o.x = p.x; o.y = p.y; o.z = p.z;
        o.rotX = p.rx; o.rotY = p.ry; o.rotZ = p.rz; o.rotW = p.rw;
        placementToWorld(o, e.x, e.y, e.z, w[0], w[1], w[2]);
    };
    for (int pass = 0; pass < 2; ++pass) {  // cars, then peds (0x00429610)
        struct Node { double x, y, z; };
        std::vector<Node> nodes;
        std::vector<std::array<int, 12>> objNodes(objs.size());
        for (size_t i = 0; i < objs.size(); ++i) {
            objNodes[i].fill(-1);
            const auto& es = pass == 0 ? objModel[i]->car : objModel[i]->ped;
            for (int k = 0; k < 12; ++k)
                if (es[size_t(k)].type == 2) {
                    double w[3];
                    world(*objs[i], es[size_t(k)], w);
                    objNodes[i][size_t(k)] = int(nodes.size());
                    nodes.push_back({w[0], w[1], w[2]});
                }
        }
        // connection points: each joins the nearest unmatched point within 1.0 (in x and in y), else waits for one
        struct Point { double x, y, z; int a, b; bool matched; };
        std::vector<Point> points;
        for (size_t i = 0; i < objs.size(); ++i) {
            const auto& es = pass == 0 ? objModel[i]->car : objModel[i]->ped;
            for (int k = 0; k < 12; ++k) {
                if (es[size_t(k)].type != 1) continue;
                double w[3];
                world(*objs[i], es[size_t(k)], w);
                int best = -1;
                double bestd = 1.0;
                for (size_t d = 0; d < points.size(); ++d) {
                    if (points[d].matched) continue;
                    const double dx = std::fabs(points[d].x - w[0]), dy = std::fabs(points[d].y - w[1]);
                    if (dx < bestd && dy < bestd) { best = int(d); bestd = std::max(dx, dy); }
                }
                // the node it belongs to: its "next", or when that is -1 the entry whose "next" is this point
                int nk = es[size_t(k)].next;
                if (nk < 0) {
                    nk = 0;
                    while (nk < 12 && es[size_t(nk)].next != k) ++nk;
                }
                const int node = nk >= 0 && nk < 12 ? objNodes[i][size_t(nk)] : -1;
                if (best < 0) points.push_back({w[0], w[1], w[2], node, -1, false});
                else { points[size_t(best)].b = node; points[size_t(best)].matched = true; }
            }
        }
        (pass == 0 ? b.carPoints : b.pedPoints) = int(points.size());
        // connections and car path links: matched points join two pieces' nodes, "next" joins nodes inside a piece
        std::vector<int> nodeLinks(nodes.size(), 0);
        std::set<std::array<long long, 4>> links;
        const int netBase = network ? int(network->nodes.size()) : 0;
        if (network)
            for (const Node& n : nodes) network->nodes.push_back({float(n.x), float(n.y), float(n.z), pass == 0, true});
        auto join = [&](int a, int c, bool between) {
            if (a < 0 || c < 0) return;
            if (network) network->links.push_back({netBase + a, netBase + c, pass == 0, between});
            ++nodeLinks[size_t(a)];
            ++nodeLinks[size_t(c)];
            b.connections += 2;
            if (pass != 0) return;
            const Node &na = nodes[size_t(a)], &nc = nodes[size_t(c)];
            double dx = nc.x - na.x, dy = nc.y - na.y;
            const double len = std::sqrt(dx * dx + dy * dy);
            if (len > 0) { dx /= len; dy /= len; }
            if (dx < 0 || (dx == 0 && dy < 0)) { dx = -dx; dy = -dy; }  // one link for both directions
            links.insert({std::llround((na.x + nc.x) * 500), std::llround((na.y + nc.y) * 500), std::llround(dx * 1e5), std::llround(dy * 1e5)});
        };
        for (const Point& p : points) {
            if (p.matched) join(p.a, p.b, true);
            else if (network) network->openPoints.push_back({float(p.x), float(p.y), float(p.z), pass == 0});
        }
        for (size_t i = 0; i < objs.size(); ++i) {
            const auto& es = pass == 0 ? objModel[i]->car : objModel[i]->ped;
            std::set<std::pair<int, int>> pairs;  // a pair named from both ends is one link
            for (int k = 0; k < 12; ++k) {
                const int n = es[size_t(k)].next;
                if (es[size_t(k)].type != 2 || n < 0 || n >= 12 || objNodes[i][size_t(n)] < 0) continue;
                const int a = objNodes[i][size_t(k)], c = objNodes[i][size_t(n)];
                if (a == c || !pairs.insert({std::min(a, c), std::max(a, c)}).second) continue;
                join(a, c, false);
            }
        }
        if (pass == 0) {
            b.carNodes = int(nodes.size());
            b.carLinks = int(links.size());
        } else {
            b.pedNodesCreated = int(nodes.size());
            int isolated = 0;
            for (size_t i = 0; i < nodeLinks.size(); ++i)
                if (nodeLinks[i] == 0) {
                    ++isolated;
                    if (network) network->nodes[size_t(netBase) + i].kept = false;
                }
            b.pedNodesKept = b.pedNodesCreated - isolated;  // "Remove isolated ped nodes"
        }
    }
    return b;
}

}  // namespace med
