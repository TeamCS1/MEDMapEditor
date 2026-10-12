// Bounds-checked reader for RenderWare binary streams (DFF models, TXD dictionaries).
// A stream is a tree of chunks: type (u32), size (u32), library id (u32), then `size` bytes.
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace med {

namespace rw {
// Chunk types used by GTA III / Vice City / San Andreas.
enum : uint32_t {
    Struct = 0x01,
    String = 0x02,
    Extension = 0x03,
    Texture = 0x06,
    Material = 0x07,
    MaterialList = 0x08,
    FrameList = 0x0E,
    Geometry = 0x0F,
    Clump = 0x10,
    Atomic = 0x14,
    TextureNative = 0x15,
    TextureDictionary = 0x16,
    GeometryList = 0x1A,
    BinMesh = 0x50E,
    FrameName = 0x253F2FE,
};

struct ChunkHeader {
    uint32_t type = 0, size = 0, libraryId = 0;
    size_t dataStart = 0, dataEnd = 0;
};

// Decode the packed library id into a version such as 0x31000 (3.1.0.0), 0x34003, 0x36003.
inline uint32_t versionFromLibraryId(uint32_t id) {
    if (id & 0xFFFF0000u) return (((id >> 14) & 0x3FF00u) + 0x30000u) | ((id >> 16) & 0x3Fu);
    return id << 8;
}
}  // namespace rw

class RwReader {
public:
    RwReader(const unsigned char* data, size_t size) : d_(data), size_(size) {}

    size_t pos() const { return pos_; }
    size_t size() const { return size_; }
    bool ok() const { return ok_; }
    void seek(size_t p) { if (p > size_) fail(); else pos_ = p; }
    void skip(size_t n) { if (n > size_ - pos_) fail(); else pos_ += n; }

    uint8_t u8() { uint8_t v = 0; read(&v, 1); return v; }
    uint16_t u16() { uint16_t v = 0; read(&v, 2); return v; }
    uint32_t u32() { uint32_t v = 0; read(&v, 4); return v; }
    int32_t i32() { return static_cast<int32_t>(u32()); }
    float f32() { float v = 0; read(&v, 4); return v; }
    void bytes(void* out, size_t n) { read(out, n); }
    std::string str(size_t n) {
        if (n > size_ - pos_) { fail(); return ""; }
        std::string s(reinterpret_cast<const char*>(d_ + pos_), n);
        pos_ += n;
        size_t z = s.find('\0');
        return z == std::string::npos ? s : s.substr(0, z);
    }

    // Read a chunk header at the current position. Fails if the chunk would run past
    // `limit` (use the parent's dataEnd, or size() for the top level).
    bool chunk(rw::ChunkHeader& h, size_t limit) {
        if (!ok_ || limit > size_ || pos_ + 12 > limit) { fail(); return false; }
        h.type = u32();
        h.size = u32();
        h.libraryId = u32();
        h.dataStart = pos_;
        if (h.size > limit - pos_) { fail(); return false; }
        h.dataEnd = pos_ + h.size;
        return true;
    }

private:
    void read(void* out, size_t n) {
        if (!ok_ || n > size_ - pos_) { fail(); std::memset(out, 0, n); return; }
        std::memcpy(out, d_ + pos_, n);
        pos_ += n;
    }
    void fail() { ok_ = false; }

    const unsigned char* d_;
    size_t size_;
    size_t pos_ = 0;
    bool ok_ = true;
};

}  // namespace med
