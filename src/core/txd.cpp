#include "txd.h"
#include <algorithm>
#include "rwstream.h"

namespace med {

namespace {

using rw::ChunkHeader;

// rasterFormat bits
constexpr uint32_t kFormatMask = 0x0F00;
constexpr uint32_t kPal8 = 0x2000, kPal4 = 0x4000;
enum RasterFormat { F1555 = 0x0100, F565 = 0x0200, F4444 = 0x0300, FLum8 = 0x0400, F8888 = 0x0500, F888 = 0x0600, F555 = 0x0700 };

bool failWith(std::string& error, const char* msg) {
    error = msg;
    return false;
}

inline uint8_t expand5(unsigned v) { return static_cast<uint8_t>((v << 3) | (v >> 2)); }
inline uint8_t expand6(unsigned v) { return static_cast<uint8_t>((v << 2) | (v >> 4)); }
inline uint8_t expand4(unsigned v) { return static_cast<uint8_t>((v << 4) | v); }

void put565(uint16_t c, uint8_t* o) {
    o[0] = expand5((c >> 11) & 31);
    o[1] = expand6((c >> 5) & 63);
    o[2] = expand5(c & 31);
}

}  // namespace

void decodeDxt(const uint8_t* src, uint16_t width, uint16_t height, int dxt, uint8_t* rgba) {
    const int blockBytes = (dxt == 1) ? 8 : 16;
    for (int by = 0; by < (height + 3) / 4; ++by) {
        for (int bx = 0; bx < (width + 3) / 4; ++bx) {
            const uint8_t* b = src + (static_cast<size_t>(by) * ((width + 3) / 4) + bx) * blockBytes;
            const uint8_t* cb = (dxt == 1) ? b : b + 8;
            uint16_t c0 = static_cast<uint16_t>(cb[0] | (cb[1] << 8));
            uint16_t c1 = static_cast<uint16_t>(cb[2] | (cb[3] << 8));
            uint8_t col[4][4];
            put565(c0, col[0]);
            put565(c1, col[1]);
            col[0][3] = col[1][3] = 255;
            if (c0 > c1 || dxt != 1) {
                for (int k = 0; k < 3; ++k) {
                    col[2][k] = static_cast<uint8_t>((2 * col[0][k] + col[1][k]) / 3);
                    col[3][k] = static_cast<uint8_t>((col[0][k] + 2 * col[1][k]) / 3);
                }
                col[2][3] = col[3][3] = 255;
            } else {
                for (int k = 0; k < 3; ++k) col[2][k] = static_cast<uint8_t>((col[0][k] + col[1][k]) / 2);
                col[2][3] = 255;
                col[3][0] = col[3][1] = col[3][2] = col[3][3] = 0;  // transparent black
            }
            uint32_t bits = cb[4] | (cb[5] << 8) | (cb[6] << 16) | (static_cast<uint32_t>(cb[7]) << 24);
            uint8_t alpha[16];
            if (dxt == 3) {
                for (int i = 0; i < 16; ++i) alpha[i] = expand4((b[i / 2] >> ((i & 1) * 4)) & 15);
            } else if (dxt == 5) {
                uint8_t a[8];
                a[0] = b[0];
                a[1] = b[1];
                if (a[0] > a[1]) {
                    for (int k = 1; k < 7; ++k) a[k + 1] = static_cast<uint8_t>(((7 - k) * a[0] + k * a[1]) / 7);
                } else {
                    for (int k = 1; k < 5; ++k) a[k + 1] = static_cast<uint8_t>(((5 - k) * a[0] + k * a[1]) / 5);
                    a[6] = 0;
                    a[7] = 255;
                }
                uint64_t ab = 0;
                for (int i = 0; i < 6; ++i) ab |= static_cast<uint64_t>(b[2 + i]) << (8 * i);
                for (int i = 0; i < 16; ++i) alpha[i] = a[(ab >> (3 * i)) & 7];
            }
            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 4; ++px) {
                    int x = bx * 4 + px, y = by * 4 + py;
                    if (x >= width || y >= height) continue;
                    int idx = (bits >> (2 * (py * 4 + px))) & 3;
                    uint8_t* o = rgba + (static_cast<size_t>(y) * width + x) * 4;
                    o[0] = col[idx][0];
                    o[1] = col[idx][1];
                    o[2] = col[idx][2];
                    o[3] = (dxt == 1) ? col[idx][3] : alpha[py * 4 + px];
                }
            }
        }
    }
}

namespace {

// Raw (uncompressed) level 0 to RGBA. `src` holds `size` bytes.
bool decodeRaw(const TxdTexture& t, const uint8_t* src, size_t size, const uint8_t* palette, uint8_t* rgba) {
    const size_t px = static_cast<size_t>(t.width) * t.height;
    if (t.paletted) {
        if (size < px) return false;
        for (size_t i = 0; i < px; ++i) {
            const uint8_t* p = palette + src[i] * 4;  // palette entries are stored R, G, B, A
            rgba[i * 4] = p[0];
            rgba[i * 4 + 1] = p[1];
            rgba[i * 4 + 2] = p[2];
            rgba[i * 4 + 3] = p[3];
        }
        return true;
    }
    uint32_t fmt = t.rasterFormat & kFormatMask;
    if (t.depth == 32 || fmt == F8888 || fmt == F888) {
        if (size < px * 4) return false;
        for (size_t i = 0; i < px; ++i) {  // stored B, G, R, A
            rgba[i * 4] = src[i * 4 + 2];
            rgba[i * 4 + 1] = src[i * 4 + 1];
            rgba[i * 4 + 2] = src[i * 4];
            rgba[i * 4 + 3] = (fmt == F888) ? 255 : src[i * 4 + 3];
        }
        return true;
    }
    if (t.depth == 16) {
        if (size < px * 2) return false;
        for (size_t i = 0; i < px; ++i) {
            uint16_t c = static_cast<uint16_t>(src[i * 2] | (src[i * 2 + 1] << 8));
            uint8_t* o = rgba + i * 4;
            switch (fmt) {
                case F1555:
                    o[0] = expand5((c >> 10) & 31); o[1] = expand5((c >> 5) & 31); o[2] = expand5(c & 31);
                    o[3] = (c & 0x8000) ? 255 : 0;
                    break;
                case F565:
                    put565(c, o); o[3] = 255;
                    break;
                case F4444:
                    o[0] = expand4((c >> 8) & 15); o[1] = expand4((c >> 4) & 15); o[2] = expand4(c & 15);
                    o[3] = expand4((c >> 12) & 15);
                    break;
                case F555:
                    o[0] = expand5((c >> 10) & 31); o[1] = expand5((c >> 5) & 31); o[2] = expand5(c & 31);
                    o[3] = 255;
                    break;
                default:
                    return false;
            }
        }
        return true;
    }
    return false;
}

bool parseNative(RwReader& r, const ChunkHeader& nat, TxdTexture& t, std::string& error) {
    ChunkHeader s;
    if (!r.chunk(s, nat.dataEnd) || s.type != rw::Struct) return failWith(error, "texture has no Struct");
    t.platform = r.u32();
    if (t.platform == 0x00325350u) return failWith(error, "PS2 texture format is not supported");
    if (t.platform != 8 && t.platform != 9) return failWith(error, "unsupported texture platform (not Direct3D 8/9)");
    r.u32();  // filter and addressing flags
    char name[32], mask[32];
    r.bytes(name, 32);
    r.bytes(mask, 32);
    t.name.assign(name, strnlen(name, 32));
    t.mask.assign(mask, strnlen(mask, 32));
    t.rasterFormat = r.u32();
    uint32_t d3dFormat = r.u32();  // D3D8: "has alpha" flag. D3D9: D3DFORMAT or a FourCC
    t.width = r.u16();
    t.height = r.u16();
    t.depth = r.u8();
    t.mipLevels = r.u8();
    r.u8();  // raster type
    uint8_t flagsOrCompression = r.u8();
    int dxt = 0;
    if (t.platform == 8) {
        t.hasAlpha = d3dFormat != 0;
        if (flagsOrCompression == 1) dxt = 1;
        else if (flagsOrCompression == 3) dxt = 3;
        else if (flagsOrCompression != 0) return failWith(error, "unknown D3D8 compression");
    } else {
        t.hasAlpha = (flagsOrCompression & 1) != 0;
        if (flagsOrCompression & 8) {  // compressed: the FourCC says which DXT
            if (d3dFormat == 0x31545844u) dxt = 1;       // 'DXT1'
            else if (d3dFormat == 0x33545844u) dxt = 3;  // 'DXT3'
            else if (d3dFormat == 0x35545844u) dxt = 5;  // 'DXT5'
            else return failWith(error, "unknown D3D9 compression");
        }
    }
    t.compressed = dxt != 0;
    t.paletted = (t.rasterFormat & (kPal8 | kPal4)) != 0;
    if (t.width == 0 || t.height == 0 || t.width > 8192 || t.height > 8192) return failWith(error, "implausible texture size");
    uint8_t palette[256 * 4] = {0};
    if (t.paletted) {
        if (t.rasterFormat & kPal4) return failWith(error, "4-bit palettes are not supported");
        r.bytes(palette, sizeof palette);
    }
    uint32_t size0 = r.u32();
    if (!r.ok() || size0 > nat.dataEnd - r.pos()) return failWith(error, "texture data runs past its chunk");
    std::vector<uint8_t> raw(size0);
    r.bytes(raw.data(), size0);
    t.rgba.assign(static_cast<size_t>(t.width) * t.height * 4, 0);
    if (dxt) {
        size_t need = static_cast<size_t>((t.width + 3) / 4) * ((t.height + 3) / 4) * (dxt == 1 ? 8 : 16);
        if (size0 < need) return failWith(error, "compressed texture data is too short");
        decodeDxt(raw.data(), t.width, t.height, dxt, t.rgba.data());
    } else if (!decodeRaw(t, raw.data(), raw.size(), palette, t.rgba.data())) {
        return failWith(error, "unsupported raster format");
    }
    return r.ok();
}

}  // namespace

bool parseTxd(const std::vector<unsigned char>& data, TxdFile& out, std::string& error) {
    out = TxdFile();
    RwReader r(data.data(), data.size());
    ChunkHeader dict;
    if (!r.chunk(dict, data.size()) || dict.type != rw::TextureDictionary) return failWith(error, "not a texture dictionary (no 0x16 chunk)");
    out.version = rw::versionFromLibraryId(dict.libraryId);
    ChunkHeader s;
    if (!r.chunk(s, dict.dataEnd) || s.type != rw::Struct) return failWith(error, "dictionary has no Struct");
    uint32_t count = r.u16();
    r.seek(s.dataEnd);
    ChunkHeader c;
    while (r.pos() < dict.dataEnd && out.textures.size() < count) {
        if (!r.chunk(c, dict.dataEnd)) return failWith(error, "truncated dictionary");
        if (c.type == rw::TextureNative) {
            TxdTexture t;
            if (!parseNative(r, c, t, error)) {
                error = "texture '" + t.name + "': " + error;
                return false;
            }
            out.textures.push_back(std::move(t));
        }
        r.seek(c.dataEnd);
    }
    if (out.textures.size() != count) return failWith(error, "texture count does not match the dictionary header");
    return true;
}

}  // namespace med
