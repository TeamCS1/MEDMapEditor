#include "poollimits.h"
#include <cstdlib>
#include <fstream>
#include "textutil.h"

namespace med {

bool parseLimitAdjusterIni(const std::string& text, PoolLimits& out) {
    bool inSection = false, found = false;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() : nl + 1;
        size_t semi = line.find(';');
        if (semi != std::string::npos) line.erase(semi);
        line = trim(line);
        if (line.empty()) continue;
        if (line[0] == '[') {
            inSection = toLower(line) == "[gta3limits]";
            found = found || inSection;
            continue;
        }
        if (!inSection) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = toLower(trim(line.substr(0, eq))), value = toLower(trim(line.substr(eq + 1)));
        int pool = key == "buildings" ? 0 : key == "treadables" ? 1 : (key == "dummys" || key == "dummies") ? 2 : -1;
        if (pool < 0 || value.empty()) continue;
        if (value == "unlimited") out.cap[pool] = kUnlimited;
        else if (std::atoll(value.c_str()) > 0) out.cap[pool] = std::atoll(value.c_str());
    }
    if (found) out.adjusted = true;
    return found;
}

std::string findLimitAdjusterIni(const std::string& gameRoot) {
    for (const char* rel : {"III.VC.SA.LimitAdjuster.ini", "scripts/III.VC.SA.LimitAdjuster.ini", "plugins/III.VC.SA.LimitAdjuster.ini"}) {
        std::ifstream f(gameRoot + "/" + rel);
        if (f) return rel;
    }
    return "";
}

std::string describeLimits(const PoolLimits& l) { return l.adjusted ? "Limit adjuster (" + l.source + ")" : "Vanilla"; }

}  // namespace med
