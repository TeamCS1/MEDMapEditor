#include "pools.h"
#include <cstdlib>
#include "textutil.h"

namespace med {

const char* poolName(Pool p) {
    switch (p) {
        case Pool::Building: return "Buildings";
        case Pool::Treadable: return "Treadables";
        case Pool::Dummy: return "Dummies";
    }
    return "";
}

std::set<std::string> parseObjectDatNames(const std::string& text) {
    std::set<std::string> names;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() : nl + 1;
        size_t semi = line.find(';');
        if (semi != std::string::npos) line.erase(semi);
        line = trim(line);
        if (line.empty()) continue;
        if (line[0] == '*') break;
        size_t end = line.find_first_of(", \t");
        names.insert(toLower(line.substr(0, end)));
    }
    return names;
}

void collectPathModelIds(const IdeFile& ide, std::set<int>& ids) {
    for (const IdeRawSection& s : ide.otherSections) {
        if (toLower(s.name) != "path") continue;
        for (const std::string& line : s.lines) {
            std::vector<std::string> f = splitFields(line);
            if (f.size() >= 2 && (toLower(f[0]) == "ped" || toLower(f[0]) == "car")) ids.insert(std::atoi(f[1].c_str()));
        }
    }
}

}  // namespace med
