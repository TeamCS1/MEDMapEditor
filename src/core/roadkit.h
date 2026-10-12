// Road pieces that join end to end: where a road model's ends are, and where to put a new piece so that one of its
// ends meets an end of a placed piece. Not in MEd (it only moves objects freely); added for map building (2026-10-12).
//
// GTA III's road kit (rd_Road1A50, rd_Corner1, rd_TJunction12, ...) is laid out on a grid: a piece's road ends lie on
// the sides of its bounding box, square to the box, and every end has the road's full cross-section (road, kerbs,
// pavement) there. A side of the box is taken as an END when at least kMinEndVertices mesh vertices lie on it
// (within kSideTolerance) and they span at least kMinEndWidth; a side that only has the kerb line of a closed edge (the
// back of a T-junction, the long sides of a straight) has far fewer vertices. The end's point is the middle of that
// span on the side, at the road surface there (the highest vertex near the middle, not the kerb: a 9.85 climb has its
// kerb at 10.00). Pieces whose ends are not square to the box (rd_turn4, rd_RoadS1, the special junctions) give fewer
// or no ends and cannot be attached to by those ends. Checked against the original map with tools/road_ends: 912 of the
// 1239 ends of placed kit pieces meet another kit piece's end exactly and none comes close without meeting (the rest
// join Rockstar's one-off road meshes or are open).
#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "ipl.h"

namespace med {

constexpr int kMinEndVertices = 12;
constexpr double kMinEndWidth = 9.5;
constexpr double kSideTolerance = 0.2;
constexpr double kMaxAngledEndWidth = 41.0;  // the widest kit road is 40; a wider slanted side is not an end

struct RoadEnd {
    double x = 0, y = 0, z = 0;  // the end's point, in the model's own coordinates
    double dx = 0, dy = 0;       // outward direction, a unit vector along x or y
    double width = 0;            // how wide the road is there
};

// The ends of a model, from its vertex positions (3 floats per vertex, all geometries together). Order: -x, +x, -y, +y.
std::vector<RoadEnd> findRoadEnds(const std::vector<float>& positions);

// findRoadEnds plus corrections for the few kit pieces whose mesh misleads it, each confirmed by where Rockstar's own
// map joins other pieces to them (tools/road_ends --probe): rd_Road1A10 and rd_Road1wayA10 are 10-long straights whose
// mesh sticks out past their ends (ends at y = -5 and +5); the industrial one-way roads' raised pavement side is not
// an end. Pieces the map contradicts (rd_CrossRda1rw22 / 1w22: a neighbour meets their side at y -15, not where the
// mesh says; rd_RoadInd1way40: its road sits 0.77 below its base at both ends) or can never confirm (one-offs built for
// one place: rd_opera_custma / b, rd_specialrd11, rd_specialrdmall, rd_warehsearea, rd_CrossRoadsa24) get no ends.
// `model` is the model name (any case).
std::vector<RoadEnd> roadEndsFor(const std::string& model, const std::vector<float>& positions);

// A model point / direction turned the way the game draws a placement (the stored quaternion applied inverted).
void placementToWorld(const IplInstance& p, double lx, double ly, double lz, double& wx, double& wy, double& wz);
void placementDirToWorld(const IplInstance& p, double lx, double ly, double lz, double& wx, double& wy, double& wz);

// An end of a placed piece, in the world.
RoadEnd endInWorld(const IplInstance& placed, const RoadEnd& end);

// Where to put a new piece so that its end `newEnd` meets the world end `target` (same point, facing it): returns the
// position and the stored quaternion (a turn about the vertical axis only). False when the target end is not level
// enough to join a level piece (the placed piece is tilted more than about 3 degrees).
bool attachPlacement(const RoadEnd& target, const RoadEnd& newEnd, double& x, double& y, double& z, double& qx, double& qy, double& qz, double& qw);

// True when two world ends meet: points within `tolerance` units and facing each other.
bool endsMeet(const RoadEnd& a, const RoadEnd& b, double tolerance = 0.3);

}  // namespace med
