#include "extract.h"
#include <filesystem>
#include <fstream>
#include "col.h"
#include "textutil.h"

namespace fs = std::filesystem;

namespace med {

namespace {

bool readFile(const std::string& path, std::vector<unsigned char>& data) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    data.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return true;
}

bool writeFile(const fs::path& path, const std::vector<unsigned char>& data) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
    return bool(f);
}

std::vector<fs::path> targets(const std::string& folder, const ExtractRequest& req) {
    const fs::path dir = fs::path(folder) / req.model;
    std::vector<fs::path> t = {dir / (req.model + ".dff"), dir / (req.model + ".col")};
    if (!req.txd.empty()) t.push_back(dir / (req.txd + ".txd"));
    if (!req.info.empty()) t.push_back(dir / (req.model + ".txt"));
    return t;
}

}  // namespace

std::vector<std::string> extractTargetsThatExist(const std::string& folder, const ExtractRequest& req) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const fs::path& p : targets(folder, req))
        if (fs::exists(p, ec)) out.push_back(p.string());
    return out;
}

bool findCollisionRecord(const std::vector<std::string>& colFiles, const std::string& model, std::vector<unsigned char>& record, std::string& source, int* matches) {
    record.clear();
    source.clear();
    int found = 0;
    const std::string want = toLower(model);
    for (const std::string& path : colFiles) {
        std::vector<unsigned char> data;
        if (!readFile(path, data)) continue;
        std::vector<ColModel> models;
        std::string err;
        parseCol(data, models, err);  // models read before a damaged part still count
        for (const ColModel& m : models) {
            if (toLower(m.name) != want) continue;
            ++found;  // the last one wins, as in the game (CFileLoader_LoadCollisionFile 0x00478b20 loads over an earlier one)
            {
                record.assign(data.begin() + std::ptrdiff_t(m.fileOffset), data.begin() + std::ptrdiff_t(m.fileOffset + m.fileSize));
                source = path;
            }
        }
    }
    if (matches) *matches = found;
    return found > 0;
}

ExtractResult extractObject(const ImgArchive& img, const std::string& folder, const ExtractRequest& req) {
    ExtractResult r;
    const fs::path dir = fs::path(folder) / req.model;
    r.folder = dir.string();
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec || !fs::is_directory(dir, ec)) {
        r.warnings.push_back("Could not create the folder " + dir.string() + ".");
        return r;
    }
    auto save = [&](const fs::path& p, const std::vector<unsigned char>& data, const std::string& what) {
        if (writeFile(p, data)) r.written.push_back(p.string());
        else r.warnings.push_back("Could not write " + p.string() + " (" + what + ").");
    };
    // the model
    {
        std::vector<unsigned char> data;
        std::string err;
        if (img.extract(req.model + ".dff", data, err)) save(dir / (req.model + ".dff"), data, "the model");
        else r.warnings.push_back("The model " + req.model + ".dff is not in " + img.path() + ".");
    }
    // the textures
    if (!req.txd.empty()) {
        std::vector<unsigned char> data;
        std::string err;
        if (img.extract(req.txd + ".txd", data, err)) save(dir / (req.txd + ".txd"), data, "the textures");
        else if (toLower(req.txd) == "generic" && !req.genericTxdPath.empty() && readFile(req.genericTxdPath, data))
            save(dir / (req.txd + ".txd"), data, "the textures");
        else r.warnings.push_back("The texture dictionary " + req.txd + ".txd is not in " + img.path() + ".");
    }
    // the collision
    {
        std::vector<unsigned char> record;
        int matches = 0;
        if (findCollisionRecord(req.colFiles, req.model, record, r.colSource, &matches)) {
            save(dir / (req.model + ".col"), record, "the collision");
            if (matches > 1)
                r.warnings.push_back(std::to_string(matches) + " COL files have a collision model named " + req.model + "; the last one (" + r.colSource + "), which the game uses, was copied.");
        } else {
            r.warnings.push_back("No collision model named " + req.model + " was found in the COL files.");
        }
    }
    // what it is, for putting it back later
    if (!req.info.empty()) {
        std::string text = req.info;
        if (!r.colSource.empty()) text += "Collision copied from: " + r.colSource + "\r\n";
        save(dir / (req.model + ".txt"), std::vector<unsigned char>(text.begin(), text.end()), "the info file");
    }
    return r;
}

}  // namespace med
