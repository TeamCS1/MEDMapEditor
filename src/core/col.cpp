#include "col.h"
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
    m.name = r.str(22);
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
        if (!parseColl(r, m, error)) return false;
        out.push_back(std::move(m));
    }
    return true;
}

}  // namespace med
