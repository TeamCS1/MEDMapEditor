#include <cmath>
#include <cstdio>
#include "../src/viewer/raycast.h"

using namespace med;

static int failures = 0;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) {                                                    \
            std::printf("FAIL line %d: %s\n", __LINE__, #c);           \
            ++failures;                                                \
        }                                                              \
    } while (0)
static bool near(double a, double b, double e = 1e-5) { return std::fabs(a - b) < e; }

int main() {
    const float a[3] = {0, 0, 0}, b[3] = {10, 0, 0}, c[3] = {0, 10, 0};  // a triangle in the z = 0 plane
    double t = 0;
    // straight down onto the triangle from z = 5
    double o1[3] = {2, 2, 5}, d1[3] = {0, 0, -1};
    CHECK(rayTriangle(o1, d1, a, b, c, t) && near(t, 5));
    // from below, up: both sides count
    double o2[3] = {2, 2, -3}, d2[3] = {0, 0, 1};
    CHECK(rayTriangle(o2, d2, a, b, c, t) && near(t, 3));
    // a direction that is not unit length gives a proportionally smaller t
    double d3[3] = {0, 0, -2};
    CHECK(rayTriangle(o1, d3, a, b, c, t) && near(t, 2.5));
    // misses: outside the triangle, parallel, pointing away
    double o4[3] = {8, 8, 5};
    CHECK(!rayTriangle(o4, d1, a, b, c, t));
    double d5[3] = {1, 0, 0};
    CHECK(!rayTriangle(o1, d5, a, b, c, t));
    double d6[3] = {0, 0, 1};
    CHECK(!rayTriangle(o1, d6, a, b, c, t));  // the triangle is behind the ray
    // an edge hit counts
    double o7[3] = {5, 0, 4};
    CHECK(rayTriangle(o7, d1, a, b, c, t));

    // affine inverse: translate + rotate 90 degrees about z + scale 2
    const float m[16] = {0, 2, 0, 0, -2, 0, 0, 0, 0, 0, 2, 0, 5, 6, 7, 1};  // column-major
    float inv[16];
    CHECK(invertAffine(m, inv));
    double p[3] = {1, 2, 3}, w[3], back[3];
    transformPoint(m, p, w);
    transformPoint(inv, w, back);
    CHECK(near(back[0], 1, 1e-4) && near(back[1], 2, 1e-4) && near(back[2], 3, 1e-4));
    // the same through directions (no translation)
    double dir[3] = {1, 0, 0}, wd[3], bd[3];
    transformDirection(m, dir, wd);
    transformDirection(inv, wd, bd);
    CHECK(near(bd[0], 1, 1e-4) && near(bd[1], 0, 1e-4) && near(bd[2], 0, 1e-4));
    // a singular matrix cannot be inverted
    const float flat[16] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    CHECK(!invertAffine(flat, inv));

    if (failures == 0) std::printf("all raycast tests passed\n");
    return failures ? 1 : 0;
}
