// COL: collision models. A .col file is a sequence of models.
// "COLL" (version 1, GTA III and Vice City) is supported. "COL2"/"COL3" (San Andreas) are
// recognised and reported as unsupported for now.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace med {

struct ColSurface {
    uint8_t material = 0, flag = 0, brightness = 0, light = 0;
};

struct ColSphere {
    float radius = 0;
    float center[3] = {0, 0, 0};
    ColSurface surface;
};

struct ColBox {
    float min[3] = {0, 0, 0};
    float max[3] = {0, 0, 0};
    ColSurface surface;
};

struct ColFace {
    uint32_t a = 0, b = 0, c = 0;
    uint8_t material = 0, light = 0;
};

struct ColModel {
    int version = 1;
    std::string name;       // matches the DFF name, e.g. "hospital"
    uint16_t modelId = 0;
    float boundRadius = 0;
    float boundCenter[3] = {0, 0, 0};
    float boundMin[3] = {0, 0, 0};
    float boundMax[3] = {0, 0, 0};
    std::vector<ColSphere> spheres;
    std::vector<ColBox> boxes;
    std::vector<float> vertices;  // 3 per vertex
    std::vector<ColFace> faces;
};

// Parse every model in the file. Returns false (with `error`) on malformed data; models parsed
// before the error stay in `out`.
bool parseCol(const std::vector<unsigned char>& data, std::vector<ColModel>& out, std::string& error);

}  // namespace med
