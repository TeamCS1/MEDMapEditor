// Ray casting for picking objects with the mouse. MEd picks with OpenGL's selection mode (a 3 x 3 pixel
// region, nearest hit: CMapView 0x0045d320). That mode takes ~11 seconds per pick on the user's graphics
// driver once models are drawn from vertex arrays, so the viewer casts rays on the CPU instead. A 3 x 3 grid
// of rays around the cursor keeps the same tolerance, and the nearest surface wins, as in MEd.
#pragma once
#include <cmath>

namespace med {

// Moller-Trumbore ray / triangle test, both sides of the triangle count (many GTA III meshes are single
// sided and drawn without culling). o = ray origin, d = direction (not necessarily unit length).
// On a hit returns true and the distance parameter t (hit = o + t * d), t > 0.
inline bool rayTriangle(const double o[3], const double d[3], const float* a, const float* b, const float* c, double& t) {
    const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const double e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const double p[3] = {d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0]};
    const double det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
    if (std::fabs(det) < 1e-12) return false;  // parallel to the triangle
    const double inv = 1.0 / det;
    const double s[3] = {o[0] - a[0], o[1] - a[1], o[2] - a[2]};
    const double u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv;
    if (u < 0.0 || u > 1.0) return false;
    const double q[3] = {s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0]};
    const double v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv;
    if (v < 0.0 || u + v > 1.0) return false;
    const double tt = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
    if (tt <= 1e-9) return false;
    t = tt;
    return true;
}

// Invert a 4x4 column-major matrix that only rotates, scales and translates (the last row is 0 0 0 1).
inline bool invertAffine(const float m[16], float out[16]) {
    // upper left 3 x 3 (column-major: m[col * 4 + row])
    const double a = m[0], b = m[4], c = m[8], d = m[1], e = m[5], f = m[9], g = m[2], h = m[6], i = m[10];
    const double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (std::fabs(det) < 1e-12) return false;
    const double inv = 1.0 / det;
    const double r[3][3] = {{(e * i - f * h) * inv, (c * h - b * i) * inv, (b * f - c * e) * inv},
                            {(f * g - d * i) * inv, (a * i - c * g) * inv, (c * d - a * f) * inv},
                            {(d * h - e * g) * inv, (b * g - a * h) * inv, (a * e - b * d) * inv}};
    const double tx = m[12], ty = m[13], tz = m[14];
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) out[col * 4 + row] = float(r[row][col]);
        out[12 + row] = float(-(r[row][0] * tx + r[row][1] * ty + r[row][2] * tz));
        out[row * 4 + 3] = 0;
    }
    out[15] = 1;
    return true;
}

inline void transformPoint(const float m[16], const double p[3], double out[3]) {
    for (int r = 0; r < 3; ++r) out[r] = m[r] * p[0] + m[4 + r] * p[1] + m[8 + r] * p[2] + m[12 + r];
}

inline void transformDirection(const float m[16], const double v[3], double out[3]) {
    for (int r = 0; r < 3; ++r) out[r] = m[r] * v[0] + m[4 + r] * v[1] + m[8 + r] * v[2];
}

}  // namespace med
