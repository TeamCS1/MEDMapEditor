// DFF: RenderWare clump (3D model). Reads frames, geometry (vertices, normals, UVs,
// triangles, materials, triangle-strip splits) and atomics. Skinning, lights and
// effect plugins are skipped.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace med {

struct DffFrame {
    float rotation[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};  // right, up, at vectors
    float position[3] = {0, 0, 0};
    int32_t parent = -1;
    uint32_t flags = 0;
    std::string name;  // from the Frame plugin, empty when absent
};

struct DffMaterial {
    uint32_t color = 0xFFFFFFFF;  // RGBA bytes packed little-endian
    float ambient = 1, specular = 1, diffuse = 1;
    bool textured = false;
    std::string texture;  // TXD entry name
    std::string mask;     // alpha mask name, usually empty
};

// One run of the BinMesh plugin: the indices for a single material.
struct DffSplit {
    uint32_t material = 0;
    std::vector<uint32_t> indices;
};

struct DffGeometry {
    uint32_t flags = 0;  // 1 tristrip, 2 positions, 4 textured, 8 prelit, 0x10 normals, 0x80 two UV sets
    float boundingSphere[4] = {0, 0, 0, 0};
    std::vector<float> positions;  // 3 per vertex
    std::vector<float> normals;    // 3 per vertex, may be empty
    std::vector<float> uvs;        // 2 per vertex (first set), may be empty
    std::vector<uint8_t> colors;   // 4 per vertex (prelit), may be empty
    std::vector<uint32_t> triangles;       // 3 vertex indices per triangle
    std::vector<uint16_t> triangleMaterial;  // one per triangle
    std::vector<DffMaterial> materials;
    std::vector<DffSplit> splits;  // empty when the model has no BinMesh plugin
    bool triangleStrips = false;   // BinMesh indices are strips rather than lists

    size_t vertexCount() const { return positions.size() / 3; }
};

struct DffAtomic {
    int32_t frame = -1;
    int32_t geometry = -1;
    uint32_t flags = 0;
};

struct DffModel {
    uint32_t version = 0;  // e.g. 0x31000 for GTA III, 0x36003 for San Andreas
    std::vector<DffFrame> frames;
    std::vector<DffGeometry> geometries;
    std::vector<DffAtomic> atomics;
};

// Returns false and fills `error` if the data is not a well-formed clump.
bool parseDff(const std::vector<unsigned char>& data, DffModel& out, std::string& error);

}  // namespace med
