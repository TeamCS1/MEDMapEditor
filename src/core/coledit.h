// Editing a collision model (the collision editor's Edit tab): shapes, push/pull, mesh faces, open edges, picking, and
// where an edited record is saved so the game uses it. No window code: everything here is tested on its own.
#pragma once
#include <array>
#include <set>
#include <string>
#include <vector>
#include "col.h"

namespace med {

constexpr float kMinBoxSize = 0.05f;    // a box side is never pushed thinner than this
constexpr float kMinSphereRadius = 0.05f;

// ---- boxes and spheres ----
// Faces of a box: 0 = -x, 1 = +x, 2 = -y, 3 = +y, 4 = -z, 5 = +z (boxes are aligned with the model's axes).
// Push/pull: the face moves outward by `d` (negative = inward). False (box unchanged) when it would get thinner than
// kMinBoxSize or `face` is out of range.
bool pushPullBoxFace(ColBox& box, int face, float d);
// Outward normal of box face `face`.
void boxFaceNormal(int face, float n[3]);
// A box or sphere around the model's current bounds, or a 1-unit one at the origin when it has none.
ColBox newBoxFor(const ColModel& m);
ColSphere newSphereFor(const ColModel& m);

// ---- the mesh ----
// Unit normal of a face (from its winding a -> b -> c); false for a degenerate face or a bad index.
bool faceNormal(const ColModel& m, size_t face, float n[3]);
// Edges used by exactly one face: holes and borders, drawn in red. Each as (lower vertex, higher vertex).
std::vector<std::array<uint32_t, 2>> openEdges(const ColModel& m);
// Remove these faces, then every vertex no face uses any more (indices are renumbered). Returns how many faces went.
size_t deleteFaces(ColModel& m, const std::set<size_t>& faces);
// Drop vertices no face uses and renumber the faces.
void removeUnusedVertices(ColModel& m);

// SketchUp-style push/pull of the selected faces by `d` along their average normal (negative = into the model):
// - a vertex used only by selected faces moves;
// - a vertex also used by unselected faces that all lie along the push direction (the sides of a box being pulled)
//   moves too, so those sides stretch;
// - any other shared vertex stays for the unselected faces and the selection gets a moved copy; every edge of the
//   selection's border where that happens gets a side wall (two triangles, the material of the selected face beside
//   it; triangles that would have no area are left out).
// `faces` keeps naming the moved faces afterwards. False with `why` (model unchanged) when nothing is selected, an
// index is bad, the faces point in opposite directions (no average normal) or d is 0.
bool pushPullFaces(ColModel& m, const std::set<size_t>& faces, float d, std::string& why);

// Faces that touch the given faces through shared edges and lie in the same plane (normals within ~1 degree, same
// plane offset): "select the whole flat side" from one clicked triangle.
std::set<size_t> coplanarNeighbours(const ColModel& m, size_t face);

// ---- picking (rays in the model's own coordinates) ----
struct ColHit {
    enum Kind { None, Sphere, Box, Face } kind = None;
    int index = -1;    // the sphere, box or face
    int boxFace = -1;  // for a box: which side was hit (0..5 as above)
    float t = 0;       // distance along the ray (ray direction need not be unit length: t is in its units)
};
// The nearest shape the ray hits (only the kinds that are shown).
ColHit pickCol(const ColModel& m, const float origin[3], const float dir[3], bool boxes = true, bool spheres = true, bool mesh = true);

// ---- surface materials (the number every box, sphere and face carries) ----
// GTA III knows materials 0 to 32 (33 and 34 exist in the numbering but GTA III does not use them). The names are the
// community's (re3, used only as a reference); the numbering is confirmed by the game itself: the grip group of each
// number below is the game's own CSurfaceTable function at 0x004aba60, and it agrees with every name (grass, sand and
// cardboard are loose, water alone is wet, pedestrians and rubber are rubber, ...). The groups are the rows of
// data/surface.dat.
constexpr int kColMaterialCount = 35;
enum class GripGroup { Rubber = 0, Hard = 1, Road = 2, Loose = 3, Wet = 4 };
// "Grass", "Water", ...; "" outside 0..34.
const char* colMaterialName(int material);
// The game's grip group of a material (0x004aba60; numbers it does not list count as Road, as in the game).
GripGroup colMaterialGrip(int material);
const char* gripGroupName(GripGroup g);  // "rubber", "hard", "road", "loose", "wet" (data/surface.dat's rows)

// ---- saving: which COL file gets an edited record ----
// The game loads every COLFILE line of gta3.dat in order and loads each record over an earlier one of the same model
// name (CFileLoader_LoadCollisionFile 0x00478b20); when the player changes island it reloads the COLFILE lines of that
// island's level only (0x00476520). So an edited copy of a Rockstar model must sit in a file of the SAME level, listed
// AFTER Rockstar's: the user's own region file data/maps/<region>/<region>.col for level 0 (the one imports use) and
// data/maps/<region>/<region>_lvl<N>.col for levels 1 to 3.
struct ColSaveTarget {
    std::string relPath;   // relative to the game folder, forward slashes, e.g. "data/maps/upstate/upstate_lvl1.col"
    std::string datLine;   // the gta3.dat line that lists it, e.g. "COLFILE 1 DATA\MAPS\upstate\upstate_lvl1.COL"
    int level = 0;
};
ColSaveTarget colSaveTarget(const std::string& region, int level);
// The level of the COLFILE line of gta3.dat that lists `relPath` (slashes and case ignored), or -1 when none does.
int colFileLevel(const std::string& gta3dat, const std::string& relPath);

}  // namespace med
