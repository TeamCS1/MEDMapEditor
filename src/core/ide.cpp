#include "ide.h"
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include "textutil.h"

namespace med {

namespace {

bool toInt(const std::string& s, int& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (*end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

bool toDouble(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    double v = std::strtod(s.c_str(), &end);
    if (*end != '\0') return false;
    out = v;
    return true;
}

std::string fmt(double d) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.9g", d);
    return buf;
}

// objs/tobj layouts (confirmed against the IDE loader in MEd.exe, 0x00418230):
//   GTA III / VC: id, model, txd, meshCount, drawDist x meshCount, flags [, timeOn, timeOff]
//   San Andreas:  id, model, txd, drawDist, flags [, timeOn, timeOff]
//                 MEd also accepts the older form with one extra column after txd
//                 (6 fields for objs, 8 for tobj) and ignores that column.
bool parseObject(const std::vector<std::string>& f, GameVersion v, bool timed, IdeObject& o) {
    if (f.size() < 5 || !toInt(f[0], o.id)) return false;
    o.model = f[1];
    o.txd = f[2];
    o.timed = timed;
    size_t i = 3;
    int meshes = 1;
    if (v != GameVersion::SanAndreas) {
        if (!toInt(f[i++], meshes) || meshes < 1 || meshes > 3) return false;
    } else if (f.size() == (timed ? 8u : 6u)) {
        ++i;  // legacy extra column
    }
    o.drawDistances.clear();
    for (int m = 0; m < meshes; ++m) {
        double d;
        if (i >= f.size() || !toDouble(f[i++], d)) return false;
        o.drawDistances.push_back(d);
    }
    if (i >= f.size() || !toInt(f[i++], o.flags)) return false;
    if (timed) {
        if (i + 1 >= f.size() || !toInt(f[i], o.timeOn) || !toInt(f[i + 1], o.timeOff)) return false;
    }
    return true;
}

}  // namespace

IdeFile parseIde(const std::string& text, GameVersion version) {
    IdeFile ide;
    ide.version = version;
    std::istringstream in(text);
    std::string raw, section;
    int rawIndex = -1;
    int lineNo = 0;
    while (std::getline(in, raw)) {
        ++lineNo;
        std::string line = trim(raw);
        if (isBlankOrComment(line)) continue;
        if (section.empty()) {
            section = toLower(line);
            rawIndex = -1;
            if (section != "objs" && section != "tobj") {
                ide.otherSections.push_back({section, {}});
                rawIndex = static_cast<int>(ide.otherSections.size()) - 1;
            }
            continue;
        }
        if (toLower(line) == "end") {
            section.clear();
            rawIndex = -1;
            continue;
        }
        if (rawIndex >= 0) {
            ide.otherSections[rawIndex].lines.push_back(line);
            continue;
        }
        IdeObject obj;
        if (parseObject(splitFields(line), version, section == "tobj", obj))
            ide.objects.push_back(obj);
        else
            ide.warnings.push_back("line " + std::to_string(lineNo) + ": bad " + section + " entry: " + line);
    }
    if (!section.empty()) ide.warnings.push_back("section '" + section + "' is missing its end");
    return ide;
}

std::string writeIde(const IdeFile& ide) {
    std::ostringstream out;
    for (int timed = 0; timed < 2; ++timed) {
        bool any = false;
        for (const IdeObject& o : ide.objects) any = any || (o.timed == (timed == 1));
        if (!any) continue;
        out << (timed ? "tobj\n" : "objs\n");
        for (const IdeObject& o : ide.objects) {
            if (o.timed != (timed == 1)) continue;
            out << o.id << ", " << o.model << ", " << o.txd;
            if (ide.version != GameVersion::SanAndreas) out << ", " << o.drawDistances.size();
            for (double d : o.drawDistances) out << ", " << fmt(d);
            out << ", " << o.flags;
            if (o.timed) out << ", " << o.timeOn << ", " << o.timeOff;
            out << "\n";
        }
        out << "end\n";
    }
    for (const IdeRawSection& s : ide.otherSections) {
        out << s.name << "\n";
        for (const std::string& l : s.lines) out << l << "\n";
        out << "end\n";
    }
    return out.str();
}

}  // namespace med
