#include <cstdio>
#include <cstring>
#include "../src/core/col.h"
#include "../src/core/dff.h"
#include "../src/core/rwstream.h"
#include "../src/core/txd.h"

using namespace med;

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

int main() {
    // library id -> version
    // The low bits are the build number: 0x1003FFFF is RenderWare 3.4.0.3.
    CHECK(rw::versionFromLibraryId(0x0800FFFF) == 0x32000);  // GTA III, most models
    CHECK(rw::versionFromLibraryId(0x0C02FFFF) == 0x33002);  // 3.3.0.2
    CHECK(rw::versionFromLibraryId(0x1003FFFF) == 0x34003);  // 3.4.0.3
    CHECK(rw::versionFromLibraryId(0x1803FFFF) == 0x36003);  // San Andreas, 3.6.0.3
    CHECK(rw::versionFromLibraryId(0x310) == 0x31000);       // old encoding (many GTA III files)

    // reader refuses to run past the end
    unsigned char two[2] = {1, 2};
    RwReader r(two, 2);
    r.u32();
    CHECK(!r.ok());

    // DXT1: one block, c0 = pure red (565 0xF800), c1 = pure blue (0x001F), all indices 0
    uint8_t block[8] = {0x00, 0xF8, 0x1F, 0x00, 0, 0, 0, 0};
    uint8_t px[4 * 4 * 4];
    decodeDxt(block, 4, 4, 1, px);
    CHECK(px[0] == 255 && px[1] == 0 && px[2] == 0 && px[3] == 255);
    // index 1 everywhere -> second colour (blue)
    uint8_t blk2[8] = {0x00, 0xF8, 0x1F, 0x00, 0x55, 0x55, 0x55, 0x55};
    decodeDxt(blk2, 4, 4, 1, px);
    CHECK(px[0] == 0 && px[1] == 0 && px[2] == 255);
    // DXT1 with c0 <= c1: index 3 is transparent
    uint8_t blk3[8] = {0x1F, 0x00, 0x00, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF};
    decodeDxt(blk3, 4, 4, 1, px);
    CHECK(px[3] == 0);
    // DXT5: alpha endpoints 255/0, all indices 0 -> alpha 255
    uint8_t blk5[16] = {255, 0, 0, 0, 0, 0, 0, 0, 0x00, 0xF8, 0x1F, 0x00, 0, 0, 0, 0};
    decodeDxt(blk5, 4, 4, 5, px);
    CHECK(px[3] == 255 && px[0] == 255);

    // malformed input is rejected, not crashed on
    DffModel m;
    TxdFile t;
    std::string err;
    CHECK(!parseDff({}, m, err));
    CHECK(!parseTxd({}, t, err));
    std::vector<unsigned char> junk;
    put32(junk, 0x10);
    put32(junk, 1000000);  // chunk claims a size larger than the data
    put32(junk, 0x0C02FFFF);
    CHECK(!parseDff(junk, m, err));

    // minimal valid empty clump parses
    std::vector<unsigned char> clump;
    put32(clump, 0x10);
    put32(clump, 16);
    put32(clump, 0x0C02FFFF);
    put32(clump, 0x01);  // Struct, 4 bytes
    put32(clump, 4);
    put32(clump, 0x0C02FFFF);
    put32(clump, 0);
    CHECK(parseDff(clump, m, err));
    CHECK(m.version == 0x33002 && m.geometries.empty());

    // COLL: a model with one box and nothing else (32 header + 40 bounds + 5 counts + 28 box = 120 bytes)
    std::vector<unsigned char> col;
    col.insert(col.end(), {'C', 'O', 'L', 'L'});
    put32(col, 120 - 8);
    char nm[22] = "testbox";
    col.insert(col.end(), nm, nm + 22);
    col.push_back(7);  // model id 7
    col.push_back(0);
    for (int i = 0; i < 10; ++i) put32(col, 0);  // bounds
    put32(col, 0);  // spheres
    put32(col, 0);  // unused
    put32(col, 1);  // boxes
    for (int i = 0; i < 7; ++i) put32(col, 0);  // min, max, surface
    put32(col, 0);  // vertices
    put32(col, 0);  // faces
    std::vector<ColModel> models;
    CHECK(col.size() == 120);
    CHECK(parseCol(col, models, err));
    CHECK(models.size() == 1 && models[0].name == "testbox" && models[0].modelId == 7 && models[0].boxes.size() == 1);
    col[4] = 100;  // wrong size must be rejected
    CHECK(!parseCol(col, models, err));
    std::vector<unsigned char> sa = {'C', 'O', 'L', '3', 0, 0, 0, 0};
    CHECK(!parseCol(sa, models, err));

    if (failures == 0) std::printf("all RenderWare tests passed\n");
    return failures ? 1 : 0;
}
