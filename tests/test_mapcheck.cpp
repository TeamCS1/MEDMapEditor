#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "../src/core/img.h"
#include "../src/core/mapcheck.h"

using namespace med;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) {                                                    \
            std::printf("FAIL line %d: %s\n", __LINE__, #c);           \
            ++failures;                                                \
        }                                                              \
    } while (0)

static void put32(std::vector<unsigned char>& v, uint32_t x) {
    unsigned char b[4];
    std::memcpy(b, &x, 4);
    v.insert(v.end(), b, b + 4);
}

// A COLL model with one box (see test_renderware.cpp), named `name`.
static std::vector<unsigned char> colModel(const char* name) {
    std::vector<unsigned char> col = {'C', 'O', 'L', 'L'};
    put32(col, 120 - 8);
    char nm[22] = {0};
    std::strncpy(nm, name, 21);
    col.insert(col.end(), nm, nm + 22);
    col.push_back(1);
    col.push_back(0);
    for (int i = 0; i < 10; ++i) put32(col, 0);
    put32(col, 0);
    put32(col, 0);
    put32(col, 1);
    for (int i = 0; i < 7; ++i) put32(col, 0);
    put32(col, 0);
    put32(col, 0);
    return col;
}

static void write(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

static bool has(const CheckResult& r, const char* code) {
    for (const CheckIssue& i : r.issues) if (i.code == code) return true;
    return false;
}
static const CheckIssue* find(const CheckResult& r, const char* code) {
    for (const CheckIssue& i : r.issues) if (i.code == code) return &i;
    return nullptr;
}

static const char* kCleanIpl =
    "# a map\r\n"
    "inst\r\n"
    "1, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0, 1\r\n"
    "2, box2, 15, 20, 30, 1, 1, 1, 0, 0, 0.5, 0.866\r\n"
    "3, LOD2, 15, 20, 30, 1, 1, 1, 0, 0, 0, 1\r\n"
    "end\r\n"
    "cull\r\n"
    "1, 2, 3, 4, 5, 6, 7\r\n"
    "end\r\n";

int main() {
    // ---- how the game reads a line ----
    GamePlacement g;
    bool cut = false;
    CHECK(gameParsePlacement("1, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0, 1", g, &cut) == 12 && !cut);
    CHECK(g.id == 1 && g.name == "box1" && g.x == 10 && g.rw == 1);
    CHECK(gameParsePlacement("1 box1 10 20 30 1 1 1 0 0 0 1", g) == 12);  // commas are only spaces
    CHECK(gameParsePlacement("  1,box1,10,20,30,1,1,1,0,0,0,1  ", g) == 12);
    CHECK(gameParsePlacement("1, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0", g) == 11);  // one value short: skipped
    CHECK(gameParsePlacement("1, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0, 1, 99, 98", g) == 12);  // extra values are ignored
    CHECK(gameParsePlacement("1, box1, 10, 20, abc, 1, 1, 1, 0, 0, 0, 1", g) == 4);  // stops at the first bad number
    CHECK(gameParsePlacement("x", g) == 0);
    std::string longLine = "1, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0, 1" + std::string(300, ' ') + "5";
    gameParsePlacement(longLine, g, &cut);
    CHECK(cut);
    // a line cut at 255 characters loses the values that were beyond the cut
    std::string padded = "1, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0," + std::string(260, ' ') + "1";
    CHECK(gameParsePlacement(padded, g, &cut) == 11 && cut);

    // ---- a small game folder ----
    fs::path root = fs::temp_directory_path() / "med_mapcheck_test";
    fs::remove_all(root);
    write(root / "data/default.dat", "");
    write(root / "data/gta3.dat", "IDE DATA\\MAPS\\a.IDE\r\nCOLFILE 0 DATA\\MAPS\\a.col\r\nIPL DATA\\MAPS\\a.IPL\r\n");
    write(root / "data/maps/a.ide", "objs\r\n1, box1, generic, 1, 100, 0\r\n2, box2, nofile, 1, 100, 0\r\n3, LOD2, generic, 1, 1000, 0\r\nend\r\n");
    write(root / "data/maps/a.ipl", kCleanIpl);
    {
        std::vector<unsigned char> col = colModel("box1");
        std::vector<unsigned char> b2 = colModel("box2");
        col.insert(col.end(), b2.begin(), b2.end());
        std::vector<unsigned char> b3 = colModel("LOD2");
        col.insert(col.end(), b3.begin(), b3.end());
        std::ofstream(root / "data/maps/a.col", std::ios::binary).write(reinterpret_cast<char*>(col.data()), col.size());
    }
    fs::create_directories(root / "models");
    ImgArchive img;
    std::string err;
    CHECK(ImgArchive::create((root / "models/gta3.img").string(), 1, img, err));
    std::vector<unsigned char> dummy(100, 1);
    for (const char* n : {"box1.dff", "box2.dff", "LOD2.dff", "generic.txd"}) CHECK(img.add(n, dummy, err));
    CHECK(img.save(err));

    CheckInput in;
    in.root = root.string();

    // box2 uses the texture dictionary "nofile", which is not in the archive: only a warning, because the
    // game falls back to "generic"; there are no errors
    CheckResult clean = checkMap(in);
    CHECK(clean.count(Severity::Error) == 0);
    CHECK(has(clean, "model-txd-missing") && find(clean, "model-txd-missing")->severity == Severity::Warning);
    CHECK(clean.summary.placements == 3 && clean.summary.modelsPlaced == 3 && clean.summary.ipl == 1);

    // with every file present the map is clean: no errors and no warnings
    write(root / "data/maps/a.ide", "objs\r\n1, box1, generic, 1, 100, 0\r\n2, box2, generic, 1, 100, 0\r\n3, LOD2, generic, 1, 1000, 0\r\nend\r\n");
    clean = checkMap(in);
    for (const CheckIssue& i : clean.issues)
        if (i.severity != Severity::Info) std::printf("  unexpected [%s] %s: %s\n", i.code.c_str(), i.file.c_str(), i.message.c_str());
    CHECK(clean.count(Severity::Error) == 0 && clean.count(Severity::Warning) == 0);
    CHECK(clean.summary.lodWithoutBuilding == 0);

    auto withIpl = [&](const std::string& text) {
        CheckInput c = in;
        c.iplText["data/maps/a.ipl"] = text;
        return checkMap(c);
    };

    // a line with too few values is skipped by the game: report it with its line number
    CheckResult r = withIpl("inst\r\n1, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0\r\nend\r\n");
    CHECK(has(r, "ipl-short-line") && find(r, "ipl-short-line")->line == 2);
    CHECK(find(r, "ipl-short-line")->severity == Severity::Warning);
    CHECK(r.summary.placements == 0);

    // an id no IDE defines is ignored by the game
    r = withIpl("inst\r\n99, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0, 1\r\nend\r\n");
    CHECK(has(r, "placement-unknown-model") && find(r, "placement-unknown-model")->hasPosition);

    // quaternion w outside -1..1 gives the game a NaN rotation
    r = withIpl("inst\r\n1, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0, 1.5\r\nend\r\n");
    CHECK(has(r, "placement-quaternion-w") && find(r, "placement-quaternion-w")->severity == Severity::Error);
    r = withIpl("inst\r\n1, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0, -1\r\nend\r\n");
    CHECK(!has(r, "placement-quaternion-w"));  // exactly -1 is fine

    // far below the map
    r = withIpl("inst\r\n1, box1, 10, 20, -2000, 1, 1, 1, 0, 0, 0, 1\r\nend\r\n");
    CHECK(has(r, "placement-too-low"));
    r = withIpl("inst\r\n1, box1, 10, 20, -200, 1, 1, 1, 0, 0, 0, 1\r\nend\r\n");
    CHECK(!has(r, "placement-too-low"));  // -200 is the known safe depth

    // section names must be lowercase for the game
    r = withIpl("INST\r\n1, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0, 1\r\nend\r\n");
    CHECK(has(r, "ipl-section-case") && r.summary.placements == 0);
    r = withIpl("inst\r\n1, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0, 1\r\nEND\r\n");
    CHECK(has(r, "ipl-end-case"));

    // a missing end line
    r = withIpl("inst\r\n1, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0, 1\r\n");
    CHECK(has(r, "ipl-no-end"));

    // a line the game would cut
    r = withIpl("inst\r\n1, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0, 1" + std::string(260, ' ') + "\r\nend\r\n");
    CHECK(has(r, "ipl-long-line"));

    // an LOD whose building was removed is counted
    r = withIpl("inst\r\n3, LOD2, 15, 20, 30, 1, 1, 1, 0, 0, 0, 1\r\nend\r\n");
    CHECK(r.summary.lodWithoutBuilding == 1);
    // ... but not when it was deleted by moving it below the map (Replace does this when the new building has no LOD)
    r = withIpl("inst\r\n3, LOD2, 15, 20, -200, 1, 1, 1, 0, 0, 0, 1\r\nend\r\n");
    CHECK(r.summary.lodWithoutBuilding == 0);
    r = withIpl("inst\r\n3, LOD2, 15, 20, -150, 1, 1, 1, 0, 0, 0, 1\r\nend\r\n");  // merely deep is still counted
    CHECK(r.summary.lodWithoutBuilding == 1);
    r = withIpl("inst\r\n2, box2, 15, 20, 30, 1, 1, 1, 0, 0, 0, 1\r\n3, LOD2, 15, 20, 30, 1, 1, 1, 0, 0, 0, 1\r\nend\r\n");
    CHECK(r.summary.lodWithoutBuilding == 0 && r.summary.buildingWithoutLod == 0);
    r = withIpl("inst\r\n2, box2, 15, 20, 30, 1, 1, 1, 0, 0, 0, 1\r\nend\r\n");
    CHECK(r.summary.buildingWithoutLod == 1);

    // duplicates are counted
    r = withIpl("inst\r\n1, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0, 1\r\n1, box1, 10, 20, 30, 1, 1, 1, 0, 0, 0, 1\r\nend\r\n");
    CHECK(r.summary.duplicates == 1);

    // the model files the game needs
    {
        fs::remove(root / "models/gta3.img");
        fs::remove(root / "models/gta3.dir");
        ImgArchive i2;
        CHECK(ImgArchive::create((root / "models/gta3.img").string(), 1, i2, err));
        CHECK(i2.add("box1.dff", dummy, err));  // box2.dff, LOD2.dff and generic.txd are missing
        CHECK(i2.save(err));
        r = checkMap(in);
        const CheckIssue* e = find(r, "model-dff-missing");
        CHECK(e && e->severity == Severity::Error && e->message.find("box2") != std::string::npos);
    }
    // a texture dictionary that is not in the archive is only a warning
    write(root / "data/maps/a.ide", "objs\r\n1, box1, missing_txd, 1, 100, 0\r\nend\r\n");
    r = checkMap(in);
    CHECK(has(r, "model-txd-missing") && find(r, "model-txd-missing")->severity == Severity::Warning);
    // a model with no collision
    write(root / "data/maps/a.ide", "objs\r\n1, box1, generic, 1, 100, 0\r\n2, nocol, generic, 1, 100, 0\r\nend\r\n");
    write(root / "data/maps/a.ipl", "inst\r\n2, nocol, 15, 20, 30, 1, 1, 1, 0, 0, 0, 1\r\nend\r\n");
    r = checkMap(in);
    CHECK(has(r, "model-col-missing"));

    // a file that gta3.dat lists but that does not exist
    write(root / "data/gta3.dat", "IDE DATA\\MAPS\\a.IDE\r\nIPL DATA\\MAPS\\gone.IPL\r\n");
    r = checkMap(in);
    CHECK(has(r, "file-missing") && find(r, "file-missing")->severity == Severity::Error);
    // no gta3.dat at all
    fs::remove(root / "data/gta3.dat");
    r = checkMap(in);
    CHECK(has(r, "level-missing"));

    fs::remove_all(root);
    if (failures == 0) std::printf("all map check tests passed\n");
    return failures ? 1 : 0;
}
