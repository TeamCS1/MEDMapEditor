#include "dff.h"
#include <algorithm>
#include "rwstream.h"

namespace med {

namespace {

using rw::ChunkHeader;

// Geometry flag bits (RpGeometryFlag).
constexpr uint32_t kTriStrip = 0x01, kTextured = 0x04, kPrelit = 0x08, kNormals = 0x10, kTextured2 = 0x80,
                   kNative = 0x01000000;

bool failWith(std::string& error, const char* msg) {
    error = msg;
    return false;
}

std::string readStringChunk(RwReader& r, const ChunkHeader& h) {
    std::string s = r.str(h.size);
    r.seek(h.dataEnd);
    return s;
}

bool parseTexture(RwReader& r, const ChunkHeader& tex, DffMaterial& m) {
    ChunkHeader c;
    int strings = 0;
    while (r.pos() < tex.dataEnd) {
        if (!r.chunk(c, tex.dataEnd)) return false;
        if (c.type == rw::String) {
            (strings == 0 ? m.texture : m.mask) = readStringChunk(r, c);
            ++strings;
        } else {
            r.seek(c.dataEnd);  // Struct (filter flags) and Extension are not needed
        }
    }
    return r.ok();
}

bool parseMaterial(RwReader& r, const ChunkHeader& mat, DffMaterial& m, uint32_t version) {
    ChunkHeader c;
    bool gotStruct = false;
    while (r.pos() < mat.dataEnd) {
        if (!r.chunk(c, mat.dataEnd)) return false;
        if (c.type == rw::Struct && !gotStruct) {
            gotStruct = true;
            r.u32();  // flags
            m.color = r.u32();
            r.u32();  // unused
            m.textured = r.u32() != 0;
            if (version >= 0x30400) {
                m.ambient = r.f32();
                m.specular = r.f32();
                m.diffuse = r.f32();
            }
            r.seek(c.dataEnd);
        } else if (c.type == rw::Texture) {
            if (!parseTexture(r, c, m)) return false;
            r.seek(c.dataEnd);
        } else {
            r.seek(c.dataEnd);
        }
    }
    return r.ok() && gotStruct;
}

bool parseMaterialList(RwReader& r, const ChunkHeader& list, std::vector<DffMaterial>& out, uint32_t version) {
    ChunkHeader c;
    std::vector<int32_t> slots;
    while (r.pos() < list.dataEnd) {
        if (!r.chunk(c, list.dataEnd)) return false;
        if (c.type == rw::Struct && slots.empty()) {
            uint32_t n = r.u32();
            if (n > (c.dataEnd - r.pos()) / 4) return false;
            slots.resize(n);
            for (uint32_t i = 0; i < n; ++i) slots[i] = r.i32();
            r.seek(c.dataEnd);
        } else if (c.type == rw::Material) {
            // The next slot that is not an instance of an earlier material.
            DffMaterial m;
            if (!parseMaterial(r, c, m, version)) return false;
            out.push_back(m);
            r.seek(c.dataEnd);
        } else {
            r.seek(c.dataEnd);
        }
    }
    // Instanced slots (index >= 0) copy an earlier material; -1 slots consumed one chunk each.
    std::vector<DffMaterial> resolved;
    size_t next = 0;
    for (int32_t s : slots) {
        if (s < 0) {
            if (next >= out.size()) return false;
            resolved.push_back(out[next++]);
        } else {
            if (static_cast<size_t>(s) >= resolved.size()) return false;
            resolved.push_back(resolved[s]);
        }
    }
    out = resolved;
    return r.ok();
}

bool parseBinMesh(RwReader& r, const ChunkHeader& c, DffGeometry& g) {
    uint32_t flags = r.u32();
    uint32_t meshes = r.u32();
    r.u32();  // total index count
    g.triangleStrips = (flags & 1) != 0;
    for (uint32_t m = 0; m < meshes; ++m) {
        DffSplit s;
        uint32_t count = r.u32();
        s.material = r.u32();
        if (count > (c.dataEnd - r.pos()) / 4) return false;
        s.indices.resize(count);
        for (uint32_t i = 0; i < count; ++i) s.indices[i] = r.u32();
        g.splits.push_back(std::move(s));
    }
    return r.ok();
}

bool parseGeometry(RwReader& r, const ChunkHeader& geo, DffGeometry& g, std::string& error) {
    uint32_t version = rw::versionFromLibraryId(geo.libraryId);
    ChunkHeader c;
    bool gotStruct = false;
    while (r.pos() < geo.dataEnd) {
        if (!r.chunk(c, geo.dataEnd)) return failWith(error, "truncated geometry chunk");
        if (c.type == rw::Struct && !gotStruct) {
            gotStruct = true;
            g.flags = r.u32();
            uint32_t triCount = r.u32();
            uint32_t vertCount = r.u32();
            uint32_t morphs = r.u32();
            if (g.flags & kNative) return failWith(error, "native (platform specific) geometry is not supported");
            if (vertCount > 1000000 || triCount > 2000000 || morphs == 0 || morphs > 8)
                return failWith(error, "implausible geometry counts");
            if (version < 0x34000) {  // lighting coefficients live here in older files
                r.f32(); r.f32(); r.f32();
            }
            if (g.flags & kPrelit) {
                g.colors.resize(static_cast<size_t>(vertCount) * 4);
                r.bytes(g.colors.data(), g.colors.size());
            }
            uint32_t uvSets = (g.flags >> 16) & 0xFF;
            if (uvSets == 0) uvSets = (g.flags & kTextured2) ? 2 : ((g.flags & kTextured) ? 1 : 0);
            for (uint32_t s = 0; s < uvSets; ++s) {
                if (s == 0) {
                    g.uvs.resize(static_cast<size_t>(vertCount) * 2);
                    for (float& f : g.uvs) f = r.f32();
                } else {
                    r.skip(static_cast<size_t>(vertCount) * 8);  // extra UV sets are not kept
                }
            }
            g.triangles.resize(static_cast<size_t>(triCount) * 3);
            g.triangleMaterial.resize(triCount);
            for (uint32_t t = 0; t < triCount; ++t) {
                uint16_t v2 = r.u16(), v1 = r.u16(), mat = r.u16(), v3 = r.u16();
                g.triangles[t * 3] = v1;
                g.triangles[t * 3 + 1] = v2;
                g.triangles[t * 3 + 2] = v3;
                g.triangleMaterial[t] = mat;
            }
            for (uint32_t m = 0; m < morphs; ++m) {
                float sphere[4];
                for (float& f : sphere) f = r.f32();
                uint32_t hasVerts = r.u32();
                uint32_t hasNormals = r.u32();
                std::vector<float> pos, nrm;
                if (hasVerts) {
                    pos.resize(static_cast<size_t>(vertCount) * 3);
                    for (float& f : pos) f = r.f32();
                }
                if (hasNormals) {
                    nrm.resize(static_cast<size_t>(vertCount) * 3);
                    for (float& f : nrm) f = r.f32();
                }
                if (m == 0) {  // only the first morph target is used
                    std::copy(sphere, sphere + 4, g.boundingSphere);
                    g.positions = std::move(pos);
                    g.normals = std::move(nrm);
                }
            }
            if (!r.ok()) return failWith(error, "geometry data runs past its chunk");
            r.seek(c.dataEnd);
        } else if (c.type == rw::MaterialList) {
            if (!parseMaterialList(r, c, g.materials, version)) return failWith(error, "bad material list");
            r.seek(c.dataEnd);
        } else if (c.type == rw::Extension) {
            ChunkHeader e;
            while (r.pos() < c.dataEnd) {
                if (!r.chunk(e, c.dataEnd)) return failWith(error, "truncated geometry extension");
                if (e.type == rw::BinMesh && !parseBinMesh(r, e, g)) return failWith(error, "bad BinMesh plugin");
                r.seek(e.dataEnd);
            }
            r.seek(c.dataEnd);
        } else {
            r.seek(c.dataEnd);
        }
    }
    if (!gotStruct) return failWith(error, "geometry has no Struct chunk");
    return r.ok();
}

bool parseFrameList(RwReader& r, const ChunkHeader& list, std::vector<DffFrame>& frames, std::string& error) {
    ChunkHeader c;
    if (!r.chunk(c, list.dataEnd) || c.type != rw::Struct) return failWith(error, "frame list has no Struct");
    uint32_t n = r.u32();
    if (n > (c.dataEnd - r.pos()) / 56) return failWith(error, "frame count runs past its chunk");
    frames.resize(n);
    for (DffFrame& f : frames) {
        for (float& v : f.rotation) v = r.f32();
        for (float& v : f.position) v = r.f32();
        f.parent = r.i32();
        f.flags = r.u32();
    }
    r.seek(c.dataEnd);
    // One Extension per frame, which may carry the frame name.
    for (uint32_t i = 0; i < n && r.pos() < list.dataEnd; ++i) {
        ChunkHeader ext;
        if (!r.chunk(ext, list.dataEnd)) return failWith(error, "truncated frame extension");
        ChunkHeader p;
        while (r.pos() < ext.dataEnd) {
            if (!r.chunk(p, ext.dataEnd)) return failWith(error, "truncated frame plugin");
            if (p.type == rw::FrameName) frames[i].name = r.str(p.size);
            r.seek(p.dataEnd);
        }
        r.seek(ext.dataEnd);
    }
    return r.ok();
}

}  // namespace

bool parseDff(const std::vector<unsigned char>& data, DffModel& out, std::string& error) {
    out = DffModel();
    RwReader r(data.data(), data.size());
    ChunkHeader clump;
    if (!r.chunk(clump, data.size()) || clump.type != rw::Clump) return failWith(error, "not a clump (no 0x10 chunk)");
    out.version = rw::versionFromLibraryId(clump.libraryId);
    ChunkHeader c;
    // Some original models (for example sam.dff) declare a clump size that is a few bytes too
    // small for the chunks inside it. The game does not enforce it, so children are only
    // bounded by the file size and the clump end is extended to cover them.
    size_t clumpEnd = clump.dataEnd;
    while (r.pos() < clumpEnd) {
        if (!r.chunk(c, data.size())) return failWith(error, "truncated clump");
        clumpEnd = std::max(clumpEnd, c.dataEnd);
        switch (c.type) {
            case rw::FrameList:
                if (!parseFrameList(r, c, out.frames, error)) return false;
                break;
            case rw::GeometryList: {
                ChunkHeader s;
                if (!r.chunk(s, c.dataEnd) || s.type != rw::Struct) return failWith(error, "geometry list has no Struct");
                uint32_t n = r.u32();
                r.seek(s.dataEnd);
                if (n > 4096) return failWith(error, "implausible geometry count");
                for (uint32_t i = 0; i < n; ++i) {
                    ChunkHeader g;
                    if (!r.chunk(g, c.dataEnd) || g.type != rw::Geometry) return failWith(error, "expected a geometry chunk");
                    DffGeometry geo;
                    if (!parseGeometry(r, g, geo, error)) return false;
                    out.geometries.push_back(std::move(geo));
                    r.seek(g.dataEnd);
                }
                break;
            }
            case rw::Atomic: {
                ChunkHeader s;
                if (!r.chunk(s, c.dataEnd) || s.type != rw::Struct) return failWith(error, "atomic has no Struct");
                DffAtomic a;
                a.frame = r.i32();
                a.geometry = r.i32();
                a.flags = r.u32();
                out.atomics.push_back(a);
                break;
            }
            default:
                break;  // clump Struct (object counts), lights, cameras, extensions
        }
        r.seek(c.dataEnd);
        if (!r.ok()) return failWith(error, "read past end of data");
    }
    return true;
}

}  // namespace med
