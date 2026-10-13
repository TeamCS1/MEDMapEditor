#include <cmath>
#include <cstdio>
#include "../src/core/ide.h"
#include "../src/core/ipl.h"
#include "../src/core/pools.h"

using namespace med;

static int failures = 0;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) {                                                    \
            std::printf("FAIL line %d: %s\n", __LINE__, #c);           \
            ++failures;                                                \
        }                                                              \
    } while (0)

int main() {
    // GTA III IPL
    const char* ipl3 =
        "# comment\n"
        "inst\n"
        "1450, hospital, 1145.68, -576.017, 13.528, 1, 1, 1, 0, 0, 0.5, 0.866\n"
        "bad line\n"
        "end\n"
        "cull\n"
        "10, 20, 30, 1, 2, 3, 4\n"
        "end\n";
    IplFile a = parseTextIpl(ipl3, GameVersion::GTA3);
    CHECK(a.instances.size() == 1);
    CHECK(a.warnings.size() == 1);
    CHECK(a.instances[0].id == 1450 && a.instances[0].model == "hospital");
    CHECK(std::fabs(a.instances[0].x - 1145.68) < 1e-9);
    CHECK(std::fabs(a.instances[0].rotW - 0.866) < 1e-9);
    CHECK(a.otherSections.size() == 1 && a.otherSections[0].lines.size() == 1);
    IplFile a2 = parseTextIpl(writeTextIpl(a), GameVersion::GTA3);
    CHECK(a2.instances.size() == 1 && a2.warnings.empty() && a2.otherSections.size() == 1);
    CHECK(a2.instances[0].z == a.instances[0].z);

    // Vice City: with and without interior
    IplFile v = parseTextIpl("inst\n5, a, 0, 1,2,3, 1,1,1, 0,0,0,1\n6, b, 1,2,3, 1,1,1, 0,0,0,1\nend\n",
                             GameVersion::ViceCity);
    CHECK(v.instances.size() == 2 && v.warnings.empty());
    CHECK(v.instances[0].interior == 0 && v.instances[1].x == 1);

    // San Andreas text + binary round trip
    IplFile s = parseTextIpl("inst\n700, model, 0, 10, 20, 30, 0, 0, 0, 1, -1\nend\n", GameVersion::SanAndreas);
    CHECK(s.instances.size() == 1 && s.instances[0].lod == kNoLod);
    std::vector<unsigned char> bin = writeBinaryIpl(s);
    CHECK(isBinaryIpl(bin) && bin.size() == 0x4C + 40);
    IplFile s2 = parseBinaryIpl(bin);
    CHECK(s2.instances.size() == 1 && s2.instances[0].id == 700 && s2.instances[0].y == 20);
    CHECK(s2.instances[0].lod == kNoLod);

    // IDE: III objs with 2 meshes, tobj, other section preserved
    const char* ide3 =
        "objs\n"
        "1450, hospital, generic, 2, 200, 100, 0\n"
        "end\n"
        "tobj\n"
        "1451, neon, generic, 1, 150, 0, 18, 6\n"
        "end\n"
        "cars\n"
        "90, taxi, taxi, car, TAXI, null, richfamily, 10, 0, 0, 4, 0.7, 0.8, 0\n"
        "end\n";
    IdeFile d = parseIde(ide3, GameVersion::GTA3);
    CHECK(d.objects.size() == 2 && d.warnings.empty());
    CHECK(d.objects[0].drawDistances.size() == 2 && d.objects[0].drawDistances[1] == 100);
    CHECK(d.objects[1].timed && d.objects[1].timeOn == 18 && d.objects[1].timeOff == 6);
    CHECK(d.otherSections.size() == 1 && d.otherSections[0].name == "cars");
    IdeFile d2 = parseIde(writeIde(d), GameVersion::GTA3);
    CHECK(d2.objects.size() == 2 && d2.otherSections[0].lines == d.otherSections[0].lines);

    // IDE: San Andreas has a single draw distance and no mesh count
    IdeFile sa = parseIde("objs\n1000, m, t, 300, 0\nend\n", GameVersion::SanAndreas);
    CHECK(sa.objects.size() == 1 && sa.objects[0].drawDistances[0] == 300);

    // IDE: San Andreas legacy forms with one extra column (6 fields objs, 8 fields tobj)
    IdeFile sl = parseIde("objs\n1001, m, t, 1, 250, 0\nend\ntobj\n1002, n, t, 1, 120, 0, 20, 6\nend\n",
                          GameVersion::SanAndreas);
    CHECK(sl.objects.size() == 2 && sl.warnings.empty());
    CHECK(sl.objects[0].drawDistances[0] == 250 && sl.objects[0].flags == 0);
    CHECK(sl.objects[1].drawDistances[0] == 120 && sl.objects[1].timeOn == 20 && sl.objects[1].timeOff == 6);

    // LOD pairing by name tail and position (GTA III / VC)
    {
        auto mk = [](const char* name, double x, double y, double z) {
            IplInstance i;
            i.model = name;
            i.x = x; i.y = y; i.z = z;
            return i;
        };
        std::vector<IplInstance> v = {mk("comSEbuilding1", 10, 20, 30),   // 0 building
                                      mk("LODSEbuilding1", 10, 20, 30),   // 1 its LOD, same spot
                                      mk("indSEbuilding1", 500, 20, 30),   // 2 same tail but far away: not a partner
                                      mk("lodSEbuilding1", 12, 20, 30),   // 3 a second LOD, 2 units away (case insensitive)
                                      mk("tree", 10, 20, 30),             // 4 unrelated
                                      mk("LOD", 10, 20, 30)};             // 5 too short to pair
        auto p0 = findLodPartners(v, 0);
        CHECK(p0.size() == 2 && p0[0] == 1 && p0[1] == 3);  // nearest first
        auto p1 = findLodPartners(v, 1);
        CHECK(p1.size() == 1 && p1[0] == 0);                // a LOD finds its building, not the far one
        CHECK(findLodPartners(v, 4).empty());
        CHECK(findLodPartners(v, 5).empty());
        CHECK(findLodPartners(v, 99).empty());
        CHECK(findLodPartners(v, 0, 1.0).size() == 1);      // the distance limit is respected
        // pairing one to one when copies share the spot (Duplicate puts the copy exactly on the original)
        std::vector<IplInstance> d = {mk("hospitaland", 0, 0, 0), mk("LODpitaland", 0, 0, 0), mk("hospitaland", 0, 0, 0), mk("LODpitaland", 0, 0, 0)};
        CHECK(findLodPartners(d, 2).size() == 2);                                     // the plain search sees both stand-ins
        auto origFirstTwo = [](size_t i) { return i < 2; };
        CHECK(pairedLodPartners(d, 0, origFirstTwo) == std::vector<size_t>{1});  // original with original
        CHECK(pairedLodPartners(d, 2, origFirstTwo) == std::vector<size_t>{3});  // copy with copy
        CHECK(pairedLodPartners(d, 1, origFirstTwo) == std::vector<size_t>{0});  // and from the stand-in's side
        CHECK(pairedLodPartners(d, 3, origFirstTwo) == std::vector<size_t>{2});
        CHECK(pairedLodPartners(d, 2) == std::vector<size_t>{3});                // without flags: file order
        // the copy moved 3 units away (still within 5 of both): nearest wins
        d[2].x = 3;
        d[3].x = 3;
        CHECK(pairedLodPartners(d, 0) == std::vector<size_t>{1} && pairedLodPartners(d, 2) == std::vector<size_t>{3});
        // two buildings, one stand-in: the nearer building gets it, the other none
        std::vector<IplInstance> e = {mk("hospitaland", 0, 0, 0), mk("LODpitaland", 0, 0, 0), mk("hospitaland", 2, 0, 0)};
        CHECK(pairedLodPartners(e, 0) == std::vector<size_t>{1} && pairedLodPartners(e, 2).empty());
        // a lone building keeps every stand-in (as findLodPartners)
        CHECK(pairedLodPartners(v, 0) == findLodPartners(v, 0) && pairedLodPartners(v, 4).empty() && pairedLodPartners(v, 99).empty());
        CHECK(lodModelNameFor("hospitaland") == "LODpitaland");
        CHECK(lodModelNameFor("comSEbuilding1") == "LODSEbuilding1");
        CHECK(lodModelNameFor("LODpitaland").empty() && lodModelNameFor("lodx1").empty());  // a LOD has no LOD
        CHECK(lodModelNameFor("abc").empty() && lodModelNameFor("").empty());
    }

    // Removing an instance repairs the LOD links of the others (MEd, 0x00424490)
    {
        std::vector<IplInstance> v(4);
        v[0].lod = 2;  // points at the one that will be removed
        v[1].lod = 3;  // points past it, must move down
        v[2].lod = kNoLod;
        v[3].lod = kNoLod;
        for (int i = 0; i < 4; ++i) v[i].id = 100 + i;
        CHECK(removeIplInstance(v, 2));
        CHECK(v.size() == 3);
        CHECK(v[0].id == 100 && v[1].id == 101 && v[2].id == 103);
        CHECK(v[0].lod == kNoLod);
        CHECK(v[1].lod == 2);
        CHECK(v[2].lod == kNoLod);
        CHECK(!removeIplInstance(v, 3));  // out of range
        CHECK(v.size() == 3);
        // a link below the removed position is untouched
        std::vector<IplInstance> w(3);
        w[2].lod = 0;
        CHECK(removeIplInstance(w, 1) && w.size() == 2 && w[1].lod == 0);
    }

    // Saving: replace only the inst block, keep every other byte (MEd save, 0x00423730)
    {
        const std::string orig =
            "# header comment\r\n"
            "inst\r\n"
            "1450, hospital, 1145.68, -576.017, 13.528, 1, 1, 1, 0, 0, 0.5, 0.866\r\n"
            "1451, other, 1, 2, 3, 1, 1, 1, 0, 0, 0, 1\r\n"
            "end\r\n"
            "cull\r\n"
            "10, 20, 30, 1, 2, 3, 4\r\n"
            "end\r\n";
        IplFile f = parseTextIpl(orig, GameVersion::GTA3);
        std::string out, err;
        // unchanged data comes back byte for byte
        CHECK(replaceIplInstSection(orig, f.instances, GameVersion::GTA3, out, err));
        CHECK(out == orig);
        // move one object: only its line changes, CRLF and the cull section are kept
        f.instances[1].x = 100.5;
        CHECK(replaceIplInstSection(orig, f.instances, GameVersion::GTA3, out, err));
        CHECK(out.find("1451, other, 100.5, 2, 3, 1, 1, 1, 0, 0, 0, 1\r\n") != std::string::npos);
        CHECK(out.find("cull\r\n10, 20, 30, 1, 2, 3, 4\r\nend\r\n") != std::string::npos);
        CHECK(out.find("# header comment\r\n") == 0);
        // deleting an instance removes its line
        f.instances.erase(f.instances.begin());
        CHECK(replaceIplInstSection(orig, f.instances, GameVersion::GTA3, out, err));
        CHECK(out.find("hospital") == std::string::npos && out.find("1451") != std::string::npos);
        // LF files stay LF
        CHECK(replaceIplInstSection("inst\n1, a, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1\nend\n", f.instances, GameVersion::GTA3, out, err));
        CHECK(out.find('\r') == std::string::npos);
        // a file without an inst block (GTA3.ZON: a zone block only): unchanged when it has no placements, and with
        // placements an inst block at the top, its own lines kept after it, in its own line endings
        CHECK(replaceIplInstSection("zone\r\nz1, 0, 1, 2, 3, 4, 5, 6\r\nend\r\n", {}, GameVersion::GTA3, out, err) && out == "zone\r\nz1, 0, 1, 2, 3, 4, 5, 6\r\nend\r\n");
        CHECK(replaceIplInstSection("zone\r\nz1, 0, 1, 2, 3, 4, 5, 6\r\nend\r\n", f.instances, GameVersion::GTA3, out, err));
        CHECK(out == "inst\r\n" + formatIplInstanceLine(f.instances[0], GameVersion::GTA3) + "\r\nend\r\nzone\r\nz1, 0, 1, 2, 3, 4, 5, 6\r\nend\r\n");
        {
            IplFile back = parseTextIpl(out, GameVersion::GTA3);
            CHECK(back.instances.size() == 1 && back.instances[0].model == f.instances[0].model && back.warnings.empty());
            std::string again;  // and saving that again finds the new block
            CHECK(replaceIplInstSection(out, back.instances, GameVersion::GTA3, again, err) && again == out);
        }
        CHECK(replaceIplInstSection("cull\n1\nend\n", f.instances, GameVersion::GTA3, out, err) && out.find("inst\n") == 0 && out.find('\r') == std::string::npos);
        // an inst block without its end line is still refused, not mangled
        CHECK(!replaceIplInstSection("inst\n1, a, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1\n", f.instances, GameVersion::GTA3, out, err));
        // numbers use 12 significant digits like QString::number(x, 'g', 12)
        IplInstance prec;
        prec.id = 5;
        prec.model = "m";
        prec.x = 1145.123456789012345;
        CHECK(formatIplInstanceLine(prec, GameVersion::GTA3) == "5, m, 1145.12345679, 0, 0, 1, 1, 1, 0, 0, 0, 1");
        // three-digit exponents, as the Visual C++ 7.1 runtime (and the game's files) print them
        prec.rotZ = -8.74228e-8;
        prec.rotX = 1.25607e-15;
        CHECK(formatIplInstanceLine(prec, GameVersion::GTA3).find("1.25607e-015") != std::string::npos);
        CHECK(formatIplInstanceLine(prec, GameVersion::GTA3).find("-8.74228e-008") != std::string::npos);
    }

    {  // pool sorting (pools.h)
        std::set<std::string> od = parseObjectDatNames("; comment\nBin_Bag, 10, 1\n  Crate ,1\n*\nafter_end, 1\n");
        CHECK(od.size() == 2 && od.count("bin_bag") && od.count("crate"));
        IdeFile ide = parseIde("objs\n1, a, a, 1, 100, 0\nend\npath\nped, 1, a\n\t0, 1, 2\nend\npath\ncar, 7, b\nend\n", GameVersion::GTA3);
        std::set<int> ids;
        collectPathModelIds(ide, ids);
        CHECK(ids.size() == 2 && ids.count(1) && ids.count(7));
        CHECK(poolFor(true, true) == Pool::Dummy && poolFor(false, true) == Pool::Treadable && poolFor(false, false) == Pool::Building);
        CHECK(kPoolCapacity[0] + kPoolCapacity[1] + kPoolCapacity[2] == 9516);
    }

    if (failures == 0) std::printf("all parser tests passed\n");
    return failures ? 1 : 0;
}
