// MEd's camera model, ported from MEd.exe v0.32 (class CMapView). Every constant here comes from
// the decompile. Addresses are given so a change can be checked against the original.
//
// State (CMapView fields): x +0x1c, y +0x20, z +0x24, yaw +0x28, pitch +0x2c, h +0x40,
// mouse speed +0x98 (default 20), shift multiplier +0xa4 (default 1).
// Bookmarks store them as  x : y : z : h : yaw : pitch : flag  (bookmarks.ini).
//
// The view is built in CMapView::paintGL (0x0045bc40):
//   projection: gluPerspective(45, aspect, 1, 8000); glRotatef(pitch, 1,0,0); glRotatef(yaw, 0,1,0)
//   modelview : gluLookAt(x, y-20, z,  x, y, z,  0,0,1); glTranslatef(0, 0, h-20)
// Angles are degrees; yaw 0 faces +Y (north) and increasing yaw turns towards +X.
#pragma once
#include <cmath>

namespace med {

struct MedCamera {
    float x = 0, y = 0, z = 0;
    float h = 0;
    float yaw = 0, pitch = 0;
    float mouseSpeed = 20;   // CMapView ctor, 0x00459800
    int shiftMultiplier = 1; // CMapView ctor, 0x00459800 (the "Shift multiplier" combo box)
};

// Keys after mapping. MEd reads the physical keys with GetKeyboardState every timer tick.
// W / Up = forward, S / Down = back, D / Right = strafe right, A / Left = strafe left,
// PgUp or R = up, PgDn or F = down; Ctrl+forward = up, Ctrl+back = down.
struct CameraKeys {
    bool forward = false, back = false, left = false, right = false;
    bool pageUp = false, pageDown = false;  // also the undocumented R and F keys
    bool shift = false, ctrl = false, alt = false;
};

namespace camera_detail {
constexpr double kDegToRad = 0.0174532925;  // 0x006d39a8
inline double rad(double deg) { return deg * kDegToRad; }
}  // namespace camera_detail

// One pollKeyboard() tick (CMapView, 0x0045e530). MEd starts a 1 ms QTimer for this, so a real
// time loop should call it once per millisecond elapsed.
inline void cameraKeyboardTick(MedCamera& c, const CameraKeys& k) {
    using namespace camera_detail;
    const double m = c.shiftMultiplier;
    // forward / back: 0.5 per tick, or shiftMultiplier * 3.0 with Shift
    const double step = k.shift ? m * 3.0 : 0.5;
    const double sy = std::sin(rad(c.yaw)), cy = std::cos(rad(c.yaw)), sp = std::sin(rad(c.pitch));
    if (k.forward && !k.ctrl && !k.alt) {
        c.x = float(c.x + sy * step);
        c.y = float(c.y + cy * step);
        c.h = float(c.h + sp * step);
    }
    if (k.back && !k.ctrl && !k.alt) {
        c.x = float(c.x - sy * step);
        c.y = float(c.y - cy * step);
        c.h = float(c.h - sp * step);
    }
    // strafe: 0.2 per tick, or shiftMultiplier * 1.2 with Shift, along yaw + 90
    const double sstep = k.shift ? m * 1.2 : 0.2;
    const double ay = rad(c.yaw + 90.0);
    if (k.right && !k.alt) {
        c.x = float(c.x + std::sin(ay) * sstep);
        c.y = float(c.y + std::cos(ay) * sstep);
    }
    if (k.left && !k.alt) {
        c.x = float(c.x - std::sin(ay) * sstep);
        c.y = float(c.y - std::cos(ay) * sstep);
    }
    // up / down: 0.5 per tick, or shiftMultiplier * 4.0 with Shift
    const double vstep = k.shift ? m * 4.0 : 0.5;
    if (((k.forward && k.ctrl) || k.pageUp) && !k.alt) c.z = float(c.z + vstep);
    if (((k.back && k.ctrl) || k.pageDown) && !k.alt) c.z = float(c.z - vstep);
}

// Left button, no Ctrl: rotate (CMapView::mouseMoveEvent, 0x00462130). dx, dy are pixels since the
// previous mouse event. The wrap behaviour is the original's: above 360 it resets to 0 and below 0
// it jumps to 359.
inline void cameraMouseRotate(MedCamera& c, double dx, double dy) {
    const float sens = c.mouseSpeed * 0.01f;
    c.yaw = c.yaw + sens * float(dx);
    c.pitch = c.pitch + sens * float(dy);
    if (c.yaw > 360.0f) c.yaw = 0;
    if (c.pitch > 360.0f) c.pitch = 0;
    if (c.yaw < 0.0f) c.yaw = 359.0f;
    if (c.pitch < 0.0f) c.pitch = 359.0f;
}

// Ctrl + left button: move. Only the direction of the mouse movement matters, not its length:
// each event moves a fixed step (forward shiftMultiplier * 2.8, sideways shiftMultiplier * 1.8,
// and 0.8 of height along the pitch).
inline void cameraMouseMove(MedCamera& c, double dx, double dy) {
    using namespace camera_detail;
    const double m = c.shiftMultiplier;
    const double sy = std::sin(rad(c.yaw)), cy = std::cos(rad(c.yaw)), sp = std::sin(rad(c.pitch));
    if (dy < 0) {
        c.x = float(c.x + sy * m * 2.8);
        c.y = float(c.y + cy * m * 2.8);
        c.h = float(c.h + sp * 0.8);
    }
    if (dy > 0) {
        c.x = float(c.x - sy * m * 2.8);
        c.y = float(c.y - cy * m * 2.8);
        c.h = float(c.h - sp * 0.8);
    }
    const double ay = rad(c.yaw + 90.0);
    if (dx < 0) {
        c.x = float(c.x - std::sin(ay) * m * 1.8);
        c.y = float(c.y - std::cos(ay) * m * 1.8);
        c.h = float(c.h + sp * 0.8);
    }
    if (dx > 0) {
        c.x = float(c.x + std::sin(ay) * m * 1.8);
        c.y = float(c.y + std::cos(ay) * m * 1.8);
        c.h = float(c.h - sp * 0.8);
    }
}

// Mouse wheel (CMapView::wheelEvent, 0x00459540): one step per wheel event, direction only. Away
// from you (delta > 0) moves forward along the view, towards you moves back. Without Shift the step is
// 0.7 for x, y and h; with Shift it is 3.7 for x and y and 10.0 for h (h changes by sin(pitch)).
inline void cameraWheel(MedCamera& c, int delta, bool shift) {
    using namespace camera_detail;
    const double step = shift ? 3.7 : 0.7;
    const double hstep = shift ? 10.0 : 0.7;
    const double sy = std::sin(rad(c.yaw)), cy = std::cos(rad(c.yaw)), sp = std::sin(rad(c.pitch));
    if (delta > 0) {
        c.x = float(c.x + sy * step);
        c.y = float(c.y + cy * step);
        c.h = float(c.h + sp * hstep);
    } else if (delta < 0) {
        c.x = float(c.x - sy * step);
        c.y = float(c.y - cy * step);
        c.h = float(c.h - sp * hstep);
    }
}

// ---- moving selected objects ----------------------------------------------------------------

struct ObjectMove {
    double dx = 0, dy = 0, dz = 0;
    bool any() const { return dx != 0 || dy != 0 || dz != 0; }
};

// One pollKeyboard() tick of "Alt + cursor keys" (CMapView, 0x0045e530): moves every selected
// object. Step is 0.1 per tick, or the shift multiplier with Shift. Alt+forward/back move along
// (sin yaw, cos yaw), Alt+left/right along yaw + 90, and Alt+Ctrl+cursor or Alt+PgUp/PgDn move
// up and down. Returns zero when Alt is not held.
inline ObjectMove objectKeyboardTick(const MedCamera& c, const CameraKeys& k) {
    using namespace camera_detail;
    ObjectMove m;
    if (!k.alt) return m;
    const double step = k.shift ? double(c.shiftMultiplier) : 0.1;
    const double sy = std::sin(rad(c.yaw)), cy = std::cos(rad(c.yaw));
    const double ay = rad(c.yaw + 90.0);
    if (k.forward && !k.ctrl) { m.dx += sy * step; m.dy += cy * step; }
    if (k.back && !k.ctrl) { m.dx -= sy * step; m.dy -= cy * step; }
    if (k.right && !k.ctrl) { m.dx += std::sin(ay) * step; m.dy += std::cos(ay) * step; }
    if (k.left && !k.ctrl) { m.dx -= std::sin(ay) * step; m.dy -= std::cos(ay) * step; }
    if ((k.forward && k.ctrl) || k.pageUp) m.dz += step;
    if ((k.back && k.ctrl) || k.pageDown) m.dz -= step;
    return m;
}

// Alt + left button drag (CMapView, 0x00459270): the mouse position is un-projected to a near
// point (window depth 0) and a far point (depth 1) and intersected with the horizontal plane at
// z - h. Returns false if the ray is parallel to the plane.
// Original bug fixed: MEd computes  near + t * (far - near) + near  for x and y, adding the near
// point twice, so a dragged object drifted away from the cursor. This returns the true
// intersection.
inline bool rayPlaneIntersection(const double nearPt[3], const double farPt[3], double planeZ, double out[3]) {
    const double dz = farPt[2] - nearPt[2];
    if (std::fabs(dz) < 1e-12) return false;
    const double t = (planeZ - nearPt[2]) / dz;
    out[0] = nearPt[0] + t * (farPt[0] - nearPt[0]);
    out[1] = nearPt[1] + t * (farPt[1] - nearPt[1]);
    out[2] = planeZ;
    return true;
}

}  // namespace med
