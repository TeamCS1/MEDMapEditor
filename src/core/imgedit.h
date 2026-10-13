// MEd's Image Archive tool (CIMGTool, 0x0043c3b0): delete, rename, add and extract entries of an IMG archive, then
// rebuild it. In MEd, Delete and Rename change only the directory in memory (CIMG 0x0041aeb0 sets the "changed" flag),
// Add appends the file's bytes to the .img at once (0x0041c790) and the directory is written by Rebuild Archive
// (0x0041b720, through "tempdir" / "tempimg"); the tool asks to rebuild on the way out ("This archive has been
// modified. Changes will not be saved until you rebuild the archive."). Here every change, Add included, is only
// staged until rebuildImg(), which never writes into the original files: it writes new ones next to them, checks every
// entry reads back exactly, and only then swaps them in by renaming (the old archive is kept as <file>.med-backup the
// first time). No window code: everything here is tested on its own.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "img.h"

namespace med {

// MEd's file type column (CIMGTool::displayFiles): by extension.
const char* imgFileType(const std::string& name);  // "3D Model", "Texture Dictionary", "Collision File", "Binary IPL File", "Unknown"

// An entry name the archive and the game can use: 1 to 23 characters (24-byte field with its terminator), printable
// ASCII without spaces, slashes or backslashes.
bool validImgEntryName(const std::string& name, std::string& why);

struct ImgEditEntry {
    std::string name;        // as it will be written
    std::string sourceName;  // its name in the archive now; "" when it comes from a file
    uint32_t offset = 0;     // in the archive (sectors), for an entry from the archive
    uint32_t size = 0;       // sectors (for a file: the sectors it will take)
    std::string file;        // an added or replacing file on disk
    uint64_t fileBytes = 0;  // that file's size when it was added (checked again before rebuilding)
    bool replaced = false;   // a file that took the place of an archive entry of the same name
};

class ImgEdit {
public:
    ImgEdit() = default;
    explicit ImgEdit(const ImgArchive& archive);

    const std::string& imgPath() const { return imgPath_; }
    int version() const { return version_; }
    const std::vector<ImgEditEntry>& entries() const { return entries_; }
    bool changed() const { return changed_; }
    int find(const std::string& name) const;  // case-insensitive, -1 when missing

    bool remove(const std::string& name, std::string& why);
    bool rename(const std::string& from, const std::string& to, std::string& why);
    // Add a file under `name` (empty: the file's own name). An entry of that name is replaced when `replace`, else it
    // is refused (MEd's Add would make a second entry of the same name).
    bool addFile(const std::string& path, const std::string& name, bool replace, std::string& why);

    // What happened to an entry, for the list: "", "added", "replaced" or "renamed from <old>".
    static std::string status(const ImgEditEntry& e);

private:
    std::string imgPath_;
    int version_ = 1;
    std::vector<ImgEditEntry> entries_;
    bool changed_ = false;
};

struct ImgRebuildReport {
    bool backupMade = false;        // the archive as it was is now <img>.med-backup (and <dir>.med-backup)
    std::string backupImg, backupDir;
    uint64_t bytesBefore = 0, bytesAfter = 0;
    size_t entries = 0;
};

// Write the staged archive and swap it in. On success `reopened` is the new archive as read back from disk. On failure
// (`why`) the original files are as they were and no temporary file is left. `progress(done, total)` is called per entry.
bool rebuildImg(const ImgEdit& edit, ImgArchive& reopened, ImgRebuildReport& report, std::string& why,
                const std::function<void(size_t, size_t)>& progress = nullptr);

}  // namespace med
