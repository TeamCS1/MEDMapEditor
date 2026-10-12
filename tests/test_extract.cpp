#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "../src/core/col.h"
#include "../src/core/extract.h"
#include "../src/core/img.h"

namespace fs = std::filesystem;
using namespace med;

static int failures = 0;
#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            std::printf("FAIL line %d: %s\n", __LINE__, #c);      \
            ++failures;                                           \
        }                                                         \
    } while (0)

static std::vector<unsigned char> readAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
static void writeAll(const fs::path& p, const std::vector<unsigned char>& d) {
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size()));
}
// The archive stores whole 2048-byte sectors: an entry reads back padded with zeros, as MEd's extract wrote it.
static std::vector<unsigned char> padded(std::vector<unsigned char> d) {
    d.resize((d.size() + 2047) / 2048 * 2048, 0);
    return d;
}
static void put32(std::vector<unsigned char>& v, uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back((unsigned char)(x >> (8 * i)));
}
// A COLL record with one box; `seed` makes the bounds differ so records can be told apart.
static std::vector<unsigned char> coll(const char* name, uint32_t seed) {
    std::vector<unsigned char> c = {'C', 'O', 'L', 'L'};
    put32(c, 120 - 8);
    char nm[22] = {};
    std::strncpy(nm, name, 21);
    c.insert(c.end(), nm, nm + 22);
    c.push_back(1);
    c.push_back(0);
    for (int i = 0; i < 10; ++i) put32(c, seed + i);
    put32(c, 0);
    put32(c, 0);
    put32(c, 1);
    for (int i = 0; i < 7; ++i) put32(c, seed * 3 + i);
    put32(c, 0);
    put32(c, 0);
    return c;
}

int main() {
    const fs::path root = fs::temp_directory_path() / "med_extract_test";
    fs::remove_all(root);
    fs::create_directories(root / "game");
    std::string err;
    ImgArchive img;
    CHECK(ImgArchive::create((root / "game/gta3.img").string(), 1, img, err));
    std::vector<unsigned char> dff(3000), txd(5000), generic(700);
    for (size_t i = 0; i < dff.size(); ++i) dff[i] = (unsigned char)(i * 7);
    for (size_t i = 0; i < txd.size(); ++i) txd[i] = (unsigned char)(i * 13 + 1);
    for (size_t i = 0; i < generic.size(); ++i) generic[i] = (unsigned char)(i + 5);
    CHECK(img.add("box.dff", dff, err) && img.add("boxtex.txd", txd, err) && img.add("plain.dff", dff, err) && img.save(err));
    writeAll(root / "game/generic.txd", generic);
    // two COL files: the box is in the first (between two others) and again in the second
    std::vector<unsigned char> colA = coll("other", 10), boxRec = coll("box", 50), tail = coll("third", 90);
    colA.insert(colA.end(), boxRec.begin(), boxRec.end());
    colA.insert(colA.end(), tail.begin(), tail.end());
    std::vector<unsigned char> colB = coll("box", 77);
    writeAll(root / "game/a.col", colA);
    writeAll(root / "game/b.col", colB);
    const std::vector<unsigned char> imgBefore = readAll(root / "game/gta3.img"), dirBefore = readAll(root / "game/gta3.dir");

    ExtractRequest req;
    req.model = "box";
    req.txd = "boxtex";
    req.colFiles = {(root / "game/a.col").string(), (root / "game/b.col").string()};
    req.genericTxdPath = (root / "game/generic.txd").string();
    req.info = "IDE line: 1, box, boxtex, 1, 100, 0\r\n";
    CHECK(extractTargetsThatExist((root / "out").string(), req).empty());
    ExtractResult r = extractObject(img, (root / "out").string(), req);
    CHECK(r.written.size() == 4);
    CHECK(readAll(root / "out/box/box.dff") == padded(dff));         // byte for byte
    CHECK(readAll(root / "out/box/boxtex.txd") == padded(txd));
    CHECK(readAll(root / "out/box/box.col") == boxRec);      // exactly the record from the FIRST file
    std::vector<ColModel> back;
    CHECK(parseCol(readAll(root / "out/box/box.col"), back, err) && back.size() == 1 && back[0].name == "box");  // a valid one-model COL file
    CHECK(r.colSource == req.colFiles[0]);
    CHECK(r.warnings.size() == 1 && r.warnings[0].find("2 COL files") == 0);  // the second copy is mentioned
    {
        const std::vector<unsigned char> info = readAll(root / "out/box/box.txt");
        const std::string text(info.begin(), info.end());
        CHECK(text.find("IDE line: 1, box, boxtex") == 0 && text.find("Collision copied from: ") != std::string::npos);
    }
    // the game files are untouched
    CHECK(readAll(root / "game/gta3.img") == imgBefore && readAll(root / "game/gta3.dir") == dirBefore);
    CHECK(readAll(root / "game/a.col") == colA && readAll(root / "game/b.col") == colB);
    // a second extraction finds the files and overwrites them with the same bytes
    CHECK(extractTargetsThatExist((root / "out").string(), req).size() == 4);
    r = extractObject(img, (root / "out").string(), req);
    CHECK(r.written.size() == 4 && readAll(root / "out/box/box.dff") == padded(dff));

    // the generic texture dictionary comes from models/generic.txd
    ExtractRequest g = req;
    g.model = "plain";
    g.txd = "generic";
    g.info.clear();
    r = extractObject(img, (root / "out").string(), g);
    CHECK(readAll(root / "out/plain/generic.txd") == generic);
    CHECK(!fs::exists(root / "out/plain/plain.txt"));  // no info asked for
    CHECK(r.warnings.size() == 1 && r.warnings[0].find("No collision") == 0);  // plain has no collision: said so, the rest still copied
    CHECK(fs::exists(root / "out/plain/plain.dff"));

    // nothing of it exists: plain warnings, no crash, nothing written but the folder
    ExtractRequest none;
    none.model = "ghost";
    none.txd = "ghosttex";
    r = extractObject(img, (root / "out").string(), none);
    CHECK(r.written.empty() && r.warnings.size() == 3);

    // a folder that cannot be made (a file is in the way)
    writeAll(root / "blocked", {1});
    r = extractObject(img, (root / "blocked").string(), req);
    CHECK(r.written.empty() && !r.warnings.empty());

    // findCollisionRecord on damaged and missing files
    std::vector<unsigned char> rec;
    std::string src;
    writeAll(root / "game/bad.col", {'C', 'O', 'L', 'L', 1, 2});
    CHECK(!findCollisionRecord({(root / "game/bad.col").string(), (root / "game/missing.col").string()}, "box", rec, src));
    CHECK(findCollisionRecord({(root / "game/bad.col").string(), (root / "game/b.col").string()}, "BOX", rec, src) && rec == colB);  // case does not matter

    fs::remove_all(root);
    if (failures == 0) std::printf("all extract tests passed\n");
    return failures ? 1 : 0;
}
