#include "roadkit.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include "rotation.h"

namespace med {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

std::vector<RoadEnd> findRoadEnds(const std::vector<float>& positions) {
    std::vector<RoadEnd> ends;
    if (positions.size() < 3) return ends;
    double lo[3] = {1e30, 1e30, 1e30}, hi[3] = {-1e30, -1e30, -1e30};
    for (size_t i = 0; i + 2 < positions.size(); i += 3)
        for (int k = 0; k < 3; ++k) {
            lo[k] = std::min(lo[k], double(positions[i + k]));
            hi[k] = std::max(hi[k], double(positions[i + k]));
        }
    for (int side = 0; side < 4; ++side) {  // -x, +x, -y, +y
        const int axis = side < 2 ? 0 : 1, other = 1 - axis;
        const bool positive = side % 2 == 1;
        const double bound = positive ? hi[axis] : lo[axis];
        // the side's plane: the most common coordinate near the bound (a stray vertex can stick out a little)
        int n = 0;
        double a0 = 1e30, a1 = -1e30, top = -1e30;
        std::vector<double> onSide;
        for (size_t i = 0; i + 2 < positions.size(); i += 3)
            if (std::fabs(positions[i + axis] - bound) <= kSideTolerance) {
                ++n;
                a0 = std::min(a0, double(positions[i + other]));
                a1 = std::max(a1, double(positions[i + other]));
                top = std::max(top, double(positions[i + 2]));
                onSide.push_back(positions[i + axis]);
            }
        if (n < kMinEndVertices || a1 - a0 < kMinEndWidth) continue;
        // the side's plane: the median of the vertices there, so a few strays (rd_Road1A5 has some 0.09 past its end)
        // do not move it; Rockstar's map joins that piece at 2.50, which the mean (2.52) missed
        std::nth_element(onSide.begin(), onSide.begin() + std::ptrdiff_t(onSide.size() / 2), onSide.end());
        const double plane = onSide[onSide.size() / 2];
        RoadEnd e;
        const double mid = (a0 + a1) / 2;
        // height: the road surface in the middle of the end, not the kerb top at its edges (one-way industrial roads have a
        // raised pavement on one side; a 9.85 climb ends with its kerb at 10.00 but the road at 9.85)
        double best = 1e30;
        for (size_t i = 0; i + 2 < positions.size(); i += 3)
            if (std::fabs(positions[i + axis] - bound) <= kSideTolerance) best = std::min(best, std::fabs(positions[i + other] - mid));
        top = -1e30;
        for (size_t i = 0; i + 2 < positions.size(); i += 3)
            if (std::fabs(positions[i + axis] - bound) <= kSideTolerance && std::fabs(positions[i + other] - mid) <= best + 1.0)
                top = std::max(top, double(positions[i + 2]));
        if (axis == 0) { e.x = std::round(plane * 100) / 100; e.y = mid; e.dx = positive ? 1 : -1; }
        else { e.y = std::round(plane * 100) / 100; e.x = mid; e.dy = positive ? 1 : -1; }
        e.z = std::round(top * 100) / 100;  // the kit is built to hundredths; float noise would add up along a road
        e.width = a1 - a0;
        if (axis == 0) e.y = std::round(e.y * 100) / 100;
        else e.x = std::round(e.x * 100) / 100;
        ends.push_back(e);
    }
    // ends at other angles (a 45-degree bend, an S-bend, a curve): the same test on planes facing every half degree,
    // away from the four square directions handled above. Adjacent directions that pass are one end; its direction is
    // the one with the most vertices on its plane (the middle of a tie).
    struct Candidate {
        double angle;
        int n;
    };
    std::vector<Candidate> cands;
    for (int step = 0; step < 720; ++step) {
        const double a = step * 0.5;
        const double fromAxis = std::fmod(a, 90.0);
        if (fromAxis < 3.0 || fromAxis > 87.0) continue;
        const double dx = std::cos(a * kPi / 180), dy = std::sin(a * kPi / 180);
        double top = -1e30;
        for (size_t i = 0; i + 2 < positions.size(); i += 3) top = std::max(top, positions[i] * dx + positions[i + 1] * dy);
        int n = 0;
        double s0 = 1e30, s1 = -1e30;
        for (size_t i = 0; i + 2 < positions.size(); i += 3)
            if (positions[i] * dx + positions[i + 1] * dy >= top - kSideTolerance) {
                ++n;
                const double side = -positions[i] * dy + positions[i + 1] * dx;
                s0 = std::min(s0, side);
                s1 = std::max(s1, side);
            }
        if (n >= kMinEndVertices && s1 - s0 >= kMinEndWidth) cands.push_back({a, n});
    }
    for (size_t i = 0; i < cands.size();) {
        size_t j = i;
        while (j + 1 < cands.size() && cands[j + 1].angle - cands[j].angle <= 0.51) ++j;
        int best = 0;
        for (size_t k = i; k <= j; ++k) best = std::max(best, cands[k].n);
        std::vector<double> at;
        for (size_t k = i; k <= j; ++k)
            if (cands[k].n == best) at.push_back(cands[k].angle);
        const double a = at[at.size() / 2];
        i = j + 1;
        const double dx = std::cos(a * kPi / 180), dy = std::sin(a * kPi / 180);
        double top = -1e30;
        for (size_t v = 0; v + 2 < positions.size(); v += 3) top = std::max(top, positions[v] * dx + positions[v + 1] * dy);
        double s0 = 1e30, s1 = -1e30, plane = 0;
        int n = 0;
        for (size_t v = 0; v + 2 < positions.size(); v += 3) {
            const double along = positions[v] * dx + positions[v + 1] * dy;
            if (along < top - kSideTolerance) continue;
            const double side = -positions[v] * dy + positions[v + 1] * dx;
            s0 = std::min(s0, side);
            s1 = std::max(s1, side);
            plane += along;
            ++n;
        }
        plane /= n;
        const double mid = (s0 + s1) / 2;
        double near = 1e30, z = -1e30;
        for (size_t v = 0; v + 2 < positions.size(); v += 3)
            if (positions[v] * dx + positions[v + 1] * dy >= top - kSideTolerance) near = std::min(near, std::fabs(-positions[v] * dy + positions[v + 1] * dx - mid));
        for (size_t v = 0; v + 2 < positions.size(); v += 3)
            if (positions[v] * dx + positions[v + 1] * dy >= top - kSideTolerance && std::fabs(-positions[v] * dy + positions[v + 1] * dx - mid) <= near + 1.0)
                z = std::max(z, double(positions[v + 2]));
        if (s1 - s0 > kMaxAngledEndWidth) continue;  // the long slanted side of a lane-shift piece, not a road end
        RoadEnd e;
        e.dx = dx;
        e.dy = dy;
        e.x = std::round((plane * dx - mid * dy) * 100) / 100;
        e.y = std::round((plane * dy + mid * dx) * 100) / 100;
        e.z = std::round(z * 100) / 100;
        e.width = s1 - s0;
        ends.push_back(e);
    }
    return ends;
}

std::vector<RoadEnd> roadEndsFor(const std::string& model, const std::vector<float>& positions) {
    std::string m = model;
    for (char& c : m) c = char(std::tolower(static_cast<unsigned char>(c)));
    if (m == "rd_road1a10" || m == "rd_road1waya10") {
        RoadEnd a, b;
        a.y = -5; a.dy = -1; a.width = 20;
        b.y = 5; b.dy = 1; b.width = 20;
        return {a, b};
    }
    // pieces whose ends Rockstar's map contradicts or can never confirm (one-offs made for one spot): not offered, no nodes
    for (const char* skip : {"rd_crossrda1rw22", "rd_crossrda1w22", "rd_roadind1way40", "rd_opera_custma", "rd_opera_custmb", "rd_specialrd11",
                             "rd_specialrdmall", "rd_warehsearea", "rd_crossroadsa24"})
        if (m == skip) return {};
    std::vector<RoadEnd> ends = findRoadEnds(positions);
    if (m.rfind("rd_roadind1way", 0) == 0)
        ends.erase(std::remove_if(ends.begin(), ends.end(), [](const RoadEnd& e) { return e.dx != 0; }), ends.end());
    return ends;
}

namespace {
// v rotated by the drawn quaternion d = conjugate of the stored one: v' = d v d*
void rotateDrawn(const IplInstance& p, double& vx, double& vy, double& vz) {
    double qx = -p.rotX, qy = -p.rotY, qz = -p.rotZ, qw = p.rotW;
    const double n = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
    if (n < 1e-12) return;
    qx /= n; qy /= n; qz /= n; qw /= n;
    // t = 2 q x v ; v' = v + w t + q x t
    const double tx = 2 * (qy * vz - qz * vy), ty = 2 * (qz * vx - qx * vz), tz = 2 * (qx * vy - qy * vx);
    const double rx = vx + qw * tx + (qy * tz - qz * ty);
    const double ry = vy + qw * ty + (qz * tx - qx * tz);
    const double rz = vz + qw * tz + (qx * ty - qy * tx);
    vx = rx; vy = ry; vz = rz;
}
}  // namespace

void placementDirToWorld(const IplInstance& p, double lx, double ly, double lz, double& wx, double& wy, double& wz) {
    wx = lx; wy = ly; wz = lz;
    rotateDrawn(p, wx, wy, wz);
}

void placementToWorld(const IplInstance& p, double lx, double ly, double lz, double& wx, double& wy, double& wz) {
    placementDirToWorld(p, lx, ly, lz, wx, wy, wz);
    wx += p.x; wy += p.y; wz += p.z;
}

RoadEnd endInWorld(const IplInstance& placed, const RoadEnd& end) {
    RoadEnd w = end;
    placementToWorld(placed, end.x, end.y, end.z, w.x, w.y, w.z);
    double dz;
    placementDirToWorld(placed, end.dx, end.dy, 0, w.dx, w.dy, dz);
    return w;
}

bool attachPlacement(const RoadEnd& target, const RoadEnd& newEnd, double& x, double& y, double& z, double& qx, double& qy, double& qz, double& qw) {
    const double len = std::hypot(target.dx, target.dy);
    if (len < 0.998) return false;  // tilted: the end points up or down by more than about 3.6 degrees
    // turn the new piece so its end faces the target end: heading(new end after turn) = heading(target) + 180
    const double want = std::atan2(-target.dy, -target.dx), have = std::atan2(newEnd.dy, newEnd.dx);
    double turn = (want - have) * 180.0 / 3.14159265358979323846;
    while (turn <= -180) turn += 360;
    while (turn > 180) turn -= 360;
    turn = std::round(turn * 1e6) / 1e6;  // whole quarter turns stay exact
    eulerDegToQuat(0, 0, turn, true, qx, qy, qz, qw);
    IplInstance probe;
    probe.rotX = qx; probe.rotY = qy; probe.rotZ = qz; probe.rotW = qw;
    double ox, oy, oz;
    placementDirToWorld(probe, newEnd.x, newEnd.y, newEnd.z, ox, oy, oz);
    x = target.x - ox;
    y = target.y - oy;
    z = target.z - oz;
    return true;
}

bool endsMeet(const RoadEnd& a, const RoadEnd& b, double tolerance) {
    const double d = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
    return d <= tolerance && a.dx * b.dx + a.dy * b.dy < -0.99;
}

}  // namespace med
