#include "ipl.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include "textutil.h"

namespace med {

namespace {

bool parseInt(const std::string& s, int& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (*end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

bool parseDouble(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    double v = std::strtod(s.c_str(), &end);
    if (*end != '\0') return false;
    out = v;
    return true;
}

// Field layouts as read from MEd.exe (IPL text loader, 0x00424b40):
//   GTA III:     id, model, pos(3), scale(3), rot(4)                12 fields
//   Vice City:   id, model, interior, pos(3), scale(3), rot(4)      13 fields (12 without interior)
//   San Andreas: id, model, interior, pos(3), rot(4), lod            11 fields
bool parseInstance(const std::vector<std::string>& f, GameVersion v, IplInstance& o) {
    size_t i = 2;
    auto num = [&](double& d) { return i < f.size() && parseDouble(f[i++], d); };
    if (f.size() < 2 || !parseInt(f[0], o.id)) return false;
    o.model = f[1];
    o.interior = 0;
    o.lod = kNoLod;
    switch (v) {
        case GameVersion::GTA3:
            if (f.size() < 12) return false;
            return num(o.x) && num(o.y) && num(o.z) && num(o.scaleX) && num(o.scaleY) && num(o.scaleZ) &&
                   num(o.rotX) && num(o.rotY) && num(o.rotZ) && num(o.rotW);
        case GameVersion::ViceCity:
            if (f.size() != 12 && f.size() != 13) return false;
            if (f.size() == 13 && !parseInt(f[i++], o.interior)) return false;
            return num(o.x) && num(o.y) && num(o.z) && num(o.scaleX) && num(o.scaleY) && num(o.scaleZ) &&
                   num(o.rotX) && num(o.rotY) && num(o.rotZ) && num(o.rotW);
        case GameVersion::SanAndreas: {
            if (f.size() < 11) return false;
            if (!parseInt(f[i++], o.interior)) return false;
            if (!(num(o.x) && num(o.y) && num(o.z) && num(o.rotX) && num(o.rotY) && num(o.rotZ) && num(o.rotW)))
                return false;
            int lod = -1;
            if (!parseInt(f[i], lod)) return false;
            o.lod = static_cast<short>(lod);
            return true;
        }
    }
    return false;
}

// QString::number(d, 'g', 12), which MEd uses for every coordinate it writes. MEd was built with
// the Visual C++ 7.1 runtime, which prints exponents with three digits ("1e-008"); the game's own
// files do too, so that is reproduced here to keep re-saved files identical to the originals.
std::string fmt(double d) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.12g", d);
    std::string s = buf;
    size_t e = s.find('e');
    if (e != std::string::npos && e + 2 < s.size()) {
        size_t digits = s.size() - (e + 2);  // after the sign
        if (digits < 3) s.insert(e + 2, 3 - digits, '0');
    }
    return s;
}

}  // namespace

IplFile parseTextIpl(const std::string& text, GameVersion version) {
    IplFile ipl;
    ipl.version = version;
    std::istringstream in(text);
    std::string raw;
    std::string section;  // current section name, empty when outside a section
    int rawIndex = -1;    // index into otherSections for the current non-inst section
    int lineNo = 0;
    while (std::getline(in, raw)) {
        ++lineNo;
        std::string line = trim(raw);
        if (isBlankOrComment(line)) continue;
        if (section.empty()) {
            section = toLower(line);
            rawIndex = -1;
            if (section != "inst") {
                ipl.otherSections.push_back({section, {}});
                rawIndex = static_cast<int>(ipl.otherSections.size()) - 1;
            }
            continue;
        }
        if (toLower(line) == "end") {
            section.clear();
            rawIndex = -1;
            continue;
        }
        if (section == "inst") {
            IplInstance inst;
            if (parseInstance(splitFields(line), version, inst))
                ipl.instances.push_back(inst);
            else
                ipl.warnings.push_back("line " + std::to_string(lineNo) + ": bad inst entry: " + line);
        } else {
            ipl.otherSections[rawIndex].lines.push_back(line);
        }
    }
    if (!section.empty()) ipl.warnings.push_back("section '" + section + "' is missing its end");
    return ipl;
}

// Line layout confirmed from MEd.exe (0x00423080). MEd writes the literal "1, 1, 1" for the scale of
// GTA III and Vice City placements, which silently discards a real scale; this writes the actual
// scale (identical output for the shipped data, where every scale is 1).
std::string formatIplInstanceLine(const IplInstance& i, GameVersion version) {
    std::ostringstream out;
    out << i.id << ", " << i.model;
    switch (version) {
        case GameVersion::GTA3:
            out << ", " << fmt(i.x) << ", " << fmt(i.y) << ", " << fmt(i.z) << ", " << fmt(i.scaleX) << ", " << fmt(i.scaleY)
                << ", " << fmt(i.scaleZ) << ", " << fmt(i.rotX) << ", " << fmt(i.rotY) << ", " << fmt(i.rotZ) << ", "
                << fmt(i.rotW);
            break;
        case GameVersion::ViceCity:
            out << ", " << i.interior << ", " << fmt(i.x) << ", " << fmt(i.y) << ", " << fmt(i.z) << ", " << fmt(i.scaleX)
                << ", " << fmt(i.scaleY) << ", " << fmt(i.scaleZ) << ", " << fmt(i.rotX) << ", " << fmt(i.rotY) << ", "
                << fmt(i.rotZ) << ", " << fmt(i.rotW);
            break;
        case GameVersion::SanAndreas:
            out << ", " << i.interior << ", " << fmt(i.x) << ", " << fmt(i.y) << ", " << fmt(i.z) << ", " << fmt(i.rotX)
                << ", " << fmt(i.rotY) << ", " << fmt(i.rotZ) << ", " << fmt(i.rotW) << ", " << (i.lod == kNoLod ? -1 : i.lod);
            break;
    }
    return out.str();
}

namespace {
bool isLodName(const std::string& name) { return name.size() > 3 && toLower(name.substr(0, 3)) == "lod"; }
}  // namespace

std::vector<size_t> pairedLodPartners(const std::vector<IplInstance>& all, size_t index, const std::function<bool(size_t)>& isOriginal, double maxDistance) {
    const std::vector<size_t> mine = findLodPartners(all, index, maxDistance);
    if (mine.empty()) return mine;
    // the group around index: its own kind (index and every same-kind partner of its partners) and the other kind
    std::vector<size_t> same = {index}, other = mine;
    for (size_t c : mine)
        for (size_t s : findLodPartners(all, c, maxDistance))
            if (std::find(same.begin(), same.end(), s) == same.end()) same.push_back(s);
    for (size_t s : same)
        for (size_t c : findLodPartners(all, s, maxDistance))
            if (std::find(other.begin(), other.end(), c) == other.end()) other.push_back(c);
    if (same.size() == 1) return mine;  // nobody else claims these stand-ins
    auto orig = [&](size_t i) { return isOriginal ? isOriginal(i) : true; };
    struct Pair {
        double d;
        int mismatch;
        size_t s, c;
    };
    std::vector<Pair> pairs;
    for (size_t s : same)
        for (size_t c : findLodPartners(all, s, maxDistance)) {
            const double dx = all[s].x - all[c].x, dy = all[s].y - all[c].y, dz = all[s].z - all[c].z;
            pairs.push_back({std::sqrt(dx * dx + dy * dy + dz * dz), orig(s) != orig(c) ? 1 : 0, s, c});
        }
    std::sort(pairs.begin(), pairs.end(), [](const Pair& a, const Pair& b) {
        if (a.d != b.d) return a.d < b.d;
        if (a.mismatch != b.mismatch) return a.mismatch < b.mismatch;
        if (a.s != b.s) return a.s < b.s;
        return a.c < b.c;
    });
    std::vector<size_t> usedS, usedC, out;
    for (const Pair& p : pairs) {
        if (std::find(usedS.begin(), usedS.end(), p.s) != usedS.end() || std::find(usedC.begin(), usedC.end(), p.c) != usedC.end()) continue;
        usedS.push_back(p.s);
        usedC.push_back(p.c);
        if (p.s == index) out.push_back(p.c);
    }
    return out;
}

std::string lodModelNameFor(const std::string& building) {
    if (building.size() <= 3 || isLodName(building)) return "";
    return "LOD" + building.substr(3);
}

std::vector<size_t> findLodPartners(const std::vector<IplInstance>& all, size_t index, double maxDistance) {
    std::vector<std::pair<double, size_t>> found;
    if (index >= all.size() || all[index].model.size() <= 3) return {};
    const IplInstance& me = all[index];
    const bool meIsLod = isLodName(me.model);
    const std::string tail = toLower(me.model.substr(3));
    for (size_t j = 0; j < all.size(); ++j) {
        if (j == index || all[j].model.size() <= 3) continue;
        if (isLodName(all[j].model) == meIsLod) continue;  // need the opposite kind
        if (toLower(all[j].model.substr(3)) != tail) continue;
        const double dx = all[j].x - me.x, dy = all[j].y - me.y, dz = all[j].z - me.z;
        const double d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d <= maxDistance) found.emplace_back(d, j);
    }
    std::sort(found.begin(), found.end());
    std::vector<size_t> out;
    for (const auto& f : found) out.push_back(f.second);
    return out;
}

bool removeIplInstance(std::vector<IplInstance>& v, size_t index) {
    if (index >= v.size()) return false;
    const int removed = static_cast<int>(index);
    for (IplInstance& i : v) {
        if (i.lod == kNoLod) continue;
        if (i.lod == removed) i.lod = kNoLod;
        else if (i.lod > removed) i.lod = static_cast<short>(i.lod - 1);
    }
    v.erase(v.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool replaceIplInstSection(const std::string& text, const std::vector<IplInstance>& instances, GameVersion version,
                           std::string& result, std::string& error) {
    size_t pos = 0;
    size_t instEnd = std::string::npos;   // offset just after the "inst" line, including its line ending
    size_t endStart = std::string::npos;  // offset of the first "end" line after it
    std::string eol = "\n";
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        size_t lineEnd = nl == std::string::npos ? text.size() : nl + 1;
        std::string word = toLower(trim(text.substr(pos, lineEnd - pos)));
        if (instEnd == std::string::npos) {
            if (word == "inst") {
                instEnd = lineEnd;
                if (nl != std::string::npos && nl > pos && text[nl - 1] == '\r') eol = "\r\n";
            }
        } else if (word == "end") {
            endStart = pos;
            break;
        }
        pos = lineEnd;
    }
    if (instEnd == std::string::npos) {
        error = "no inst section in the file";
        return false;
    }
    if (endStart == std::string::npos) {
        error = "the inst section has no end line";
        return false;
    }
    std::string out = text.substr(0, instEnd);
    for (const IplInstance& i : instances) out += formatIplInstanceLine(i, version) + eol;
    out += text.substr(endStart);
    result = std::move(out);
    return true;
}

std::string writeTextIpl(const IplFile& ipl) {
    std::ostringstream out;
    out << "inst\n";
    for (const IplInstance& i : ipl.instances) out << formatIplInstanceLine(i, ipl.version) << "\n";
    out << "end\n";
    for (const IplRawSection& s : ipl.otherSections) {
        out << s.name << "\n";
        for (const std::string& l : s.lines) out << l << "\n";
        out << "end\n";
    }
    return out.str();
}

// ---- binary ----------------------------------------------------------------------------------
// MEd.exe's binary writer (0x00423730) emits "bnry", the instance count, then per instance
// x y z (float), rx ry rz rw (float), id, interior, lod: the 40-byte San Andreas record.
// The 0x4C-byte header layout and the instance offset at 0x1C come from public SA documentation.
// The offset is validated on read; it has not been confirmed against MEd's own header writer yet.
namespace {
constexpr size_t kHeaderSize = 0x4C;
constexpr size_t kInstOffsetField = 0x1C;
constexpr size_t kInstSize = 40;

uint32_t rd32(const std::vector<unsigned char>& d, size_t o) {
    uint32_t v;
    std::memcpy(&v, &d[o], 4);
    return v;
}
float rdf(const std::vector<unsigned char>& d, size_t o) {
    float v;
    std::memcpy(&v, &d[o], 4);
    return v;
}
void wr32(std::vector<unsigned char>& d, uint32_t v) {
    unsigned char b[4];
    std::memcpy(b, &v, 4);
    d.insert(d.end(), b, b + 4);
}
void wrf(std::vector<unsigned char>& d, double v) {
    float f = static_cast<float>(v);
    unsigned char b[4];
    std::memcpy(b, &f, 4);
    d.insert(d.end(), b, b + 4);
}
}  // namespace

bool isBinaryIpl(const std::vector<unsigned char>& d) {
    return d.size() >= 4 && d[0] == 'b' && d[1] == 'n' && d[2] == 'r' && d[3] == 'y';
}

IplFile parseBinaryIpl(const std::vector<unsigned char>& d) {
    IplFile ipl;
    ipl.version = GameVersion::SanAndreas;
    if (!isBinaryIpl(d) || d.size() < kHeaderSize) {
        ipl.warnings.push_back("not a binary IPL");
        return ipl;
    }
    uint32_t n = rd32(d, 4);
    size_t off = rd32(d, kInstOffsetField);
    if (off < 0x20 || off + static_cast<size_t>(n) * kInstSize > d.size()) off = kHeaderSize;
    if (off + static_cast<size_t>(n) * kInstSize > d.size()) {
        ipl.warnings.push_back("instance table runs past end of file");
        return ipl;
    }
    for (uint32_t k = 0; k < n; ++k) {
        size_t p = off + k * kInstSize;
        IplInstance i;
        i.x = rdf(d, p);
        i.y = rdf(d, p + 4);
        i.z = rdf(d, p + 8);
        i.rotX = rdf(d, p + 12);
        i.rotY = rdf(d, p + 16);
        i.rotZ = rdf(d, p + 20);
        i.rotW = rdf(d, p + 24);
        i.id = static_cast<int>(rd32(d, p + 28));
        i.interior = static_cast<int>(rd32(d, p + 32));
        i.lod = static_cast<short>(static_cast<int>(rd32(d, p + 36)));
        ipl.instances.push_back(i);
    }
    return ipl;
}

std::vector<unsigned char> writeBinaryIpl(const IplFile& ipl) {
    std::vector<unsigned char> d(kHeaderSize, 0);
    std::memcpy(d.data(), "bnry", 4);
    uint32_t n = static_cast<uint32_t>(ipl.instances.size());
    std::memcpy(&d[4], &n, 4);
    uint32_t off = kHeaderSize;
    std::memcpy(&d[kInstOffsetField], &off, 4);
    for (const IplInstance& i : ipl.instances) {
        wrf(d, i.x);
        wrf(d, i.y);
        wrf(d, i.z);
        wrf(d, i.rotX);
        wrf(d, i.rotY);
        wrf(d, i.rotZ);
        wrf(d, i.rotW);
        wr32(d, static_cast<uint32_t>(i.id));
        wr32(d, static_cast<uint32_t>(i.interior));
        wr32(d, static_cast<uint32_t>(i.lod == kNoLod ? -1 : i.lod));
    }
    return d;
}

}  // namespace med
