#include <cmath>
#include <cstdio>
#include <cstring>
#include "../src/core/col.h"
#include "../src/core/importer.h"
#include <filesystem>
#include <fstream>
#include <iterator>

using namespace med;

static int failures = 0;
#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            std::printf("FAIL line %d: %s\n", __LINE__, #c);      \
            ++failures;                                           \
        }                                                         \
    } while (0)

static bool near(double a, double b, double e = 1e-4) { return std::fabs(a - b) <= e; }

static bool sameCol(const ColModel& a, const ColModel& b) {
    if (a.name != b.name || a.modelId != b.modelId || a.boundRadius != b.boundRadius) return false;
    for (int k = 0; k < 3; ++k)
        if (a.boundCenter[k] != b.boundCenter[k] || a.boundMin[k] != b.boundMin[k] || a.boundMax[k] != b.boundMax[k]) return false;
    if (a.spheres.size() != b.spheres.size() || a.boxes.size() != b.boxes.size() || a.vertices != b.vertices || a.faces.size() != b.faces.size()) return false;
    for (size_t i = 0; i < a.spheres.size(); ++i)
        if (a.spheres[i].radius != b.spheres[i].radius || std::memcmp(a.spheres[i].center, b.spheres[i].center, 12) || a.spheres[i].surface.material != b.spheres[i].surface.material ||
            a.spheres[i].surface.light != b.spheres[i].surface.light)
            return false;
    for (size_t i = 0; i < a.boxes.size(); ++i)
        if (std::memcmp(a.boxes[i].min, b.boxes[i].min, 12) || std::memcmp(a.boxes[i].max, b.boxes[i].max, 12) || a.boxes[i].surface.material != b.boxes[i].surface.material)
            return false;
    for (size_t i = 0; i < a.faces.size(); ++i)
        if (a.faces[i].a != b.faces[i].a || a.faces[i].b != b.faces[i].b || a.faces[i].c != b.faces[i].c || a.faces[i].material != b.faces[i].material || a.faces[i].light != b.faces[i].light)
            return false;
    return true;
}

// A model of two parts: a 2 x 2 x 2 cube at the origin, and the same cube's top face on a frame moved to x = 10 and
// turned 90 degrees about z.
static DffModel testModel() {
    DffModel m;
    DffFrame root;
    DffFrame moved;
    moved.parent = 0;
    moved.position[0] = 10;
    // right = (0,1,0), up = (-1,0,0), at = (0,0,1): a quarter turn about z
    const float rot[9] = {0, 1, 0, -1, 0, 0, 0, 0, 1};
    std::memcpy(moved.rotation, rot, sizeof rot);
    m.frames = {root, moved};
    DffGeometry cube;
    const float p[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
    for (auto& v : p) cube.positions.insert(cube.positions.end(), v, v + 3);
    cube.positions.insert(cube.positions.end(), {1, 1, 1});  // a duplicate of corner 6: welded
    const uint32_t t[] = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4, 2, 3, 7, 2, 7, 6, 1, 2, 6, 1, 6, 5, 3, 0, 4, 3, 4, 7, 6, 8, 5, /* degenerate: */ 0, 0, 1};
    cube.triangles.assign(t, t + sizeof t / sizeof t[0]);
    DffGeometry top;
    const float q[4][3] = {{0, 0, 1}, {2, 0, 1}, {2, 3, 1}, {0, 3, 1}};
    for (auto& v : q) top.positions.insert(top.positions.end(), v, v + 3);
    top.triangles = {0, 1, 2, 0, 2, 3};
    m.geometries = {cube, top};
    DffAtomic a0;
    a0.frame = 0;
    a0.geometry = 0;
    DffAtomic a1;
    a1.frame = 1;
    a1.geometry = 1;
    m.atomics = {a0, a1};
    return m;
}

// ---- builders for the import tests: a minimal model and texture dictionary the game's format readers accept ----
static void p32(std::vector<unsigned char>& v, uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back((unsigned char)(x >> (8 * i)));
}
static void p16(std::vector<unsigned char>& v, uint16_t x) {
    v.push_back((unsigned char)(x & 0xff));
    v.push_back((unsigned char)(x >> 8));
}
static void pf(std::vector<unsigned char>& v, float f) {
    uint32_t x;
    std::memcpy(&x, &f, 4);
    p32(v, x);
}
static std::vector<unsigned char> chunk(uint32_t type, const std::vector<unsigned char>& payload) {
    std::vector<unsigned char> c;
    p32(c, type);
    p32(c, uint32_t(payload.size()));
    p32(c, 0x0800FFFF);  // RenderWare 3.2, as most GTA III models
    c.insert(c.end(), payload.begin(), payload.end());
    return c;
}
static void add(std::vector<unsigned char>& v, const std::vector<unsigned char>& w) { v.insert(v.end(), w.begin(), w.end()); }

// A box-shaped model (8 corners, 12 triangles), w x d x h, standing on z = 0, textured with `texture`.
static std::vector<unsigned char> buildDff(const std::string& texture, float w, float d, float h) {
    std::vector<unsigned char> frameStruct;
    p32(frameStruct, 1);
    const float rot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    for (float f : rot) pf(frameStruct, f);
    for (int i = 0; i < 3; ++i) pf(frameStruct, 0);
    p32(frameStruct, 0xFFFFFFFF);
    p32(frameStruct, 0);
    std::vector<unsigned char> frameList = chunk(0x01, frameStruct);
    add(frameList, chunk(0x03, {}));
    const float x0 = -w / 2, x1 = w / 2, y0 = -d / 2, y1 = d / 2;
    const float p[8][3] = {{x0, y0, 0}, {x1, y0, 0}, {x1, y1, 0}, {x0, y1, 0}, {x0, y0, h}, {x1, y0, h}, {x1, y1, h}, {x0, y1, h}};
    const uint16_t t[12][3] = {{0, 1, 2}, {0, 2, 3}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4}, {2, 3, 7}, {2, 7, 6}, {1, 2, 6}, {1, 6, 5}, {3, 0, 4}, {3, 4, 7}};
    std::vector<unsigned char> gs;
    p32(gs, 0x02 | 0x04);  // positions, textured
    p32(gs, 12);
    p32(gs, 8);
    p32(gs, 1);
    pf(gs, 1); pf(gs, 1); pf(gs, 1);           // lighting (versions before 3.4)
    for (int i = 0; i < 8; ++i) { pf(gs, 0); pf(gs, 0); }  // UVs
    for (auto& tri : t) { p16(gs, tri[1]); p16(gs, tri[0]); p16(gs, 0); p16(gs, tri[2]); }
    pf(gs, 0); pf(gs, 0); pf(gs, h / 2); pf(gs, w + d + h);  // bounding sphere
    p32(gs, 1);
    p32(gs, 0);
    for (auto& v : p) for (float f : v) pf(gs, f);
    std::vector<unsigned char> matStruct;
    p32(matStruct, 0); p32(matStruct, 0xFFFFFFFF); p32(matStruct, 0); p32(matStruct, 1);
    pf(matStruct, 1); pf(matStruct, 1); pf(matStruct, 1);
    std::vector<unsigned char> texStruct;
    p32(texStruct, 0x1106);
    std::vector<unsigned char> nameBytes(texture.begin(), texture.end());
    nameBytes.resize((texture.size() / 4 + 1) * 4, 0);
    std::vector<unsigned char> tex = chunk(0x01, texStruct);
    add(tex, chunk(0x02, nameBytes));
    add(tex, chunk(0x02, std::vector<unsigned char>(4, 0)));
    add(tex, chunk(0x03, {}));
    std::vector<unsigned char> mat = chunk(0x01, matStruct);
    add(mat, chunk(0x06, tex));
    add(mat, chunk(0x03, {}));
    std::vector<unsigned char> mlStruct;
    p32(mlStruct, 1);
    p32(mlStruct, 0xFFFFFFFF);
    std::vector<unsigned char> ml = chunk(0x01, mlStruct);
    add(ml, chunk(0x07, mat));
    std::vector<unsigned char> geo = chunk(0x01, gs);
    add(geo, chunk(0x08, ml));
    add(geo, chunk(0x03, {}));
    std::vector<unsigned char> glStruct;
    p32(glStruct, 1);
    std::vector<unsigned char> gl = chunk(0x01, glStruct);
    add(gl, chunk(0x0F, geo));
    std::vector<unsigned char> atStruct;
    p32(atStruct, 0); p32(atStruct, 0); p32(atStruct, 5); p32(atStruct, 0);
    std::vector<unsigned char> atomic = chunk(0x01, atStruct);
    add(atomic, chunk(0x03, {}));
    std::vector<unsigned char> clStruct;
    p32(clStruct, 1); p32(clStruct, 0); p32(clStruct, 0);
    std::vector<unsigned char> clump = chunk(0x01, clStruct);
    add(clump, chunk(0x0E, frameList));
    add(clump, chunk(0x1A, gl));
    add(clump, chunk(0x14, atomic));
    add(clump, chunk(0x03, {}));
    return chunk(0x10, clump);
}

// A texture dictionary with 2 x 2 32-bit textures of these names; `shade` makes two dictionaries differ.
static std::vector<unsigned char> buildTxd(const std::vector<std::string>& names, unsigned char shade = 200) {
    std::vector<unsigned char> s;
    p16(s, uint16_t(names.size()));
    p16(s, 0);
    std::vector<unsigned char> dict = chunk(0x01, s);
    for (const std::string& n : names) {
        std::vector<unsigned char> ns;
        p32(ns, 8);       // Direct3D 8
        p32(ns, 0x1102);  // filter
        char name[32] = {}, mask[32] = {};
        std::memcpy(name, n.data(), std::min<size_t>(n.size(), 31));
        ns.insert(ns.end(), name, name + 32);
        ns.insert(ns.end(), mask, mask + 32);
        p32(ns, 0x500);   // 8888
        p32(ns, 0);       // no alpha
        p16(ns, 2); p16(ns, 2);
        ns.push_back(32); ns.push_back(1); ns.push_back(4); ns.push_back(0);
        p32(ns, 16);
        for (int i = 0; i < 4; ++i) { ns.push_back(shade); ns.push_back(100); ns.push_back(50); ns.push_back(255); }
        std::vector<unsigned char> nat = chunk(0x01, ns);
        add(nat, chunk(0x03, {}));
        add(dict, chunk(0x15, nat));
    }
    add(dict, chunk(0x03, {}));
    return chunk(0x16, dict);
}

static void writeFile(const std::string& path, const std::vector<unsigned char>& d) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size()));
}
static void writeText(const std::string& path, const std::string& s) { writeFile(path, std::vector<unsigned char>(s.begin(), s.end())); }
static std::string readText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
static bool has(const std::vector<std::string>& v, const std::string& part) {
    for (const std::string& s : v)
        if (s.find(part) != std::string::npos) return true;
    return false;
}

// A small game: default.ide (a car), one Rockstar region A with two objects, the user's region "upstate" (an IPL only),
// gta3.img with box.dff, shared.txd and other.txd.
static std::string makeGame(const std::string& base) {
    namespace fs = std::filesystem;
    fs::remove_all(base);
    const std::string root = base + "/game";
    writeText(root + "/data/default.dat", "IDE DATA\\DEFAULT.IDE\r\nCOLFILE 0 MODELS\\COLL\\GENERIC.COL\r\n");
    writeText(root + "/data/default.ide", "cars\r\n90, cop_car, cop_car, car, POLICE, COP, null, normal, 10, 7, 0\r\nend\r\n");
    writeText(root + "/data/gta3.dat",
              "IDE DATA\\MAPS\\A\\A.IDE\r\nCOLFILE 1 DATA\\MAPS\\A\\A.COL\r\nIPL DATA\\MAPS\\A\\A.IPL\r\nIPL DATA\\MAPS\\upstate\\upstate.IPL\r\n");
    writeText(root + "/data/maps/a/a.ide", "objs\r\n100, box, shared, 1, 100, 0\r\n3153, taken, shared, 1, 100, 0\r\nend\r\n");
    writeText(root + "/data/maps/upstate/upstate.ipl", "inst\r\nend\r\n");
    std::filesystem::create_directories(root + "/models");
    ImgArchive img;
    std::string err;
    ImgArchive::create(root + "/models/gta3.img", 1, img, err);
    img.add("box.dff", buildDff("boxside", 1, 1, 1), err);
    img.add("shared.txd", buildTxd({"boxside"}), err);
    img.add("other.txd", buildTxd({"x"}), err);
    img.save(err);
    return root;
}

static void importTests() {
    namespace fs = std::filesystem;
    const std::string base = (fs::temp_directory_path() / "med_import_test").string();
    const std::string root = makeGame(base);
    const std::string src = base + "/src";
    writeFile(src + "/mybin.dff", buildDff("binside", 2, 1, 1.5f));
    writeFile(src + "/mybin.txd", buildTxd({"binside"}));
    writeFile(src + "/shared.txd", buildTxd({"boxside"}));         // the very file already in the game
    writeFile(src + "/other.txd", buildTxd({"binside"}, 30));      // a different file with a name the game has
    {
        ColModel a, b;
        a.name = "something";
        b.name = "else";
        ColBox box;
        box.max[0] = 1;
        a.boxes = {box};
        std::vector<unsigned char> one = writeColl(a), two = writeColl(a);
        const std::vector<unsigned char> bb = writeColl(b);
        two.insert(two.end(), bb.begin(), bb.end());
        writeFile(src + "/single.col", one);
        writeFile(src + "/two.col", two);
    }
    std::string err;
    ImgArchive img;
    CHECK(ImgArchive::open(root + "/models/gta3.img", img, err));
    std::set<int> ids;
    std::set<std::string> names;
    gameIdsAndNames(root, ids, names);
    CHECK(ids == std::set<int>({90, 100, 3153}) && names.count("box") && names.count("taken") && names.count("cop_car"));
    CHECK(proposeModelId(ids) == 3154);

    ImportRequest r;
    r.gameRoot = root;
    r.dffPath = src + "/mybin.dff";
    r.txdPath = src + "/mybin.txd";
    r.name = "mybin";
    r.id = proposeModelId(ids);
    r.region = "upstate";
    ImportPlan p = planImport(r, img, ids, names);
    CHECK(p.problems.empty() && p.model.geometries.size() == 1 && p.textures.textures.size() == 1 && p.txdName == "mybin" && !p.txdReused);
    CHECK(!p.collisionFromFile && p.collision.boxes.empty() && p.collision.faces.empty() && has(p.notes, "starts empty"));
    CHECK(p.collision.name == "mybin" && p.collision.modelId == 3154 && near(p.collision.boundMax[0], 1) && near(p.collision.boundMax[2], 1.5));

    // every refusal
    auto problemsWith = [&](ImportRequest q) { return planImport(q, img, ids, names).problems; };
    ImportRequest q = r;
    q.name = "box";
    CHECK(has(problemsWith(q), "already exists"));
    q = r; q.name = "TAKEN";
    CHECK(has(problemsWith(q), "already exists"));            // names are compared without case
    q = r; q.name = "a name";
    CHECK(has(problemsWith(q), "Model name"));
    q = r; q.name = std::string(20, 'm');
    CHECK(has(problemsWith(q), "Model name"));
    q = r; q.id = 3153;
    CHECK(has(problemsWith(q), "already used") && has(problemsWith(q), "3154"));
    q = r; q.id = 90;
    CHECK(has(problemsWith(q), "already used"));               // a car's id counts too
    q = r; q.id = 5500;
    CHECK(has(problemsWith(q), "5499"));
    q = r; q.drawDistance = 0;
    CHECK(has(problemsWith(q), "draw distance"));
    q = r; q.txdPath.clear();
    CHECK(has(problemsWith(q), "required"));
    q = r; q.txdPath = src + "/missing.txd";
    CHECK(has(problemsWith(q), "Cannot read the textures"));
    q = r; q.txdPath = src + "/other.txd";
    CHECK(has(problemsWith(q), "different other.txd"));
    q = r; q.dffPath = src + "/mybin.txd";
    CHECK(has(problemsWith(q), "not a model"));
    q = r; q.region = "nowhere";
    CHECK(has(problemsWith(q), "New region"));
    q = r; q.region.clear();
    CHECK(has(problemsWith(q), "Pick the region"));
    q = r; q.colPath = src + "/two.col";
    CHECK(has(problemsWith(q), "none is named mybin"));
    q = r; q.colPath = src + "/mybin.txd";
    CHECK(has(problemsWith(q), "not a collision file"));
    // the same dictionary the game has: shared, not added again; a model texture missing from it: noted
    q = r; q.txdPath = src + "/shared.txd";
    ImportPlan ps = planImport(q, img, ids, names);
    CHECK(ps.problems.empty() && ps.txdReused && has(ps.notes, "shared") && has(ps.notes, "binside"));
    // a .col with one record of another name: renamed after the model
    q = r; q.colPath = src + "/single.col";
    ImportPlan pc = planImport(q, img, ids, names);
    CHECK(pc.problems.empty() && pc.collisionFromFile && pc.collision.name == "mybin" && pc.collision.modelId == 3154 && pc.collision.boxes.size() == 1 && has(pc.notes, "renamed"));
    // the start choices
    q = r; q.start = CollisionStart::Box;
    CHECK(planImport(q, img, ids, names).collision.boxes.size() == 1);
    q = r; q.start = CollisionStart::Mesh;
    CHECK(planImport(q, img, ids, names).collision.faces.size() == 12);

    // apply: the model with a box collision
    const std::string dirBefore = readText(root + "/models/gta3.dir");
    const auto imgSizeBefore = fs::file_size(root + "/models/gta3.img");
    const std::string aIde = readText(root + "/data/maps/a/a.ide");
    r.start = CollisionStart::Box;
    p = planImport(r, img, ids, names);
    std::vector<std::string> written;
    CHECK(applyImport(r, p, img, written, err));
    CHECK(readText(root + "/models/gta3.dir.before-mybin.med-backup") == dirBefore);            // the directory as it was
    CHECK(fs::exists(root + "/data/gta3.dat.med-backup") && fs::file_size(root + "/models/gta3.img") > imgSizeBefore);
    ImgArchive again;
    CHECK(ImgArchive::open(root + "/models/gta3.img", again, err) && again.find("mybin.dff") >= 0 && again.find("mybin.txd") >= 0 && again.find("box.dff") >= 0);
    std::vector<unsigned char> back;
    CHECK(again.extract("mybin.dff", back, err));
    std::vector<unsigned char> want = p.dffBytes;
    want.resize(back.size(), 0);
    CHECK(back == want);                                                                          // byte for byte (sector padded)
    CHECK(readText(root + "/data/maps/upstate/upstate.ide") == "objs\r\n3154, mybin, mybin, 1, 200, 0\r\nend\r\n");
    std::vector<ColModel> cols;
    const std::string colText = readText(root + "/data/maps/upstate/upstate.col");
    CHECK(parseCol(std::vector<unsigned char>(colText.begin(), colText.end()), cols, err) && cols.size() == 1 && cols[0].name == "mybin" && cols[0].boxes.size() == 1);
    CHECK(readText(root + "/data/gta3.dat") ==
          "IDE DATA\\MAPS\\A\\A.IDE\r\nIDE DATA\\MAPS\\upstate\\upstate.IDE\r\nCOLFILE 1 DATA\\MAPS\\A\\A.COL\r\nCOLFILE 0 DATA\\MAPS\\upstate\\upstate.COL\r\n"
          "IPL DATA\\MAPS\\A\\A.IPL\r\nIPL DATA\\MAPS\\upstate\\upstate.IPL\r\n");
    CHECK(readText(root + "/data/maps/a/a.ide") == aIde);                                         // Rockstar's IDE untouched
    CHECK(readText(root + "/models/med-imports.txt").find("mybin.dff and mybin.txd") != std::string::npos);
    ids.clear();
    names.clear();
    gameIdsAndNames(root, ids, names);
    CHECK(ids.count(3154) && names.count("mybin"));                                               // the game now knows it
    CHECK(has(planImport(r, again, ids, names).problems, "already exists"));                      // and a second import of it is refused

    // a second model into the same region, sharing a texture dictionary the game has: one line more, one record more,
    // gta3.dat unchanged, no dictionary added
    ImportRequest r2 = r;
    r2.dffPath = src + "/mybin.dff";
    r2.name = "mybin2";
    r2.txdPath = src + "/shared.txd";
    r2.id = proposeModelId(ids);
    r2.start = CollisionStart::Empty;
    const std::string datBefore = readText(root + "/data/gta3.dat");
    const size_t txdsBefore = again.entries().size();
    ImportPlan p2 = planImport(r2, again, ids, names);
    written.clear();
    CHECK(p2.problems.empty() && applyImport(r2, p2, again, written, err));
    CHECK(again.entries().size() == txdsBefore + 1 && again.find("shared.txd") >= 0);             // only mybin2.dff added
    CHECK(readText(root + "/data/gta3.dat") == datBefore);
    CHECK(readText(root + "/data/maps/upstate/upstate.ide") == "objs\r\n3154, mybin, mybin, 1, 200, 0\r\n3155, mybin2, shared, 1, 200, 0\r\nend\r\n");
    const std::string col2 = readText(root + "/data/maps/upstate/upstate.col");
    CHECK(parseCol(std::vector<unsigned char>(col2.begin(), col2.end()), cols, err) && cols.size() == 2 && cols[1].name == "mybin2" && cols[1].boxes.empty());
    CHECK(fs::exists(root + "/data/maps/upstate/upstate.ide.med-backup") && fs::exists(root + "/data/maps/upstate/upstate.col.med-backup"));

    // a plan with problems is never applied
    ImportPlan bad = planImport(r2, again, ids, names);
    CHECK(!bad.problems.empty() && !applyImport(r2, bad, again, written, err) && !err.empty());
    fs::remove_all(base);
}

int main() {
    // writeColl / parseCol round trip, every kind of shape, odd values
    {
        ColModel c;
        c.name = "a_rather_long_name_21";
        c.modelId = 4321;
        c.boundRadius = 12.5f;
        const float bc[3] = {1, -2, 3.25f}, bmin[3] = {-5, -6, -7}, bmax[3] = {5, 6, 7.5f};
        std::memcpy(c.boundCenter, bc, 12);
        std::memcpy(c.boundMin, bmin, 12);
        std::memcpy(c.boundMax, bmax, 12);
        ColSphere s;
        s.radius = 1.5f;
        s.center[0] = 0.125f;
        s.surface.material = 19;
        s.surface.light = 77;
        c.spheres = {s, s};
        ColBox b;
        b.min[0] = -1; b.max[0] = 2; b.max[1] = 3; b.max[2] = 4;
        b.surface.material = 2;
        c.boxes = {b};
        c.vertices = {0, 0, 0, 1, 0, 0, 0, 1, 0, -1e-7f, 3e6f, 2.5f};
        ColFace f;
        f.a = 0; f.b = 1; f.c = 2; f.material = 5; f.light = 9;
        ColFace g = f;
        g.c = 3;
        c.faces = {f, g};
        const std::vector<unsigned char> rec = writeColl(c);
        std::vector<ColModel> back;
        std::string err;
        CHECK(parseCol(rec, back, err) && back.size() == 1 && sameCol(back[0], c));
        CHECK(back.size() == 1 && back[0].fileSize == rec.size());
        ColModel empty;
        empty.name = "x";
        CHECK(parseCol(writeColl(empty), back, err) && back.size() == 1 && back[0].name == "x" && back[0].faces.empty());
        ColModel longName;
        longName.name = std::string(30, 'n');  // cut to 21 characters
        CHECK(parseCol(writeColl(longName), back, err) && back[0].name == std::string(21, 'n'));
    }
    // computeColBounds
    {
        ColModel c;
        ColBox b;
        b.min[0] = -1; b.min[1] = -1; b.min[2] = 0;
        b.max[0] = 1; b.max[1] = 1; b.max[2] = 2;
        ColSphere s;
        s.center[0] = 5;
        s.radius = 1;
        c.boxes = {b};
        c.spheres = {s};
        computeColBounds(c);
        CHECK(near(c.boundMin[0], -1) && near(c.boundMax[0], 6) && near(c.boundMin[2], -1) && near(c.boundMax[2], 2));
        CHECK(near(c.boundCenter[0], 2.5) && near(c.boundCenter[2], 0.5));
        // the farthest point: the box corner (-1, +-1, 2), 3.94 from the centre (2.5, 0, 0.5); the sphere reaches 2.55 + 1 = 3.55
        CHECK(near(c.boundRadius, std::sqrt(3.5 * 3.5 + 1 + 1.5 * 1.5), 1e-4));
        ColModel none;
        none.boundRadius = 7;
        computeColBounds(none);
        CHECK(none.boundRadius == 7);  // no shapes: kept
    }
    // the model's mesh: frames applied, equal corners welded, degenerate triangles dropped
    {
        const DffModel m = testModel();
        std::vector<float> v;
        std::vector<uint32_t> t;
        dffMesh(m, v, t);
        CHECK(v.size() / 3 == 8 + 4);   // 8 cube corners (the duplicate welded) + 4 top corners
        CHECK(t.size() / 3 == 12 + 2);  // 12 cube faces + 2 top; (6, dup-of-6, 5) is degenerate once welded, and (0, 0, 1) too
        float mn[3], mx[3];
        CHECK(dffBounds(m, mn, mx));
        // the top part: x in 0..2, y in 0..3 turned a quarter (x' = -y, y' = x) and moved to x = 10 -> x 7..10, y 0..2
        CHECK(near(mn[0], -1) && near(mx[0], 10) && near(mn[1], -1) && near(mx[1], 2) && near(mn[2], -1) && near(mx[2], 1));
        DffModel nothing;
        CHECK(!dffBounds(nothing, mn, mx));
    }
    // the three ways a collision starts
    {
        const DffModel m = testModel();
        ColModel e = makeCollision(m, "newbin", 3153, CollisionStart::Empty);
        CHECK(e.name == "newbin" && e.modelId == 3153 && e.boxes.empty() && e.spheres.empty() && e.faces.empty());
        CHECK(near(e.boundMin[0], -1) && near(e.boundMax[0], 10) && e.boundRadius > 5);  // the model's size, still
        ColModel b = makeCollision(m, "newbin", 3153, CollisionStart::Box, 6);
        CHECK(b.boxes.size() == 1 && near(b.boxes[0].min[0], -1) && near(b.boxes[0].max[0], 10) && b.boxes[0].surface.material == 6);
        CHECK(near(b.boundMin[1], -1) && near(b.boundMax[1], 2));
        ColModel s = makeCollision(m, "newbin", 3153, CollisionStart::Mesh, 1);
        CHECK(s.vertices.size() == 12 * 3 && s.faces.size() == 14 && s.faces[0].material == 1);
        for (const ColFace& f : s.faces) CHECK(f.a < 12 && f.b < 12 && f.c < 12);
        std::vector<ColModel> back;
        std::string err;
        CHECK(parseCol(writeColl(s), back, err) && back.size() == 1 && sameCol(back[0], s));  // the game's reader would take it
    }
    // ids
    {
        const std::string ide =
            "# a comment\r\nobjs\r\n100, a, b, 1, 50, 0\r\n  101, c, d, 1, 50, 0 # trailing comment\r\nend\r\ntobj\r\n200, e, f, 1, 50, 0, 20, 6\r\nend\r\n"
            "cars\r\n90, car, car, car, X, Y, null, normal, 10, 7, 0\r\nend\r\npeds\r\n1, cop, cop\r\nend\r\nweap\r\n170, colt, colt, x\r\nend\r\nhier\r\n150, h, h\r\nend\r\n"
            "path\r\nped, 100, a\r\n9999, 0, 0\r\nend\r\n2dfx\r\n777, 0, 0, 0\r\nend\r\n";
        std::set<int> ids;
        collectIdeIds(ide, ids);
        CHECK(ids == std::set<int>({1, 90, 100, 101, 150, 170, 200}));  // not the path or 2dfx numbers
        CHECK(proposeModelId({3153, 3154, 3156}) == 3155);
        CHECK(proposeModelId({}, 3153) == 3153);
        std::set<int> full;
        for (int i = 3153; i < kGta3ModelLimit; ++i) full.insert(i);
        CHECK(proposeModelId(full) == 0);  // nothing above: the lowest gap
        for (int i = 0; i < 3153; ++i) full.insert(i);
        CHECK(proposeModelId(full) == -1);
        CHECK(proposeModelId({5499}, 5499) == 0);
    }
    // names
    {
        std::string why;
        CHECK(validModelName("my_bin-01", why) && validModelName(std::string(19, 'a'), why));
        CHECK(!validModelName("", why) && !validModelName(std::string(20, 'a'), why) && why.find("19") != std::string::npos);
        CHECK(!validModelName("my bin", why) && !validModelName("bin,2", why) && !validModelName("bin.dff", why));
    }
    // IDE lines
    {
        CHECK(ideObjsLine(3153, "mybin", "mybintex", 150, 0) == "3153, mybin, mybintex, 1, 150, 0");
        CHECK(ideObjsLine(3154, "a", "b", 299.5, 4) == "3154, a, b, 1, 299.5, 4");
        std::string ide;
        addIdeObjsLine(ide, "1, a, b, 1, 1, 0");
        CHECK(ide == "objs\r\n1, a, b, 1, 1, 0\r\nend\r\n");  // a new file: Windows line endings, as the game's
        addIdeObjsLine(ide, "2, c, d, 1, 1, 0");
        CHECK(ide == "objs\r\n1, a, b, 1, 1, 0\r\n2, c, d, 1, 1, 0\r\nend\r\n");
        std::string unixIde = "objs\nend\n";
        addIdeObjsLine(unixIde, "9, z, z, 1, 1, 0");
        CHECK(unixIde == "objs\n9, z, z, 1, 1, 0\nend\n");  // an existing file keeps its own line endings
        std::string crlf = "# mine\r\ntobj\r\n5, t, t, 1, 1, 0, 1, 2\r\nend\r\nobjs\r\nend\r\n";
        addIdeObjsLine(crlf, "3, e, f, 1, 1, 0");
        CHECK(crlf == "# mine\r\ntobj\r\n5, t, t, 1, 1, 0, 1, 2\r\nend\r\nobjs\r\n3, e, f, 1, 1, 0\r\nend\r\n");
        std::string onlyTobj = "tobj\r\nend";
        addIdeObjsLine(onlyTobj, "4, g, h, 1, 1, 0");
        CHECK(onlyTobj == "tobj\r\nend\r\nobjs\r\n4, g, h, 1, 1, 0\r\nend\r\n");
    }
    // gta3.dat lines
    {
        std::string dat = "IDE DATA\\MAPS\\A\\A.IDE\r\nCOLFILE 1 DATA\\MAPS\\A\\A.COL\r\nIDE DATA\\MAPS\\B\\B.IDE\r\nIPL DATA\\MAPS\\A\\A.IPL\r\n";
        CHECK(addDatLine(dat, "IDE", "IDE DATA\\MAPS\\UPSTATE\\UPSTATE.IDE"));
        CHECK(addDatLine(dat, "COLFILE", "COLFILE 0 DATA\\MAPS\\UPSTATE\\UPSTATE.COL"));
        CHECK(dat == "IDE DATA\\MAPS\\A\\A.IDE\r\nCOLFILE 1 DATA\\MAPS\\A\\A.COL\r\nCOLFILE 0 DATA\\MAPS\\UPSTATE\\UPSTATE.COL\r\nIDE DATA\\MAPS\\B\\B.IDE\r\n"
                     "IDE DATA\\MAPS\\UPSTATE\\UPSTATE.IDE\r\nIPL DATA\\MAPS\\A\\A.IPL\r\n");
        const std::string before = dat;
        CHECK(!addDatLine(dat, "IDE", "ide  data/maps/upstate/upstate.ide") && dat == before);  // already there (case, slashes, spaces)
        std::string noIde = "# x\nIPL DATA\\MAPS\\A\\A.IPL\n";
        CHECK(addDatLine(noIde, "IDE", "IDE DATA\\MAPS\\U\\U.IDE") && noIde == "# x\nIDE DATA\\MAPS\\U\\U.IDE\nIPL DATA\\MAPS\\A\\A.IPL\n");
        std::string bare = "SPLASH x";
        CHECK(addDatLine(bare, "COLFILE", "COLFILE 0 X.COL") && bare == "SPLASH x\nCOLFILE 0 X.COL\n");
    }
    // the region's own COL file: append, replace (case ignored), refuse a damaged file
    {
        ColModel a, b, c;
        a.name = "alpha";
        b.name = "beta";
        c.name = "gamma";
        a.modelId = 1;
        b.modelId = 2;
        c.modelId = 3;
        std::vector<unsigned char> file;
        std::string err;
        CHECK(putColRecord(file, a, err) && putColRecord(file, b, err) && putColRecord(file, c, err));
        std::vector<ColModel> back;
        CHECK(parseCol(file, back, err) && back.size() == 3 && back[1].name == "beta");
        ColModel b2 = b;
        b2.name = "BETA";
        b2.modelId = 22;
        ColBox box;
        box.max[0] = 4;
        b2.boxes = {box};
        CHECK(putColRecord(file, b2, err));
        CHECK(parseCol(file, back, err) && back.size() == 3 && back[0].name == "alpha" && back[1].name == "BETA" && back[1].modelId == 22 && back[1].boxes.size() == 1 &&
              back[2].name == "gamma");
        std::vector<unsigned char> broken = {'C', 'O', 'L', 'L', 9, 9, 9, 9};
        const std::vector<unsigned char> keep = broken;
        CHECK(!putColRecord(broken, a, err) && broken == keep && !err.empty());
    }
    importTests();
    // a hard-linked gta3.img (the self-test's game copies) is refused: the import would write into the other folder too
    {
        namespace fs = std::filesystem;
        const std::string base = (fs::temp_directory_path() / "med_import_link").string();
        const std::string root = makeGame(base);
        fs::create_directories(base + "/other");
        std::error_code ec;
        fs::create_hard_link(root + "/models/gta3.img", base + "/other/gta3.img", ec);
        CHECK(!ec);
        writeFile(base + "/src/mybin.dff", buildDff("binside", 2, 1, 1.5f));
        writeFile(base + "/src/mybin.txd", buildTxd({"binside"}));
        std::string err;
        ImgArchive img;
        CHECK(ImgArchive::open(root + "/models/gta3.img", img, err));
        std::set<int> ids;
        std::set<std::string> names;
        gameIdsAndNames(root, ids, names);
        ImportRequest r;
        r.gameRoot = root;
        r.dffPath = base + "/src/mybin.dff";
        r.txdPath = base + "/src/mybin.txd";
        r.name = "mybin";
        r.id = 3154;
        r.region = "upstate";
        ImportPlan p = planImport(r, img, ids, names);
        CHECK(p.problems.size() == 1 && has(p.problems, "hard link"));
        const uintmax_t before = fs::file_size(base + "/other/gta3.img");
        std::vector<std::string> written;
        CHECK(!applyImport(r, p, img, written, err) && written.empty() && fs::file_size(base + "/other/gta3.img") == before);
        fs::remove(base + "/other/gta3.img");  // the link gone: allowed again
        CHECK(planImport(r, img, ids, names).problems.empty());
        fs::remove_all(base, ec);
    }
    if (failures == 0) std::printf("all importer tests passed\n");
    return failures ? 1 : 0;
}
