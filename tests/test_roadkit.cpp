#include <cmath>
#include <cstdio>
#include "../src/core/roadkit.h"
#include "../src/core/rotation.h"

using namespace med;

static int failures = 0;
#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            std::printf("FAIL line %d: %s\n", __LINE__, #c);      \
            ++failures;                                           \
        }                                                         \
    } while (0)

static bool near(double a, double b, double eps = 1e-4) { return std::fabs(a - b) <= eps; }

// A road cross-section across x (pavement 0.15, road 0) at y, for a straight along y. Each end gets 16 vertices.
static void section(std::vector<float>& v, float y, float base, float halfWidth) {
    for (int i = 0; i < 16; ++i) {
        const float x = -halfWidth + 2 * halfWidth * float(i) / 15.0f;
        const bool pavement = std::fabs(x) > halfWidth - 3;
        v.insert(v.end(), {x, y, base + (pavement ? 0.15f : 0.0f)});
    }
}

// a straight of `length` along y, rising `rise` from -y to +y (like rd_Road2C50: rise 9.85, half width 15)
static std::vector<float> straight(float length, float rise, float halfWidth) {
    std::vector<float> v;
    section(v, -length / 2, 0, halfWidth);
    section(v, length / 2, rise, halfWidth);
    // the long sides only carry a few kerb vertices
    for (float y : {-length / 4, 0.0f, length / 4}) {
        v.insert(v.end(), {-halfWidth, y, rise / 2 + 0.15f});
        v.insert(v.end(), {halfWidth, y, rise / 2 + 0.15f});
    }
    return v;
}

static IplInstance placed(double x, double y, double z, double headingDeg) {
    IplInstance p;
    p.x = x; p.y = y; p.z = z;
    eulerDegToQuat(0, 0, headingDeg, true, p.rotX, p.rotY, p.rotZ, p.rotW);
    return p;
}

int main() {
    // ends of a straight slope: two, at -y (z 0) and +y (z = rise, the road surface, not the kerb)
    const std::vector<float> slope = straight(50, 9.85f, 15);
    std::vector<RoadEnd> e = findRoadEnds(slope);
    CHECK(e.size() == 2);
    if (e.size() == 2) {
        CHECK(near(e[0].x, 0) && near(e[0].y, -25) && near(e[0].z, 0) && e[0].dy == -1 && near(e[0].width, 30));
        CHECK(near(e[1].x, 0) && near(e[1].y, 25) && near(e[1].z, 9.85, 1e-3) && e[1].dy == 1);
    }

    // a corner: road from its -y side round to its +x side (like rd_Corner1: box x -20..0, y 0..20)
    std::vector<float> corner;
    for (int i = 0; i < 16; ++i) {
        const float t = 20.0f * float(i) / 15.0f;
        corner.insert(corner.end(), {-20 + t, 0, 0});  // -y side, x from -20 to 0
        corner.insert(corner.end(), {0, t, 0});        // +x side, y from 0 to 20
    }
    corner.insert(corner.end(), {-20, 20, 0.15f});  // the outer corner
    e = findRoadEnds(corner);
    CHECK(e.size() == 2);
    if (e.size() == 2) {
        CHECK(near(e[0].x, 0) && near(e[0].y, 10) && e[0].dx == 1);    // +x side
        CHECK(near(e[1].x, -10) && near(e[1].y, 0) && e[1].dy == -1);  // -y side
    }

    // a few stray vertices past an end do not move it (the plane is the median; rd_Road1A5 has strays 0.09 past -2.5)
    {
        std::vector<float> v = straight(5, 0, 10);
        for (int i = 0; i < 4; ++i) v.insert(v.end(), {float(i), -2.59f, 0.0f});
        const std::vector<RoadEnd> s5 = findRoadEnds(v);
        CHECK(s5.size() == 2 && near(s5[0].y, -2.5) && near(s5[1].y, 2.5));
    }

    // an angled end (a 22.5-degree bend like rd_turn4): a straight start at -y, the other end turned 22.5 degrees left
    {
        std::vector<float> v;
        section(v, -25, 0, 15);
        const double a = 22.5 * 3.14159265358979323846 / 180;  // the far end faces (-sin a, cos a)
        const double cx = -25 * std::sin(a), cy = 25 * std::cos(a);
        for (int i = 0; i < 16; ++i) {
            const double s = -15 + 30.0 * i / 15.0;  // along the end edge, perpendicular to its facing
            v.insert(v.end(), {float(cx + s * std::cos(a)), float(cy + s * std::sin(a)), 0.0f});
        }
        const std::vector<RoadEnd> b = findRoadEnds(v);
        CHECK(b.size() == 2);
        if (b.size() == 2) {
            CHECK(near(b[0].y, -25) && b[0].dy == -1);
            CHECK(near(b[1].x, cx, 0.02) && near(b[1].y, cy, 0.02) && near(b[1].dx, -std::sin(a), 1e-6) && near(b[1].dy, std::cos(a), 1e-6) && near(b[1].width, 30, 0.01));
            // attach a copy of the bend to its own angled end: it continues the curve, ends meet exactly
            IplInstance p;
            const RoadEnd target = endInWorld(p, b[1]);
            double x, y, z, qx, qy, qz, qw;
            CHECK(attachPlacement(target, b[0], x, y, z, qx, qy, qz, qw));
            IplInstance n;
            n.x = x; n.y = y; n.z = z; n.rotX = qx; n.rotY = qy; n.rotZ = qz; n.rotW = qw;
            CHECK(endsMeet(target, endInWorld(n, b[0]), 1e-6));
            const RoadEnd far = endInWorld(n, b[1]);  // two bends make 45 degrees
            CHECK(near(far.dx, -std::sin(2 * a), 1e-6) && near(far.dy, std::cos(2 * a), 1e-6));
        }
    }

    // a slanted side wider than any kit road (the long side of a lane-shift piece) is not an end
    {
        std::vector<float> v;
        for (int i = 0; i < 20; ++i) {
            const double s = -30 + 60.0 * i / 19.0;
            v.insert(v.end(), {float(20 + s * std::cos(0.5)), float(s * std::sin(0.5)), 0.0f});
        }
        bool wide = false;
        for (const RoadEnd& r : findRoadEnds(v)) wide = wide || (r.width > kMaxAngledEndWidth && r.dx != 0 && r.dy != 0);
        CHECK(!wide);
    }

    // pieces the map contradicts or cannot confirm have no ends
    CHECK(roadEndsFor("rd_CrossRda1w22", slope).empty() && roadEndsFor("RD_OPERA_CUSTMA", slope).empty() && roadEndsFor("rd_RoadInd1way40", slope).empty());

    // too few vertices or too narrow: no end; empty input: nothing
    std::vector<float> thin = {0, 0, 0, 1, 1, 0};
    CHECK(findRoadEnds(thin).empty() && findRoadEnds({}).empty());

    // corrections: the 10-long straight with stray geometry, and the one-way road's pavement side
    std::vector<RoadEnd> r = roadEndsFor("RD_ROAD1A10", slope);
    CHECK(r.size() == 2 && near(r[0].y, -5) && near(r[1].y, 5));
    std::vector<float> withSide = slope;
    for (int i = 0; i < 16; ++i) withSide.insert(withSide.end(), {-15, -25 + 50.0f * float(i) / 15.0f, 1.0f});  // a full -x side
    CHECK(findRoadEnds(withSide).size() == 3 && roadEndsFor("rd_RoadInd1way50", withSide).size() == 2);

    // world ends follow the drawn turn (the stored quaternion applied inverted, as the game draws it)
    {
        const IplInstance p = placed(100, 200, 10, 90);  // turned 90 degrees counter-clockwise: local +y points to world -x
        const RoadEnd w = endInWorld(p, e.size() ? RoadEnd{0, 25, 9.85, 0, 1, 30} : RoadEnd{});
        CHECK(near(w.x, 75) && near(w.y, 200) && near(w.z, 19.85) && near(w.dx, -1) && near(w.dy, 0));
        const IplInstance user180 = [] { IplInstance q; q.rotZ = -1; q.rotW = 0; return q; }();  // the user's stored (0, 0, -1, 0)
        const RoadEnd u = endInWorld(user180, RoadEnd{0, 25, 9.85, 0, 1, 30});
        CHECK(near(u.y, -25) && near(u.dy, -1));
    }

    // attaching: the new end meets the target end, for every heading of the placed piece and every end of the new one
    const RoadEnd ends2C50[2] = {{0, -25, 0, 0, -1, 30}, {0, 25, 9.85, 0, 1, 30}};
    for (int heading = -180; heading < 180; heading += 15)
        for (int from = 0; from < 2; ++from)
            for (int use = 0; use < 2; ++use) {
                const IplInstance base = placed(-280.004, 407.448, 77.6713, heading);
                const RoadEnd target = endInWorld(base, ends2C50[from]);
                double x, y, z, qx, qy, qz, qw;
                CHECK(attachPlacement(target, ends2C50[use], x, y, z, qx, qy, qz, qw));
                IplInstance n;
                n.x = x; n.y = y; n.z = z; n.rotX = qx; n.rotY = qy; n.rotZ = qz; n.rotW = qw;
                CHECK(endsMeet(target, endInWorld(n, ends2C50[use]), 1e-6));
                CHECK(near(qx, 0, 1e-12) && near(qy, 0, 1e-12) && near(qx * qx + qy * qy + qz * qz + qw * qw, 1, 1e-12));  // a turn about z only
            }

    // the user's road (2026-10-12): Rockstar's rd_Road2C50 at (-280.004, 407.448, 77.6713) climbs north to 87.5213; three
    // pieces attached downhill by their HIGH end give exactly the corrected heights 77.6713, 67.8213, 57.9713
    {
        IplInstance cur = placed(-280.004, 407.448, 77.6713, 0);
        RoadEnd open = endInWorld(cur, ends2C50[1]);
        CHECK(near(open.y, 432.448) && near(open.z, 87.5213));
        const double wantY[3] = {457.448, 507.448, 557.448}, wantZ[3] = {77.6713, 67.8213, 57.9713};
        for (int k = 0; k < 3; ++k) {
            double x, y, z, qx, qy, qz, qw;
            CHECK(attachPlacement(open, ends2C50[1], x, y, z, qx, qy, qz, qw));
            CHECK(near(x, -280.004) && near(y, wantY[k]) && near(z, wantZ[k]));
            CHECK(near(std::fabs(qz), 1, 1e-9) && near(qw, 0, 1e-9));  // turned 180 degrees, as the user did by hand
            cur.x = x; cur.y = y; cur.z = z; cur.rotX = qx; cur.rotY = qy; cur.rotZ = qz; cur.rotW = qw;
            open = endInWorld(cur, ends2C50[0]);  // continue from the new piece's other (low) end
        }
    }

    // quarter turns stay exact (no 1e-17 noise in the saved file)
    {
        double x, y, z, qx, qy, qz, qw;
        CHECK(attachPlacement(RoadEnd{0, 0, 0, 1, 0, 20}, RoadEnd{0, -25, 0, 0, -1, 20}, x, y, z, qx, qy, qz, qw));
        CHECK(near(x, 25, 1e-9) && near(y, 0, 1e-9) && near(std::fabs(qz), std::sqrt(0.5), 1e-12));
    }

    // a tilted target end (the placed piece leans 10 degrees) is refused
    {
        IplInstance tilt;
        eulerDegToQuat(10, 0, 0, true, tilt.rotX, tilt.rotY, tilt.rotZ, tilt.rotW);
        const RoadEnd t = endInWorld(tilt, RoadEnd{0, 25, 0, 0, 1, 20});
        double x, y, z, qx, qy, qz, qw;
        CHECK(!attachPlacement(t, ends2C50[0], x, y, z, qx, qy, qz, qw));
    }

    // endsMeet: same point facing each other only
    CHECK(endsMeet(RoadEnd{0, 0, 0, 1, 0, 20}, RoadEnd{0.1, 0, 0, -1, 0, 20}));
    CHECK(!endsMeet(RoadEnd{0, 0, 0, 1, 0, 20}, RoadEnd{0, 0, 0, 1, 0, 20}));     // same direction
    CHECK(!endsMeet(RoadEnd{0, 0, 0, 1, 0, 20}, RoadEnd{0, 0, 0.5, -1, 0, 20}));  // a step of 0.5

    if (failures == 0) std::printf("all road kit tests passed\n");
    return failures ? 1 : 0;
}
