#include <cstdio>
#include <filesystem>
#include <fstream>
#include "../src/core/region.h"

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

static std::string get(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main() {
    CHECK(validRegionName("upstate") && validRegionName("Up-state_2") && !validRegionName("") && !validRegionName("2up") && !validRegionName("my region") &&
          !validRegionName("a/b") && !validRegionName("..") && !validRegionName(std::string(25, 'a')));

    const fs::path root = fs::temp_directory_path() / "med_region_test";
    fs::remove_all(root);
    fs::create_directories(root / "data");
    const std::string bs(1, char(92));
    const std::string original = "# comment\r\nIPL DATA" + bs + "MAPS" + bs + "a" + bs + "a.IPL\r\nIPL DATA" + bs + "MAPS" + bs + "b" + bs + "b.IPL\r\nMAPZONE x\r\nEXIT\r\n";
    std::ofstream(root / "data/gta3.dat", std::ios::binary) << original;

    std::string rel, err;
    CHECK(addIplRegion(root.string(), "upstate", rel, err));
    CHECK(rel == "DATA/MAPS/upstate/upstate.IPL");
    CHECK(get(root / "data/maps/upstate/upstate.ipl") == "inst\r\nend\r\n");
    const std::string expected = "# comment\r\nIPL DATA" + bs + "MAPS" + bs + "a" + bs + "a.IPL\r\nIPL DATA" + bs + "MAPS" + bs + "b" + bs + "b.IPL\r\nIPL DATA" + bs + "MAPS" + bs +
                                 "upstate" + bs + "upstate.IPL\r\nMAPZONE x\r\nEXIT\r\n";
    CHECK(get(root / "data/gta3.dat") == expected);          // inserted after the last IPL line, line endings kept
    CHECK(get(root / "data/gta3.dat.med-backup") == original);  // the original is kept

    CHECK(!addIplRegion(root.string(), "upstate", rel, err) && !err.empty());  // already there
    CHECK(!addIplRegion(root.string(), "bad name", rel, err));
    CHECK(addIplRegion(root.string(), "second", rel, err));
    CHECK(get(root / "data/gta3.dat.med-backup") == original);  // still the first original, not overwritten
    CHECK(!addIplRegion((root / "nothing").string(), "x", rel, err));
    const std::vector<std::string> added = regionsAddedSinceBackup(root.string());
    CHECK(added.size() == 2 && added[0] == "DATA/MAPS/upstate/upstate.IPL" && added[1] == "DATA/MAPS/second/second.IPL");
    CHECK(regionsAddedSinceBackup((root / "nothing").string()).empty());

    fs::remove_all(root);
    if (failures == 0) std::printf("all region tests passed\n");
    return failures ? 1 : 0;
}
