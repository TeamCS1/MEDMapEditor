#include "mapcheck.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <set>
#include <tuple>
#include <unordered_map>
#include "col.h"
#include "ide.h"
#include "img.h"
#include "ipl.h"
#include "pools.h"
#include "textutil.h"

namespace med {

namespace {

// Generated from the unmodified GTA III 1.0 data by `validate_gta3 <folder> --baseline` (see CLAUDE.md).
// Each entry is "model|x|y|z" with the position in hundredths. Three null terminated lists.
#include "mapcheck_baseline.inc"

constexpr size_t kGameLineLength = 255;  // CFileLoader::LoadLine reads a 256 byte buffer (0x004761d0)

std::string readFile(const std::string& path, bool& ok) {
    std::ifstream f(path, std::ios::binary);
    ok = static_cast<bool>(f);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

std::string slashes(std::string s) {
    for (char& c : s) if (c == '\\') c = '/';
    return s;
}

// The game's line reader: cut at 255 characters, control characters and commas become spaces.
std::string gameLine(std::string raw, bool* truncated = nullptr) {
    while (!raw.empty() && (raw.back() == '\r' || raw.back() == '\n')) raw.pop_back();
    const bool cut = raw.size() > kGameLineLength;
    if (truncated) *truncated = cut;
    if (cut) raw.resize(kGameLineLength);
    for (char& c : raw)
        if (static_cast<unsigned char>(c) < 0x20 || c == ',') c = ' ';
    return raw;
}

std::string skipBlanks(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && static_cast<unsigned char>(s[i]) < 0x21) ++i;
    return s.substr(i);
}

bool startsWith(const std::string& s, const char* prefix) {
    return s.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

std::string firstWord(const std::string& s) {
    size_t e = s.find_first_of(" \t");
    return e == std::string::npos ? s : s.substr(0, e);
}

std::string afterWords(const std::string& s, int words) {
    size_t i = 0;
    for (int w = 0; w < words; ++w) {
        while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    }
    return trim(s.substr(i));
}

void add(CheckResult& r, Severity sev, const char* code, std::string message, std::string file = "", int line = 0) {
    CheckIssue i;
    i.severity = sev;
    i.code = code;
    i.message = std::move(message);
    i.file = std::move(file);
    i.line = line;
    r.issues.push_back(std::move(i));
}

void addAt(CheckResult& r, Severity sev, const char* code, std::string message, std::string file, int line, double x, double y,
           double z) {
    add(r, sev, code, std::move(message), std::move(file), line);
    CheckIssue& i = r.issues.back();
    i.hasPosition = true;
    i.x = x;
    i.y = y;
    i.z = z;
}

struct Placed {
    GamePlacement p;
    std::string file;
    int line = 0;
};

struct Def {
    std::string model, txd, file;
};

bool isLodName(const std::string& n) { return n.size() > 3 && toLower(n.substr(0, 3)) == "lod"; }
std::string tailOf(const std::string& n) { return toLower(n.substr(3)); }

std::string positionText(double x, double y, double z) {
    char b[96];
    std::snprintf(b, sizeof b, "(%.1f, %.1f, %.1f)", x, y, z);
    return b;
}

}  // namespace

int gameParsePlacement(const std::string& rawLine, GamePlacement& out, bool* lineTruncated) {
    const std::string s = gameLine(rawLine, lineTruncated);
    const char* p = s.c_str();
    char* end = nullptr;
    int n = 0;
    auto skipWs = [&] { while (*p && std::isspace(static_cast<unsigned char>(*p))) ++p; };
    skipWs();
    long id = std::strtol(p, &end, 10);  // %d
    if (end == p) return n;
    out.id = static_cast<int>(id);
    p = end;
    ++n;
    skipWs();  // %s
    if (!*p) return n;
    const char* b = p;
    while (*p && !std::isspace(static_cast<unsigned char>(*p))) ++p;
    out.name.assign(b, p);
    ++n;
    double* fields[10] = {&out.x, &out.y, &out.z, &out.sx, &out.sy, &out.sz, &out.rx, &out.ry, &out.rz, &out.rw};
    for (double* f : fields) {  // %f x 10
        skipWs();
        double d = std::strtod(p, &end);
        if (end == p) return n;
        *f = d;
        p = end;
        ++n;
    }
    return n;
}

CheckResult checkMap(const CheckInput& in) {
    CheckResult r;
    const std::string root = in.root;
    auto exists = [&](const std::string& rel) {
        bool ok;
        readFile(root + "/" + rel, ok);
        return ok;
    };

    // ---- gta3.dat and default.dat, read the way CFileLoader::LoadLevel reads them (0x00476290) ----------
    struct Listed { std::string path; std::string from; int line; };
    std::vector<Listed> ides, ipls, cols, others;
    for (const char* levelFile : {"data/default.dat", "data/gta3.dat"}) {
        bool ok;
        std::string text = readFile(root + "/" + levelFile, ok);
        if (!ok) {
            add(r, Severity::Error, "level-missing", std::string("The game cannot start: ") + levelFile + " is missing.", levelFile);
            continue;
        }
        size_t pos = 0;
        int lineNo = 0;
        while (pos < text.size()) {
            size_t nl = text.find('\n', pos);
            std::string raw = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
            pos = nl == std::string::npos ? text.size() : nl + 1;
            ++lineNo;
            std::string line = skipBlanks(gameLine(raw));
            if (line.empty() || line[0] == '#') continue;
            std::string kw = toLower(firstWord(line));
            if (kw == "exit") break;
            auto listed = [&](std::vector<Listed>& v, const std::string& path) { v.push_back({slashes(trim(path)), levelFile, lineNo}); };
            if (kw == "ide") listed(ides, afterWords(line, 1));
            else if (kw == "ipl") listed(ipls, afterWords(line, 1));
            else if (kw == "colfile") listed(cols, afterWords(line, 2));
            else if (kw == "texdiction" || kw == "modelfile" || kw == "hierfile" || kw == "cdimage" || kw == "mapzone")
                listed(others, afterWords(line, 1));
        }
    }
    for (const auto* list : {&ides, &ipls, &cols, &others})
        for (const Listed& l : *list)
            if (!exists(l.path))
                add(r, Severity::Error, "file-missing", "Listed in " + l.from + " but not found: " + l.path + ". The game cannot open it.", l.from, l.line);
    r.summary.ide = int(ides.size());
    r.summary.ipl = int(ipls.size());
    r.summary.col = int(cols.size());

    // ---- object definitions (IDE) ------------------------------------------------------------------
    std::map<int, Def> defs;
    std::set<int> pathIds;                // model ids with ped/car path data (treadables)
    std::set<std::string> lodModelTails;  // tails of LOD models that exist in the IDE
    for (const Listed& l : ides) {
        bool ok;
        std::string text = readFile(root + "/" + l.path, ok);
        if (!ok) continue;
        IdeFile f = parseIde(text, GameVersion::GTA3);
        collectPathModelIds(f, pathIds);
        for (const IdeObject& o : f.objects) {
            auto it = defs.find(o.id);
            if (it != defs.end() && toLower(it->second.model) != toLower(o.model))
                add(r, Severity::Warning, "ide-duplicate-id",
                    "Model id " + std::to_string(o.id) + " is defined twice with different names (" + it->second.model + " in " + it->second.file + ", " + o.model +
                        " in " + l.path + "). Only one of them can be used.", l.path);
            defs[o.id] = {o.model, o.txd, l.path};
            if (isLodName(o.model)) lodModelTails.insert(tailOf(o.model));
        }
    }
    r.summary.objectDefinitions = int(defs.size());

    // ---- game archive and collision --------------------------------------------------------------
    ImgArchive img;
    bool imgOk = false;
    std::set<std::string> imgNames;
    {
        std::string err;
        imgOk = ImgArchive::open(root + "/models/gta3.img", img, err);
        if (!imgOk) add(r, Severity::Error, "img-missing", "models/gta3.img cannot be opened (" + err + "). The game cannot load models.", "models/gta3.img");
        else {
            for (const ImgEntry& e : img.entries()) imgNames.insert(toLower(e.name));
            r.summary.imgEntries = int(imgNames.size());
        }
    }
    std::set<std::string> colNames;
    for (const Listed& l : cols) {
        bool ok;
        std::string bytes = readFile(root + "/" + l.path, ok);
        if (!ok) continue;
        std::vector<ColModel> models;
        std::string err;
        if (!parseCol(std::vector<unsigned char>(bytes.begin(), bytes.end()), models, err))
            add(r, Severity::Error, "col-unreadable", "Collision file " + l.path + " is damaged or not in the GTA III format (" + err + ").", l.path);
        for (const ColModel& m : models) colNames.insert(toLower(m.name));
    }

    // ---- placements, read the way CFileLoader::LoadScene reads them (0x00478370) -------------------------
    std::vector<Placed> placed;
    for (const Listed& l : ipls) {
        const std::string key = toLower(l.path);
        bool ok = true;
        std::string text;
        auto ov = in.iplText.find(key);
        if (ov != in.iplText.end()) text = ov->second;
        else text = readFile(root + "/" + l.path, ok);
        if (!ok) continue;
        int section = 0;
        std::string sectionName;
        int sectionLine = 0;
        size_t pos = 0;
        int lineNo = 0;
        while (pos < text.size()) {
            size_t nl = text.find('\n', pos);
            std::string raw = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
            pos = nl == std::string::npos ? text.size() : nl + 1;
            ++lineNo;
            bool truncated = false;
            std::string line = skipBlanks(gameLine(raw, &truncated));
            if (line.empty() || line[0] == '#') continue;
            if (section == 0) {
                if (startsWith(line, "inst")) section = 1;
                else if (startsWith(line, "zone")) section = 2;
                else if (startsWith(line, "cull")) section = 3;
                else if (startsWith(line, "pick")) section = 4;
                else if (startsWith(line, "path")) section = 5;
                else {
                    std::string w = toLower(firstWord(line));
                    if (w == "inst" || w == "zone" || w == "cull" || w == "pick" || w == "path")
                        add(r, Severity::Warning, "ipl-section-case",
                            "Section name \"" + firstWord(line) + "\" is not lowercase. The game only recognises lowercase names, so it ignores this whole section.", l.path, lineNo);
                }
                if (section != 0) { sectionName = toLower(firstWord(line)); sectionLine = lineNo; }
                continue;
            }
            if (startsWith(line, "end")) {
                section = 0;
                continue;
            }
            std::string w = toLower(firstWord(line));
            if (w == "end") {
                add(r, Severity::Warning, "ipl-end-case", "\"" + firstWord(line) + "\" is not lowercase \"end\". The game does not recognise it and keeps reading this section.", l.path, lineNo);
                continue;
            }
            if (section != 1) continue;
            if (truncated)
                add(r, Severity::Warning, "ipl-long-line", "This line is longer than 255 characters. The game reads only the first 255, so the end of the line is lost.", l.path, lineNo);
            Placed p;
            int values = gameParsePlacement(raw, p.p, nullptr);
            if (values < 12) {
                add(r, Severity::Warning, "ipl-short-line",
                    "Only " + std::to_string(values) + " of the 12 values of a placement could be read. The game silently skips this line, so the object will not exist: " + trim(raw), l.path, lineNo);
                continue;
            }
            p.file = l.path;
            p.line = lineNo;
            placed.push_back(std::move(p));
        }
        if (section != 0)
            add(r, Severity::Warning, "ipl-no-end", "The \"" + sectionName + "\" section starting here has no \"end\" line. The game reads to the end of the file as part of it.", l.path, sectionLine);
    }
    r.summary.placements = int(placed.size());

    // ---- per placement ---------------------------------------------------------------------------
    std::set<int> modelsUsed;
    std::map<int, int> placementsOfModel;
    std::map<int, const Placed*> firstOfModel;
    int nameMismatch = 0;
    const Placed* firstMismatch = nullptr;
    for (const Placed& pl : placed) {
        const GamePlacement& g = pl.p;
        const double vals[10] = {g.x, g.y, g.z, g.sx, g.sy, g.sz, g.rx, g.ry, g.rz, g.rw};
        bool finite = true;
        for (double v : vals) finite = finite && std::isfinite(v);
        if (!finite) {
            add(r, Severity::Error, "placement-not-finite", "A value is not a finite number. The game builds a corrupt matrix from it.", pl.file, pl.line);
            continue;
        }
        if (std::fabs(g.rw) > 1.0)
            addAt(r, Severity::Error, "placement-quaternion-w",
                  "Rotation w = " + std::to_string(g.rw) + " is outside -1 to 1. The game computes acos(w) with sqrt(1 - w*w), which is not a number here, and the object gets a corrupt matrix.",
                  pl.file, pl.line, g.x, g.y, g.z);
        if (g.z < -250.0)
            addAt(r, Severity::Warning, "placement-too-low",
                  "Height " + std::to_string(g.z) + " is far below the map. From experience -200 is safe and -2000 crashed the game (not verified in the game code).", pl.file, pl.line, g.x, g.y, g.z);
        auto d = defs.find(g.id);
        if (d == defs.end()) {
            addAt(r, Severity::Warning, "placement-unknown-model",
                  "Model id " + std::to_string(g.id) + " (" + g.name + ") is not defined in any IDE file. The game silently ignores this placement.", pl.file, pl.line, g.x, g.y, g.z);
            continue;
        }
        modelsUsed.insert(g.id);
        ++placementsOfModel[g.id];
        if (!firstOfModel.count(g.id)) firstOfModel[g.id] = &pl;
        if (toLower(d->second.model) != toLower(g.name)) {
            ++nameMismatch;
            if (!firstMismatch) firstMismatch = &pl;
        }
    }
    r.summary.modelsPlaced = int(modelsUsed.size());
    if (nameMismatch > 0 && firstMismatch)
        addAt(r, Severity::Info, "placement-name-differs",
              std::to_string(nameMismatch) + " placement(s) name a model differently from the IDE (for example id " + std::to_string(firstMismatch->p.id) + " is \"" + firstMismatch->p.name +
                  "\" here and \"" + defs[firstMismatch->p.id].model + "\" in the IDE). The game uses the id and ignores the name.",
              firstMismatch->file, firstMismatch->line, firstMismatch->p.x, firstMismatch->p.y, firstMismatch->p.z);

    // ---- per model: the files the game needs to load it ----------------------------------------------------
    for (int id : modelsUsed) {
        const Def& d = defs[id];
        const Placed* first = firstOfModel[id];
        const int n = placementsOfModel[id];
        const std::string where = std::to_string(n) + " placement(s)";
        if (imgOk && !imgNames.count(toLower(d.model) + ".dff"))
            addAt(r, Severity::Error, "model-dff-missing",
                  "Model \"" + d.model + "\" (id " + std::to_string(id) + ", " + where + ") has no " + d.model + ".dff in models/gta3.img. The game cannot load it.", d.file, 0, first->p.x, first->p.y, first->p.z);
        if (imgOk && toLower(d.txd) != "generic" && !imgNames.count(toLower(d.txd) + ".txd"))
            addAt(r, Severity::Warning, "model-txd-missing",
                  "Model \"" + d.model + "\" uses the texture dictionary \"" + d.txd + "\", which is not in models/gta3.img. The game falls back to \"generic\" and the textures will be wrong.", d.file, 0, first->p.x, first->p.y, first->p.z);
        if (!colNames.empty() && !colNames.count(toLower(d.model)))
            addAt(r, Severity::Warning, "model-col-missing",
                  "Model \"" + d.model + "\" (" + where + ") has no collision model in any COLFILE. Every object in the original game has one; a missing one may make the game crash (not verified).", d.file, 0, first->p.x, first->p.y, first->p.z);
    }

    // ---- the pools -----------------------------------------------------------------------------------------
    {
        bool ok;
        const std::set<std::string> objectDat = parseObjectDatNames(readFile(root + "/data/object.dat", ok));
        int used[kPoolCount] = {0, 0, 0};
        for (const Placed& pl : placed) {
            auto d = defs.find(pl.p.id);
            if (d == defs.end()) continue;  // unknown ids are ignored by the game and take no slot
            ++used[int(poolFor(objectDat.count(toLower(d->second.model)) > 0, pathIds.count(pl.p.id) > 0))];
        }
        for (int k = 0; k < kPoolCount; ++k) {
            r.summary.poolUsed[k] = used[k];
            const std::string what = std::string(poolName(Pool(k))) + ": " + std::to_string(used[k]) + " of " + std::to_string(kPoolCapacity[k]);
            const long long real = in.limits.cap[k];  // vanilla, or what the limit adjuster file sets
            if (used[k] > real)
                add(r, Severity::Error, "pool-overflow", what + " slots" + (in.limits.adjusted ? " (limit adjuster: " + std::to_string(real) + ")" : "") +
                                                             ". The game has no room for the extra ones and crashes while loading.");
            else if (used[k] > kPoolCapacity[k])
                add(r, Severity::Warning, "pool-over-vanilla",
                    what + " slots: " + std::to_string(used[k] - kPoolCapacity[k]) + " over the original game's size. This is only a hint: the unmodified game crashes while loading such a map, but a limit adjuster that raises " +
                        std::string(poolName(Pool(k))) + " to at least " + std::to_string(used[k]) + " makes it fine" + (in.limits.adjusted ? " (the adjuster file in this game allows " + std::to_string(real) + ")." : " (no adjuster file was found in this game folder, so the editor cannot tell whether you have one)."));
            else if (used[k] * 100 >= kPoolCapacity[k] * 95)
                add(r, Severity::Info, "pool-nearly-full", what + " slots are used. Only " + std::to_string(kPoolCapacity[k] - used[k]) + " more of this kind fit.");
            else
                add(r, Severity::Info, "pool-usage", what + " slots are used.");
        }
    }

    // ---- LOD pairs and duplicates, compared with the original game -------------------------------------
    // The original game itself has LODs without a building, buildings without a LOD and duplicate placements
    // (kBaseline* in mapcheck_baseline.inc, measured on the unmodified GTA III 1.0 data). Only the ones that are
    // NOT in that list are reported, because those are the ones an edit created.
    {
        std::unordered_map<std::string, std::vector<int>> byTail;
        for (size_t i = 0; i < placed.size(); ++i)
            if (placed[i].p.name.size() > 3) byTail[tailOf(placed[i].p.name)].push_back(int(i));
        auto hasPartner = [&](int i) {
            const GamePlacement& a = placed[i].p;
            const bool lod = isLodName(a.name);
            auto it = byTail.find(tailOf(a.name));
            if (it == byTail.end()) return false;
            for (int j : it->second) {
                if (j == i || isLodName(placed[j].p.name) == lod) continue;
                const GamePlacement& b = placed[j].p;
                if (std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)) <= 5.0) return true;
            }
            return false;
        };
        auto keyOf = [&](int i) {
            const GamePlacement& g = placed[i].p;
            char b[200];
            std::snprintf(b, sizeof b, "%s|%lld|%lld|%lld", toLower(g.name).c_str(), std::llround(g.x * 100), std::llround(g.y * 100), std::llround(g.z * 100));
            return std::string(b);
        };
        auto counts = [](const char* const* list) {
            std::map<std::string, int> m;
            for (; *list; ++list) ++m[*list];
            return m;
        };
        const std::map<std::string, int> baseLod = counts(kBaselineLodAlone), baseBuilding = counts(kBaselineBuildingAlone),
                                         baseDuplicate = counts(kBaselineDuplicates);
        // the entries of `all` beyond what the original game has for the same key
        auto fresh = [&](const std::vector<int>& all, const std::map<std::string, int>& base) {
            std::map<std::string, int> used;
            std::vector<int> out;
            for (int i : all) {
                const std::string k = keyOf(i);
                auto b = base.find(k);
                if (++used[k] > (b == base.end() ? 0 : b->second)) out.push_back(i);
            }
            return out;
        };

        std::vector<int> lodAlone, buildingAlone, dupes;
        for (size_t i = 0; i < placed.size(); ++i) {
            const std::string& n = placed[i].p.name;
            if (n.size() <= 3) continue;
            // a placement at the editor's "deleted" depth (Delete puts it at -200, below the map) was removed on purpose
            // and needs no partner: Replace moves the old LOD stand-in there when the new building has no LOD model
            if (placed[i].p.z <= kDeletedDepth) continue;
            if (isLodName(n)) {
                if (!hasPartner(int(i))) lodAlone.push_back(int(i));
            } else if (lodModelTails.count(tailOf(n)) && !hasPartner(int(i))) {
                buildingAlone.push_back(int(i));
            }
        }
        std::map<std::tuple<int, long long, long long, long long>, std::vector<int>> seen;
        for (size_t i = 0; i < placed.size(); ++i) {
            const GamePlacement& g = placed[i].p;
            seen[{g.id, std::llround(g.x * 100), std::llround(g.y * 100), std::llround(g.z * 100)}].push_back(int(i));
        }
        for (const auto& kv : seen)
            for (size_t k = 1; k < kv.second.size(); ++k) dupes.push_back(kv.second[k]);
        std::sort(dupes.begin(), dupes.end());

        for (int i : lodAlone) r.lodAloneKeys.push_back(keyOf(i));
        for (int i : buildingAlone) r.buildingAloneKeys.push_back(keyOf(i));
        for (int i : dupes) r.duplicateKeys.push_back(keyOf(i));
        const std::vector<int> newLod = fresh(lodAlone, baseLod), newBuilding = fresh(buildingAlone, baseBuilding), newDupes = fresh(dupes, baseDuplicate);
        r.summary.lodWithoutBuilding = int(lodAlone.size());
        r.summary.buildingWithoutLod = int(buildingAlone.size());
        r.summary.duplicates = int(dupes.size());
        r.summary.newLodWithoutBuilding = int(newLod.size());
        r.summary.newBuildingWithoutLod = int(newBuilding.size());
        r.summary.newDuplicates = int(newDupes.size());

        auto listThem = [&](const std::vector<int>& v, const char* code, const char* what) {
            int shown = 0;
            for (int i : v) {
                if (shown++ >= 100) break;
                const Placed& p = placed[i];
                addAt(r, Severity::Warning, code, std::string(what) + ": " + p.p.name + " " + positionText(p.p.x, p.p.y, p.p.z), p.file, p.line, p.p.x, p.p.y, p.p.z);
            }
        };
        if (!newLod.empty()) {
            add(r, Severity::Warning, "lod-without-building",
                std::to_string(newLod.size()) + " LOD placement(s) have no building of the same name next to them, which the original game does not have. A building and its LOD should be moved or removed together (experience, see the notes on LOD pairs). They are listed below.");
            listThem(newLod, "lod-without-building-item", "LOD without building");
        }
        if (!newBuilding.empty()) {
            add(r, Severity::Warning, "building-without-lod",
                std::to_string(newBuilding.size()) + " building(s) have no LOD next to them although a LOD model exists, which the original game does not have. They are listed below.");
            listThem(newBuilding, "building-without-lod-item", "Building without LOD");
        }
        if (!newDupes.empty()) {
            add(r, Severity::Warning, "duplicate-placements", std::to_string(newDupes.size()) + " placement(s) repeat another placement exactly (same model and position), which the original game does not have. They are listed below.");
            listThem(newDupes, "duplicate-placement-item", "Duplicate");
        }
    }

    std::stable_sort(r.issues.begin(), r.issues.end(), [](const CheckIssue& a, const CheckIssue& b) { return int(a.severity) > int(b.severity); });
    return r;
}

std::string checkReportText(const CheckResult& r) {
    std::string out;
    char head[256];
    std::snprintf(head, sizeof head, "Map check: %d error(s), %d warning(s), %d note(s)\r\n%d placements, %d models placed, %d IPL files, %d IDE files, %d COL files\r\n\r\n",
                  r.count(Severity::Error), r.count(Severity::Warning), r.count(Severity::Info), r.summary.placements, r.summary.modelsPlaced, r.summary.ipl,
                  r.summary.ide, r.summary.col);
    out += head;
    for (const CheckIssue& i : r.issues) {
        out += i.severity == Severity::Error ? "[ERROR]   " : i.severity == Severity::Warning ? "[warning] " : "[note]    ";
        out += i.message + "\r\n";
        if (!i.file.empty()) {
            out += "          " + i.file;
            if (i.line > 0) out += ":" + std::to_string(i.line);
            out += "\r\n";
        }
        if (i.hasPosition) out += "          at " + positionText(i.x, i.y, i.z) + "\r\n";
    }
    return out;
}

}  // namespace med
