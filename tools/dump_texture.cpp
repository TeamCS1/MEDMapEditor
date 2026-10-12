// Extract one texture from a TXD inside gta3.img as raw RGBA, for visual checks.
// usage: dump_texture <GTA III folder> <txd entry> <texture name | index> <out.rgba>
// Writes "<width> <height>\n" to <out.rgba>.txt and the raw pixels to <out.rgba>. Read-only on the game.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include "../src/core/img.h"
#include "../src/core/textutil.h"
#include "../src/core/txd.h"

using namespace med;

int main(int argc, char** argv) {
    if (argc < 5) {
        std::printf("usage: dump_texture <GTA III folder> <txd entry> <texture name|index> <out.rgba>\n");
        return 2;
    }
    ImgArchive img;
    std::string err;
    if (!ImgArchive::open(std::string(argv[1]) + "/models/gta3.img", img, err)) { std::printf("%s\n", err.c_str()); return 1; }
    std::vector<unsigned char> data;
    if (!img.extract(argv[2], data, err)) { std::printf("%s\n", err.c_str()); return 1; }
    TxdFile txd;
    if (!parseTxd(data, txd, err)) { std::printf("%s\n", err.c_str()); return 1; }
    std::string want = argv[3];
    const TxdTexture* pick = nullptr;
    for (size_t i = 0; i < txd.textures.size(); ++i) {
        if (toLower(txd.textures[i].name) == toLower(want) || std::to_string(i) == want) { pick = &txd.textures[i]; break; }
    }
    if (!pick) {
        std::printf("textures in %s:", argv[2]);
        for (const TxdTexture& t : txd.textures) std::printf(" %s", t.name.c_str());
        std::printf("\n");
        return 1;
    }
    std::ofstream(std::string(argv[4]), std::ios::binary).write(reinterpret_cast<const char*>(pick->rgba.data()), pick->rgba.size());
    std::ofstream(std::string(argv[4]) + ".txt") << pick->width << " " << pick->height << "\n";
    std::printf("%s: %ux%u depth %u paletted %d alpha %d\n", pick->name.c_str(), pick->width, pick->height, pick->depth, pick->paletted, pick->hasAlpha);
    return 0;
}
