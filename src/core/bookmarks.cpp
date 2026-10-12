#include "bookmarks.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include "textutil.h"

namespace med {

namespace {

// The text between "<tag id=N>" and "</tag>" for every N in a line.
void collect(const std::string& line, const std::string& tag, std::map<int, std::string>& out) {
    const std::string open = "<" + tag + " id=", close = "</" + tag + ">";
    size_t pos = 0;
    while ((pos = line.find(open, pos)) != std::string::npos) {
        const size_t idStart = pos + open.size();
        const size_t gt = line.find('>', idStart);
        if (gt == std::string::npos) return;
        const std::string idText = line.substr(idStart, gt - idStart);
        char* end = nullptr;
        const long id = std::strtol(idText.c_str(), &end, 10);
        const size_t stop = line.find(close, gt + 1);
        if (stop == std::string::npos) return;
        if (!idText.empty() && *end == 0) out[int(id)] = line.substr(gt + 1, stop - gt - 1);
        pos = stop + close.size();
    }
}

std::string number(double v) {
    char b[64];
    std::snprintf(b, sizeof b, "%.6g", v);
    return b;
}

}  // namespace

std::vector<Bookmark> parseBookmarks(const std::string& text) {
    std::map<int, std::string> names, locations;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        const std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() : nl + 1;
        collect(line, "bookmark", names);
        collect(line, "location", locations);
    }
    std::vector<Bookmark> list;
    for (const auto& kv : names) {  // in id order, like MEd's list
        auto loc = locations.find(kv.first);
        if (loc == locations.end()) continue;
        std::vector<std::string> f;
        const std::string& v = loc->second;
        for (size_t p = 0; p <= v.size();) {
            size_t c = v.find(':', p);
            f.push_back(v.substr(p, c == std::string::npos ? std::string::npos : c - p));
            if (c == std::string::npos) break;
            p = c + 1;
        }
        if (f.size() < 6) continue;
        Bookmark b;
        b.name = kv.second;
        double* dst[6] = {&b.x, &b.y, &b.z, &b.h, &b.yaw, &b.pitch};
        bool ok = true;
        for (int i = 0; i < 6; ++i) {
            char* end = nullptr;
            const std::string s = trim(f[i]);
            *dst[i] = std::strtod(s.c_str(), &end);
            ok = ok && !s.empty() && *end == 0;
        }
        if (!ok) continue;
        b.flag = f.size() > 6 ? std::atoi(f[6].c_str()) : 0;
        list.push_back(b);
    }
    return list;
}

std::string writeBookmarks(const std::vector<Bookmark>& list) {
    std::string out;
    for (size_t i = 0; i < list.size(); ++i) {
        const Bookmark& b = list[i];
        const std::string id = std::to_string(i);
        out += "<bookmark id=" + id + ">" + b.name + "</bookmark><location id=" + id + ">" + number(b.x) + ":" + number(b.y) + ":" + number(b.z) + ":" +
               number(b.h) + ":" + number(b.yaw) + ":" + number(b.pitch) + ":" + std::to_string(b.flag) + "</location>\r\n";
    }
    return out + "\r\n";  // MEd's file ends with an empty line
}

bool validBookmarkName(const std::string& name) {
    if (trim(name).empty()) return false;
    for (char c : name)
        if (c == '<' || c == '>' || c == '\r' || c == '\n') return false;
    return true;
}

}  // namespace med
