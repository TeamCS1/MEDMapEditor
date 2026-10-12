// Extract Object: copies of an object's files, written to a folder. After MEd's CMapView::extractObject (0x0045c7f0),
// which writes <model>.dff and <txd>.txd out of the IMG archive; this one also copies the collision and writes an info
// file. Everything is a COPY: the archive and the COL files are only read, never changed.
//
//   <folder>/<model>/<model>.dff   the model, byte for byte as stored in models/gta3.img (the archive keeps whole 2048-byte
//                                  sectors, so the copy can end in zero bytes, exactly as MEd's extract wrote it; it is
//                                  not trimmed, because some shipped models (sam.dff) declare a smaller size than they use)
//   <folder>/<model>/<txd>.txd     its texture dictionary, byte for byte (from gta3.img, or models/generic.txd for "generic")
//   <folder>/<model>/<model>.col   its collision: the one COLL record copied byte for byte out of the COL file that holds
//                                  it, which on its own is a valid one-model COL file
//   <folder>/<model>/<model>.txt   what the caller passes as info (IDE line, where the files came from)
#pragma once
#include <string>
#include <vector>
#include "img.h"

namespace med {

struct ExtractRequest {
    std::string model;                  // DFF name without extension
    std::string txd;                    // texture dictionary name without extension
    std::vector<std::string> colFiles;  // the game's COL files, full paths, in gta3.dat order (searched for the model)
    std::string genericTxdPath;         // models/generic.txd, for objects that use the "generic" dictionary
    std::string info;                   // text for <model>.txt (empty: no info file)
};

struct ExtractResult {
    std::string folder;                 // <folder>/<model>
    std::vector<std::string> written;   // files written, full paths
    std::vector<std::string> warnings;  // what could not be copied, in plain words
    std::string colSource;              // the COL file the collision came from ("" when none)
};

// The files extractObject would write that already exist (to ask before overwriting).
std::vector<std::string> extractTargetsThatExist(const std::string& folder, const ExtractRequest& req);

// Copy the object's files into folder/<model>/. Existing files are overwritten (ask first with extractTargetsThatExist).
ExtractResult extractObject(const ImgArchive& img, const std::string& folder, const ExtractRequest& req);

// The raw COLL record of `model` in the COL files (the LAST one in gta3.dat order: the game loads each record over an earlier one of the same name), or false. `source` is the file.
bool findCollisionRecord(const std::vector<std::string>& colFiles, const std::string& model, std::vector<unsigned char>& record, std::string& source,
                         int* matches = nullptr);

}  // namespace med
