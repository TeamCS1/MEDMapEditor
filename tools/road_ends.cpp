// Read-only check of the road-end measurement (core/roadkit.h) against the original map: every road piece's ends are
// measured from its model, every placed road piece's ends are turned into the world, and each end is checked for
// another placed piece's end at the same point facing it. Where Rockstar joined pieces, the ends must meet.
// usage: road_ends <GTA III folder> [--pieces]    (--pieces also lists every road model with its ends)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include "../src/core/dff.h"
#include "../src/core/ide.h"
#include "../src/core/img.h"
#include "../src/core/ipl.h"
#include "../src/core/roadkit.h"
#include "../src/core/textutil.h"

using namespace med;
namespace fs = std::filesystem;

static std::string slurp(const fs::path& p, bool& ok) {
    std::ifstream f(p, std::ios::binary);
    ok = static_cast<bool>(f);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: road_ends <GTA III folder> [--pieces]\n"); return 2; }
    const fs::path root = argv[1];
    const bool listPieces = argc > 2 && std::string(argv[2]) == "--pieces";
    const bool strict = argc > 2 && std::string(argv[2]) == "--strict";  // ends must meet within 0.02 instead of 0.3
    bool ok;
    std::vector<std::string> ides, ipls;
    for (const char* dat : {"data/default.dat", "data/gta3.dat"}) {
        std::istringstream in(slurp(root / dat, ok));
        std::string line;
        while (std::getline(in, line)) {
            line = trim(line);
            for (char& c : line) if (c == '\\') c = '/';
            if (line.rfind("IDE ", 0) == 0) ides.push_back(trim(line.substr(4)));
            if (line.rfind("IPL ", 0) == 0) ipls.push_back(trim(line.substr(4)));
        }
    }
    std::map<int, std::string> name;
    for (const std::string& p : ides) {
        IdeFile f = parseIde(slurp(root / p, ok), GameVersion::GTA3);
        for (const IdeObject& o : f.objects) name[o.id] = o.model;
    }
    ImgArchive img;
    std::string err;
    if (!ImgArchive::open((root / "models/gta3.img").string(), img, err)) { std::printf("%s\n", err.c_str()); return 1; }
    std::map<int, std::vector<RoadEnd>> endsOf;  // road models only
    for (const auto& kv : name) {
        if (toLower(kv.second).rfind("rd_", 0) != 0) continue;
        std::vector<unsigned char> data;
        DffModel m;
        if (!img.extract(kv.second + ".dff", data, err) || !parseDff(data, m, err)) continue;
        std::vector<float> pos;
        for (const DffGeometry& g : m.geometries) pos.insert(pos.end(), g.positions.begin(), g.positions.end());
        endsOf[kv.first] = roadEndsFor(kv.second, pos);
        if (listPieces) {
            std::printf("%-20s %zu end(s):", kv.second.c_str(), endsOf[kv.first].size());
            for (const RoadEnd& e : endsOf[kv.first]) std::printf("  (%.2f, %.2f, %.2f) facing (%g, %g) width %.1f", e.x, e.y, e.z, e.dx, e.dy, e.width);
            std::printf("\n");
        }
    }
    struct Placed {
        int id;
        RoadEnd w;
    };
    std::vector<Placed> all;
    for (const std::string& p : ipls) {
        IplFile f = parseTextIpl(slurp(root / p, ok), GameVersion::GTA3);
        for (const IplInstance& o : f.instances) {
            auto e = endsOf.find(o.id);
            if (e == endsOf.end()) continue;
            for (const RoadEnd& end : e->second) all.push_back({o.id, endInWorld(o, end)});
        }
    }
    // each end: does another placed end meet it? (grid buckets of 5 units)
    std::map<std::pair<long, long>, std::vector<size_t>> grid;
    for (size_t i = 0; i < all.size(); ++i) grid[{std::lround(all[i].w.x / 5), std::lround(all[i].w.y / 5)}].push_back(i);
    std::map<int, std::pair<int, int>> perModel;  // id -> (ends, met)
    int met = 0, nearMiss = 0;
    for (size_t i = 0; i < all.size(); ++i) {
        bool found = false, near = false;
        const long gx = std::lround(all[i].w.x / 5), gy = std::lround(all[i].w.y / 5);
        for (long a = gx - 1; a <= gx + 1 && !found; ++a)
            for (long b = gy - 1; b <= gy + 1 && !found; ++b) {
                auto g = grid.find({a, b});
                if (g == grid.end()) continue;
                for (size_t j : g->second) {
                    if (j == i) continue;
                    if (endsMeet(all[i].w, all[j].w, strict ? 0.02 : 0.3)) { found = true; break; }
                    const double d = std::hypot(all[i].w.x - all[j].w.x, all[i].w.y - all[j].w.y);
                    if (d < 3 && all[i].w.dx * all[j].w.dx + all[i].w.dy * all[j].w.dy < -0.99) near = true;
                }
            }
        if (strict && !found) {  // what is it close to?
            for (size_t j = 0; j < all.size(); ++j) {
                if (j == i || !endsMeet(all[i].w, all[j].w, 0.3)) continue;
                std::printf("  near miss: %s end at (%.3f %.3f %.3f) vs %s end at (%.3f %.3f %.3f): off by %.3f %.3f %.3f\n", name[all[i].id].c_str(), all[i].w.x, all[i].w.y, all[i].w.z,
                            name[all[j].id].c_str(), all[j].w.x, all[j].w.y, all[j].w.z, all[j].w.x - all[i].w.x, all[j].w.y - all[i].w.y, all[j].w.z - all[i].w.z);
            }
        }
        ++perModel[all[i].id].first;
        if (found) { ++met; ++perModel[all[i].id].second; }
        else if (near) ++nearMiss;
    }
    if (argc > 3 && std::string(argv[2]) == "--probe") {  // where other pieces' ends sit around a model, in its own frame
        const std::string want = toLower(argv[3]);
        std::map<std::string, int> seen;
        for (const std::string& p : ipls) {
            IplFile f = parseTextIpl(slurp(root / p, ok), GameVersion::GTA3);
            for (const IplInstance& o : f.instances) {
                if (toLower(name[o.id]) != want) continue;
                for (const Placed& q : all) {
                    const double dx = q.w.x - o.x, dy = q.w.y - o.y;
                    if (std::hypot(dx, dy) > 16) continue;
                    // into the placement's frame: the inverse of the drawn turn
                    IplInstance inv = o;
                    inv.rotX = -o.rotX; inv.rotY = -o.rotY; inv.rotZ = -o.rotZ;
                    double lx, ly, lz, ldx, ldy, ldz;
                    placementDirToWorld(inv, dx, dy, q.w.z - o.z, lx, ly, lz);
                    placementDirToWorld(inv, q.w.dx, q.w.dy, 0, ldx, ldy, ldz);
                    char b[160];
                    std::snprintf(b, sizeof b, "(%.2f, %.2f, %.2f) facing (%.0f, %.0f)", lx, ly, lz, ldx, ldy);
                    ++seen[b];
                }
            }
        }
        for (const auto& kv : seen) std::printf("  %3d x  another piece's end at %s\n", kv.second, kv.first.c_str());
        return 0;
    }
    if (argc > 2 && std::string(argv[2]) == "--open-ends") {  // open ends with free ground ahead (for demonstrations)
        std::vector<IplInstance> every;
        for (const std::string& p : ipls) {
            IplFile f = parseTextIpl(slurp(root / p, ok), GameVersion::GTA3);
            every.insert(every.end(), f.instances.begin(), f.instances.end());
        }
        int shown = 0;
        for (size_t i = 0; i < all.size() && shown < 15; ++i) {
            bool met = false;
            for (size_t j = 0; j < all.size() && !met; ++j) met = j != i && endsMeet(all[i].w, all[j].w);
            if (met) continue;
            const double ax = all[i].w.x + all[i].w.dx * 40, ay = all[i].w.y + all[i].w.dy * 40;
            int near = 0;
            for (const IplInstance& o : every) near += std::hypot(o.x - ax, o.y - ay) < 45;
            if (near > 1) continue;
            std::printf("  open end of %s at %.2f %.2f %.2f facing %.0f %.0f (nothing within 45 of the ground ahead)\n", name[all[i].id].c_str(), all[i].w.x, all[i].w.y, all[i].w.z,
                        all[i].w.dx, all[i].w.dy);
            ++shown;
        }
        return 0;
    }
    std::printf("road models measured: %zu (with ends: %zu)\n", endsOf.size(),
                size_t(std::count_if(endsOf.begin(), endsOf.end(), [](const auto& kv) { return !kv.second.empty(); })));
    std::printf("placed road ends: %zu, meeting another piece's end: %d (%.1f%%), close but not meeting (within 3 units, facing): %d\n", all.size(), met,
                all.empty() ? 0.0 : 100.0 * met / all.size(), nearMiss);
    std::printf("per model (ends placed / meeting):\n");
    for (const auto& kv : perModel) std::printf("  %-20s %5d / %5d  (%.0f%%)\n", name[kv.first].c_str(), kv.second.first, kv.second.second, 100.0 * kv.second.second / kv.second.first);
    return 0;
}
