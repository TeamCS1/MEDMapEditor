#include <cmath>
#include <cstdio>
#include "../src/viewer/camera.h"

using namespace med;

static int failures = 0;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) {                                                    \
            std::printf("FAIL line %d: %s\n", __LINE__, #c);           \
            ++failures;                                                \
        }                                                              \
    } while (0)
static bool near(double a, double b, double e = 1e-4) { return std::fabs(a - b) < e; }

int main() {
    // yaw 0 faces +Y; forward is 0.5 per tick
    MedCamera c;
    CameraKeys k;
    k.forward = true;
    cameraKeyboardTick(c, k);
    CHECK(near(c.x, 0) && near(c.y, 0.5) && near(c.z, 0));

    // yaw 90 turns towards +X
    c = MedCamera();
    c.yaw = 90;
    cameraKeyboardTick(c, k);
    CHECK(near(c.x, 0.5) && near(c.y, 0, 1e-3));

    // Shift multiplies: forward = multiplier * 3.0
    c = MedCamera();
    c.shiftMultiplier = 4;
    k.shift = true;
    cameraKeyboardTick(c, k);
    CHECK(near(c.y, 12.0));

    // strafe right at yaw 0 is +X (yaw + 90), 0.2 per tick
    c = MedCamera();
    k = CameraKeys();
    k.right = true;
    cameraKeyboardTick(c, k);
    CHECK(near(c.x, 0.2) && near(c.y, 0, 1e-3));

    // Ctrl+forward moves up, not forward
    c = MedCamera();
    k = CameraKeys();
    k.forward = k.ctrl = true;
    cameraKeyboardTick(c, k);
    CHECK(near(c.z, 0.5) && near(c.y, 0));

    // Alt blocks camera movement (Alt+cursor moves the selected object instead)
    c = MedCamera();
    k = CameraKeys();
    k.forward = k.alt = true;
    cameraKeyboardTick(c, k);
    CHECK(near(c.y, 0));

    // mouse rotate: speed 20 -> 0.2 degrees per pixel
    c = MedCamera();
    cameraMouseRotate(c, 10, 5);
    CHECK(near(c.yaw, 2.0) && near(c.pitch, 1.0));
    // original wrap quirk: below 0 jumps to 359, above 360 resets to 0
    c = MedCamera();
    cameraMouseRotate(c, -1, 0);
    CHECK(near(c.yaw, 359.0));
    c.yaw = 360.0f;
    cameraMouseRotate(c, 10, 0);
    CHECK(near(c.yaw, 0));

    // Ctrl-drag: direction only, fixed step
    c = MedCamera();
    cameraMouseMove(c, 0, -50);
    CHECK(near(c.y, 2.8));
    cameraMouseMove(c, 0, -1);
    CHECK(near(c.y, 5.6));

    if (failures == 0) std::printf("all camera tests passed\n");
    return failures ? 1 : 0;
}
