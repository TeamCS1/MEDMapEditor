// Turning a placed object about the vertical axis, on the quaternion that an IPL line stores (rotX rotY rotZ rotW).
//
// The game, and MEd, apply the stored quaternion INVERTED (CMapView draws with the negated axis, see
// Quaternion_toNegatedAxisAngle in CLAUDE.md). `conjugate` is that convention: when true the model that is drawn
// uses (-x, -y, -z, w). A turn of +degrees is counter-clockwise seen from above (+z), about the object's own origin,
// in the drawn world, whatever the convention is.
#pragma once
#include <cmath>

namespace med {

inline void rotateQuatAboutZ(double& x, double& y, double& z, double& w, double degrees, bool conjugate = true) {
    const double half = degrees * 3.14159265358979323846 / 360.0, s = std::sin(half), c = std::cos(half);
    const double sg = conjugate ? -1 : 1;
    // the quaternion that is drawn
    const double bx = sg * x, by = sg * y, bz = sg * z, bw = w;
    // turn it: (0, 0, s, c) * (bx, by, bz, bw), the Hamilton product
    double nx = c * bx - s * by, ny = c * by + s * bx, nz = c * bz + s * bw, nw = c * bw - s * bz;
    const double n = std::sqrt(nx * nx + ny * ny + nz * nz + nw * nw);
    if (n > 0) { nx /= n; ny /= n; nz /= n; nw /= n; }
    if (nw < -1e-12) { nx = -nx; ny = -ny; nz = -nz; nw = -nw; }  // q and -q are the same turn; keep w positive like the shipped files
    auto clean = [](double v) { return std::fabs(v) < 1e-12 ? 0.0 : v; };  // quarter turns come out as exact zeros
    x = clean(sg * nx);
    y = clean(sg * ny);
    z = clean(sg * nz);
    w = clean(nw);
}

// Rotation as three angles in degrees, for typing and nudging (MEd's properties panel shows Rotation X / Y / Z and steps
// them by 0.2 and 10 degrees, converting the quaternion to angles and back: 0x00465900 / 0x00465b10). The angles describe
// the DRAWN rotation as R = Rz(z) * Ry(y) * Rx(x): z is the heading (turn about the vertical axis, the same turn as
// rotateQuatAboutZ), x and y tilt the object. Angles come back in (-180, 180], y in [-90, 90].
inline void quatToEulerDeg(double x, double y, double z, double w, bool conjugate, double& ax, double& ay, double& az) {
    const double sg = conjugate ? -1 : 1;
    double qx = sg * x, qy = sg * y, qz = sg * z, qw = w;
    const double n = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
    if (n > 0) { qx /= n; qy /= n; qz /= n; qw /= n; }
    const double toDeg = 180.0 / 3.14159265358979323846;
    const double sp = 2 * (qw * qy - qz * qx);
    ax = std::atan2(2 * (qw * qx + qy * qz), 1 - 2 * (qx * qx + qy * qy)) * toDeg;
    ay = std::asin(sp > 1 ? 1 : sp < -1 ? -1 : sp) * toDeg;
    az = std::atan2(2 * (qw * qz + qx * qy), 1 - 2 * (qy * qy + qz * qz)) * toDeg;
    auto tidy = [](double v) {
        if (std::fabs(v) < 1e-9) return 0.0;
        if (v <= -180.0 + 1e-9) return 180.0;  // one name for a half turn
        return v;
    };
    ax = tidy(ax);
    ay = tidy(ay);
    az = tidy(az);
}

inline void eulerDegToQuat(double ax, double ay, double az, bool conjugate, double& x, double& y, double& z, double& w) {
    const double toRad = 3.14159265358979323846 / 360.0;  // half angles
    const double cr = std::cos(ax * toRad), sr = std::sin(ax * toRad);
    const double cp = std::cos(ay * toRad), sp = std::sin(ay * toRad);
    const double cy = std::cos(az * toRad), sy = std::sin(az * toRad);
    double qw = cr * cp * cy + sr * sp * sy;
    double qx = sr * cp * cy - cr * sp * sy;
    double qy = cr * sp * cy + sr * cp * sy;
    double qz = cr * cp * sy - sr * sp * cy;
    if (qw < -1e-12) { qx = -qx; qy = -qy; qz = -qz; qw = -qw; }  // w positive, like the shipped files
    auto clean = [](double v) { return std::fabs(v) < 1e-12 ? 0.0 : v; };
    const double sg = conjugate ? -1 : 1;
    x = clean(sg * qx);
    y = clean(sg * qy);
    z = clean(sg * qz);
    w = clean(qw);
}

}  // namespace med
