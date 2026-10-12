#include <cstdio>
#include <filesystem>
#include <string>
#include "../src/core/img.h"

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

static std::vector<unsigned char> blob(size_t n, unsigned char seed) {
    std::vector<unsigned char> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = static_cast<unsigned char>(seed + i);
    return v;
}

static void exercise(int version, const fs::path& dir) {
    std::string img = (dir / (version == 1 ? "gta3.img" : "gta3sa.img")).string();
    std::string err;
    ImgArchive a;
    CHECK(ImgArchive::create(img, version, a, err));
    CHECK(a.add("hospital.dff", blob(5000, 1), err));   // 3 sectors
    CHECK(a.add("generic.txd", blob(2048, 7), err));    // 1 sector
    CHECK(a.add("tiny.col", blob(10, 3), err));         // 1 sector
    CHECK(a.save(err));

    ImgArchive b;
    CHECK(ImgArchive::open(img, b, err));
    CHECK(b.version() == version);
    CHECK(b.entries().size() == 3);
    CHECK(b.find("HOSPITAL.DFF") == 0);  // case-insensitive
    CHECK(b.entries()[0].size == 3);

    std::vector<unsigned char> d;
    CHECK(b.extract("hospital.dff", d, err));
    CHECK(d.size() == 3 * kImgSectorSize);
    CHECK(d[0] == 1 && d[4999] == static_cast<unsigned char>(1 + 4999) && d[5000] == 0);

    // remove one in the middle, save (rebuild), the others must survive intact
    CHECK(b.remove("generic.txd", err));
    CHECK(b.save(err));
    ImgArchive c;
    CHECK(ImgArchive::open(img, c, err));
    CHECK(c.entries().size() == 2 && c.find("generic.txd") < 0);
    CHECK(c.extract("tiny.col", d, err));
    CHECK(d[0] == 3 && d[9] == 12 && d[10] == 0);
    CHECK(c.extract("hospital.dff", d, err) && d[4999] == static_cast<unsigned char>(1 + 4999));

    // replacing an entry
    CHECK(c.add("tiny.col", blob(4096, 9), err));
    CHECK(c.save(err));
    ImgArchive e;
    CHECK(ImgArchive::open(img, e, err));
    CHECK(e.entries().size() == 2 && e.entries()[e.find("tiny.col")].size == 2);

    CHECK(!c.add("this_name_is_far_too_long_for_it.dff", blob(1, 0), err));
    CHECK(!c.extract("missing.dff", d, err));
}

int main() {
    fs::path dir = fs::temp_directory_path() / "med_img_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    exercise(1, dir);
    exercise(2, dir);

    // San Andreas: directory that outgrows its header sector must force a rebuild
    std::string img = (dir / "big.img").string();
    std::string err;
    ImgArchive a;
    CHECK(ImgArchive::create(img, 2, a, err));
    for (int i = 0; i < 100; ++i) CHECK(a.add("file" + std::to_string(i) + ".dff", blob(100, static_cast<unsigned char>(i)), err));
    CHECK(a.save(err));
    ImgArchive b;
    CHECK(ImgArchive::open(img, b, err));
    CHECK(b.entries().size() == 100);
    std::vector<unsigned char> d;
    CHECK(b.extract("file0.dff", d, err) && d[0] == 0);
    CHECK(b.extract("file99.dff", d, err) && d[0] == 99 && d[99] == static_cast<unsigned char>(99 + 99));
    CHECK(b.entries()[0].offset >= 2);  // 8 + 3200 bytes of directory = 2 sectors

    fs::remove_all(dir);
    if (failures == 0) std::printf("all IMG tests passed\n");
    return failures ? 1 : 0;
}
