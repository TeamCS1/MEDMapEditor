#include "ideedit.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "textutil.h"

namespace med {

namespace {

const IdeFlagInfo kFlags[kIdeFlagCount] = {
    {1, "normalCull - Normal cull", "Renders Wet Effect", true},
    {2, "noFade - No fade-in", "TOBJ Night Flag", true},
    {4, "drawLast - Draw last, for transparency", "Alpha Transparency 1", true},
    {8, "additive - Additive blending, also draws last", "Alpha Transparency 2", true},
    {16, "isSubway - Subway object", "TOBJ Day Flag", true},
    {32, "ignoreLight - Ignore lighting", "Interior Object", true},
    {64, "noZwrite - No depth writes", "Disable Shadow Culling", true},
    {128, "Exclude surface from culling - not used by GTA III", "Exclude Surface from Culling", false},
    {256, "Disable draw distance - not used by GTA III", "Disable Draw Distance", false},
    {512, "Breakable window 1 - not used by GTA III", "Breakable Window 1", false},
    {1024, "Breakable window 2 - not used by GTA III", "Breakable Window 2", false},
};

// The shortest text that reads back as exactly the same number (152.25 stays 152.25, 300 stays 300).
std::string num(double d) {
    char b[48];
    for (int precision = 6; precision <= 17; ++precision) {  // from 6: no exponent below a million
        std::snprintf(b, sizeof b, "%.*g", precision, d);
        if (std::strtod(b, nullptr) == d) break;
    }
    return b;
}

std::string eolOf(const std::string& text) {
    const size_t n = text.find('\n');
    if (n == std::string::npos) return "\r\n";  // Rockstar's files use Windows line endings
    return n > 0 && text[n - 1] == '\r' ? "\r\n" : "\n";
}

// One physical line: [begin, end) without "\r\n" / "\n"; next = start of the following line.
struct Line { size_t begin, end, next; };
std::vector<Line> linesOf(const std::string& text) {
    std::vector<Line> out;
    size_t p = 0;
    while (p < text.size()) {
        size_t n = text.find('\n', p);
        const size_t next = n == std::string::npos ? text.size() : n + 1;
        size_t e = n == std::string::npos ? text.size() : n;
        if (e > p && text[e - 1] == '\r') --e;
        out.push_back({p, e, next});
        p = next;
    }
    return out;
}

bool validTxdName(const std::string& s) {
    if (s.empty() || s.size() > 19) return false;
    for (char c : s)
        if (c <= ' ' || c == ',' || c == '#' || static_cast<unsigned char>(c) > 126) return false;
    return true;
}

}  // namespace

const IdeFlagInfo& ideFlag(int index) { return kFlags[index < 0 ? 0 : index >= kIdeFlagCount ? kIdeFlagCount - 1 : index]; }

std::string formatIdeEntry(const IdeObject& o) {
    std::string s = std::to_string(o.id) + ", " + o.model + ", " + o.txd + ", " + std::to_string(o.drawDistances.size());
    for (double d : o.drawDistances) s += ", " + num(d);
    s += ", " + std::to_string(o.flags);
    if (o.timed) s += ", " + std::to_string(o.timeOn) + ", " + std::to_string(o.timeOff);
    return s;
}

bool findIdeEntry(const std::string& text, int id, IdeEntryLoc& loc) {
    std::string section;
    for (const Line& l : linesOf(text)) {
        const std::string line = trim(text.substr(l.begin, l.end - l.begin));
        if (isBlankOrComment(line)) continue;
        if (section.empty()) { section = toLower(line); continue; }
        if (toLower(line) == "end") { section.clear(); continue; }
        if (section != "objs" && section != "tobj") continue;
        IdeFile one = parseIde(section + "\n" + line + "\nend\n", GameVersion::GTA3);
        if (one.objects.size() != 1 || one.objects[0].id != id) continue;
        loc.begin = l.begin;
        loc.end = l.end;
        loc.timed = section == "tobj";
        loc.object = one.objects[0];
        return true;
    }
    return false;
}

bool validIdeEdit(const IdeObject& original, const IdeObject& e, std::string& why) {
    if (e.id != original.id) { why = "The ID number cannot be changed here (placements and the collision refer to it)."; return false; }
    if (e.model != original.model) { why = "The model name cannot be changed here (gta3.img and the collision refer to it)."; return false; }
    if (e.drawDistances.size() != original.drawDistances.size()) { why = "The number of draw distances is fixed by the model."; return false; }
    for (double d : e.drawDistances)
        if (!std::isfinite(d) || d < 0 || d > kMaxDrawDistance) {
            why = "A draw distance must be a number from 0 to " + num(kMaxDrawDistance) + ".";
            return false;
        }
    if (e.flags < 0 || e.flags > 2047) { why = "The flags must be a number from 0 to 2047."; return false; }
    if (!validTxdName(e.txd)) { why = "The texture dictionary name must be 1 to 19 characters, without spaces or commas."; return false; }
    if (e.timed) {
        if (e.timeOn < 0 || e.timeOn > 23 || e.timeOff < 0 || e.timeOff > 23) { why = "Time on and time off are hours from 0 to 23."; return false; }
        if (e.timeOn == e.timeOff) { why = "Time on and time off are the same hour, so the game would never show the object."; return false; }
    }
    return true;
}

bool sameIdeEntry(const IdeObject& a, const IdeObject& b) {
    return a.id == b.id && a.model == b.model && a.txd == b.txd && a.drawDistances == b.drawDistances && a.flags == b.flags && a.timed == b.timed &&
           (!a.timed || (a.timeOn == b.timeOn && a.timeOff == b.timeOff));
}

bool setIdeEntry(std::string& text, const IdeObject& edited, std::string& why) {
    IdeEntryLoc loc;
    if (!findIdeEntry(text, edited.id, loc)) { why = "This IDE file has no definition with ID " + std::to_string(edited.id) + "."; return false; }
    if (!validIdeEdit(loc.object, edited, why)) return false;
    const std::string line = formatIdeEntry(edited);
    if (edited.timed == loc.timed) {
        text.replace(loc.begin, loc.end - loc.begin, line);
        return true;
    }
    // move to the other section: take the whole line out, then put it before the end of the first such section
    const std::string eol = eolOf(text);
    std::string out = text;
    size_t lineNext = out.find('\n', loc.end);
    lineNext = lineNext == std::string::npos ? out.size() : lineNext + 1;
    out.erase(loc.begin, lineNext - loc.begin);
    const std::string want = edited.timed ? "tobj" : "objs";
    std::string section;
    bool inWanted = false;
    for (const Line& l : linesOf(out)) {
        const std::string t = trim(out.substr(l.begin, l.end - l.begin));
        if (isBlankOrComment(t)) continue;
        if (section.empty()) { section = toLower(t); inWanted = section == want; continue; }
        if (toLower(t) == "end") {
            if (inWanted) {
                out.insert(l.begin, line + eol);
                text = out;
                return true;
            }
            section.clear();
        }
    }
    if (!out.empty() && out.back() != '\n') out += eol;
    out += want + eol + line + eol + "end" + eol;
    text = out;
    return true;
}

}  // namespace med
