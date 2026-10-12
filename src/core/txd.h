// TXD: RenderWare texture dictionary (PC, Direct3D 8 for GTA III/VC, Direct3D 9 for San Andreas).
// Textures are decoded to 8-bit RGBA (first mip level only).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace med {

struct TxdTexture {
    std::string name;
    std::string mask;
    uint32_t platform = 0;     // 8 = Direct3D 8, 9 = Direct3D 9
    uint32_t rasterFormat = 0; // RenderWare raster format flags
    uint16_t width = 0, height = 0;
    uint8_t depth = 0;         // bits per pixel of the stored data
    uint8_t mipLevels = 1;
    bool hasAlpha = false;
    bool compressed = false;   // DXT1/DXT3/DXT5
    bool paletted = false;
    std::vector<uint8_t> rgba; // width * height * 4, level 0
};

struct TxdFile {
    uint32_t version = 0;
    std::vector<TxdTexture> textures;
};

bool parseTxd(const std::vector<unsigned char>& data, TxdFile& out, std::string& error);

// Block decoders, exposed for tests. `dxt` is 1, 3 or 5.
void decodeDxt(const uint8_t* src, uint16_t width, uint16_t height, int dxt, uint8_t* rgba);

}  // namespace med
