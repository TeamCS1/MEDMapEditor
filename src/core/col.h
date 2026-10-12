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
    std::string nameField;  // the 22 bytes of the name as stored (Rockstar's tool left memory leftovers after the
                            // terminator); writeColl reuses them while the name is unchanged, so records come back exact
    uint16_t modelId = 0;
    float boundRadius = 0;
    float boundCenter[3] = {0, 0, 0};
    float boundMin[3] = {0, 0, 0};
    float boundMax[3] = {0, 0, 0};
    std::vector<ColSphere> spheres;
    std::vector<ColBox> boxes;
    std::vector<float> vertices;  // 3 per vertex
    std::vector<ColFace> faces;
    size_t fileOffset = 0, fileSize = 0;  // where this model's record ("COLL", size, data) sits in the file it was read from
};

// Parse every model in the file. Returns false (with `error`) on malformed data; models parsed
// before the error stay in `out`.
bool parseCol(const std::vector<unsigned char>& data, std::vector<ColModel>& out, std::string& error);

// One "COLL" (version 1) record for a model: the exact layout parseCol reads (name cut to 21 characters plus the
// terminator, the unknown count written as 0, face padding 0). parseCol(writeColl(m)) gives m back.
std::vector<unsigned char> writeColl(const ColModel& m);

// Set the bounding box, sphere centre and radius from the model's shapes: the box holds every sphere, box and mesh
// vertex; the centre is the box's middle; the radius reaches the farthest point. A model without shapes keeps its bounds.
void computeColBounds(ColModel& m);

}  // namespace med
