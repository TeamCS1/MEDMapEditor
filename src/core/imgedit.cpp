#include "imgedit.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include "textutil.h"

namespace fs = std::filesystem;

namespace med {

namespace {

uint32_t sectorsFor(uint64_t bytes) { return static_cast<uint32_t>((bytes + kImgSectorSize - 1) / kImgSectorSize); }

void put32(unsigned char* p, uint32_t v) { std::memcpy(p, &v, 4); }

uint64_t fnv(const char* p, size_t n, uint64_t h = 1469598103934665603ull) {
    for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<unsigned char>(p[i])) * 1099511628211ull;
    return h;
}

std::string extOf(const std::string& name) {
    const size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? "" : toLower(name.substr(dot + 1));
}

// "gta3.img" -> "gta3<tag>.img" (the .dir name follows the .img name, see ImgArchive::dirPath)
std::string tagged(const std::string& imgPath, const std::string& tag) {
    if (imgPath.size() >= 4 && toLower(imgPath.substr(imgPath.size() - 4)) == ".img")
        return imgPath.substr(0, imgPath.size() - 4) + tag + imgPath.substr(imgPath.size() - 4);
    return imgPath + tag;
}

std::string dirOf(const std::string& imgPath) {
    if (imgPath.size() >= 4 && toLower(imgPath.substr(imgPath.size() - 4)) == ".img")
        return imgPath.substr(0, imgPath.size() - 4) + (imgPath[imgPath.size() - 3] == 'I' ? ".DIR" : ".dir");
    return imgPath + ".dir";
}

}  // namespace

const char* imgFileType(const std::string& name) {
    const std::string e = extOf(name);
    if (e == "dff") return "3D Model";
    if (e == "txd") return "Texture Dictionary";
    if (e == "col") return "Collision File";
    if (e == "ipl") return "Binary IPL File";
    return "Unknown";
}

bool validImgEntryName(const std::string& name, std::string& why) {
    if (name.empty() || name.size() > kImgMaxNameLength) { why = "An archive name must be 1 to 23 characters (\"" + name + "\" has " + std::to_string(name.size()) + ")."; return false; }
    for (char c : name)
        if (c <= ' ' || c > '~' || c == '/' || c == '\\') { why = "An archive name cannot have spaces, slashes or other special characters."; return false; }
    return true;
}

ImgEdit::ImgEdit(const ImgArchive& a) : imgPath_(a.path()), version_(a.version()) {
    for (const ImgEntry& e : a.entries()) {
        ImgEditEntry x;
        x.name = x.sourceName = e.name;
        x.offset = e.offset;
        x.size = e.size;
        entries_.push_back(x);
    }
}

int ImgEdit::find(const std::string& name) const {
    const std::string n = toLower(name);
    for (size_t i = 0; i < entries_.size(); ++i)
        if (toLower(entries_[i].name) == n) return static_cast<int>(i);
    return -1;
}

bool ImgEdit::remove(const std::string& name, std::string& why) {
    const int i = find(name);
    if (i < 0) { why = "There is no entry " + name + " in the archive."; return false; }
    entries_.erase(entries_.begin() + i);
    changed_ = true;
    return true;
}

bool ImgEdit::rename(const std::string& from, const std::string& to, std::string& why) {
    const int i = find(from);
    if (i < 0) { why = "There is no entry " + from + " in the archive."; return false; }
    if (!validImgEntryName(to, why)) return false;
    const int j = find(to);
    if (j >= 0 && j != i) { why = "The archive already has an entry called " + entries_[size_t(j)].name + "."; return false; }
    if (entries_[size_t(i)].name == to) return true;  // nothing to do
    entries_[size_t(i)].name = to;
    changed_ = true;
    return true;
}

bool ImgEdit::addFile(const std::string& path, const std::string& nameIn, bool replace, std::string& why) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) { why = "Cannot read " + path + "."; return false; }
    const uint64_t bytes = fs::file_size(path, ec);
    if (ec) { why = "Cannot read " + path + "."; return false; }
    const std::string name = nameIn.empty() ? fs::path(path).filename().string() : nameIn;
    if (!validImgEntryName(name, why)) return false;
    if (bytes == 0) { why = name + " is empty."; return false; }
    ImgEditEntry x;
    x.name = name;
    x.file = path;
    x.fileBytes = bytes;
    x.size = sectorsFor(bytes);
    const int i = find(name);
    if (i >= 0) {
        if (!replace) { why = "The archive already has an entry called " + entries_[size_t(i)].name + "."; return false; }
        x.replaced = entries_[size_t(i)].replaced || !entries_[size_t(i)].sourceName.empty();
        entries_[size_t(i)] = x;  // same place in the list
    } else {
        entries_.push_back(x);
    }
    changed_ = true;
    return true;
}

std::string ImgEdit::status(const ImgEditEntry& e) {
    if (!e.file.empty()) return e.replaced ? "replaced" : "added";
    if (e.name != e.sourceName) return "renamed from " + e.sourceName;
    return "";
}

bool rebuildImg(const ImgEdit& edit, ImgArchive& reopened, ImgRebuildReport& report, std::string& why,
                const std::function<void(size_t, size_t)>& progress) {
    report = ImgRebuildReport();
    const std::string img = edit.imgPath(), dir = dirOf(img);
    const bool v2 = edit.version() == 2;
    const std::string newImg = tagged(img, ".med-new"), newDir = dirOf(newImg);
    const std::string oldImg = tagged(img, ".med-old"), oldDir = dirOf(oldImg);
    std::error_code ec;
    auto cleanup = [&] {
        fs::remove(newImg, ec);
        if (!v2) fs::remove(newDir, ec);
    };
    if (fs::exists(newImg, ec) || fs::exists(oldImg, ec)) {
        why = "A file from an earlier rebuild is in the way: " + (fs::exists(newImg, ec) ? newImg : oldImg) + ". Move it away first.";
        return false;
    }
    report.bytesBefore = fs::file_size(img, ec);
    if (ec) { why = "Cannot read " + img + "."; return false; }
    // every added file must still be what was added
    for (const ImgEditEntry& e : edit.entries())
        if (!e.file.empty() && (fs::file_size(e.file, ec) != e.fileBytes || ec)) {
            why = e.file + " changed or went away after it was added. Add it again.";
            return false;
        }

    // 1. write the new archive (data packed back to back, in the list's order) and its directory
    const std::vector<ImgEditEntry>& in = edit.entries();
    std::vector<ImgEntry> out(in.size());
    std::vector<uint64_t> hashes(in.size());
    {
        std::ifstream src(img, std::ios::binary);
        std::ofstream dst(newImg, std::ios::binary | std::ios::trunc);
        if (!src || !dst) { why = "Cannot open " + (src ? newImg : img) + "."; cleanup(); return false; }
        uint32_t next = v2 ? sectorsFor(8 + uint64_t(in.size()) * 32) : 0;
        std::vector<char> zero(size_t(next) * kImgSectorSize, 0);
        dst.write(zero.data(), std::streamsize(zero.size()));
        std::vector<char> buf;
        for (size_t i = 0; i < in.size(); ++i) {
            const ImgEditEntry& e = in[i];
            buf.assign(size_t(e.size) * kImgSectorSize, 0);
            if (e.file.empty()) {
                src.clear();
                src.seekg(std::streamoff(e.offset) * kImgSectorSize);
                src.read(buf.data(), std::streamsize(buf.size()));
                if (size_t(src.gcount()) != buf.size()) { why = e.sourceName + " runs past the end of " + img + "; the archive is damaged."; dst.close(); cleanup(); return false; }
            } else {
                std::ifstream f(e.file, std::ios::binary);
                f.read(buf.data(), std::streamsize(e.fileBytes));
                if (!f || uint64_t(f.gcount()) != e.fileBytes) { why = "Cannot read " + e.file + "."; dst.close(); cleanup(); return false; }
            }
            dst.write(buf.data(), std::streamsize(buf.size()));
            hashes[i] = fnv(buf.data(), buf.size());
            out[i].name = e.name;
            out[i].offset = next;
            out[i].size = e.size;
            next += e.size;
            if (progress) progress(i + 1, in.size());
        }
        std::vector<unsigned char> d(out.size() * 32, 0);
        for (size_t i = 0; i < out.size(); ++i) {
            put32(&d[i * 32], out[i].offset);
            put32(&d[i * 32 + 4], out[i].size);
            std::memcpy(&d[i * 32 + 8], out[i].name.data(), std::min<size_t>(out[i].name.size(), kImgMaxNameLength));
        }
        if (v2) {
            unsigned char head[8] = {'V', 'E', 'R', '2', 0, 0, 0, 0};
            put32(head + 4, uint32_t(out.size()));
            dst.seekp(0);
            dst.write(reinterpret_cast<const char*>(head), 8);
            dst.write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size()));
        } else {
            std::ofstream df(newDir, std::ios::binary | std::ios::trunc);
            df.write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size()));
            df.close();
            if (!df) { why = "Cannot write " + newDir + "."; dst.close(); cleanup(); return false; }
        }
        dst.close();
        if (!dst) { why = "Cannot write " + newImg + " (is the disk full?)."; cleanup(); return false; }
    }

    // 2. read it back: the same names, places and bytes
    {
        ImgArchive check;
        std::string e;
        if (!ImgArchive::open(newImg, check, e) || check.entries().size() != out.size()) { why = "The new archive does not read back: " + e; cleanup(); return false; }
        std::ifstream f(newImg, std::ios::binary);
        std::vector<char> buf;
        for (size_t i = 0; i < out.size(); ++i) {
            const ImgEntry& c = check.entries()[i];
            if (c.name != out[i].name || c.offset != out[i].offset || c.size != out[i].size) { why = "The new archive's directory differs at " + out[i].name + "."; cleanup(); return false; }
            buf.assign(size_t(c.size) * kImgSectorSize, 0);
            f.seekg(std::streamoff(c.offset) * kImgSectorSize);
            f.read(buf.data(), std::streamsize(buf.size()));
            if (size_t(f.gcount()) != buf.size() || fnv(buf.data(), buf.size()) != hashes[i]) { why = "The new archive's data differs at " + out[i].name + "."; cleanup(); return false; }
        }
    }

    // 3. swap by renaming (the old files are never written to)
    auto mv = [&](const std::string& a, const std::string& b) {
        std::error_code e2;
        fs::rename(a, b, e2);
        return !e2;
    };
    if (!mv(img, oldImg)) { why = "Cannot replace " + img + " (is the game or another program using it?)."; cleanup(); return false; }
    if (!v2 && !mv(dir, oldDir)) { mv(oldImg, img); why = "Cannot replace " + dir + " (is it in use?)."; cleanup(); return false; }
    if (!mv(newImg, img)) {
        mv(oldImg, img);
        if (!v2) mv(oldDir, dir);
        why = "Cannot put the new archive in place of " + img + ".";
        cleanup();
        return false;
    }
    if (!v2 && !mv(newDir, dir)) {
        mv(img, newImg);
        mv(oldImg, img);
        mv(oldDir, dir);
        why = "Cannot put the new directory in place of " + dir + ".";
        cleanup();
        return false;
    }
    // the archive as it was: kept once as .med-backup, later ones are dropped
    report.backupImg = img + ".med-backup";
    report.backupDir = v2 ? "" : dir + ".med-backup";
    if (!fs::exists(report.backupImg, ec)) {
        report.backupMade = mv(oldImg, report.backupImg);
        if (!v2) mv(oldDir, report.backupDir);
    }
    fs::remove(oldImg, ec);
    if (!v2) fs::remove(oldDir, ec);

    std::string e;
    if (!ImgArchive::open(img, reopened, e)) { why = "The rebuilt archive was put in place but does not open: " + e; return false; }
    report.bytesAfter = fs::file_size(img, ec);
    report.entries = out.size();
    return true;
}

}  // namespace med
