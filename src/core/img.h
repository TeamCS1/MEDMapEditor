// IMG archives.
//   Version 1 (GTA III, Vice City): <name>.img holds raw data, <name>.dir holds the directory.
//   Version 2 (San Andreas):        a single .img with "VER2", entry count and the directory
//                                   at the start, followed by the data.
// All offsets and sizes are in 2048-byte sectors. Directory entries are 32 bytes:
// offset (u32), size (u32), name (24 bytes, NUL terminated).
// Recovered from MEd.exe: load 0x0041b1e0, extract 0x0041a4e0, rebuild 0x0041b720,
// append 0x0041c790, create 0x0041cce0.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace med {

constexpr uint32_t kImgSectorSize = 2048;
constexpr size_t kImgMaxNameLength = 23;  // 24-byte field including the terminator

struct ImgEntry {
    std::string name;     // e.g. "hospital.dff"
    uint32_t offset = 0;  // sectors
    uint32_t size = 0;    // sectors
};

class ImgArchive {
public:
    // Create an empty archive on disk and open it.
    static bool create(const std::string& imgPath, int version, ImgArchive& out, std::string& error);
    static bool open(const std::string& imgPath, ImgArchive& out, std::string& error);

    int version() const { return version_; }
    const std::string& path() const { return imgPath_; }
    const std::vector<ImgEntry>& entries() const { return entries_; }

    // Case-insensitive lookup. Returns -1 when not found.
    int find(const std::string& name) const;

    bool extract(const std::string& name, std::vector<unsigned char>& data, std::string& error) const;
    bool extractToFile(const std::string& name, const std::string& destPath, std::string& error) const;

    // Append a file (replacing an existing entry of the same name). Call save() afterwards.
    bool add(const std::string& name, const std::vector<unsigned char>& data, std::string& error);
    // Remove an entry from the directory. Space is reclaimed by save().
    bool remove(const std::string& name, std::string& error);

    // Write the directory. Rewrites the whole archive if entries were removed or the
    // version 2 directory no longer fits in front of the data.
    bool save(std::string& error);

private:
    bool rebuild(std::string& error);
    bool writeDirectory(std::string& error) const;
    std::string dirPath() const;

    int version_ = 1;
    std::string imgPath_;
    std::vector<ImgEntry> entries_;
    bool needsRebuild_ = false;
};

}  // namespace med
