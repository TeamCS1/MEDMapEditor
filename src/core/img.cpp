#include "img.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "textutil.h"

namespace fs = std::filesystem;

namespace med {

namespace {

uint32_t sectorsFor(uint64_t bytes) { return static_cast<uint32_t>((bytes + kImgSectorSize - 1) / kImgSectorSize); }

void put32(unsigned char* p, uint32_t v) { std::memcpy(p, &v, 4); }
uint32_t get32(const unsigned char* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

// 32-byte directory record.
void packEntry(const ImgEntry& e, unsigned char* out) {
    std::memset(out, 0, 32);
    put32(out, e.offset);
    put32(out + 4, e.size);
    std::memcpy(out + 8, e.name.data(), std::min<size_t>(e.name.size(), kImgMaxNameLength));
}

ImgEntry unpackEntry(const unsigned char* in) {
    ImgEntry e;
    e.offset = get32(in);
    e.size = get32(in + 4);
    e.name.assign(reinterpret_cast<const char*>(in + 8), strnlen(reinterpret_cast<const char*>(in + 8), 24));
    return e;
}

bool readAll(const std::string& path, std::vector<unsigned char>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

}  // namespace

std::string ImgArchive::dirPath() const {
    std::string p = imgPath_;
    if (p.size() >= 4) {
        std::string ext = p.substr(p.size() - 4);
        if (toLower(ext) == ".img") {
            bool upper = ext[1] == 'I';
            return p.substr(0, p.size() - 4) + (upper ? ".DIR" : ".dir");
        }
    }
    return p + ".dir";
}

int ImgArchive::find(const std::string& name) const {
    std::string n = toLower(name);
    for (size_t i = 0; i < entries_.size(); ++i)
        if (toLower(entries_[i].name) == n) return static_cast<int>(i);
    return -1;
}

bool ImgArchive::create(const std::string& imgPath, int version, ImgArchive& out, std::string& error) {
    if (version != 1 && version != 2) {
        error = "version must be 1 or 2";
        return false;
    }
    out = ImgArchive();
    out.version_ = version;
    out.imgPath_ = imgPath;
    std::ofstream img(imgPath, std::ios::binary | std::ios::trunc);
    if (!img) {
        error = "cannot create " + imgPath;
        return false;
    }
    if (version == 2) {
        // MEd (0x0041cce0) writes two ints and pads the header out to one 2048-byte sector.
        std::vector<char> header(kImgSectorSize, 0);
        std::memcpy(header.data(), "VER2", 4);
        img.write(header.data(), header.size());
    }
    img.close();
    if (version == 1) {
        std::ofstream dir(out.dirPath(), std::ios::binary | std::ios::trunc);
        if (!dir) {
            error = "cannot create " + out.dirPath();
            return false;
        }
    }
    return true;
}

bool ImgArchive::open(const std::string& imgPath, ImgArchive& out, std::string& error) {
    out = ImgArchive();
    out.imgPath_ = imgPath;
    std::ifstream img(imgPath, std::ios::binary);
    if (!img) {
        error = "Cannot open image file " + imgPath;
        return false;
    }
    char magic[4] = {0, 0, 0, 0};
    img.read(magic, 4);
    std::vector<unsigned char> dir;
    if (img.gcount() == 4 && std::memcmp(magic, "VER2", 4) == 0) {
        out.version_ = 2;
        unsigned char countBytes[4];
        img.read(reinterpret_cast<char*>(countBytes), 4);
        if (img.gcount() != 4) {
            error = "truncated IMG header";
            return false;
        }
        uint32_t count = get32(countBytes);
        dir.resize(static_cast<size_t>(count) * 32);
        img.read(reinterpret_cast<char*>(dir.data()), dir.size());
        if (static_cast<size_t>(img.gcount()) != dir.size()) {
            error = "IMG directory is truncated";
            return false;
        }
    } else {
        out.version_ = 1;
        if (!readAll(out.dirPath(), dir)) {
            error = "Cannot open directory file " + out.dirPath();
            return false;
        }
        if (dir.size() % 32 != 0) {
            error = "directory file size is not a multiple of 32";
            return false;
        }
    }
    for (size_t p = 0; p + 32 <= dir.size(); p += 32) out.entries_.push_back(unpackEntry(&dir[p]));
    return true;
}

bool ImgArchive::extract(const std::string& name, std::vector<unsigned char>& data, std::string& error) const {
    int i = find(name);
    if (i < 0) {
        error = "no such entry: " + name;
        return false;
    }
    const ImgEntry& e = entries_[i];
    std::ifstream img(imgPath_, std::ios::binary);
    if (!img) {
        error = "cannot open " + imgPath_;
        return false;
    }
    data.assign(static_cast<size_t>(e.size) * kImgSectorSize, 0);
    img.seekg(static_cast<std::streamoff>(e.offset) * kImgSectorSize);
    img.read(reinterpret_cast<char*>(data.data()), data.size());
    if (static_cast<size_t>(img.gcount()) != data.size()) {
        error = "entry " + name + " runs past the end of the archive";
        return false;
    }
    return true;  // note: includes sector padding; the true size is not stored in the directory
}

bool ImgArchive::extractToFile(const std::string& name, const std::string& destPath, std::string& error) const {
    std::vector<unsigned char> data;
    if (!extract(name, data, error)) return false;
    std::ofstream out(destPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "cannot write " + destPath;
        return false;
    }
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

bool ImgArchive::add(const std::string& name, const std::vector<unsigned char>& data, std::string& error) {
    if (name.empty() || name.size() > kImgMaxNameLength) {
        error = "entry name must be 1-23 characters";
        return false;
    }
    if (find(name) >= 0 && !remove(name, error)) return false;
    std::error_code ec;
    uint64_t size = fs::file_size(imgPath_, ec);
    if (ec) {
        error = "cannot stat " + imgPath_;
        return false;
    }
    std::fstream img(imgPath_, std::ios::binary | std::ios::in | std::ios::out);
    if (!img) {
        error = "cannot open " + imgPath_ + " for writing";
        return false;
    }
    uint32_t startSector = sectorsFor(size);
    // Pad to the sector boundary (MEd: 0x0041c790 pads with 0x800 - (size & 0x7ff)), then write.
    std::vector<char> padded(static_cast<size_t>(sectorsFor(data.size())) * kImgSectorSize, 0);
    std::memcpy(padded.data(), data.data(), data.size());
    img.seekp(static_cast<std::streamoff>(startSector) * kImgSectorSize);
    img.write(padded.data(), padded.size());
    if (!img) {
        error = "write failed";
        return false;
    }
    ImgEntry e;
    e.name = name;
    e.offset = startSector;
    e.size = sectorsFor(data.size());
    entries_.push_back(e);
    return true;
}

bool ImgArchive::remove(const std::string& name, std::string& error) {
    int i = find(name);
    if (i < 0) {
        error = "no such entry: " + name;
        return false;
    }
    entries_.erase(entries_.begin() + i);
    needsRebuild_ = true;
    return true;
}

bool ImgArchive::writeDirectory(std::string& error) const {
    std::vector<unsigned char> buf(entries_.size() * 32);
    for (size_t i = 0; i < entries_.size(); ++i) packEntry(entries_[i], &buf[i * 32]);
    if (version_ == 1) {
        std::ofstream dir(dirPath(), std::ios::binary | std::ios::trunc);
        if (!dir) {
            error = "cannot write " + dirPath();
            return false;
        }
        dir.write(reinterpret_cast<const char*>(buf.data()), buf.size());
        return true;
    }
    std::fstream img(imgPath_, std::ios::binary | std::ios::in | std::ios::out);
    if (!img) {
        error = "cannot open " + imgPath_ + " for writing";
        return false;
    }
    unsigned char head[8] = {'V', 'E', 'R', '2', 0, 0, 0, 0};
    put32(head + 4, static_cast<uint32_t>(entries_.size()));
    img.seekp(0);
    img.write(reinterpret_cast<const char*>(head), 8);
    img.write(reinterpret_cast<const char*>(buf.data()), buf.size());
    return static_cast<bool>(img);
}

// Rewrite the archive with entries packed back to back. MEd does the same through the
// "tempdir" / "tempimg" files (0x0041b720).
bool ImgArchive::rebuild(std::string& error) {
    std::string tmpPath = imgPath_ + ".tmp";
    std::vector<ImgEntry> fresh = entries_;
    uint32_t next = 0;
    if (version_ == 2) next = sectorsFor(8 + fresh.size() * 32);
    {
        std::ifstream src(imgPath_, std::ios::binary);
        std::ofstream dst(tmpPath, std::ios::binary | std::ios::trunc);
        if (!src || !dst) {
            error = "cannot open files for rebuild";
            return false;
        }
        std::vector<char> zero(static_cast<size_t>(next) * kImgSectorSize, 0);
        dst.write(zero.data(), zero.size());
        std::vector<char> buf;
        for (ImgEntry& e : fresh) {
            buf.assign(static_cast<size_t>(e.size) * kImgSectorSize, 0);
            src.clear();
            src.seekg(static_cast<std::streamoff>(e.offset) * kImgSectorSize);
            src.read(buf.data(), buf.size());
            dst.write(buf.data(), buf.size());
            e.offset = next;
            next += e.size;
        }
        if (!dst) {
            error = "write failed during rebuild";
            return false;
        }
    }
    std::error_code ec;
    fs::remove(imgPath_, ec);
    fs::rename(tmpPath, imgPath_, ec);
    if (ec) {
        error = "cannot replace " + imgPath_ + ": " + ec.message();
        return false;
    }
    entries_ = fresh;
    needsRebuild_ = false;
    return true;
}

bool ImgArchive::save(std::string& error) {
    bool rebuildNeeded = needsRebuild_;
    if (version_ == 2 && !rebuildNeeded && !entries_.empty()) {
        uint32_t firstData = entries_[0].offset;
        for (const ImgEntry& e : entries_) firstData = std::min(firstData, e.offset);
        if (sectorsFor(8 + entries_.size() * 32) > firstData) rebuildNeeded = true;  // directory outgrew its sector(s)
    }
    if (rebuildNeeded && !rebuild(error)) return false;
    return writeDirectory(error);
}

}  // namespace med
