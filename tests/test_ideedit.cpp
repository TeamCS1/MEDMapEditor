// Tests for core/ideedit.h: finding and changing one objs / tobj definition inside an IDE file.
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>
#include <string>

#include "ideedit.h"

using namespace med;

static int failures = 0;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) {                                                    \
            ++failures;                                                \
            std::printf("FAIL line %d: %s\n", __LINE__, #c);           \
        }                                                              \
    } while (0)

// Shaped like Rockstar's files: CRLF, a comment, objs with decimals and a two-atomic model, tobj, and a path block
// whose lines also start with numbers.
static const char* kIde =
    "# a comment, 5, x\r\n"
    "objs\r\n"
    "435, iten_block07, inditaly1, 1, 110, 0\r\n"
    "2077, tenament1ad, tenblock6aa, 1, 152.25, 4\r\n"
    "900, twoParts, twotex, 2, 30, 150, 0\r\n"
    "end\r\n"
    "tobj\r\n"
    "2209, block4_scplights, comlights, 1, 3000, 12, 18, 3\r\n"
    "end\r\n"
    "path\r\n"
    "car, 435, iten_block07\r\n"
    "\t2, -1, 0, 0, 0, 0, 160, 1, 1\r\n"
    "end\r\n";

static IdeObject parsed(const std::string& text, int id) {
    IdeEntryLoc loc;
    if (!findIdeEntry(text, id, loc)) return IdeObject{};
    return loc.object;
}

// every definition other than `id` is unchanged, and the file parses without warnings
static bool othersSame(const std::string& before, const std::string& after, int id) {
    IdeFile a = parseIde(before, GameVersion::GTA3), b = parseIde(after, GameVersion::GTA3);
    if (!b.warnings.empty() || a.objects.size() != b.objects.size()) return false;
    if (a.otherSections.size() != b.otherSections.size()) return false;
    for (size_t i = 0; i < a.otherSections.size(); ++i)
        if (a.otherSections[i].lines != b.otherSections[i].lines) return false;
    for (const IdeObject& o : a.objects) {
        if (o.id == id) continue;
        bool found = false;
        for (const IdeObject& p : b.objects)
            if (p.id == o.id) found = p.model == o.model && p.txd == o.txd && p.drawDistances == o.drawDistances && p.flags == o.flags &&
                                         p.timed == o.timed && (!o.timed || (p.timeOn == o.timeOn && p.timeOff == o.timeOff));
        if (!found) return false;
    }
    return true;
}

int main() {
    const std::string ide = kIde;

    // ---- finding ----
    IdeEntryLoc loc;
    CHECK(findIdeEntry(ide, 2077, loc) && !loc.timed && ide.substr(loc.begin, loc.end - loc.begin) == "2077, tenament1ad, tenblock6aa, 1, 152.25, 4");
    CHECK(loc.object.drawDistances.size() == 1 && loc.object.drawDistances[0] == 152.25 && loc.object.flags == 4);
    CHECK(findIdeEntry(ide, 2209, loc) && loc.timed && loc.object.timeOn == 18 && loc.object.timeOff == 3);
    CHECK(findIdeEntry(ide, 900, loc) && loc.object.drawDistances.size() == 2);
    CHECK(!findIdeEntry(ide, 5, loc));     // only in the comment
    CHECK(!findIdeEntry(ide, 2, loc));     // only a path line starts with 2
    CHECK(!findIdeEntry(ide, 12345, loc));
    CHECK(findIdeEntry("OBJS\nend\nObjs\n7, a, b, 1, 9, 0\nEND\n", 7, loc) && !loc.timed);  // case of section names
    CHECK(findIdeEntry("objs\n7, a, b, 1, 9, 0", 7, loc));  // no line ending at the end of the file

    // ---- formatting: Rockstar's own lines come back exactly ----
    for (int id : {435, 2077, 900, 2209}) {
        CHECK(findIdeEntry(ide, id, loc));
        CHECK(formatIdeEntry(loc.object) == ide.substr(loc.begin, loc.end - loc.begin));
    }

    // ---- an edit changes only that line ----
    {
        std::string t = ide, why;
        IdeObject o = parsed(t, 2077);
        o.drawDistances[0] = 250.5;
        o.flags = 4 | 64;
        o.txd = "newtex";
        CHECK(setIdeEntry(t, o, why));
        const std::string expect = std::string(kIde).replace(ide.find("2077,"), std::string("2077, tenament1ad, tenblock6aa, 1, 152.25, 4").size(),
                                                              "2077, tenament1ad, newtex, 1, 250.5, 68");
        CHECK(t == expect);
        CHECK(othersSame(ide, t, 2077));
        IdeObject back = parsed(t, 2077);
        CHECK(back.drawDistances[0] == 250.5 && back.flags == 68 && back.txd == "newtex" && !back.timed);
    }
    {  // both distances of a two-atomic model; an LF file stays LF
        std::string lf = "objs\n900, twoParts, twotex, 2, 30, 150, 0\nend\n", why;
        IdeObject o = parsed(lf, 900);
        o.drawDistances = {35, 175.125};
        CHECK(setIdeEntry(lf, o, why));
        CHECK(lf == "objs\n900, twoParts, twotex, 2, 35, 175.125, 0\nend\n");
    }
    {  // tobj hours
        std::string t = ide, why;
        IdeObject o = parsed(t, 2209);
        o.timeOn = 20; o.timeOff = 6;
        CHECK(setIdeEntry(t, o, why));
        CHECK(t.find("2209, block4_scplights, comlights, 1, 3000, 12, 20, 6\r\n") != std::string::npos && othersSame(ide, t, 2209));
    }

    // ---- objs <-> tobj ----
    {
        std::string t = ide, why;
        IdeObject o = parsed(t, 435);
        o.timed = true; o.timeOn = 20; o.timeOff = 6;
        CHECK(setIdeEntry(t, o, why));
        CHECK(t.find("435, iten_block07") > t.find("tobj"));  // now in the tobj block
        CHECK(t.find("435, iten_block07, inditaly1, 1, 110, 0, 20, 6\r\nend\r\npath") != std::string::npos);
        CHECK(findIdeEntry(t, 435, loc) && loc.timed && othersSame(ide, t, 435));
        o.timed = false;  // and back: the end of the objs block
        CHECK(setIdeEntry(t, o, why));
        CHECK(t.find("900, twoParts, twotex, 2, 30, 150, 0\r\n435, iten_block07, inditaly1, 1, 110, 0\r\nend\r\ntobj") != std::string::npos);
        CHECK(findIdeEntry(t, 435, loc) && !loc.timed && othersSame(ide, t, 435));
        CHECK(t.size() == ide.size());  // the same lines, one moved
    }
    {  // a file without a tobj section gets one at the end, with its own line endings
        std::string t = "objs\r\n1, a, b, 1, 100, 0\r\nend", why;
        IdeObject o = parsed(t, 1);
        o.timed = true; o.timeOn = 19; o.timeOff = 5;
        CHECK(setIdeEntry(t, o, why));
        CHECK(t == "objs\r\nend\r\ntobj\r\n1, a, b, 1, 100, 0, 19, 5\r\nend\r\n");
        CHECK(findIdeEntry(t, 1, loc) && loc.timed && parseIde(t, GameVersion::GTA3).warnings.empty());
    }

    // ---- refusals leave the text alone ----
    {
        const IdeObject good = parsed(ide, 2077);
        auto refused = [&](IdeObject o) {
            std::string t = ide, why;
            const bool ok = setIdeEntry(t, o, why);
            return !ok && t == ide && !why.empty();
        };
        IdeObject o = good; o.id = 2078; CHECK(refused(o));
        o = good; o.model = "other"; CHECK(refused(o));
        o = good; o.drawDistances.push_back(5); CHECK(refused(o));
        for (double d : {-0.5, -1.0, 10000.5, std::nan(""), std::numeric_limits<double>::infinity()}) { o = good; o.drawDistances[0] = d; CHECK(refused(o)); }
        o = good; o.drawDistances[0] = 0; { std::string t = ide, why; CHECK(setIdeEntry(t, o, why)); }
        o = good; o.drawDistances[0] = kMaxDrawDistance; { std::string t = ide, why; CHECK(setIdeEntry(t, o, why)); }
        for (int f : {-1, 2048, 1 << 20}) { o = good; o.flags = f; CHECK(refused(o)); }
        for (const char* x : {"", "has space", "a,b", "#x", "twentycharacterslong"}) { o = good; o.txd = x; CHECK(refused(o)); }
        o = good; o.txd = "nineteencharacters1"; { std::string t = ide, why; CHECK(setIdeEntry(t, o, why)); }
        o = good; o.timed = true; o.timeOn = 24; o.timeOff = 6; CHECK(refused(o));
        o = good; o.timed = true; o.timeOn = 5; o.timeOff = -1; CHECK(refused(o));
        o = good; o.timed = true; o.timeOn = 7; o.timeOff = 7; CHECK(refused(o));
        o = good; o.timed = false; o.timeOn = 7; o.timeOff = 7; { std::string t = ide, why; CHECK(setIdeEntry(t, o, why)); }  // hours only matter for tobj
        o = good; o.id = 4444; o.model = "nothing"; { std::string t = ide, why; CHECK(!setIdeEntry(t, o, why) && t == ide); }
    }

    // ---- 2dfx lights, read as MEd reads them ----
    {
        const IdeFile f = parseIde(
            "2dfx\r\n"
            "2082, -33.7737, -15.3476, -0.129492, 117, 118, 119, 200, 0, \"coronastar\", \"shad_exp\", 100, 0, 2, 0, 40, 1, 0, 0, 0\r\n"
            "1887, 59.9111, 9.28214, -14.94, 255, 0, 10, 200, 0, \"coronastar\", \"shad_exp\", 100, 5, 1.5, 2, 40, 12, 0, 0, 0\r\n"
            "1887, 1, 2, 3, 255, 255, 255, 200, 1, 5, 0, 0, 1, 2.5\r\n"  // a particle: 14 fields, not a light
            "end\r\n", GameVersion::GTA3);
        const std::vector<Ide2dfxLight> l = ide2dfxLights(f);
        CHECK(l.size() == 2);
        if (l.size() == 2) {
            CHECK(l[0].id == 2082 && std::fabs(l[0].x + 33.7737f) < 1e-4f && std::fabs(l[0].z + 0.129492f) < 1e-6f && l[0].r == 117 && l[0].g == 118 && l[0].b == 119);
            CHECK(l[0].size == 2 && l[0].flash == 1);
            CHECK(l[1].id == 1887 && l[1].size == 1.5f && l[1].flash == 12 && l[1].r == 255 && l[1].g == 0 && l[1].b == 10);
        }
        // MEd's rule (0x00416a70): even types before 21:00, odd ones after 19:00, 12 always
        for (int hour = 0; hour < 24; ++hour) {
            CHECK(ide2dfxLightShown(0, hour) == (hour <= 20) && ide2dfxLightShown(10, hour) == (hour <= 20));
            CHECK(ide2dfxLightShown(1, hour) == (hour >= 20) && ide2dfxLightShown(11, hour) == (hour >= 20));
            CHECK(ide2dfxLightShown(12, hour) && !ide2dfxLightShown(13, hour) && !ide2dfxLightShown(-1, hour));
        }
    }

    // ---- the flag table ----
    for (int i = 0; i < kIdeFlagCount; ++i) {
        CHECK(ideFlag(i).bit == 1 << i);
        CHECK(ideFlag(i).usedByGta3 == (i < 7));  // the game's loaders read bits 1 to 64 only
    }

    // ---- random edits: after each, the edited definition reads back exactly and nothing else changed ----
    std::mt19937 rng(2026);
    std::string t = ide;
    const int ids[4] = {435, 2077, 900, 2209};
    int done = 0;
    for (int step = 0; step < 3000; ++step) {
        const int id = ids[rng() % 4];
        IdeObject o = parsed(t, id);
        const std::string before = t;
        switch (rng() % 5) {
            case 0: for (double& d : o.drawDistances) d = double(rng() % 400000) / 40.0 - 5; break;  // some invalid
            case 1: o.flags = int(rng() % 2300) - 50; break;
            case 2: o.timed = !o.timed; o.timeOn = int(rng() % 26) - 1; o.timeOff = int(rng() % 26) - 1; break;
            case 3: { static const char* names[] = {"tex", "a", "x y", "", "lights2", "verylongtexturename1"}; o.txd = names[rng() % 6]; break; }
            case 4: o.timeOn = int(rng() % 24); o.timeOff = (o.timeOn + 1 + int(rng() % 23)) % 24; break;
        }
        std::string why;
        const bool valid = validIdeEdit(parsed(before, id), o, why);
        const bool ok = setIdeEntry(t, o, why);
        CHECK(ok == valid);
        if (!ok) { CHECK(t == before); continue; }
        ++done;
        const IdeObject back = parsed(t, id);
        CHECK(back.drawDistances == o.drawDistances && back.flags == o.flags && back.txd == o.txd && back.timed == o.timed);
        if (!(back.drawDistances == o.drawDistances && back.txd == o.txd && back.flags == o.flags && back.timed == o.timed)) std::printf("  distance %.17g -> read back %.17g (txd %s)\n", o.drawDistances[0], back.drawDistances[0], back.txd.c_str());
        if (o.timed) CHECK(back.timeOn == o.timeOn && back.timeOff == o.timeOff);
        CHECK(othersSame(before, t, id));
        if (failures > 20) break;
    }
    CHECK(done > 500);

    std::printf("%s (%d failures, %d random edits applied)\n", failures ? "FAILED" : "ok", failures, done);
    return failures ? 1 : 0;
}
