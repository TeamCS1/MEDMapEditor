// Making a new region: an empty IPL file that the game loads. A region is nothing special to the game: it is an IPL file
// listed on an "IPL <path>" line of data/gta3.dat (see GUIDE.md). Adding one takes two things, both done here:
//   - the file data/maps/<name>/<name>.ipl with an empty "inst" section,
//   - the line "IPL DATA\MAPS\<name>\<name>.IPL" in data/gta3.dat, after the last IPL line.
// gta3.dat is first copied to gta3.dat.med-backup (once). The game's loading bar has a fixed number of steps (73), so one
// more IPL line makes the bar run one step past the end at the last step; that is only cosmetic.
#pragma once
#include <string>
#include <vector>

namespace med {

// Letters, digits, '_' and '-', starting with a letter, at most 24 characters.
bool validRegionName(const std::string& name);

// Creates the region in a game folder. On success `rel` is the path as the editor keeps it ("DATA/MAPS/<name>/<name>.IPL").
bool addIplRegion(const std::string& gameRoot, const std::string& name, std::string& rel, std::string& error);

// Delete a region again (MEd's IPL tab "Delete File", CIPLTree::delFile 0x004287c0: "Are you sure you want to delete
// ...?", then the file's line goes out of gta3.dat). Only a region added since gta3.dat.med-backup (regionsAddedSinceBackup)
// can go: Rockstar's own files are refused. Its gta3.dat line is removed (line endings kept) and the .ipl is renamed to
// <file>.med-deleted (MEd leaves the file where it is; renaming lets the same name be created again and loses nothing).
// `rel` as the editor keeps it ("DATA/MAPS/<name>/<name>.IPL").
bool removeIplRegion(const std::string& gameRoot, const std::string& rel, std::string& error);

// IPL files listed in data/gta3.dat that are not listed in data/gta3.dat.med-backup, that is the regions added with
// addIplRegion ("DATA/MAPS/<name>/<name>.IPL"). Empty when there is no backup.
std::vector<std::string> regionsAddedSinceBackup(const std::string& gameRoot);

}  // namespace med
