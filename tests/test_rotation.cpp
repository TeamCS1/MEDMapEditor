#include <cmath>
#include <cstdio>
#include <initializer_list>
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

// The matrix the viewer draws with (quatToMatrix in viewer/main.cpp, column-major), applied to a point.
static void drawMatrixApply(double x, double y, double z, double w, bool conjugate, const double v[3], double out[3]) {
    const double sg = conjugate ? -1 : 1;
    x *= sg; y *= sg; z *= sg;
    const double n = std::sqrt(x * x + y * y + z * z + w * w);
    x /= n; y /= n; z /= n; w /= n;
    const double m[9] = {1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w),   // first column
                         2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w),   // second
                         2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y)};  // third
    for (int r = 0; r < 3; ++r) out[r] = m[r] * v[0] + m[3 + r] * v[1] + m[6 + r] * v[2];
}

static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

int main() {
    for (bool conj : {true, false}) {
        // Turning the identity by +90 about z maps the drawn +x axis onto +y, and -90 onto -y
        double x = 0, y = 0, z = 0, w = 1;
        rotateQuatAboutZ(x, y, z, w, 90, conj);
        const double ex[3] = {1, 0, 0};
        double o[3];
        drawMatrixApply(x, y, z, w, conj, ex, o);
        CHECK(near(o[0], 0) && near(o[1], 1) && near(o[2], 0));
        x = y = z = 0; w = 1;
        rotateQuatAboutZ(x, y, z, w, -90, conj);
        drawMatrixApply(x, y, z, w, conj, ex, o);
        CHECK(near(o[0], 0) && near(o[1], -1) && near(o[2], 0));

        // A tilted object: the turn is a world-space turn about z, applied after the object's own rotation
        double q[4] = {0.2, -0.3, 0.4, 0};
        q[3] = std::sqrt(1 - 0.04 - 0.09 - 0.16);
        const double v[3] = {0.3, -1.2, 0.8};
        double before[3];
        drawMatrixApply(q[0], q[1], q[2], q[3], conj, v, before);
        for (double deg : {90.0, -90.0, 30.0, 180.0}) {
            double r[4] = {q[0], q[1], q[2], q[3]};
            rotateQuatAboutZ(r[0], r[1], r[2], r[3], deg, conj);
            double after[3];
            drawMatrixApply(r[0], r[1], r[2], r[3], conj, v, after);
            const double a = deg * 3.14159265358979323846 / 180;
            CHECK(near(after[0], std::cos(a) * before[0] - std::sin(a) * before[1]));
            CHECK(near(after[1], std::sin(a) * before[0] + std::cos(a) * before[1]));
            CHECK(near(after[2], before[2]));
            CHECK(near(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3], 1));
        }
        // four quarter turns come back
        double r[4] = {q[0], q[1], q[2], q[3]};
        for (int i = 0; i < 4; ++i) rotateQuatAboutZ(r[0], r[1], r[2], r[3], 90, conj);
        CHECK(near(r[0], q[0]) && near(r[1], q[1]) && near(r[2], q[2]) && near(r[3], q[3]));
    }
    // the shipped rotation of a cone, -0.707107 in z and 0.707107 in w, turned by 90 gives a clean value
    double x = 0, y = 0, z = -0.7071067811865476, w = 0.7071067811865476;
    rotateQuatAboutZ(x, y, z, w, 90, true);
    CHECK(near(x, 0) && near(y, 0) && (near(z, 0) || near(std::fabs(z), 1)) && near(std::fabs(w) + std::fabs(z), 1));

    // angles: a pure heading reads back as itself, for both conventions
    for (bool conj : {true, false})
        for (double h : {0.0, 45.0, 90.0, -90.0, 179.0, 180.0, -135.5}) {
            double x, y, z, w, ax, ay, az;
            eulerDegToQuat(0, 0, h, conj, x, y, z, w);
            quatToEulerDeg(x, y, z, w, conj, ax, ay, az);
            CHECK(near(ax, 0) && near(ay, 0) && std::fabs(az - h) < 1e-9);
            // and it is the same turn as rotateQuatAboutZ from no rotation
            double rx = 0, ry = 0, rz = 0, rw = 1;
            rotateQuatAboutZ(rx, ry, rz, rw, h, conj);
            CHECK(near(rx, x) && near(ry, y) && near(rz, z) && near(rw, w));
        }
    // angles round trip, and they describe the drawn matrix R = Rz * Ry * Rx
    {
        const double cases[][3] = {{10, 20, 30}, {-45, 60, -120}, {170, -80, 5}, {0.2, 0, 0}, {0, 89.5, 0}, {-30, 0, 180}};
        for (bool conj : {true, false})
            for (const auto& c : cases) {
                double x, y, z, w, ax, ay, az;
                eulerDegToQuat(c[0], c[1], c[2], conj, x, y, z, w);
                CHECK(near(x * x + y * y + z * z + w * w, 1));
                quatToEulerDeg(x, y, z, w, conj, ax, ay, az);
                CHECK(std::fabs(ax - c[0]) < 1e-7 && std::fabs(ay - c[1]) < 1e-7 && std::fabs(az - c[2]) < 1e-7);
                // the drawn matrix applied to a point equals Rz(z) Ry(y) Rx(x) applied to it
                const double v[3] = {0.3, -1.1, 0.7};
                double got[3];
                drawMatrixApply(x, y, z, w, conj, v, got);
                const double d = 3.14159265358979323846 / 180;
                const double a = c[0] * d, b = c[1] * d, g = c[2] * d;
                double p1[3] = {v[0], std::cos(a) * v[1] - std::sin(a) * v[2], std::sin(a) * v[1] + std::cos(a) * v[2]};       // Rx
                double p2[3] = {std::cos(b) * p1[0] + std::sin(b) * p1[2], p1[1], -std::sin(b) * p1[0] + std::cos(b) * p1[2]};  // Ry
                double p3[3] = {std::cos(g) * p2[0] - std::sin(g) * p2[1], std::sin(g) * p2[0] + std::cos(g) * p2[1], p2[2]};  // Rz
                CHECK(near(got[0], p3[0]) && near(got[1], p3[1]) && near(got[2], p3[2]));
            }
    }
    // changing the heading by an angle is exactly rotateQuatAboutZ, also for a tilted object
    {
        double x, y, z, w, ax, ay, az;
        eulerDegToQuat(15, -25, 40, true, x, y, z, w);
        double rx = x, ry = y, rz = z, rw = w;
        rotateQuatAboutZ(rx, ry, rz, rw, 33, true);
        quatToEulerDeg(rx, ry, rz, rw, true, ax, ay, az);
        CHECK(std::fabs(ax - 15) < 1e-7 && std::fabs(ay + 25) < 1e-7 && std::fabs(az - 73) < 1e-7);
    }
    // the shipped quaternion of a cone (0, 0, -0.707107, 0.707107) is a heading of +90 (the game applies it inverted)
    {
        double ax, ay, az;
        quatToEulerDeg(0, 0, -0.7071067811865476, 0.7071067811865476, true, ax, ay, az);
        CHECK(near(ax, 0) && near(ay, 0) && std::fabs(az - 90) < 1e-7);
    }

    if (failures == 0) std::printf("all rotation tests passed\n");
    return failures ? 1 : 0;
}
