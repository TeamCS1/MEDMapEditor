#include "col.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "rwstream.h"

namespace med {

namespace {

bool failWith(std::string& error, const std::string& msg) {
    error = msg;
    return false;
}

ColSurface readSurface(RwReader& r) {
    ColSurface s;
    s.material = r.u8();
    s.flag = r.u8();
    s.brightness = r.u8();
    s.light = r.u8();
    return s;
}

void readVec3(RwReader& r, float* v) {
    v[0] = r.f32();
    v[1] = r.f32();
    v[2] = r.f32();
}

// COLL layout: "COLL", size (bytes after this field), name[22], modelId u16, bounds
// (radius, center, min, max), spheres, one unused count, boxes, vertices, faces.
bool parseColl(RwReader& r, ColModel& m, std::string& error) {
    const size_t start = r.pos();
    r.skip(4);  // "COLL"
    uint32_t size = r.u32();
    {
        char raw[22] = {};
        r.bytes(raw, 22);
        m.nameField.assign(raw, 22);
        m.name.assign(raw, strnlen(raw, 22));
    }
    m.modelId = r.u16();
    m.boundRadius = r.f32();
    readVec3(r, m.boundCenter);
    readVec3(r, m.boundMin);
    readVec3(r, m.boundMax);
    const size_t end = start + 8 + size;
    if (!r.ok() || end > r.size()) return failWith(error, "COLL model runs past the end of the file");

    auto countOk = [&](uint32_t n, size_t each) { return r.ok() && n <= (end - r.pos()) / each; };
    uint32_t n = r.u32();
    if (!countOk(n, 20)) return failWith(error, "bad sphere count in " + m.name);
    m.spheres.resize(n);
    for (ColSphere& s : m.spheres) {
        s.radius = r.f32();
        readVec3(r, s.center);
        s.surface = readSurface(r);
    }
    uint32_t unused = r.u32();  // always 0 in the shipped game data
    if (unused != 0) return failWith(error, "unexpected non-zero unknown count in " + m.name);
    n = r.u32();
    if (!countOk(n, 28)) return failWith(error, "bad box count in " + m.name);
    m.boxes.resize(n);
    for (ColBox& b : m.boxes) {
        readVec3(r, b.min);
        readVec3(r, b.max);
        b.surface = readSurface(r);
    }
    n = r.u32();
    if (!countOk(n, 12)) return failWith(error, "bad vertex count in " + m.name);
    m.vertices.resize(static_cast<size_t>(n) * 3);
    for (float& f : m.vertices) f = r.f32();
    n = r.u32();
    if (!countOk(n, 16)) return failWith(error, "bad face count in " + m.name);
    m.faces.resize(n);
    for (ColFace& f : m.faces) {
        f.a = r.u32();
        f.b = r.u32();
        f.c = r.u32();
        f.material = r.u8();
        f.light = r.u8();
        r.skip(2);  // padding
    }
    if (!r.ok()) return failWith(error, "COLL data runs past its model");
    // The declared size must account for exactly the bytes we read.
    if (r.pos() != end) return failWith(error, "COLL size does not match its contents in " + m.name);
    return true;
}

}  // namespace

namespace {
void put32(std::vector<unsigned char>& v, uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<unsigned char>(x >> (8 * i)));
}
void putF(std::vector<unsigned char>& v, float f) {
    uint32_t x;
    std::memcpy(&x, &f, 4);
    put32(v, x);
}
void putSurface(std::vector<unsigned char>& v, const ColSurface& s) {
    v.push_back(s.material);
    v.push_back(s.flag);
    v.push_back(s.brightness);
    v.push_back(s.light);
}
}  // namespace

std::vector<unsigned char> writeColl(const ColModel& m) {
    std::vector<unsigned char> v = {'C', 'O', 'L', 'L', 0, 0, 0, 0};
    char name[22] = {};
    if (m.nameField.size() == 22 && m.name.size() <= 21 && m.nameField.compare(0, m.name.size(), m.name) == 0 && m.nameField[m.name.size()] == '\0')
        std::memcpy(name, m.nameField.data(), 22);  // unchanged name: the bytes as they were, leftovers and all
    else
        std::memcpy(name, m.name.data(), std::min<size_t>(m.name.size(), 21));
    v.insert(v.end(), name, name + 22);
    v.push_back(static_cast<unsigned char>(m.modelId & 0xff));
    v.push_back(static_cast<unsigned char>(m.modelId >> 8));
    putF(v, m.boundRadius);
    for (const float* a : {m.boundCenter, m.boundMin, m.boundMax})
        for (int k = 0; k < 3; ++k) putF(v, a[k]);
    put32(v, uint32_t(m.spheres.size()));
    for (const ColSphere& s : m.spheres) {
        putF(v, s.radius);
        for (int k = 0; k < 3; ++k) putF(v, s.center[k]);
        putSurface(v, s.surface);
    }
    put32(v, 0);  // the unknown count, always 0
    put32(v, uint32_t(m.boxes.size()));
    for (const ColBox& b : m.boxes) {
        for (int k = 0; k < 3; ++k) putF(v, b.min[k]);
        for (int k = 0; k < 3; ++k) putF(v, b.max[k]);
        putSurface(v, b.surface);
    }
    put32(v, uint32_t(m.vertices.size() / 3));
    for (size_t i = 0; i + 2 < m.vertices.size(); i += 3)
        for (int k = 0; k < 3; ++k) putF(v, m.vertices[i + size_t(k)]);
    put32(v, uint32_t(m.faces.size()));
    for (const ColFace& f : m.faces) {
        put32(v, f.a);
        put32(v, f.b);
        put32(v, f.c);
        v.push_back(f.material);
        v.push_back(f.light);
        v.push_back(0);
        v.push_back(0);
    }
    const uint32_t size = uint32_t(v.size() - 8);
    for (int i = 0; i < 4; ++i) v[size_t(4 + i)] = static_cast<unsigned char>(size >> (8 * i));
    return v;
}

void computeColBounds(ColModel& m) {
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    bool any = false;
    auto grow = [&](float x, float y, float z) {
        const float p[3] = {x, y, z};
        for (int k = 0; k < 3; ++k) { lo[k] = std::min(lo[k], p[k]); hi[k] = std::max(hi[k], p[k]); }
        any = true;
    };
    for (const ColSphere& s : m.spheres) {
        grow(s.center[0] - s.radius, s.center[1] - s.radius, s.center[2] - s.radius);
        grow(s.center[0] + s.radius, s.center[1] + s.radius, s.center[2] + s.radius);
    }
    for (const ColBox& b : m.boxes) {
        grow(b.min[0], b.min[1], b.min[2]);
        grow(b.max[0], b.max[1], b.max[2]);
    }
    for (size_t i = 0; i + 2 < m.vertices.size(); i += 3) grow(m.vertices[i], m.vertices[i + 1], m.vertices[i + 2]);
    if (!any) return;
    float r = 0;
    for (int k = 0; k < 3; ++k) {
        m.boundMin[k] = lo[k];
        m.boundMax[k] = hi[k];
        m.boundCenter[k] = (lo[k] + hi[k]) / 2;
    }
    auto reach = [&](float x, float y, float z, float extra) {
        const float dx = x - m.boundCenter[0], dy = y - m.boundCenter[1], dz = z - m.boundCenter[2];
        r = std::max(r, std::sqrt(dx * dx + dy * dy + dz * dz) + extra);
    };
    for (const ColSphere& s : m.spheres) reach(s.center[0], s.center[1], s.center[2], s.radius);
    for (const ColBox& b : m.boxes)
        for (int c = 0; c < 8; ++c) reach((c & 1) ? b.max[0] : b.min[0], (c & 2) ? b.max[1] : b.min[1], (c & 4) ? b.max[2] : b.min[2], 0);
    for (size_t i = 0; i + 2 < m.vertices.size(); i += 3) reach(m.vertices[i], m.vertices[i + 1], m.vertices[i + 2], 0);
    m.boundRadius = r;
}

bool parseCol(const std::vector<unsigned char>& data, std::vector<ColModel>& out, std::string& error) {
    out.clear();
    RwReader r(data.data(), data.size());
    while (r.pos() < data.size()) {
        if (data.size() - r.pos() < 8) return failWith(error, "trailing bytes after the last collision model");
        const unsigned char* p = data.data() + r.pos();
        std::string tag(reinterpret_cast<const char*>(p), 4);
        if (tag == "COL2" || tag == "COL3" || tag == "COL4") return failWith(error, "San Andreas collision (" + tag + ") is not supported yet");
        if (tag != "COLL") return failWith(error, "unknown collision tag at offset " + std::to_string(r.pos()));
        ColModel m;
        m.fileOffset = r.pos();
        if (!parseColl(r, m, error)) return false;
        m.fileSize = r.pos() - m.fileOffset;
        out.push_back(std::move(m));
    }
    return true;
}

}  // namespace med
