// Tests for core/imgedit.h: staged archive edits (MEd's Image Archive tool) and the safe rebuild.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

#include "imgedit.h"
#include "textutil.h"

using namespace med;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) {                                                    \
            ++failures;                                                \
            std::printf("FAIL line %d: %s\n", __LINE__, #c);           \
        }                                                              \
    } while (0)

static std::vector<unsigned char> bytesOf(size_t n, unsigned seed) {
    std::vector<unsigned char> v(n);
    std::mt19937 r(seed);
    for (auto& b : v) b = static_cast<unsigned char>(r());
    return v;
}
static std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}
static void writeFile(const fs::path& p, const std::vector<unsigned char>& v) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(v.data()), std::streamsize(v.size()));
}
static std::vector<unsigned char> padded(std::vector<unsigned char> v) {
    v.resize((v.size() + 2047) / 2048 * 2048, 0);
    return v;
}

// The archive on disk holds exactly `expect` (name -> data before padding), in this order.
static bool archiveIs(const std::string& img, const std::vector<std::pair<std::string, std::vector<unsigned char>>>& expect) {
    ImgArchive a;
    std::string e;
    if (!ImgArchive::open(img, a, e) || a.entries().size() != expect.size()) return false;
    uint32_t next = a.version() == 2 ? (8 + uint32_t(expect.size()) * 32 + 2047) / 2048 : 0;
    for (size_t i = 0; i < expect.size(); ++i) {
        std::vector<unsigned char> d;
        if (a.entries()[i].name != expect[i].first || !a.extract(expect[i].first, d, e) || d != padded(expect[i].second)) return false;
        if (a.entries()[i].offset != next && a.entries()[i].size != 0) return false;  // packed back to back
        next += a.entries()[i].size;
    }
    std::error_code ec;
    return fs::file_size(img, ec) == uint64_t(next) * 2048;
}

int main() {
    const fs::path root = fs::temp_directory_path() / ("med_test_imgedit_" + std::to_string(::GetCurrentProcessId()));
    fs::remove_all(root);
    fs::create_directories(root / "files");

    // ---- names and types ----
    std::string why;
    CHECK(std::string(imgFileType("a.DFF")) == "3D Model" && std::string(imgFileType("b.txd")) == "Texture Dictionary");
    CHECK(std::string(imgFileType("c.col")) == "Collision File" && std::string(imgFileType("d.ipl")) == "Binary IPL File");
    CHECK(std::string(imgFileType("e.ifp")) == "Unknown" && std::string(imgFileType("noext")) == "Unknown");
    CHECK(validImgEntryName("hospitaland.dff", why) && validImgEntryName("abcdefghijklmnopqrs.dff", why));  // 23
    CHECK(!validImgEntryName("abcdefghijklmnopqrst.dff", why) && !why.empty());                            // 24
    for (const char* bad : {"", "a b.dff", "a/b.dff", "a\\b.dff", "tab\t.dff", "\xe9.dff"}) CHECK(!validImgEntryName(bad, why));

    // ---- a version 1 archive with five entries (one empty placeholder, as Rockstar's has) ----
    const std::string img = (root / "gta3.img").string();
    std::vector<std::pair<std::string, std::vector<unsigned char>>> model;
    {
        ImgArchive a;
        CHECK(ImgArchive::create(img, 1, a, why));
        const char* names[] = {"one.dff", "two.txd", "three.col", "four.dff"};
        size_t sizes[] = {5000, 2048, 100, 70000};
        for (int i = 0; i < 4; ++i) {
            model.push_back({names[i], bytesOf(sizes[i], unsigned(i + 1))});
            CHECK(a.add(names[i], model.back().second, why));
        }
        CHECK(a.save(why));
    }
    {  // and an empty entry at the end, written straight into the directory
        std::ofstream d(root / "gta3.dir", std::ios::binary | std::ios::app);
        unsigned char rec[32] = {};
        std::memcpy(rec + 8, "empty.dff", 9);
        d.write(reinterpret_cast<const char*>(rec), 32);
        model.push_back({"empty.dff", {}});
    }
    CHECK(archiveIs(img, model));
    const std::string origImg = readFile(img), origDir = readFile(root / "gta3.dir");

    ImgArchive arc;
    CHECK(ImgArchive::open(img, arc, why));
    ImgEdit ed(arc);
    CHECK(!ed.changed() && ed.entries().size() == 5 && ed.find("TWO.TXD") == 1 && ed.find("nothing") < 0);

    // ---- refusals change nothing ----
    CHECK(!ed.remove("missing.dff", why) && !ed.changed());
    CHECK(!ed.rename("missing.dff", "x.dff", why) && !ed.changed());
    CHECK(!ed.rename("one.dff", "two.txd", why) && !ed.changed());      // taken
    CHECK(!ed.rename("one.dff", "TWO.TXD", why) && !ed.changed());      // taken, any case
    CHECK(!ed.rename("one.dff", "has space.dff", why) && !ed.changed());
    CHECK(ed.rename("one.dff", "one.dff", why) && !ed.changed());       // the same name: nothing to do
    writeFile(root / "files" / "new.dff", bytesOf(3000, 77));
    writeFile(root / "files" / "two.txd", bytesOf(9000, 78));
    writeFile(root / "files" / "empty.bin", {});
    CHECK(!ed.addFile((root / "files" / "two.txd").string(), "", false, why) && !ed.changed());  // exists, no replace
    CHECK(!ed.addFile((root / "files" / "missing").string(), "", false, why) && !ed.changed());
    CHECK(!ed.addFile((root / "files" / "empty.bin").string(), "", false, why) && !ed.changed());
    CHECK(!ed.addFile((root / "files" / "new.dff").string(), "much_too_long_a_name_here.dff", false, why) && !ed.changed());

    // ---- staged changes ----
    CHECK(ed.remove("three.col", why));
    CHECK(ed.rename("four.dff", "Four2.dff", why));
    CHECK(ed.addFile((root / "files" / "new.dff").string(), "", false, why));
    CHECK(ed.addFile((root / "files" / "two.txd").string(), "", true, why));
    CHECK(ed.changed() && ed.entries().size() == 5);
    CHECK(ImgEdit::status(ed.entries()[size_t(ed.find("four2.dff"))]) == "renamed from four.dff");
    CHECK(ImgEdit::status(ed.entries()[size_t(ed.find("new.dff"))]) == "added");
    CHECK(ImgEdit::status(ed.entries()[size_t(ed.find("two.txd"))]) == "replaced");
    CHECK(ed.find("two.txd") == 1);  // a replacement keeps the place
    CHECK(ImgEdit::status(ed.entries()[0]).empty());
    CHECK(readFile(img) == origImg && readFile(root / "gta3.dir") == origDir);  // nothing on disk yet

    // ---- a failed rebuild leaves everything as it was ----
    {
        std::ofstream(root / "gta3.med-new.img") << "stale";
        ImgArchive r;
        ImgRebuildReport rep;
        CHECK(!rebuildImg(ed, r, rep, why) && why.find("in the way") != std::string::npos);
        fs::remove(root / "gta3.med-new.img");
    }
#ifdef _WIN32
    {  // the game holding the archive open: the swap fails and is rolled back
        HANDLE h = CreateFileA(img.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        CHECK(h != INVALID_HANDLE_VALUE);
        ImgArchive r;
        ImgRebuildReport rep;
        CHECK(!rebuildImg(ed, r, rep, why));
        CloseHandle(h);
        CHECK(readFile(img) == origImg && readFile(root / "gta3.dir") == origDir);
        CHECK(!fs::exists(root / "gta3.med-new.img") && !fs::exists(root / "gta3.med-new.dir") && !fs::exists(root / "gta3.med-old.img"));
        CHECK(!fs::exists(img + ".med-backup"));
    }
    {  // the directory held open: rolled back after the archive was already moved
        HANDLE h = CreateFileA((root / "gta3.dir").string().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        ImgArchive r;
        ImgRebuildReport rep;
        CHECK(!rebuildImg(ed, r, rep, why));
        CloseHandle(h);
        CHECK(readFile(img) == origImg && readFile(root / "gta3.dir") == origDir);
        CHECK(!fs::exists(root / "gta3.med-new.img") && !fs::exists(root / "gta3.med-old.img") && !fs::exists(root / "gta3.med-old.dir"));
    }
#endif
    {  // an added file that changed since it was added
        writeFile(root / "files" / "new.dff", bytesOf(3001, 77));
        ImgArchive r;
        ImgRebuildReport rep;
        CHECK(!rebuildImg(ed, r, rep, why) && why.find("changed") != std::string::npos);
        writeFile(root / "files" / "new.dff", bytesOf(3000, 77));
        CHECK(readFile(img) == origImg);
    }

    // ---- a hard link elsewhere (the self-test copies) is never written through ----
    fs::create_directories(root / "linked");
    fs::create_hard_link(img, root / "linked" / "gta3.img");
    fs::create_hard_link(root / "gta3.dir", root / "linked" / "gta3.dir");

    // ---- the rebuild ----
    {
        ImgArchive r;
        ImgRebuildReport rep;
        size_t calls = 0, lastDone = 0;
        CHECK(rebuildImg(ed, r, rep, why, [&](size_t done, size_t total) { ++calls; lastDone = done; (void)total; }));
        CHECK(calls == 5 && lastDone == 5);
        std::vector<std::pair<std::string, std::vector<unsigned char>>> expect = {
            {"one.dff", model[0].second}, {"two.txd", bytesOf(9000, 78)}, {"Four2.dff", model[3].second}, {"empty.dff", {}}, {"new.dff", bytesOf(3000, 77)}};
        CHECK(archiveIs(img, expect));
        CHECK(r.entries().size() == 5 && r.find("four2.dff") == 2);
        CHECK(rep.backupMade && readFile(img + ".med-backup") == origImg && readFile((root / "gta3.dir").string() + ".med-backup") == origDir);
        CHECK(readFile(root / "linked" / "gta3.img") == origImg && readFile(root / "linked" / "gta3.dir") == origDir);
        CHECK(!fs::exists(root / "gta3.med-new.img") && !fs::exists(root / "gta3.med-old.img") && !fs::exists(root / "gta3.med-old.dir"));
        CHECK(rep.entries == 5 && rep.bytesBefore == origImg.size() && rep.bytesAfter == fs::file_size(img));
        // a second rebuild (nothing staged: compacts only) keeps the first backup
        ImgEdit again(r);
        ImgArchive r2;
        CHECK(rebuildImg(again, r2, rep, why) && !rep.backupMade && archiveIs(img, expect));
        CHECK(readFile(img + ".med-backup") == origImg);
        model = expect;
    }

    // ---- a damaged archive (an entry past the end) is refused ----
    {
        const fs::path dimg = root / "damaged.img";
        ImgArchive a;
        CHECK(ImgArchive::create(dimg.string(), 1, a, why));
        CHECK(a.add("x.dff", bytesOf(4000, 9), why) && a.save(why));
        fs::resize_file(dimg, 2048);  // cut the second sector off
        ImgArchive b;
        CHECK(ImgArchive::open(dimg.string(), b, why));
        ImgEdit de(b);
        ImgArchive r;
        ImgRebuildReport rep;
        const std::string before = readFile(dimg);
        CHECK(!rebuildImg(de, r, rep, why) && why.find("damaged") != std::string::npos && readFile(dimg) == before);
        CHECK(!fs::exists(root / "damaged.med-new.img"));
    }

    // ---- version 2 (San Andreas): one file, the directory inside ----
    {
        const std::string v2 = (root / "gta3sa.img").string();
        ImgArchive a;
        CHECK(ImgArchive::create(v2, 2, a, why));
        std::vector<std::pair<std::string, std::vector<unsigned char>>> m2;
        for (int i = 0; i < 70; ++i) {  // 70 entries: the directory needs two sectors
            m2.push_back({"e" + std::to_string(i) + ".dff", bytesOf(size_t(100 + i * 300), unsigned(i + 100))});
            CHECK(a.add(m2.back().first, m2.back().second, why));
        }
        CHECK(a.save(why));
        ImgArchive b;
        CHECK(ImgArchive::open(v2, b, why) && b.version() == 2);
        ImgEdit e2(b);
        CHECK(e2.remove("e5.dff", why) && e2.rename("e6.dff", "six.dff", why));
        m2.erase(m2.begin() + 5);
        m2[5].first = "six.dff";
        ImgArchive r;
        ImgRebuildReport rep;
        CHECK(rebuildImg(e2, r, rep, why) && r.version() == 2 && archiveIs(v2, m2) && rep.backupMade && rep.backupDir.empty());
    }

    // ---- random edits and rebuilds against an independent model ----
    std::mt19937 rng(4242);
    int rebuilds = 0, applied = 0;
    for (int step = 0; step < 400 && failures < 20; ++step) {
        ImgArchive cur;
        CHECK(ImgArchive::open(img, cur, why));
        ImgEdit e(cur);
        auto expect = model;
        const int n = 1 + int(rng() % 6);
        for (int k = 0; k < n; ++k) {
            const int op = int(rng() % 4);
            const std::string some = expect.empty() ? "none.dff" : expect[rng() % expect.size()].first;
            if (op == 0) {
                const bool ok = e.remove(some, why);
                CHECK(ok == !expect.empty());
                for (size_t i = 0; i < expect.size(); ++i) if (toLower(expect[i].first) == toLower(some)) { expect.erase(expect.begin() + long(i)); break; }
            } else if (op == 1) {
                const std::string to = "r" + std::to_string(rng() % 40) + ".dff";
                bool taken = false;
                for (auto& x : expect) taken = taken || (toLower(x.first) == toLower(to) && toLower(x.first) != toLower(some));
                const bool ok = e.rename(some, to, why);
                CHECK(ok == (!expect.empty() && !taken));
                if (ok) for (auto& x : expect) if (toLower(x.first) == toLower(some)) { x.first = to; break; }
            } else {
                const std::string name = "f" + std::to_string(rng() % 40) + (op == 2 ? ".txd" : ".col");
                const auto data = bytesOf(1 + rng() % 9000, unsigned(rng()));
                const fs::path file = root / "files" / ("s" + std::to_string(step) + "_" + std::to_string(k));
                writeFile(file, data);
                const bool replace = rng() % 2 == 0;
                int at = -1;
                for (size_t i = 0; i < expect.size(); ++i) if (toLower(expect[i].first) == toLower(name)) at = int(i);
                const bool ok = e.addFile(file.string(), name, replace, why);
                CHECK(ok == (at < 0 || replace));
                if (ok) { if (at >= 0) expect[size_t(at)] = {name, data}; else expect.push_back({name, data}); }
            }
            ++applied;
        }
        CHECK(int(e.entries().size()) == int(expect.size()));
        if (rng() % 3 == 0) continue;  // staged only, thrown away: the archive must not change
        ImgArchive r;
        ImgRebuildReport rep;
        CHECK(rebuildImg(e, r, rep, why));
        ++rebuilds;
        CHECK(archiveIs(img, expect));
        model = expect;
    }
    CHECK(archiveIs(img, model) && rebuilds > 100);
    CHECK(readFile(root / "linked" / "gta3.img") == origImg);  // still the original through all of it

    fs::remove_all(root);
    std::printf("%s (%d failures, %d rebuilds, %d staged changes)\n", failures ? "FAILED" : "ok", failures, rebuilds, applied);
    return failures ? 1 : 0;
}
