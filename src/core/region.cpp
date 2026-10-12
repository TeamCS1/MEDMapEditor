#include "region.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <vector>
#include "textutil.h"

namespace fs = std::filesystem;

namespace med {

namespace {

std::string readAll(const fs::path& p, bool& ok) {
    std::ifstream f(p, std::ios::binary);
    ok = bool(f);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

std::string normalised(std::string s) {
    for (char& c : s) {
        if (c == char(92)) c = '/';
        c = char(std::tolower(static_cast<unsigned char>(c)));
    }
    return trim(s);
}

}  // namespace

bool validRegionName(const std::string& name) {
    if (name.empty() || name.size() > 24 || !std::isalpha(static_cast<unsigned char>(name[0]))) return false;
    for (char c : name)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')) return false;
    return true;
}

bool addIplRegion(const std::string& gameRoot, const std::string& name, std::string& rel, std::string& error) {
    if (!validRegionName(name)) {
        error = "The region name must start with a letter and use only letters, digits, '_' and '-' (at most 24 characters).";
        return false;
    }
    const fs::path game(gameRoot), datPath = game / "data" / "gta3.dat";
    bool ok;
    const std::string dat = readAll(datPath, ok);
    if (!ok) {
        error = "Cannot read data/gta3.dat in " + gameRoot;
        return false;
    }
    const std::string bs(1, char(92));
    const std::string datLine = "IPL DATA" + bs + "MAPS" + bs + name + bs + name + ".IPL";
    rel = "DATA/MAPS/" + name + "/" + name + ".IPL";
    const fs::path iplPath = game / "data" / "maps" / name / (name + ".ipl");

    // the lines of gta3.dat, keeping the line ending style
    const std::string eol = dat.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos < dat.size()) {
        size_t nl = dat.find('\n', pos);
        std::string l = dat.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
        pos = nl == std::string::npos ? dat.size() : nl + 1;
    }
    int lastIpl = -1;
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string l = trim(lines[i]);
        if (l.size() > 4 && toLower(l.substr(0, 4)) == "ipl ") {
            lastIpl = int(i);
            if (normalised(l.substr(4)) == normalised(datLine.substr(4))) {
                error = "A region with this name is already listed in gta3.dat.";
                return false;
            }
        }
    }
    std::error_code ec;
    if (fs::exists(iplPath, ec)) {
        error = "The file " + iplPath.string() + " already exists.";
        return false;
    }
    if (lastIpl < 0) {  // no IPL line yet: before an EXIT line, else at the end
        lastIpl = int(lines.size()) - 1;
        for (size_t i = 0; i < lines.size(); ++i)
            if (toLower(trim(lines[i])) == "exit") { lastIpl = int(i) - 1; break; }
    }
    lines.insert(lines.begin() + (lastIpl + 1), datLine);

    fs::create_directories(iplPath.parent_path(), ec);
    {
        std::ofstream f(iplPath, std::ios::binary);
        f << "inst" << eol << "end" << eol;
        if (!f) {
            error = "Cannot write " + iplPath.string();
            return false;
        }
    }
    const fs::path backup = datPath.string() + ".med-backup";
    if (!fs::exists(backup, ec)) fs::copy_file(datPath, backup, ec);
    if (ec) {
        fs::remove(iplPath, ec);
        error = "Cannot back up gta3.dat.";
        return false;
    }
    std::ofstream out(datPath, std::ios::binary | std::ios::trunc);
    for (const std::string& l : lines) out << l << eol;
    if (!out) {
        error = "Cannot write data/gta3.dat (read-only?). The new IPL file was left in place.";
        return false;
    }
    return true;
}

std::vector<std::string> regionsAddedSinceBackup(const std::string& gameRoot) {
    std::vector<std::string> added;
    const fs::path dat = fs::path(gameRoot) / "data" / "gta3.dat", backup = dat.string() + ".med-backup";
    bool ok1, ok2;
    const std::string now = readAll(dat, ok1), before = readAll(backup, ok2);
    if (!ok1 || !ok2) return added;
    auto iplLines = [](const std::string& text) {
        std::vector<std::string> v;
        size_t pos = 0;
        while (pos < text.size()) {
            size_t nl = text.find('\n', pos);
            const std::string l = trim(text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos));
            pos = nl == std::string::npos ? text.size() : nl + 1;
            if (l.size() > 4 && toLower(l.substr(0, 4)) == "ipl ") v.push_back(trim(l.substr(4)));
        }
        return v;
    };
    std::vector<std::string> old;
    for (const std::string& p : iplLines(before)) old.push_back(normalised(p));
    for (std::string p : iplLines(now)) {
        if (std::find(old.begin(), old.end(), normalised(p)) != old.end()) continue;
        for (char& c : p) if (c == char(92)) c = '/';
        added.push_back(p);
    }
    return added;
}

}  // namespace med
