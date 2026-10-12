#include <cstdio>
#include "../src/core/original.h"

using namespace med;

static int failures = 0;
#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            std::printf("FAIL line %d: %s\n", __LINE__, #c);      \
            ++failures;                                           \
        }                                                         \
    } while (0)

static IplInstance mk(int id, double x, double y, double z) {
    IplInstance p;
    p.id = id;
    p.x = x;
    p.y = y;
    p.z = z;
    return p;
}

int main() {
    // keys and hashes
    CHECK(iplFileKey("DATA/MAPS/INDUSTSE/INDUSTSE.IPL") == "industse.ipl");
    CHECK(iplFileKey("data\\maps\\a\\B.ipl") == "b.ipl" && iplFileKey("plain.ipl") == "plain.ipl");
    CHECK(placementHash("A.ipl", 1, 10, 20, 30) == placementHash("data/maps/a/a.ipl", 1, 10.001, 19.999, 30.004));  // rounding to 0.01, file key
    CHECK(placementHash("a.ipl", 1, 10, 20, 30) != placementHash("a.ipl", 2, 10, 20, 30));
    CHECK(placementHash("a.ipl", 1, 10, 20, 30) != placementHash("a.ipl", 1, 10.02, 20, 30));
    CHECK(placementHash("a.ipl", 1, 10, 20, 30) != placementHash("b.ipl", 1, 10, 20, 30));

    // the "original game": a.ipl has building 1 (twice) and LOD 2, b.ipl has prop 3
    const OriginalEntry table[] = {
        {"a.ipl", 1, placementHash("a.ipl", 1, 0, 0, 0)},
        {"a.ipl", 1, placementHash("a.ipl", 1, 50, 0, 0)},
        {"a.ipl", 2, placementHash("a.ipl", 2, 0, 0, 0)},
        {"b.ipl", 3, placementHash("b.ipl", 3, 5, 5, 5)},
    };
    const size_t n = sizeof table / sizeof table[0];
    auto run = [&](const std::vector<IplInstance>& v, const std::vector<std::string>& f) { return classifyOriginal(v, f, table, n); };

    // untouched: everything original
    std::vector<IplInstance> v = {mk(1, 0, 0, 0), mk(1, 50, 0, 0), mk(2, 0, 0, 0), mk(3, 5, 5, 5)};
    std::vector<std::string> f = {"data/maps/a/a.ipl", "data/maps/a/a.ipl", "data/maps/a/a.ipl", "data/b.ipl"};
    std::vector<bool> r = run(v, f);
    CHECK(r == std::vector<bool>({true, true, true, true}));

    // a duplicate of the building and of its LOD (appended, same place): the copies are added, the originals stay
    v.push_back(mk(1, 0, 0, 0));
    v.push_back(mk(2, 0, 0, 0));
    f.push_back("data/maps/a/a.ipl");
    f.push_back("data/maps/a/a.ipl");
    r = run(v, f);
    CHECK(r == std::vector<bool>({true, true, true, true, false, false}));

    // the copies moved away: still added
    v[4].x = 300;
    v[5].x = 300;
    r = run(v, f);
    CHECK(!r[4] && !r[5] && r[0] && r[2]);

    // an ORIGINAL moved (not copied): still original
    std::vector<IplInstance> m = {mk(1, 0, 0, 0), mk(1, 77, 7, 7), mk(2, 0, 0, 0), mk(3, 5, 5, 5)};
    r = run(m, {"a.ipl", "a.ipl", "a.ipl", "b.ipl"});
    CHECK(r == std::vector<bool>({true, true, true, true}));

    // an original moved into a new region (another file): still original (its model has an unaccounted original)
    r = run(m, {"a.ipl", "upstate.ipl", "a.ipl", "b.ipl"});
    CHECK(r == std::vector<bool>({true, true, true, true}));

    // a new object in a new region: added
    std::vector<IplInstance> w = v;
    std::vector<std::string> g = f;
    w.push_back(mk(3, 1, 1, 1));
    g.push_back("upstate.ipl");
    r = run(w, g);
    CHECK(!r.back());

    // a model the original game never placed: added wherever it is
    w = {mk(1, 0, 0, 0), mk(9, 0, 0, 0)};
    r = run(w, {"a.ipl", "a.ipl"});
    CHECK(r[0] && !r[1]);

    // an original removed and a copy of the same model added elsewhere: the copy counts as original (the safe side)
    w = {mk(1, 0, 0, 0), mk(2, 0, 0, 0), mk(3, 5, 5, 5), mk(1, 900, 0, 0)};
    r = run(w, {"a.ipl", "a.ipl", "b.ipl", "a.ipl"});
    CHECK(r[3]);

    // a game the table does not know (no file name matches): everything original
    r = run({mk(1, 0, 0, 0), mk(42, 1, 2, 3)}, {"oceandrv.ipl", "haiti.ipl"});
    CHECK(r == std::vector<bool>({true, true}));

    // bad input: no table, or a file list of the wrong length: everything original
    CHECK(classifyOriginal(v, f, nullptr, 0) == std::vector<bool>(v.size(), true));
    CHECK(classifyOriginal(v, {"a.ipl"}, table, n) == std::vector<bool>(v.size(), true));
    CHECK(classifyOriginal({}, {}, table, n).empty());

    // the real table is there and complete
    size_t size = 0;
    const OriginalEntry* t = gta3OriginalTable(size);
    CHECK(t != nullptr && size == 8689);

    if (failures == 0) std::printf("all original-placement tests passed\n");
    return failures ? 1 : 0;
}
