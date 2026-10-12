// Importing a model into the game: the pieces that do not need the editor's window (rules in CLAUDE.md, "Import rules").
// The collision a model starts with, model ids, the gta3.dat and IDE lines, and the region's own COL file.
#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "col.h"
#include "dff.h"
#include "img.h"
#include "txd.h"

namespace med {

constexpr int kGta3ModelLimit = 5500;  // the game's model table (0x157c in CGame_Initialise, the IDE loader and CPools)
constexpr int kGta3TxdLimit = 850;     // the game's texture dictionary store (CTxdStore_Initialise_maybe 0x00527440)
constexpr int kGta3FirstFreeId = 3153; // GTA III 1.0 uses ids 0 to 3152 (every IDE section); new models start here
constexpr size_t kMaxModelNameLength = 19;  // "<name>.dff" must fit the archive's 23-character entry names

// The model's triangles in its own coordinates (each atomic through its frame chain), with equal positions welded into
// one vertex. Degenerate triangles (two corners the same) are left out.
void dffMesh(const DffModel& model, std::vector<float>& vertices, std::vector<uint32_t>& triangles);
// The box round the model's mesh. False when it has no vertices.
bool dffBounds(const DffModel& model, float mn[3], float mx[3]);

enum class CollisionStart {
    Empty,  // a clean slate: no shapes (nothing solid), only the bounds of the model
    Box,    // one box round the model
    Mesh,   // the model's own triangles
};
// The collision an imported model starts with when no .col comes with it. `material` is the surface of every shape.
ColModel makeCollision(const DffModel& model, const std::string& name, uint16_t modelId, CollisionStart how, uint8_t material = 0);

// The model ids an IDE file uses: the first number of every line in its objs, tobj, cars, peds, weap and hier sections.
void collectIdeIds(const std::string& ideText, std::set<int>& ids);
// The lowest id from `from` up that is not used and is below `limit`; else the lowest free id at all; -1 when full.
int proposeModelId(const std::set<int>& used, int from = kGta3FirstFreeId, int limit = kGta3ModelLimit);

// A model name the game and the archive accept: 1 to 19 characters, letters, digits, '_' and '-'. `why` says what is wrong.
bool validModelName(const std::string& name, std::string& why);

// "id, model, txd, 1, drawDistance, flags", the GTA III objs line.
std::string ideObjsLine(int id, const std::string& model, const std::string& txd, double drawDistance, int flags);
// Add a line inside the objs block of an IDE file (an objs ... end block is added when there is none). The file's line
// endings are kept; a new (empty) file gets Windows line endings, as the game's own files have.
void addIdeObjsLine(std::string& ideText, const std::string& line);

// gta3.dat: add `line` after the last line that starts with `keyword` (for example "IDE" or "COLFILE"); when there is
// none, before the first IPL line, else at the end. The file's line endings are kept. False (and no change) when the
// same line (ignoring case and slashes) is already there.
bool addDatLine(std::string& datText, const std::string& keyword, const std::string& line);

// A COL file's bytes with the record of model `m` (same name, case ignored) replaced by writeColl(m), or appended.
// False with `error` when the existing file cannot be read as COLL records (it is then left alone).
bool putColRecord(std::vector<unsigned char>& colFile, const ColModel& m, std::string& error);

// What the game already has: every model id and model name of every IDE section of the IDE files that data/default.dat
// and data/gta3.dat list (names lower case).
void gameIdsAndNames(const std::string& gameRoot, std::set<int>& ids, std::set<std::string>& names);

// ---- importing one model: model, textures and collision together (CLAUDE.md "Import rules") ----
struct ImportRequest {
    std::string gameRoot;
    std::string dffPath, txdPath, colPath;  // colPath may be empty: then `start` makes the collision
    std::string name;                       // the model's name in the game (its .dff entry without ".dff")
    int id = -1;
    double drawDistance = 200;
    int flags = 0;
    CollisionStart start = CollisionStart::Empty;
    std::string region;                     // the destination region: data/maps/<region>/<region>.ide and .col
};

struct ImportPlan {
    DffModel model;
    TxdFile textures;
    std::string txdName;                 // the dictionary's name in the game (its file name without .txd)
    bool txdReused = false;              // the same dictionary (byte for byte) is already in gta3.img
    ColModel collision;                  // as it will be written (named after the model, with its id)
    bool collisionFromFile = false;      // from the .col that came with it (else made by `start`)
    std::vector<std::string> problems;   // reasons the import cannot go ahead (empty = it can)
    std::vector<std::string> notes;      // things worth knowing (missing textures, a renamed collision record, ...)
    std::vector<unsigned char> dffBytes, txdBytes;
};

// Read the files and check everything against the game (the rules): nothing is written.
ImportPlan planImport(const ImportRequest& req, const ImgArchive& img, const std::set<int>& usedIds, const std::set<std::string>& usedNames);

// Do it: back up, append the model (and the dictionary unless reused) to gta3.img, add the IDE line and the collision to
// the region's own files, list them in gta3.dat. `written` lists what changed. False with `error` when something could
// not be written (what was written before stays, the backups and models/med-imports.txt say how to undo it).
bool applyImport(const ImportRequest& req, const ImportPlan& plan, ImgArchive& img, std::vector<std::string>& written, std::string& error);

}  // namespace med
