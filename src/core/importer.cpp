#include "importer.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <tuple>
#include "textutil.h"

namespace fs = std::filesystem;

namespace med {

namespace {

// A frame's matrix in the model's own space (RenderWare: right, up, at, position), through its parents.
void frameMatrix(const DffModel& m, int index, float out[12], int depth = 0) {
    const DffFrame& f = m.frames[size_t(index)];
    float local[12];
    for (int i = 0; i < 9; ++i) local[i] = f.rotation[i];
    for (int i = 0; i < 3; ++i) local[9 + i] = f.position[i];
    if (f.parent >= 0 && f.parent < int(m.frames.size()) && f.parent != index && depth < 64) {
        float p[12];
        frameMatrix(m, f.parent, p, depth + 1);
        // out = p * local: columns right/up/at turned by the parent, position = parent position + parent turn * local position
        for (int c = 0; c < 3; ++c)
            for (int r = 0; r < 3; ++r) out[c * 3 + r] = p[0 * 3 + r] * local[c * 3 + 0] + p[1 * 3 + r] * local[c * 3 + 1] + p[2 * 3 + r] * local[c * 3 + 2];
        for (int r = 0; r < 3; ++r) out[9 + r] = p[9 + r] + p[0 * 3 + r] * local[9] + p[1 * 3 + r] * local[10] + p[2 * 3 + r] * local[11];
    } else {
        std::copy(local, local + 12, out);
    }
}

std::string lineEnding(const std::string& text) { return text.find("\r\n") != std::string::npos ? "\r\n" : "\n"; }

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string l = text.substr(pos, nl - pos);
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
        pos = nl + 1;
    }
    return lines;
}

std::string joinLines(const std::vector<std::string>& lines, const std::string& eol, bool endWithEol) {
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        out += lines[i];
        if (i + 1 < lines.size() || endWithEol) out += eol;
    }
    return out;
}

std::string datKey(std::string s) {
    s = toLower(trim(s));
    std::replace(s.begin(), s.end(), '\\', '/');
    std::string out;
    bool space = false;
    for (char c : s) {
        if (c == ' ' || c == '\t') { space = true; continue; }
        if (space && !out.empty()) out += ' ';
        space = false;
        out += c;
    }
    return out;
}

}  // namespace

void dffMesh(const DffModel& model, std::vector<float>& vertices, std::vector<uint32_t>& triangles) {
    vertices.clear();
    triangles.clear();
    std::map<std::tuple<int64_t, int64_t, int64_t>, uint32_t> weld;  // positions rounded to 1 mm
    for (const DffAtomic& a : model.atomics) {
        if (a.geometry < 0 || a.geometry >= int(model.geometries.size())) continue;
        float m[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
        if (a.frame >= 0 && a.frame < int(model.frames.size())) frameMatrix(model, a.frame, m);
        const DffGeometry& g = model.geometries[size_t(a.geometry)];
        std::vector<uint32_t> map(g.vertexCount());
        for (size_t i = 0; i < g.vertexCount(); ++i) {
            const float x = g.positions[i * 3], y = g.positions[i * 3 + 1], z = g.positions[i * 3 + 2];
            const float w[3] = {m[0] * x + m[3] * y + m[6] * z + m[9], m[1] * x + m[4] * y + m[7] * z + m[10], m[2] * x + m[5] * y + m[8] * z + m[11]};
            const auto key = std::make_tuple(std::llround(w[0] * 1000), std::llround(w[1] * 1000), std::llround(w[2] * 1000));
            auto f = weld.find(key);
            if (f == weld.end()) {
                f = weld.emplace(key, uint32_t(vertices.size() / 3)).first;
                vertices.insert(vertices.end(), w, w + 3);
            }
            map[i] = f->second;
        }
        for (size_t t = 0; t + 2 < g.triangles.size(); t += 3) {
            if (g.triangles[t] >= map.size() || g.triangles[t + 1] >= map.size() || g.triangles[t + 2] >= map.size()) continue;
            const uint32_t a0 = map[g.triangles[t]], b0 = map[g.triangles[t + 1]], c0 = map[g.triangles[t + 2]];
            if (a0 == b0 || b0 == c0 || a0 == c0) continue;
            triangles.push_back(a0);
            triangles.push_back(b0);
            triangles.push_back(c0);
        }
    }
}

bool dffBounds(const DffModel& model, float mn[3], float mx[3]) {
    std::vector<float> v;
    std::vector<uint32_t> t;
    dffMesh(model, v, t);
    if (v.empty()) return false;
    for (int k = 0; k < 3; ++k) { mn[k] = 1e30f; mx[k] = -1e30f; }
    for (size_t i = 0; i + 2 < v.size(); i += 3)
        for (int k = 0; k < 3; ++k) { mn[k] = std::min(mn[k], v[i + size_t(k)]); mx[k] = std::max(mx[k], v[i + size_t(k)]); }
    return true;
}

ColModel makeCollision(const DffModel& model, const std::string& name, uint16_t modelId, CollisionStart how, uint8_t material) {
    ColModel c;
    c.name = name;
    c.modelId = modelId;
    float mn[3] = {0, 0, 0}, mx[3] = {0, 0, 0};
    const bool has = dffBounds(model, mn, mx);
    if (how == CollisionStart::Box && has) {
        ColBox b;
        std::copy(mn, mn + 3, b.min);
        std::copy(mx, mx + 3, b.max);
        b.surface.material = material;
        c.boxes.push_back(b);
    } else if (how == CollisionStart::Mesh && has) {
        std::vector<uint32_t> tris;
        dffMesh(model, c.vertices, tris);
        for (size_t i = 0; i + 2 < tris.size(); i += 3) {
            ColFace f;
            f.a = tris[i];
            f.b = tris[i + 1];
            f.c = tris[i + 2];
            f.material = material;
            c.faces.push_back(f);
        }
    }
    // the bounds: from the shapes, else (Empty) the model's own box, so the game still knows how big it is
    std::copy(mn, mn + 3, c.boundMin);
    std::copy(mx, mx + 3, c.boundMax);
    for (int k = 0; k < 3; ++k) c.boundCenter[k] = (mn[k] + mx[k]) / 2;
    c.boundRadius = std::sqrt((mx[0] - mn[0]) * (mx[0] - mn[0]) + (mx[1] - mn[1]) * (mx[1] - mn[1]) + (mx[2] - mn[2]) * (mx[2] - mn[2])) / 2;
    computeColBounds(c);
    return c;
}

void collectIdeIds(const std::string& ideText, std::set<int>& ids) {
    std::string section;
    for (const std::string& raw : splitLines(ideText)) {
        std::string l = trim(raw);
        const size_t hash = l.find('#');
        if (hash != std::string::npos) l = trim(l.substr(0, hash));
        if (l.empty()) continue;
        const std::string low = toLower(l);
        if (low == "end") { section.clear(); continue; }
        if (section.empty()) {
            section = low;
            continue;
        }
        if (section != "objs" && section != "tobj" && section != "cars" && section != "peds" && section != "weap" && section != "hier") continue;
        size_t i = 0;
        while (i < l.size() && (l[i] == ' ' || l[i] == '\t')) ++i;
        size_t j = i;
        while (j < l.size() && std::isdigit(static_cast<unsigned char>(l[j]))) ++j;
        if (j > i) ids.insert(std::atoi(l.substr(i, j - i).c_str()));
    }
}

int proposeModelId(const std::set<int>& used, int from, int limit) {
    for (int id = std::max(0, from); id < limit; ++id)
        if (!used.count(id)) return id;
    for (int id = 0; id < std::min(from, limit); ++id)
        if (!used.count(id)) return id;
    return -1;
}

bool validModelName(const std::string& name, std::string& why) {
    if (name.empty()) { why = "The name is empty."; return false; }
    if (name.size() > kMaxModelNameLength) {
        why = "The name is " + std::to_string(name.size()) + " characters long; the game's archive allows " + std::to_string(kMaxModelNameLength) + ".";
        return false;
    }
    for (char c : name)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            why = std::string("The name may only use letters, digits, '_' and '-' (it has '") + c + "').";
            return false;
        }
    return true;
}

std::string ideObjsLine(int id, const std::string& model, const std::string& txd, double drawDistance, int flags) {
    char dd[32];
    std::snprintf(dd, sizeof dd, "%g", drawDistance);
    return std::to_string(id) + ", " + model + ", " + txd + ", 1, " + dd + ", " + std::to_string(flags);
}

void addIdeObjsLine(std::string& ideText, const std::string& line) {
    const std::string eol = ideText.empty() ? std::string("\r\n") : lineEnding(ideText);  // a new file: Windows line endings, as Rockstar's
    std::vector<std::string> lines = splitLines(ideText);
    const bool endedWithEol = ideText.empty() || ideText.back() == '\n';
    // the end of the objs block
    bool inObjs = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string low = toLower(trim(lines[i]));
        if (!inObjs && low == "objs") inObjs = true;
        else if (inObjs && low == "end") {
            lines.insert(lines.begin() + std::ptrdiff_t(i), line);
            ideText = joinLines(lines, eol, endedWithEol);
            return;
        }
    }
    if (!ideText.empty() && !endedWithEol) ideText += eol;
    ideText += "objs" + eol + line + eol + "end" + eol;
}

bool addDatLine(std::string& datText, const std::string& keyword, const std::string& line) {
    const std::string eol = lineEnding(datText);
    std::vector<std::string> lines = splitLines(datText);
    const bool endedWithEol = datText.empty() || datText.back() == '\n';
    const std::string want = datKey(line), kw = toLower(keyword);
    int lastKw = -1, firstIpl = -1;
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string k = datKey(lines[i]);
        if (k == want) return false;
        if (k.rfind(kw + " ", 0) == 0) lastKw = int(i);
        if (firstIpl < 0 && k.rfind("ipl ", 0) == 0) firstIpl = int(i);
    }
    const size_t at = lastKw >= 0 ? size_t(lastKw + 1) : firstIpl >= 0 ? size_t(firstIpl) : lines.size();
    lines.insert(lines.begin() + std::ptrdiff_t(at), line);
    datText = joinLines(lines, eol, endedWithEol || at == lines.size() - 1);
    return true;
}

namespace {
bool readAll(const std::string& path, std::vector<unsigned char>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return true;
}
bool writeAll(const std::string& path, const std::string& data) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(data.data(), std::streamsize(data.size()));
    return bool(f);
}
std::string stemOf(const std::string& path) {
    std::string s = fs::path(path).filename().string();
    const size_t dot = s.find_last_of('.');
    return dot == std::string::npos ? s : s.substr(0, dot);
}
std::string slashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}
// The IDE and DAT files of the game (data/default.dat then data/gta3.dat): their IDE lines.
std::vector<std::string> ideFilesOf(const std::string& root) {
    std::vector<std::string> out;
    for (const char* dat : {"data/default.dat", "data/gta3.dat"}) {
        std::vector<unsigned char> d;
        if (!readAll(root + "/" + dat, d)) continue;
        std::istringstream in(std::string(d.begin(), d.end()));
        std::string l;
        while (std::getline(in, l)) {
            l = trim(l);
            if (toLower(l).rfind("ide ", 0) == 0) out.push_back(root + "/" + slashes(trim(l.substr(4))));
        }
    }
    return out;
}
}  // namespace

void gameIdsAndNames(const std::string& gameRoot, std::set<int>& ids, std::set<std::string>& names) {
    for (const std::string& path : ideFilesOf(gameRoot)) {
        std::vector<unsigned char> d;
        if (!readAll(path, d)) continue;
        const std::string text(d.begin(), d.end());
        collectIdeIds(text, ids);
        // the names: the second field of every line inside a section
        std::string section;
        for (const std::string& raw : splitLines(text)) {
            std::string l = trim(raw);
            const size_t hash = l.find('#');
            if (hash != std::string::npos) l = trim(l.substr(0, hash));
            if (l.empty()) continue;
            if (toLower(l) == "end") { section.clear(); continue; }
            if (section.empty()) { section = toLower(l); continue; }
            if (section != "objs" && section != "tobj" && section != "cars" && section != "peds" && section != "weap" && section != "hier") continue;
            const size_t c1 = l.find(',');
            if (c1 == std::string::npos) continue;
            const size_t c2 = l.find(',', c1 + 1);
            names.insert(toLower(trim(l.substr(c1 + 1, c2 == std::string::npos ? std::string::npos : c2 - c1 - 1))));
        }
    }
}

ImportPlan planImport(const ImportRequest& req, const ImgArchive& img, const std::set<int>& usedIds, const std::set<std::string>& usedNames) {
    ImportPlan p;
    std::string err, why;
    // the model
    if (!readAll(req.dffPath, p.dffBytes)) p.problems.push_back("Cannot read the model " + req.dffPath + ".");
    else if (!parseDff(p.dffBytes, p.model, err)) p.problems.push_back(req.dffPath + " is not a model the editor can read: " + err);
    else if (p.model.geometries.empty()) p.problems.push_back(req.dffPath + " has no geometry.");
    // its name and id
    if (!validModelName(req.name, why)) p.problems.push_back("Model name: " + why);
    else if (usedNames.count(toLower(req.name))) p.problems.push_back("A model named " + req.name + " already exists in the game; pick another name.");
    else if (img.find(req.name + ".dff") >= 0) p.problems.push_back("gta3.img already holds " + req.name + ".dff; pick another name.");
    if (req.id < 0 || req.id >= kGta3ModelLimit) p.problems.push_back("The model ID must be between 0 and " + std::to_string(kGta3ModelLimit - 1) + " (the game's model table).");
    else if (usedIds.count(req.id)) p.problems.push_back("Model ID " + std::to_string(req.id) + " is already used; the first free one is " + std::to_string(proposeModelId(usedIds)) + ".");
    if (!(req.drawDistance > 0 && req.drawDistance <= 3000)) p.problems.push_back("The draw distance must be between 1 and 3000.");
    // the textures: required; the same name already in the archive only when it is the very same file
    if (req.txdPath.empty()) p.problems.push_back("A texture dictionary (.txd) is required.");
    else if (!readAll(req.txdPath, p.txdBytes)) p.problems.push_back("Cannot read the textures " + req.txdPath + ".");
    else if (!parseTxd(p.txdBytes, p.textures, err)) p.problems.push_back(req.txdPath + " is not a texture dictionary the editor can read: " + err);
    else {
        p.txdName = stemOf(req.txdPath);
        if (p.txdName.empty() || p.txdName.size() > kImgMaxNameLength - 4 || !validModelName(p.txdName.substr(0, std::min<size_t>(p.txdName.size(), kMaxModelNameLength)), why))
            p.problems.push_back("The texture dictionary's file name (" + p.txdName + ") must be 1 to 19 letters, digits, '_' or '-'.");
        std::vector<unsigned char> existing;
        if (img.find(p.txdName + ".txd") >= 0 && img.extract(p.txdName + ".txd", existing, err)) {
            std::vector<unsigned char> padded = p.txdBytes;
            padded.resize(existing.size(), 0);  // the archive keeps whole 2048-byte sectors
            if (existing == padded && existing.size() - p.txdBytes.size() < kImgSectorSize) {
                p.txdReused = true;
                p.notes.push_back(p.txdName + ".txd is already in the game (the same file): it is shared, not added again.");
            } else {
                p.problems.push_back("gta3.img already holds a different " + p.txdName + ".txd; rename your texture dictionary.");
            }
        } else {
            size_t slots = 0;
            for (const ImgEntry& e : img.entries())
                if (toLower(e.name).size() > 4 && toLower(e.name).substr(e.name.size() - 4) == ".txd") ++slots;
            if (slots + 1 > size_t(kGta3TxdLimit))
                p.problems.push_back("The game has room for " + std::to_string(kGta3TxdLimit) + " texture dictionaries and " + std::to_string(slots) + " are used.");
        }
        // every texture the model asks for
        std::set<std::string> have;
        for (const TxdTexture& t : p.textures.textures) have.insert(toLower(t.name));
        std::set<std::string> missing;
        for (const DffGeometry& g : p.model.geometries)
            for (const DffMaterial& m : g.materials)
                if (m.textured && !have.count(toLower(m.texture))) missing.insert(m.texture);
        for (const std::string& m : missing) p.notes.push_back("The model uses the texture " + m + ", which is not in " + p.txdName + ".txd (the game draws it plain).");
    }
    // the collision: from the .col (the record named like the model, or its only record), else made
    if (!req.colPath.empty()) {
        std::vector<unsigned char> bytes;
        std::vector<ColModel> models;
        if (!readAll(req.colPath, bytes)) p.problems.push_back("Cannot read the collision " + req.colPath + ".");
        else if (!parseCol(bytes, models, err) || models.empty()) p.problems.push_back(req.colPath + " is not a collision file the editor can read" + (err.empty() ? "." : ": " + err));
        else {
            const ColModel* pick = nullptr;
            for (const ColModel& m : models)
                if (toLower(m.name) == toLower(req.name) || toLower(m.name) == toLower(stemOf(req.dffPath))) pick = &m;
            if (!pick && models.size() == 1) pick = &models[0];
            if (!pick) p.problems.push_back(req.colPath + " holds " + std::to_string(models.size()) + " collision models and none is named " + req.name + ".");
            else {
                p.collision = *pick;
                p.collisionFromFile = true;
                if (toLower(pick->name) != toLower(req.name)) p.notes.push_back("The collision record " + pick->name + " is renamed " + req.name + ".");
            }
        }
    }
    if (!p.collisionFromFile && p.problems.empty()) p.collision = makeCollision(p.model, req.name, uint16_t(std::max(0, req.id)), req.start);
    p.collision.name = req.name;
    p.collision.nameField.clear();
    p.collision.modelId = uint16_t(std::max(0, req.id));
    if (!p.collisionFromFile && req.start == CollisionStart::Empty)
        p.notes.push_back("The collision starts empty: nothing of this model is solid in the game until you give it shapes.");
    // the destination region
    std::string rwhy;
    if (req.region.empty()) p.problems.push_back("Pick the region the model belongs to.");
    else if (!fs::exists(req.gameRoot + "/data/maps/" + req.region + "/" + req.region + ".ipl") && !fs::exists(req.gameRoot + "/data/maps/" + req.region + "/" + req.region + ".IPL"))
        p.problems.push_back("The region " + req.region + " has no data/maps/" + req.region + "/" + req.region + ".ipl; make it with New region first.");
    // the archive is written in place: a hard-linked gta3.img (a test copy sharing the file with another game folder)
    // would change that other game too
    std::error_code ec;
    for (const std::string& f : {img.path(), img.version() == 1 ? img.path().substr(0, img.path().size() - 4) + ".dir" : std::string()})
        if (!f.empty() && fs::exists(f, ec) && fs::hard_link_count(f, ec) > 1)
            p.problems.push_back(fs::path(f).filename().string() + " is shared with another folder (a hard link); importing would change that folder too. Make a real copy first.");
    return p;
}

bool applyImport(const ImportRequest& req, const ImportPlan& plan, ImgArchive& img, std::vector<std::string>& written, std::string& error) {
    if (!plan.problems.empty()) {
        error = plan.problems[0];
        return false;
    }
    std::error_code ec;
    const std::string root = req.gameRoot;
    // 1. backups: gta3.dir for this import, gta3.dat once; the undo note
    const std::string dirPath = img.path().substr(0, img.path().size() - 4) + ".dir";
    const std::string dirBackup = dirPath + ".before-" + req.name + ".med-backup";
    if (img.version() == 1) {
        fs::copy_file(dirPath, dirBackup, fs::copy_options::overwrite_existing, ec);
        if (ec) { error = "Could not back up " + dirPath + ": " + ec.message(); return false; }
        written.push_back(dirBackup);
    }
    const uintmax_t imgSize = fs::file_size(img.path(), ec);
    const std::string datPath = root + "/data/gta3.dat";
    if (!fs::exists(datPath + ".med-backup", ec)) {
        fs::copy_file(datPath, datPath + ".med-backup", ec);
        if (ec) { error = "Could not back up gta3.dat: " + ec.message(); return false; }
        written.push_back(datPath + ".med-backup");
    }
    {
        std::ofstream note(fs::path(img.path()).parent_path() / "med-imports.txt", std::ios::app);
        const std::time_t now = std::time(nullptr);
        char when[32];
        std::strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        note << when << "  imported " << req.name << ".dff" << (plan.txdReused ? "" : " and " + plan.txdName + ".txd") << " (id " << req.id << ", region " << req.region
             << "); to undo: restore " << fs::path(dirBackup).filename().string() << " as gta3.dir and cut gta3.img back to " << imgSize << " bytes, remove the IDE line and "
             << "the collision record, and the lines added to gta3.dat\n";
    }
    // 2. the archive: append, then the directory
    if (!img.add(req.name + ".dff", plan.dffBytes, error)) return false;
    if (!plan.txdReused && !img.add(plan.txdName + ".txd", plan.txdBytes, error)) return false;
    if (!img.save(error)) return false;
    written.push_back(img.path());
    // 3. the region's IDE file and its line in gta3.dat
    const std::string folder = root + "/data/maps/" + req.region;
    const std::string idePath = folder + "/" + req.region + ".ide";
    std::vector<unsigned char> ideBytes;
    readAll(idePath, ideBytes);
    std::string ide(ideBytes.begin(), ideBytes.end());
    if (!ide.empty() && !fs::exists(idePath + ".med-backup", ec)) fs::copy_file(idePath, idePath + ".med-backup", ec);
    addIdeObjsLine(ide, ideObjsLine(req.id, req.name, plan.txdName, req.drawDistance, req.flags));
    if (!writeAll(idePath, ide)) { error = "Could not write " + idePath + "."; return false; }
    written.push_back(idePath);
    // 4. the region's COL file
    const std::string colPath = folder + "/" + req.region + ".col";
    std::vector<unsigned char> colBytes;
    readAll(colPath, colBytes);
    if (!colBytes.empty() && !fs::exists(colPath + ".med-backup", ec)) fs::copy_file(colPath, colPath + ".med-backup", ec);
    if (!putColRecord(colBytes, plan.collision, error)) return false;
    if (!writeAll(colPath, std::string(colBytes.begin(), colBytes.end()))) { error = "Could not write " + colPath + "."; return false; }
    written.push_back(colPath);
    // 5. gta3.dat: the IDE file (before the IPL files) and the collision file, level 0 (always loaded)
    std::vector<unsigned char> datBytes;
    if (!readAll(datPath, datBytes)) { error = "Could not read gta3.dat."; return false; }
    std::string dat(datBytes.begin(), datBytes.end());
    std::string up = req.region;
    for (char& c : up) c = char(std::toupper(static_cast<unsigned char>(c)));
    const bool a = addDatLine(dat, "IDE", "IDE DATA\\MAPS\\" + req.region + "\\" + req.region + ".IDE");
    const bool b = addDatLine(dat, "COLFILE", "COLFILE 0 DATA\\MAPS\\" + req.region + "\\" + req.region + ".COL");
    if ((a || b) && !writeAll(datPath, dat)) { error = "Could not write gta3.dat."; return false; }
    if (a || b) written.push_back(datPath);
    return true;
}

bool putColRecord(std::vector<unsigned char>& colFile, const ColModel& m, std::string& error) {
    std::vector<ColModel> models;
    if (!colFile.empty() && !parseCol(colFile, models, error)) return false;
    const std::vector<unsigned char> rec = writeColl(m);
    for (const ColModel& old : models)
        if (toLower(old.name) == toLower(m.name)) {
            std::vector<unsigned char> out(colFile.begin(), colFile.begin() + std::ptrdiff_t(old.fileOffset));
            out.insert(out.end(), rec.begin(), rec.end());
            out.insert(out.end(), colFile.begin() + std::ptrdiff_t(old.fileOffset + old.fileSize), colFile.end());
            colFile = std::move(out);
            return true;
        }
    colFile.insert(colFile.end(), rec.begin(), rec.end());
    return true;
}

}  // namespace med
