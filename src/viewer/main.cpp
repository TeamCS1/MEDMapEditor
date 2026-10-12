// 3D map viewer for GTA III, built on med_core. Win32 + fixed-function OpenGL, like MEd itself.
//   med_viewer <GTA III folder>                     interactive
//   med_viewer <GTA III folder> --shot out.bmp [--bookmark n] [--pos x y z] [--h h] [--yaw deg]
//                                [--pitch deg] [--size w h] [--select x y] [--move dx dy dz]
//                                [--lod-near n] [--no-lod] [--only a,b] [--benchmark n] [--check] [--sink] [--delete] [--centre] [--undo n] [--redo n] [--save] [--in-place] [--save-to dir]
//                                                            render one frame to a BMP and exit
// Camera and controls follow MEd v0.32 (see camera.h and the help window text in MEd.exe):
//   W/S/A/D or cursor keys move, Ctrl+W/S or PgUp/PgDn (also R/F) up and down, Shift = faster,
//   left button drag rotates, Ctrl + left button drag moves.
//   Double-click selects an object (Shift+double-click adds or removes), Esc cancels the selection.
//   Alt + left button drag, or Alt + cursor keys / W/A/S/D, moves the selected objects;
//   Alt+Ctrl+cursor or Alt+PgUp/PgDn moves them up and down.
//   Del "deletes" the selected objects by moving them (and their LOD partners) to z = -200, below the
//   map, because removing a placement from the IPL file crashes the game. Shift+Del removes them from the
//   file for real (together with their LOD partners). Right-click opens MEd's object menu with both.
//   Space centres the camera on the selected object.
//   Ctrl+Z undoes and Ctrl+Y redoes (MEd has toolbar buttons only); the last 20 steps are kept
//   (MEd keeps 5). One step is everything done to one selection.
//   Mouse wheel moves the camera forward and back (Shift = faster).
//   Far stand-ins (LOD models) are drawn only beyond a minimum distance that the Scene panel can change, and a
//   LOD that has a building partner waits until that building is out of range, so they never overlap the
//   real models (see isFarModel and collectVisible).
//   The Scene panel on the left lists the IPL files ("regions": Portland, Staunton Island, Shoreside Vale, ...) with a
//   check box each, like MEd's Scene tab: only checked regions are drawn and picked, which saves frame rate.
//   The panel on the right shows the selected object (name, id, IDE data, file, position, rotation, LOD partner,
//   collision) and lets you type a new position or move it by 10 points, like MEd's object properties.
//   The toolbar has Save All, Undo, Redo, Centre on Object, Delete and Check Map. F7 also runs Check Map: a
//   wizard window that lists everything the game would skip, reject or choke on in the map as it would be
//   saved (see mapcheck.h), with a "Go to" button for each problem.
//   Ctrl+S saves the changed IPL files. By default they go to a separate folder (MEd_saved, next to the
//   game folder) and the game install is never touched; --in-place overwrites the install instead,
//   after copying each original to <file>.med-backup.
// Extra viewer keys (not in MEd): F2 toggles the quaternion convention, F3 cycles vertex colours.
#include <windows.h>
#include <shlobj.h>
#include <commctrl.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <string>
#include <vector>
#include "camera.h"
#include "col.h"
#include "dff.h"
#include "history.h"
#include "ide.h"
#include "img.h"
#include "ipl.h"
#include "mapcheck.h"
#include "pools.h"
#include "extract.h"
#include "original.h"
#include "roadkit.h"
#include "importer.h"
#include "coledit.h"
#include "paths.h"
#include "bookmarks.h"
#include "region.h"
#include "rotation.h"
#include "poollimits.h"
#include "menu.h"
#include "raycast.h"
#include "textutil.h"
#include "txd.h"

using namespace med;
namespace fs = std::filesystem;


namespace {

std::string slurp(const std::string& path, bool& ok) {
    std::ifstream f(path, std::ios::binary);
    ok = static_cast<bool>(f);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

std::string fixSlashes(std::string s) {
    for (char& c : s) if (c == '\\') c = '/';
    return s;
}

std::string ideLineOf(const IdeObject& o);  // Extract Object block

struct ModelDef {
    std::string model, txd;
    std::string name;  // the model name as written in the IDE (model is lower case)
    std::string ide;   // the IDE file that defines it (file name only)
    std::string ideLine;  // its line in that IDE file, rebuilt from the parsed values (for Extract's info file)
    bool timed = false;   // a tobj entry
    double drawDistance = 300;
    Pool pool = Pool::Building;  // which game pool a placement of this model takes (pools.h)
    bool farModel = false;  // a LOD stand-in: named LOD..., or an IDE draw distance of 500 or more
};

// In the GTA III data the LOD models have draw distances from 600 to 3000 and everything else is below
// 300 (measured on the original IDE files), and 919 of them are named LOD..., plus the island meshes
// (IslandLOD...). A name alone misses stand-ins such as the island meshes, and they were drawn next to the
// camera on top of the real terrain.
bool isFarModel(const std::string& model, double drawDistance) {
    std::string m = model;
    for (char& c : m) c = char(std::tolower(static_cast<unsigned char>(c)));
    return m.compare(0, 3, "lod") == 0 || m.find("islandlod") != std::string::npos || drawDistance >= 500;
}

// A model is kept as vertex arrays, drawn with one glDrawElements call per material. (It used to be one
// display list built from a glVertex call per vertex; this driver runs those slowly: about 50 ms for 40,000
// triangles, independent of the window size.)
struct GlBatch {
    GLuint tex = 0;
    uint8_t color[4] = {255, 255, 255, 255};
    int first = 0;  // first index
    int count = 0;  // number of indices
};

struct GlGeometry {
    float matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};  // the atomic's frame
    std::vector<float> pos, nrm, uv;
    std::vector<uint8_t> col;  // per-vertex colours, only when the baked colours are switched on (F3)
    std::vector<GLuint> idx;
    std::vector<GlBatch> batches;
};

struct GlModel {
    GLuint list = 0;  // unused now, kept for the F3 reset code
    float radius = 1;
    bool valid = false;
    int triangles = 0;  // for the benchmark
    int materials = 0;
    std::vector<GlGeometry> geoms;
};

// The whole editable document: what MEd snapshots for undo (all placements and which files changed).
struct DocState {
    std::vector<IplInstance> instances;
    std::vector<int> instFile;  // for each instance, the index of its IPL file (changes when objects are deleted)
    std::set<int> dirty;
};

struct App {
    std::string root;
    ImgArchive img;
    std::map<int, ModelDef> defs;
    std::vector<std::string> colFiles;  // the COL files of gta3.dat, full paths, in order (Extract Object copies from them)
    std::vector<Bookmark> bookmarks;  // the Bookmarks window's list (MEd's format)
    std::string bookmarkFile;         // where it is saved: bookmarks.ini next to the editor
    PoolLimits limits;                     // pool sizes the game can really have: vanilla, or from a limit adjuster file in the game folder
    std::map<int, PathModelData> pathData; // the IDE path blocks (traffic and pedestrian paths of road pieces)
    std::set<int> dynamicIds;              // models listed in object.dat (they never become road objects)
    PathTableLimits pathLimits;            // the game's path tables: original, or from the III.PathLimits plugin's ini when it is in the game folder
    bool forceVanilla = false;             // --vanilla: ignore the adjuster file
    std::vector<IplInstance> instances;
    std::map<std::string, ColModel> cols;  // collision bounds by model name (used for the selection box)
    std::vector<int> visible;              // instances drawn in the last frame (also the picking candidates)
    std::vector<int> selected;             // indices into instances
    double lastPick[3] = {0, 0, 0};        // last un-projected mouse position while dragging objects
    bool movingObjects = false;
    UndoHistory<DocState> history;  // last 20 steps (MEd: 5)
    std::vector<std::string> iplPaths;     // IPL files that were loaded, relative to the game folder
    std::vector<int> instFile;             // for each instance, the index into iplPaths
    std::set<int> dirty;                   // IPL files with moved objects
    std::string saveRoot;                  // where Ctrl+S writes (unless saveInPlace)
    bool saveInPlace = false;
    bool backupTarget = false;             // Save as Mod: copy an existing file in the target folder to <file>.med-backup first
    HWND menubar = nullptr;   // top row: File, Save All, Reference
    HWND toolbar = nullptr;   // second row: the editor tools
    int toolbarH = 0;  // height of both toolbar rows together; the 3D view is the rest of the window
    int menubarH = 0;  // height of the top row
    HWND inspector = nullptr;
    int inspectorW = 0;  // width of the properties panel on the right
    HWND scenePanel = nullptr;
    HWND leftTabs = nullptr;   // Scene | Placements tabs above the left panels
    int leftTabH = 0;
    int leftW = 0;                 // width of the Scene panel on the left
    int viewX = 0;                 // the 3D view starts here (window coordinates)
    std::vector<char> regionOn;    // per IPL file: drawn and pickable
    std::vector<char> regionHideable;  // per IPL file: empty when the map was loaded (GTA3.ZON, CULL.IPL): hidden from the lists unless View > Show hidden regions
    bool showEmptyRegions = false;     // View > Show hidden regions (off at start)
    bool showPaths = false;            // View > Show traffic paths (--paths)
    bool showLod = true;                 // draw the far stand-ins at all
    std::vector<IplInstance> clipboard;  // Copy: the copied placements
    size_t clipboardAnchor = 0;          // the one that lands on the mouse point when pasting (the last copied selected placement)
    bool lodView = false;                // LOD view: show the LOD stand-in of the selected object instead of the object
    std::string lodViewText;             // what LOD view is doing right now, for the window title
    float lodNear = 350;                 // ... but not closer than this many units
    std::vector<float> lodPartnerMin;    // per instance: the draw distance of its building partner (a LOD waits for it), or 0
    double msCollect = 0, msSubmit = 0;  // time spent choosing and submitting objects, summed over frames (benchmark)
    std::map<std::string, GlModel> models;                            // by lower-case model name
    std::map<std::string, std::map<std::string, GLuint>> textures;   // txd -> texture name -> GL id
    std::set<std::string> loadedTxds;
    MedCamera cam;
    bool conjugate = true;  // GTA III stores the inverse rotation quaternion
    int prelit = 0;         // vertex colours: 0 ignore, 1 as stored, 2 doubled (the game brightens them)
    int width = 1280, height = 720;
    int drawn = 0;
    bool keys[256] = {};
    bool dragging = false;
    POINT lastMouse = {0, 0};
    POINT downMouse = {0, 0};
    float farExtra = 0;      // --far n: every draw distance n longer (overview screenshots)
    bool roadsOnly = false;  // --roads-only: draw road pieces only (screenshots)  // where the left button went down (a click without a drag opens a road node)
    DWORD lastTick = 0;
    double tickRemainder = 0;  // milliseconds not yet turned into 1 ms keyboard ticks
    HWND hwnd = nullptr;
    HDC dc = nullptr;
    HGLRC rc = nullptr;
} app;

// bookmarks.ini lines look like
//   <bookmark id=0>name</bookmark><location id=0>x:y:z:h:yaw:pitch:flag</location>
// ---- loading -----------------------------------------------------------------------------

std::vector<bool> originalFlags();
struct ModelViewer;
ModelViewer* openDffViewer(HWND owner, int id);
ModelViewer* openColViewer(HWND owner, int id);
struct TxdViewer;
TxdViewer* openTxdViewer(HWND owner, const std::string& txdName, const std::string& file = "");
void updateTxdViewers();
void updateViewers();
const std::vector<RoadEnd>& roadEndsOf(int id);
bool isRoadPiece(int idx);
bool roadEndJoined(int idx, const RoadEnd& w);

bool loadGameData() {
    std::string err;
    {  // a limit adjuster file in the game folder says how big the game's pools really are; the editor still shows the original sizes
        const std::string rel = findLimitAdjusterIni(app.root);
        bool read = false;
        if (!rel.empty() && !app.forceVanilla) {
            PoolLimits l;
            if (parseLimitAdjusterIni(slurp(app.root + "/" + rel, read), l) && read) {
                l.source = rel;
                app.limits = l;
            }
        }
    }
    if (!ImgArchive::open(app.root + "/models/gta3.img", app.img, err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return false;
    }
    bool ok;
    std::string dat = slurp(app.root + "/data/gta3.dat", ok);
    if (!ok) {
        std::fprintf(stderr, "cannot read data/gta3.dat\n");
        return false;
    }
    const std::set<std::string> objectDat = parseObjectDatNames(slurp(app.root + "/data/object.dat", ok));
    std::set<int> pathIds;
    std::vector<std::string> ides, ipls, colPaths;
    size_t pos = 0;
    while (pos < dat.size()) {
        size_t e = dat.find('\n', pos);
        std::string line = trim(dat.substr(pos, e == std::string::npos ? std::string::npos : e - pos));
        pos = e == std::string::npos ? dat.size() : e + 1;
        if (isBlankOrComment(line)) continue;
        size_t sp = line.find_first_of(" \t");
        if (sp == std::string::npos) continue;
        std::string kind = toLower(line.substr(0, sp)), path = fixSlashes(trim(line.substr(sp + 1)));
        if (kind == "ide") ides.push_back(path);
        if (kind == "ipl") ipls.push_back(path);
        if (kind == "colfile") {  // "COLFILE <slot> <path>"
            size_t sp2 = path.find_first_of(" \t");
            if (sp2 != std::string::npos) colPaths.push_back(fixSlashes(trim(path.substr(sp2 + 1))));
        }
    }
    for (const std::string& p : ides) {
        std::string text = slurp(app.root + "/" + p, ok);
        if (!ok) continue;
        IdeFile f = parseIde(text, GameVersion::GTA3);
        collectPathModelIds(f, pathIds);
        collectIdePaths(text, app.pathData);
        for (const IdeObject& o : f.objects) {
            ModelDef d;
            d.model = toLower(o.model);
            d.name = o.model;
            d.ide = p.substr(p.find_last_of('/') + 1);
            d.ideLine = ideLineOf(o);
            d.timed = o.timed;
            d.txd = toLower(o.txd);
            d.drawDistance = o.drawDistances.empty() ? 300 : *std::max_element(o.drawDistances.begin(), o.drawDistances.end());
            d.farModel = isFarModel(d.model, d.drawDistance);
            app.defs[o.id] = d;
        }
    }
    for (auto& kv : app.defs) kv.second.pool = poolFor(objectDat.count(kv.second.model) > 0, pathIds.count(kv.first) > 0);
    for (const auto& kv : app.defs)
        if (objectDat.count(kv.second.model)) app.dynamicIds.insert(kv.first);
    app.pathLimits = PathTableLimits();
    if (readPathLimitsPlugin(app.root, app.pathLimits) && !app.forceVanilla)
        std::printf("path limits from %s: %d nodes, %d car path links, %d connections\n", app.pathLimits.source.c_str(), app.pathLimits.nodes, app.pathLimits.carLinks,
                    app.pathLimits.connections);
    else app.pathLimits = PathTableLimits();
    for (const std::string& p : ipls) {
        std::string text = slurp(app.root + "/" + p, ok);
        if (!ok) continue;
        IplFile f = parseTextIpl(text, GameVersion::GTA3);
        app.instances.insert(app.instances.end(), f.instances.begin(), f.instances.end());
        app.instFile.insert(app.instFile.end(), f.instances.size(), int(app.iplPaths.size()));
        app.iplPaths.push_back(p);
    }
    for (const std::string& cp : colPaths) {
        app.colFiles.push_back(app.root + "/" + cp);
        std::string bytes = slurp(app.root + "/" + cp, ok);
        if (!ok) continue;
        std::vector<ColModel> models;
        std::string cerr;
        if (!parseCol(std::vector<unsigned char>(bytes.begin(), bytes.end()), models, cerr)) continue;
        for (ColModel& m : models) app.cols[toLower(m.name)] = std::move(m);  // a later file wins, as in the game (0x00478b20)
    }
    std::printf("loaded %zu collision models\n", app.cols.size());
    std::printf("loaded %zu object definitions, %zu placements, %zu archive entries\n", app.defs.size(),
                app.instances.size(), app.img.entries().size());
    const std::vector<bool> orig = originalFlags();
    std::printf("placements: %zu original, %zu added\n", size_t(std::count(orig.begin(), orig.end(), true)), size_t(std::count(orig.begin(), orig.end(), false)));
    return true;
}

// ---- textures ----------------------------------------------------------------------------

void uploadTxd(const std::string& txdName, const TxdFile& txd) {
    auto& table = app.textures[txdName];
    for (const TxdTexture& t : txd.textures) {
        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        gluBuild2DMipmaps(GL_TEXTURE_2D, 4, t.width, t.height, GL_RGBA, GL_UNSIGNED_BYTE, t.rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        table[toLower(t.name)] = id;
    }
}

void ensureTxd(const std::string& txdName) {
    if (app.loadedTxds.count(txdName)) return;
    app.loadedTxds.insert(txdName);
    std::vector<unsigned char> data;
    std::string err;
    if (txdName == "generic") {  // shared textures live outside the archive
        bool ok;
        std::string s = slurp(app.root + "/models/generic.txd", ok);
        if (!ok) return;
        data.assign(s.begin(), s.end());
    } else if (!app.img.extract(txdName + ".txd", data, err)) {
        return;
    }
    TxdFile txd;
    if (parseTxd(data, txd, err)) uploadTxd(txdName, txd);
}

GLuint findTexture(const std::string& txdName, const std::string& tex) {
    std::string t = toLower(tex);
    for (const std::string& n : {txdName, std::string("generic")}) {
        ensureTxd(n);
        auto it = app.textures.find(n);
        if (it == app.textures.end()) continue;
        auto f = it->second.find(t);
        if (f != it->second.end()) return f->second;
    }
    return 0;
}

// ---- models ------------------------------------------------------------------------------

// 4x4 column-major matrix of a frame in the model's local space (RenderWare stores right, up, at, pos).
void frameMatrix(const DffFrame& f, float* m) {
    for (int i = 0; i < 3; ++i) {
        m[i] = f.rotation[i];
        m[4 + i] = f.rotation[3 + i];
        m[8 + i] = f.rotation[6 + i];
        m[12 + i] = f.position[i];
    }
    m[3] = m[7] = m[11] = 0;
    m[15] = 1;
}

void mul(const float* a, const float* b, float* out) {  // out = a * b, column-major
    float r[16];
    for (int c = 0; c < 4; ++c)
        for (int rI = 0; rI < 4; ++rI) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a[k * 4 + rI] * b[c * 4 + k];
            r[c * 4 + rI] = s;
        }
    std::copy(r, r + 16, out);
}

void frameWorld(const DffModel& m, int index, float* out) {
    float local[16];
    frameMatrix(m.frames[index], local);
    int parent = m.frames[index].parent;
    if (parent >= 0 && parent < static_cast<int>(m.frames.size()) && parent != index) {
        float p[16];
        frameWorld(m, parent, p);
        mul(p, local, out);
    } else {
        std::copy(local, local + 16, out);
    }
}

void buildGeometry(const DffGeometry& g, const std::string& txdName, GlGeometry& out) {
    out.pos = g.positions;
    out.nrm = g.normals;
    out.uv = g.uvs;
    const bool prelit = !g.colors.empty() && app.prelit != 0;
    const int mulc = app.prelit == 2 ? 2 : 1;
    if (prelit) {
        out.col.resize(g.colors.size());
        for (size_t i = 0; i < g.colors.size(); ++i) out.col[i] = (i % 4 == 3) ? g.colors[i] : uint8_t(std::min<int>(255, g.colors[i] * mulc));
    }
    const size_t mats = std::max<size_t>(g.materials.size(), 1);
    for (size_t mi = 0; mi < mats; ++mi) {
        GlBatch b;
        b.first = int(out.idx.size());
        if (mi < g.materials.size()) {
            const DffMaterial& m = g.materials[mi];
            std::memcpy(b.color, &m.color, 4);
            if (m.textured && !m.texture.empty()) b.tex = findTexture(txdName, m.texture);
        }
        for (size_t t = 0; t < g.triangleMaterial.size(); ++t) {
            if (g.triangleMaterial[t] != mi) continue;
            for (int k = 0; k < 3; ++k) out.idx.push_back(g.triangles[t * 3 + k]);
        }
        b.count = int(out.idx.size()) - b.first;
        if (b.count > 0) out.batches.push_back(b);
    }
}

// Draw a model: set the arrays up once per geometry, then one glDrawElements per material.
void drawModel(const GlModel& m) {
    for (const GlGeometry& g : m.geoms) {
        glPushMatrix();
        glMultMatrixf(g.matrix);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, g.pos.data());
        if (!g.nrm.empty()) {
            glEnableClientState(GL_NORMAL_ARRAY);
            glNormalPointer(GL_FLOAT, 0, g.nrm.data());
        } else {
            glDisableClientState(GL_NORMAL_ARRAY);
            glNormal3f(0, 0, 1);
        }
        if (!g.uv.empty()) {
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, 0, g.uv.data());
        } else {
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        }
        if (!g.col.empty()) {
            glEnableClientState(GL_COLOR_ARRAY);
            glColorPointer(4, GL_UNSIGNED_BYTE, 0, g.col.data());
        } else {
            glDisableClientState(GL_COLOR_ARRAY);
        }
        for (const GlBatch& b : g.batches) {
            if (b.tex) {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, b.tex);
            } else {
                glDisable(GL_TEXTURE_2D);
            }
            if (g.col.empty()) glColor4ubv(b.color);
            glDrawElements(GL_TRIANGLES, b.count, GL_UNSIGNED_INT, g.idx.data() + b.first);
        }
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        glPopMatrix();
    }
}

// A GL model from a parsed DFF; its textures come from the dictionary `txdName` (a game TXD, or a loose file's key).
void buildGlModel(GlModel& gm, const DffModel& dff, const std::string& txdName) {
    float radius = 0;
    for (const DffAtomic& a : dff.atomics) {
        if (a.geometry < 0 || a.geometry >= static_cast<int>(dff.geometries.size())) continue;
        float w[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        if (a.frame >= 0 && a.frame < static_cast<int>(dff.frames.size())) frameWorld(dff, a.frame, w);
        const DffGeometry& g = dff.geometries[a.geometry];
        gm.triangles += int(g.triangles.size() / 3);
        gm.materials += int(g.materials.size());
        gm.geoms.emplace_back();
        std::copy(w, w + 16, gm.geoms.back().matrix);
        buildGeometry(g, txdName, gm.geoms.back());
        float c = std::sqrt(g.boundingSphere[0] * g.boundingSphere[0] + g.boundingSphere[1] * g.boundingSphere[1] +
                            g.boundingSphere[2] * g.boundingSphere[2]);
        float p = std::sqrt(w[12] * w[12] + w[13] * w[13] + w[14] * w[14]);
        radius = std::max(radius, c + g.boundingSphere[3] + p);
    }
    gm.radius = std::max(radius, 1.0f);
    gm.valid = true;
}

GlModel& getModel(const std::string& name, const std::string& txdName) {
    auto it = app.models.find(name);
    if (it != app.models.end()) return it->second;
    GlModel& gm = app.models[name];
    std::vector<unsigned char> data;
    std::string err;
    DffModel dff;
    if (!app.img.extract(name + ".dff", data, err) || !parseDff(data, dff, err)) return gm;
    buildGlModel(gm, dff, txdName);
    return gm;
}

// ---- rendering ---------------------------------------------------------------------------

void quatToMatrix(double x, double y, double z, double w, float* m) {
    double n = std::sqrt(x * x + y * y + z * z + w * w);
    if (n > 0) { x /= n; y /= n; z /= n; w /= n; }
    m[0] = float(1 - 2 * (y * y + z * z)); m[1] = float(2 * (x * y + z * w));     m[2] = float(2 * (x * z - y * w));     m[3] = 0;
    m[4] = float(2 * (x * y - z * w));     m[5] = float(1 - 2 * (x * x + z * z)); m[6] = float(2 * (y * z + x * w));     m[7] = 0;
    m[8] = float(2 * (x * z + y * w));     m[9] = float(2 * (y * z - x * w));     m[10] = float(1 - 2 * (x * x + y * y)); m[11] = 0;
    m[12] = m[13] = m[14] = 0; m[15] = 1;
}

struct ViewInfo {
    double eyeX, eyeY, eyeZ, fx, fy, fz;
};

// Where the camera is and which way it faces. The eye sits at (x, y - 20, z + 20 - h).
ViewInfo viewInfo() {
    const double yawR = app.cam.yaw * 0.0174532925, pitchR = app.cam.pitch * 0.0174532925;
    return {app.cam.x, app.cam.y - 20.0, app.cam.z + 20.0 - app.cam.h, std::sin(yawR) * std::cos(pitchR),
            std::cos(yawR) * std::cos(pitchR), -std::sin(pitchR)};
}

// The placements to draw: within their IDE draw distance, in front of the camera, with a loadable
// model, and not far LOD stand-ins. (MEd draws every placement of the visible files; this keeps the
// frame rate up and also defines what can be picked.)
std::vector<int> withLodPartners(const std::vector<int>& selection);

void collectVisible() {
    app.visible.clear();
    const ViewInfo v = viewInfo();
    // LOD view: for the selected placements and their LOD partners, the LOD stand-in is drawn (at any distance) and the
    // normal model is hidden, so you can see what the far version of the selected object looks like.
    std::set<int> group;
    bool swap = false;
    app.lodViewText.clear();
    if (app.lodView && !app.selected.empty()) {
        for (int idx : withLodPartners(app.selected)) group.insert(idx);
        for (int idx : group) {
            auto d = app.defs.find(app.instances[idx].id);
            if (d != app.defs.end() && d->second.farModel) swap = true;
        }
        app.lodViewText = swap ? "LOD view: showing the LOD stand-in" : "LOD view: the selection has no LOD version";
    }
    for (size_t i = 0; i < app.instances.size(); ++i) {
        const IplInstance& inst = app.instances[i];
        auto d = app.defs.find(inst.id);
        if (d == app.defs.end()) continue;
        const ModelDef& def = d->second;
        if (!app.regionOn.empty() && !app.regionOn[app.instFile[i]]) continue;  // its region is switched off in the Scene panel
        if (app.roadsOnly && def.model.rfind("rd_", 0) != 0) continue;           // --roads-only (screenshots of road layouts)
        const bool swapped = swap && group.count(int(i)) > 0;
        if (swapped && !def.farModel) continue;  // LOD view: the normal model gives way to its stand-in
        double dx = inst.x - v.eyeX, dy = inst.y - v.eyeY, dz = inst.z - v.eyeZ;
        double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!swapped && dist > def.drawDistance + 20 + app.farExtra) continue;
        if (def.farModel && !swapped) {  // a LOD stand-in appears only beyond a minimum distance, and after its building has dropped out of range
            if (!app.showLod) continue;
            const double partner = i < app.lodPartnerMin.size() ? app.lodPartnerMin[i] : 0;
            if (dist < std::max<double>(app.lodNear, partner)) continue;
        }
        double ahead = dx * v.fx + dy * v.fy + dz * v.fz;
        if (ahead < -60) continue;  // behind the camera (generous: the bounding radius is not known before loading)
        GlModel& gm = getModel(def.model, def.txd);
        if (!gm.valid) continue;
        double scale = std::max(inst.scaleX, std::max(inst.scaleY, inst.scaleZ));
        if (ahead < -gm.radius * scale) continue;
        app.visible.push_back(int(i));
    }
}

// glTranslatef(position) then the inverse rotation of the quaternion, as in CMapView::paintGL.
void applyPlacement(const IplInstance& inst) {
    glTranslated(inst.x, inst.y, inst.z);
    float rot[16];
    double s = app.conjugate ? -1 : 1;
    quatToMatrix(s * inst.rotX, s * inst.rotY, s * inst.rotZ, inst.rotW, rot);
    glMultMatrixf(rot);
}

// View matrices only (no light), identical to the ones paintGL builds. pickX < 0 means no pick region.
void setViewMatrices(double pickX, double pickY) {
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (pickX >= 0) gluPickMatrix(pickX, vp[3] - pickY, 3.0, 3.0, vp);  // MEd picks in a 3 x 3 pixel region
    gluPerspective(45.0, double(app.width) / app.height, 1.0, 8000.0);
    glRotatef(app.cam.pitch, 1, 0, 0);
    glRotatef(app.cam.yaw, 0, 1, 0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(app.cam.x, app.cam.y - 20, app.cam.z, app.cam.x, app.cam.y, app.cam.z, 0, 0, 1);
    glTranslatef(0, 0, app.cam.h - 20);
}

struct PickRay {
    double o[3], d[3];
    int dx = 0, dy = 0;  // offset from the cursor, in pixels
};

// What one ray hits first.
struct PickHit {
    int idx = -1;        // placement, -1 for nothing
    double t = 1e300;    // distance along the ray (the same scale for every placement)
    double radius = 0;   // bounding radius of the model
    int dx = 0, dy = 0;
};

// The nearest placement under each ray (the same ray-against-triangles test as before, one result per ray).
std::vector<PickHit> pickCollect(const std::vector<PickRay>& rays) {
    std::vector<PickHit> hits(rays.size());
    for (size_t k = 0; k < rays.size(); ++k) {
        hits[k].dx = rays[k].dx;
        hits[k].dy = rays[k].dy;
    }
    for (int idx : app.visible) {
        const IplInstance& inst = app.instances[idx];
        auto d = app.defs.find(inst.id);
        if (d == app.defs.end()) continue;
        auto mi = app.models.find(d->second.model);
        if (mi == app.models.end() || !mi->second.valid) continue;
        const GlModel& gm = mi->second;
        float rot[16];
        const double sg = app.conjugate ? -1 : 1;
        quatToMatrix(sg * inst.rotX, sg * inst.rotY, sg * inst.rotZ, inst.rotW, rot);  // the same rotation applyPlacement uses
        const double sc[3] = {inst.scaleX != 0 ? inst.scaleX : 1, inst.scaleY != 0 ? inst.scaleY : 1, inst.scaleZ != 0 ? inst.scaleZ : 1};
        // world -> placement space: undo the translation, the rotation (transpose) and the scale
        auto toLocal = [&](const double v[3], bool point, double out[3]) {
            const double u[3] = {point ? v[0] - inst.x : v[0], point ? v[1] - inst.y : v[1], point ? v[2] - inst.z : v[2]};
            for (int c = 0; c < 3; ++c) out[c] = (rot[c * 4] * u[0] + rot[c * 4 + 1] * u[1] + rot[c * 4 + 2] * u[2]) / sc[c];
        };
        for (size_t k = 0; k < rays.size(); ++k) {
            const PickRay& r = rays[k];
            double o[3], dir[3];
            toLocal(r.o, true, o);
            toLocal(r.d, false, dir);
            const double dd = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
            if (dd < 1e-18) continue;
            const double t0 = -(o[0] * dir[0] + o[1] * dir[1] + o[2] * dir[2]) / dd;  // closest approach to the model origin
            const double cx = o[0] + t0 * dir[0], cy = o[1] + t0 * dir[1], cz = o[2] + t0 * dir[2];
            if (std::sqrt(cx * cx + cy * cy + cz * cz) > gm.radius) continue;  // the ray misses the model's bounding sphere
            for (const GlGeometry& g : gm.geoms) {
                float inv[16];
                if (!invertAffine(g.matrix, inv)) continue;
                double go[3], gd[3];
                transformPoint(inv, o, go);
                transformDirection(inv, dir, gd);
                for (size_t i = 0; i + 2 < g.idx.size(); i += 3) {
                    double t;
                    if (rayTriangle(go, gd, &g.pos[g.idx[i] * 3], &g.pos[g.idx[i + 1] * 3], &g.pos[g.idx[i + 2] * 3], t) && t < hits[k].t) {
                        hits[k].t = t;
                        hits[k].idx = idx;
                        hits[k].radius = gm.radius;
                    }
                }
            }
        }
    }
    return hits;
}

// Picking, replacing MEd's OpenGL selection mode (3 x 3 pixel region, nearest hit wins), which took 11 seconds per pick
// with vertex arrays on the user's driver. Rays are cast on the CPU against every candidate placement's triangles
// (raycast.h). Returns an instance index or -1. (mx, my) are window x and the y from the top of the 3D view.
//
// Same as MEd: nine rays in the 3 x 3 pixels around the cursor, the nearest hit wins. Added: a small thing (traffic
// cone, bin, hydrant: bounding radius up to kSmallPickRadius) is only a few pixels wide, and clicking beside it hits
// the large ground or wall behind it, so when the nearest hit is not small, a small placement hit by a wider grid of
// rays (5 x 5 rays 3 pixels apart, up to 6 pixels away) is taken instead, as long as it is not hidden behind the
// nearest hit. Clicking clear ground still picks the ground.
constexpr double kSmallPickRadius = 4.0;

// A grid of (2 * half + 1)^2 rays through the 3D view around the pixel (mx, my), `step` pixels apart (my from the top of the view).
std::vector<PickRay> rayGrid(int mx, int my, int half, int step) {
    glViewport(app.viewX, 0, app.width, app.height);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    setViewMatrices(-1, -1);
    GLdouble proj[16], model[16];
    GLint vp[4];
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetIntegerv(GL_VIEWPORT, vp);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    std::vector<PickRay> rays;
    for (int dy = -half; dy <= half; ++dy)
        for (int dx = -half; dx <= half; ++dx) {
            const double wy = (vp[3] - (my + dy * step)) - 1.0;
            double n[3], f[3];
            if (gluUnProject(mx + dx * step, wy, 0.0, model, proj, vp, &n[0], &n[1], &n[2]) != GL_TRUE) continue;
            if (gluUnProject(mx + dx * step, wy, 1.0, model, proj, vp, &f[0], &f[1], &f[2]) != GL_TRUE) continue;
            PickRay r;
            for (int k = 0; k < 3; ++k) { r.o[k] = n[k]; r.d[k] = f[k] - n[k]; }
            r.dx = dx * step;
            r.dy = dy * step;
            rays.push_back(r);
        }
    return rays;
}

// The point on the nearest drawn surface under the pixel (mx, my), as the depth buffer would give it. False when the
// ray hits nothing. (The hit distance t is along the ray in world units because the placement transform is affine.)
bool pickPoint(int mx, int my, double out[3]) {
    const std::vector<PickRay> rays = rayGrid(mx, my, 0, 1);
    if (rays.empty()) return false;
    const std::vector<PickHit> hits = pickCollect(rays);
    if (hits.empty() || hits[0].idx < 0) return false;
    for (int k = 0; k < 3; ++k) out[k] = rays[0].o[k] + hits[0].t * rays[0].d[k];
    return true;
}

int pickInstance(int mx, int my) {
    auto makeRays = [&](int half, int step) { return rayGrid(mx, my, half, step); };

    PickHit nearest;
    for (const PickHit& h : pickCollect(makeRays(1, 1)))
        if (h.idx >= 0 && h.t < nearest.t) nearest = h;
    if (nearest.idx >= 0 && nearest.radius <= kSmallPickRadius) return nearest.idx;  // a small thing right under the cursor

    // a small placement a few pixels away, in front of or level with whatever is under the cursor
    const PickHit* small = nullptr;
    const std::vector<PickHit> wide = pickCollect(makeRays(2, 3));
    for (const PickHit& h : wide) {
        if (h.idx < 0 || h.radius > kSmallPickRadius) continue;
        if (nearest.idx >= 0 && h.t > nearest.t * 1.15) continue;  // hidden behind the nearest hit
        if (!small || std::abs(h.dx) + std::abs(h.dy) < std::abs(small->dx) + std::abs(small->dy) ||
            (std::abs(h.dx) + std::abs(h.dy) == std::abs(small->dx) + std::abs(small->dy) && h.t < small->t))
            small = &h;
    }
    return small ? small->idx : nearest.idx;
}


// Double-click, as in CMapView::mouseDoubleClickEvent (0x0045e2a0): nothing under the cursor clears
// the selection; without Shift, picking the first selected object deselects everything, otherwise the
// pick replaces the selection; with Shift the pick is added or removed.
void doubleClickSelect(int mx, int my, bool shift) {
    app.history.endSession();  // a different selection starts a new undo step
    LARGE_INTEGER pa, pb, pf;
    QueryPerformanceFrequency(&pf);
    QueryPerformanceCounter(&pa);
    int hit = pickInstance(mx, my);
    QueryPerformanceCounter(&pb);
    if (std::getenv("MED_DEBUG")) { std::printf("pick took %.1f ms\n", 1000.0 * double(pb.QuadPart - pa.QuadPart) / double(pf.QuadPart)); std::fflush(stdout); }
    if (std::getenv("MED_DEBUG")) { std::printf("double-click at view (%d, %d) in a %d x %d view, %zu candidates -> hit %d\n", mx, my, app.width, app.height, app.visible.size(), hit); std::fflush(stdout); }
    if (hit < 0) {
        app.selected.clear();
        return;
    }
    if (!shift) {
        if (!app.selected.empty() && app.selected[0] == hit) {
            app.selected.clear();
            return;
        }
        app.selected.assign(1, hit);
        return;
    }
    auto it = std::find(app.selected.begin(), app.selected.end(), hit);
    if (it != app.selected.end()) app.selected.erase(it);
    else app.selected.push_back(hit);
}

// Mouse position -> point on the horizontal plane z - h (CMapView, 0x00459270), with the original's
// double-counted near point removed (see rayPlaneIntersection in camera.h).
bool unprojectToPlane(int mx, int my, double out[3]) {
    glViewport(app.viewX, 0, app.width, app.height);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    setViewMatrices(-1, -1);
    GLdouble proj[16], model[16];
    GLint vp[4];
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetIntegerv(GL_VIEWPORT, vp);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    const double wy = (vp[3] - my) - 1.0;
    double n[3], f[3];
    if (gluUnProject(mx, wy, 0.0, model, proj, vp, &n[0], &n[1], &n[2]) != GL_TRUE) return false;
    if (gluUnProject(mx, wy, 1.0, model, proj, vp, &f[0], &f[1], &f[2]) != GL_TRUE) return false;
    return rayPlaneIntersection(n, f, double(app.cam.z) - double(app.cam.h), out);
}

void buildLodPartnerRanges();
void plRefreshAfterEdit();  // the Placements tab: rows, counts and sort order follow the edits
void itRefreshAfterEdit();  // the Item tab: placed counts follow the edits
void showBookmarksWindow(HWND owner);
void replaceSelectedUi(HWND owner);
void extractObjectsUi(HWND owner, const std::vector<int>& modelIds);
std::vector<int> modelIdsOf(const std::vector<int>& placements);
void setShowEmptyRegions(bool on);
void refreshRegions();

DocState captureState() { return DocState{app.instances, app.instFile, app.dirty}; }

// Undo and redo, as in MEd's toolbar handlers (0x0044f8e0 and 0x0044f930): restore the document, then
// cancel the selection.
bool undoAction() {
    DocState cur = captureState();
    if (!app.history.undo(cur)) return false;
    app.instances = std::move(cur.instances);
    app.instFile = std::move(cur.instFile);
    app.dirty = std::move(cur.dirty);
    app.selected.clear();
    buildLodPartnerRanges();
    refreshRegions();
    return true;
}

bool redoAction() {
    DocState cur = captureState();
    if (!app.history.redo(cur)) return false;
    app.instances = std::move(cur.instances);
    app.instFile = std::move(cur.instFile);
    app.dirty = std::move(cur.dirty);
    app.selected.clear();
    buildLodPartnerRanges();
    refreshRegions();
    return true;
}

// The LOD partner(s) of one placement, paired one to one when copies share the spot (core pairedLodPartners): after a
// Duplicate the copy sits exactly on the original, and each building must take only its own stand-in along.
std::vector<size_t> lodPartnersOf(int idx) {
    std::vector<bool> flags;
    bool have = false;
    auto isOriginal = [&](size_t i) {
        if (!have) { flags = originalFlags(); have = true; }  // only needed when copies share a spot
        return i < flags.size() ? bool(flags[i]) : true;
    };
    return pairedLodPartners(app.instances, size_t(idx), isOriginal);
}

// The selection plus the LOD partner of each selected placement: a building and its LOD stand-in must
// always be moved or removed together, otherwise the game crashes (findLodPartners, ipl.h).
std::vector<int> withLodPartners(const std::vector<int>& selection) {
    std::set<int> all(selection.begin(), selection.end());
    for (int idx : selection)
        for (size_t partner : lodPartnersOf(idx)) all.insert(int(partner));
    return std::vector<int>(all.begin(), all.end());
}

// "Delete" without removing anything: put the selected placements and their LOD partners at
// z = -200 (kSunkZ), where the game keeps them harmlessly out of sight. Takes an undo snapshot first,
// like MEd's delete, and clears the selection. Returns how many placements were moved.
size_t sinkSelected() {
    if (app.selected.empty()) return 0;
    app.history.snapshot(captureState());
    const std::vector<int> all = withLodPartners(app.selected);
    for (int idx : all) {
        app.instances[idx].z = kSunkZ;
        app.dirty.insert(app.instFile[idx]);
    }
    app.selected.clear();
    app.history.endSession();
    plRefreshAfterEdit();
    return all.size();
}

// Duplicate, after MEd's CMapView::duplicateObject (slot 32 of the CMapView meta object, 0x00460290): the copy has the
// id, model name, position and rotation of the original and sits EXACTLY at its position (no offset); the file it is
// added to is marked changed; the copy becomes part of the selection so it can be moved straight away. MEd works on
// the one current object and adds the copy to the destination IPL chosen in its Placements window (it refuses with
// "Please select a destination IPL from the Placements window" when none is chosen, and with "You are attempting to
// paste an item to an IPL that is not visible..." when that file is hidden); this viewer copies into the file of the
// original, which is always a visible one. Differences on purpose: it works on the whole selection, it also copies each
// LOD partner (MEd does not) so a building and its stand-in stay a pair, and all the copies are selected (the two are
// moved together). One undo step. Every copy takes a slot of the game's pool of its model (core/pools.h).
size_t duplicateSelected() {
    if (app.selected.empty()) return 0;
    app.history.snapshot(captureState());
    const std::vector<int> all = withLodPartners(app.selected);
    std::map<int, int> copyOf;  // original index -> index of its copy
    for (int idx : all) {
        IplInstance c = app.instances[idx];
        copyOf[idx] = int(app.instances.size());
        app.instances.push_back(c);
        app.instFile.push_back(app.instFile[idx]);
        app.dirty.insert(app.instFile[idx]);
    }
    std::vector<int> fresh;
    for (int idx : all) fresh.push_back(copyOf[idx]);
    app.selected = fresh;
    app.history.endSession();
    buildLodPartnerRanges();  // the copies need their own LOD range
    refreshRegions();
    return all.size();
}

// Move the selected placements, and their LOD partners so a building and its stand-in stay in one file, into another
// region (IPL file). Both files are rewritten by the next save. One undo step. Returns how many placements moved.
// (MEd chooses a destination IPL in its Placements window for pasting; there is no move in MEd.)
size_t moveSelectedToRegion(int file) {
    if (app.selected.empty() || file < 0 || file >= int(app.iplPaths.size())) return 0;
    const std::vector<int> all = withLodPartners(app.selected);
    bool any = false;
    for (int idx : all) any = any || app.instFile[idx] != file;
    if (!any) return 0;
    app.history.snapshot(captureState());
    size_t moved = 0;
    for (int idx : all) {
        if (app.instFile[idx] == file) continue;
        app.dirty.insert(app.instFile[idx]);
        app.dirty.insert(file);
        app.instFile[idx] = file;
        ++moved;
    }
    app.history.endSession();
    refreshRegions();
    return moved;
}

// New object, after CMapView::newObject (slot 18, 0x00460dd0): a new placement of an IDE object (id and model name from
// the IDE) is added to the destination IPL file at a ground point of the view, with no rotation, scale 1, and becomes the
// selection; MEd then centres the view on it. One undo step. MEd takes the ground point under the mouse from the depth
// buffer and the height from the selected object (else the camera height); this one uses the surface hit by a ray, else
// the plane at the camera height.
int addNewObject(int id, int file, double x, double y, double z) {
    auto d = app.defs.find(id);
    if (d == app.defs.end() || file < 0 || file >= int(app.iplPaths.size())) return -1;
    app.history.snapshot(captureState());
    IplInstance o;
    o.id = id;
    o.model = d->second.name;
    o.x = x; o.y = y; o.z = z;
    app.instances.push_back(o);
    app.instFile.push_back(file);
    app.dirty.insert(file);
    const int idx = int(app.instances.size()) - 1;
    app.selected.assign(1, idx);
    app.history.endSession();
    buildLodPartnerRanges();
    refreshRegions();
    return idx;
}

// Copy, after CMapView::copyObject / copyObjects (slots 13 and 14, 0x00459d20 and 0x00459c80): MEd empties its clipboard
// list and puts the serial ids of the selected placements on it. This one keeps a real copy of each placement instead of an
// id, so a later delete or undo cannot break a paste, and it also takes the LOD partners (like Duplicate) so a building and
// its stand-in are pasted as a pair. The selection comes first, in selection order, then the partners; the anchor of a paste
// is the LAST selected placement (MEd's rule, see pasteClipboard).
size_t copySelected() {
    if (app.selected.empty()) return 0;
    std::vector<int> order = app.selected;
    for (int idx : withLodPartners(app.selected))
        if (std::find(order.begin(), order.end(), idx) == order.end()) order.push_back(idx);
    app.clipboard.clear();
    for (int idx : order) app.clipboard.push_back(app.instances[idx]);
    app.clipboardAnchor = app.selected.size() - 1;
    return app.clipboard.size();
}

// Paste, after CMapView::pasteObject (slot 16, 0x004613f0, read in full on 2026-10-12): it does nothing with an empty
// clipboard; needs a destination IPL that is visible; takes the point on the surface under the mouse; shifts every copy by
// (mouse x - x of the LAST copied placement, mouse y - y of it), so the last copied placement lands on the mouse point and
// the others keep their offsets from it; the HEIGHT IS NOT CHANGED (z stays as in the original, even when the ground is
// higher or lower there); clears the selection, takes an undo snapshot, adds each copy to the destination file with the id,
// name, position, rotation and LOD index of the original, marks the file changed, and selects each pasted placement.
// Here the last copied SELECTED placement is the anchor (the LOD partners that Copy adds come after it). One undo step.
size_t pasteClipboard(int file, double x, double y) {
    if (app.clipboard.empty() || file < 0 || file >= int(app.iplPaths.size())) return 0;
    app.history.snapshot(captureState());
    const IplInstance anchor = app.clipboard[std::min(app.clipboardAnchor, app.clipboard.size() - 1)];
    const double dx = x - anchor.x, dy = y - anchor.y;
    std::vector<int> fresh;
    for (const IplInstance& src : app.clipboard) {
        IplInstance c = src;
        c.x = src.x + dx;
        c.y = src.y + dy;
        app.instances.push_back(c);
        app.instFile.push_back(file);
        fresh.push_back(int(app.instances.size()) - 1);
    }
    app.dirty.insert(file);
    app.selected = fresh;
    app.history.endSession();
    buildLodPartnerRanges();
    refreshRegions();
    return fresh.size();
}

// Centre View on Object, as in CMapView (0x00459130): camera x = object x, y = object y - 20,
// z = object z + 20, yaw 0, pitch 45, h 0. (MEd also takes an undo snapshot here although nothing in the
// document changes, which wastes one of the undo steps; that is not copied.)
void centreOnObject(int idx) {
    const IplInstance& o = app.instances[idx];
    app.cam.x = float(o.x);
    app.cam.y = float(o.y - 20);
    app.cam.z = float(o.z + 20);
    app.cam.h = 0;
    app.cam.yaw = 0;
    app.cam.pitch = 45;
}

// Remove placements from their files for good (MEd's IPL_removeInstance, 0x00424490, per file), repairing the LOD links
// of each file. No undo snapshot and no selection handling: the callers do that. The selection indices above a removed
// placement must be shifted down by the caller (shiftAfterRemoval).
void removePlacements(std::vector<int> idxs) {
    std::sort(idxs.begin(), idxs.end(), std::greater<int>());  // highest first so lower indices stay valid
    idxs.erase(std::unique(idxs.begin(), idxs.end()), idxs.end());
    for (int idx : idxs) {
        if (idx < 0 || idx >= int(app.instances.size())) continue;
        const int file = app.instFile[idx];
        std::vector<int> members;
        std::vector<IplInstance> fileInsts;
        for (size_t i = 0; i < app.instances.size(); ++i)
            if (app.instFile[i] == file) {
                members.push_back(int(i));
                fileInsts.push_back(app.instances[i]);
            }
        const size_t local = size_t(std::find(members.begin(), members.end(), idx) - members.begin());
        removeIplInstance(fileInsts, local);
        app.instances.erase(app.instances.begin() + idx);
        app.instFile.erase(app.instFile.begin() + idx);
        // copy the repaired LOD links back onto the remaining members of that file, in order
        size_t k = 0;
        for (size_t i = 0; i < app.instances.size() && k < fileInsts.size(); ++i)
            if (app.instFile[i] == file) app.instances[i].lod = fileInsts[k++].lod;
        app.dirty.insert(file);
    }
}

// A list of placement indices after `removed` were taken out: removed ones dropped, the others shifted down.
std::vector<int> shiftAfterRemoval(const std::vector<int>& list, std::vector<int> removed) {
    std::sort(removed.begin(), removed.end());
    std::vector<int> out;
    for (int i : list) {
        if (std::binary_search(removed.begin(), removed.end(), i)) continue;
        out.push_back(i - int(std::lower_bound(removed.begin(), removed.end(), i) - removed.begin()));
    }
    return out;
}

// Delete, as in CMapView::deleteSelected (0x0045d0a0): do nothing without a selection; always take an
// undo snapshot first (not only once per selection); remove every selected placement from its IPL file,
// repairing the LOD links of that file (removeIplInstance); clear the selection. The file is rewritten
// on the next save.
bool deleteSelected() {
    if (app.selected.empty()) return false;
    app.history.snapshot(captureState());
    removePlacements(withLodPartners(app.selected));  // MEd leaves the LOD behind, which crashes the game
    app.selected.clear();
    app.history.endSession();
    buildLodPartnerRanges();
    refreshRegions();
    return true;
}

bool newObjectTargetFile(int& file, std::string& problem);
bool chooseObject(HWND owner, int& id, const char* title = nullptr, const char* okText = nullptr, int preselect = -1);

// "New Object": pick the destination file, let the user choose an IDE object, and put it where the pixel (mx, my) of the
// window points at (my from the top of the window).
void newObjectAt(HWND owner, int mx, int my, int presetId = -1) {
    int file;
    std::string problem;
    if (!newObjectTargetFile(file, problem)) {
        MessageBoxA(owner, problem.c_str(), "New Object", MB_ICONINFORMATION);
        return;
    }
    int id = presetId;
    if (id < 0 && !chooseObject(owner, id)) return;
    double p[3];
    if (!pickPoint(mx, my - app.toolbarH, p) && !unprojectToPlane(mx, my - app.toolbarH, p)) {
        p[0] = app.cam.x; p[1] = app.cam.y + 20; p[2] = double(app.cam.z) - double(app.cam.h);
    }
    const int idx = addNewObject(id, file, p[0], p[1], p[2]);
    if (idx >= 0) centreOnObject(idx);  // MEd: adding a new object centres the view on it
}

// "Paste": the destination file as for New Object, the point on the surface under the pixel (mx, my from the top of the window).
void pasteAt(HWND owner, int mx, int my) {
    if (app.clipboard.empty()) return;
    int file;
    std::string problem;
    if (!newObjectTargetFile(file, problem)) {
        MessageBoxA(owner, problem.c_str(), "Paste", MB_ICONINFORMATION);
        return;
    }
    double p[3];
    if (!pickPoint(mx, my - app.toolbarH, p) && !unprojectToPlane(mx, my - app.toolbarH, p)) {
        p[0] = app.cam.x; p[1] = app.cam.y + 20; p[2] = double(app.cam.z) - double(app.cam.h);
    }
    pasteClipboard(file, p[0], p[1]);  // the height of the copies is kept, as in MEd
}

// MEd's right-click object menu (CMapView::mousePressEvent, 0x0045f1e0). Picks the object under the cursor
// (no object: no menu, like MEd), then offers the entries of objectMenu().
void showObjectMenu(HWND hwnd, int mx, int my) {
    const int hit = pickInstance(mx, my - app.toolbarH);
    if (hit < 0) {  // empty ground: MEd's menu has New Object and Paste Object
        HMENU empty = CreatePopupMenu();
        for (const MenuItem& item : emptyGroundMenu(!app.clipboard.empty())) AppendMenuA(empty, MF_STRING | (item.enabled ? MF_ENABLED : MF_GRAYED), UINT_PTR(item.command), item.text.c_str());
        POINT ep = {mx, my};
        ClientToScreen(hwnd, &ep);
        const int pick = int(TrackPopupMenu(empty, TPM_RETURNCMD | TPM_RIGHTBUTTON, ep.x, ep.y, 0, hwnd, nullptr));
        DestroyMenu(empty);
        if (pick == kCmdNewObject) newObjectAt(hwnd, mx, my);
        else if (pick == kCmdPaste) pasteAt(hwnd, mx, my);
        return;
    }
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool inSelection = std::find(app.selected.begin(), app.selected.end(), hit) != app.selected.end();
    const bool multiple = useMultipleMenu(shift, app.selected.size(), inSelection);
    if (multiple) {
        if (!inSelection) app.selected.push_back(hit);  // Shift + right-click adds the clicked object
    } else {
        app.selected.assign(1, hit);                     // the menu is for this object
    }
    app.history.endSession();
    HMENU menu = CreatePopupMenu();
    for (const MenuItem& item : objectMenu(multiple)) {
        if (item.text.empty() && item.command == kCmdNone) {
            AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
            continue;
        }
        UINT flags = MF_STRING | (item.enabled ? MF_ENABLED : MF_GRAYED);
        AppendMenuA(menu, flags, UINT_PTR(item.command == kCmdNone ? 0xFFFF : item.command), item.text.c_str());
        if (item.isDefault) SetMenuDefaultItem(menu, UINT(item.command), FALSE);
    }
    POINT pt = {mx, my};
    ClientToScreen(hwnd, &pt);
    const int cmd = int(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr));
    DestroyMenu(menu);
    switch (cmd) {
        case kCmdCentre: if (!app.selected.empty()) centreOnObject(app.selected[0]); break;
        case kCmdDuplicate: duplicateSelected(); break;
        case kCmdCopy: copySelected(); break;
        case kCmdReplace: replaceSelectedUi(hwnd); break;
        case kCmdExtract: extractObjectsUi(hwnd, modelIdsOf(app.selected)); break;
        case kCmdDffView: if (!app.selected.empty()) openDffViewer(hwnd, app.instances[size_t(app.selected[0])].id); break;
        case kCmdColView: if (!app.selected.empty()) openColViewer(hwnd, app.instances[size_t(app.selected[0])].id); break;
        case kCmdTxdView:
            if (!app.selected.empty() && app.defs.count(app.instances[size_t(app.selected[0])].id))
                openTxdViewer(hwnd, app.defs[app.instances[size_t(app.selected[0])].id].txd);
            break;
        case kCmdDeleteRelocate: sinkSelected(); break;
        case kCmdDeleteHard: deleteSelected(); break;
        default: break;
    }
}

// Turn the selected placements, and their LOD partners so a building and its stand-in keep matching, by `degrees` about
// the vertical axis through each one's own position (counter-clockwise seen from above). One undo step per press.
// Not in MEd (its properties panel only moves objects); the stored quaternion is changed by core/rotation.h.
void rotateSelected(double degrees) {
    if (app.selected.empty()) return;
    app.history.snapshot(captureState());
    for (int idx : withLodPartners(app.selected)) {
        IplInstance& o = app.instances[idx];
        rotateQuatAboutZ(o.rotX, o.rotY, o.rotZ, o.rotW, degrees, app.conjugate);
        app.dirty.insert(app.instFile[idx]);
    }
    app.history.endSession();
}

void moveSelected(double dx, double dy, double dz) {
    if (app.selected.empty() || (dx == 0 && dy == 0 && dz == 0)) return;
    // MEd snapshots once before the first change, and again only after the selection has changed.
    if (app.history.needsSnapshot()) app.history.snapshot(captureState());
    // A building and its LOD stand-in move together (MEd moves only the selection; the stand-in left behind would show
    // the building at its old place from far away), as Delete, Duplicate, Copy and the rotations here do.
    for (int idx : withLodPartners(app.selected)) {
        app.instances[idx].x += dx;
        app.instances[idx].y += dy;
        app.instances[idx].z += dz;
        app.dirty.insert(app.instFile[idx]);
    }
}

// Save every changed IPL file the way MEd does (IPL save, 0x00423730): re-read the original, keep
// everything outside the inst block, rewrite the inst block. Differences from MEd, on purpose:
//  - files go to app.saveRoot unless --in-place is given (MEd always overwrites the install);
//  - with --in-place each original is copied to <file>.med-backup first;
//  - a failure is reported (MEd silently loses the changes when the file is read-only).
// Returns a human readable report.
std::string saveAll(bool* allOk) {
    std::string report;
    bool ok = true;
    if (app.dirty.empty()) {
        if (allOk) *allOk = true;
        return "Nothing to save.";
    }
    for (int fileIndex : std::set<int>(app.dirty)) {
        const std::string& rel = app.iplPaths[fileIndex];
        const std::string source = app.root + "/" + rel;
        bool readOk;
        std::string original = slurp(source, readOk);
        if (!readOk) { report += "FAILED " + rel + ": cannot read the original\n"; ok = false; continue; }
        std::vector<IplInstance> mine;
        for (size_t i = 0; i < app.instances.size(); ++i)
            if (app.instFile[i] == fileIndex) mine.push_back(app.instances[i]);
        std::string text, err;
        if (!replaceIplInstSection(original, mine, GameVersion::GTA3, text, err)) {
            report += "FAILED " + rel + ": " + err + "\n";
            ok = false;
            continue;
        }
        std::string target = app.saveInPlace ? source : app.saveRoot + "/" + rel;
        std::error_code ec;
        if (app.saveInPlace) {
            std::string backup = source + ".med-backup";
            if (!fs::exists(backup, ec)) fs::copy_file(source, backup, ec);
            if (ec) { report += "FAILED " + rel + ": cannot write the backup\n"; ok = false; continue; }
        } else {
            fs::create_directories(fs::path(target).parent_path(), ec);
            // "Save as mod" into a folder that already holds this file (for example a game folder): keep the old copy once
            if (app.backupTarget && fs::exists(target, ec) && !fs::exists(target + ".med-backup", ec)) {
                fs::copy_file(target, target + ".med-backup", ec);
                if (ec) { report += "FAILED " + rel + ": cannot write the backup of " + target + "\n"; ok = false; continue; }
            }
        }
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if (!out) {
            report += "FAILED " + rel + ": cannot write " + target + " (read-only?)\n";
            ok = false;
            continue;
        }
        out.write(text.data(), std::streamsize(text.size()));
        out.close();
        if (!out) { report += "FAILED " + rel + ": write error\n"; ok = false; continue; }
        report += "saved " + target + "\n";
        app.dirty.erase(fileIndex);
    }
    if (allOk) *allOk = ok;
    return report;
}

// What MEd draws around a selected object (FUN_00404410): green, red and blue axes through the
// bounding-sphere centre (200 units each way with a tick every 10), a circle of the sphere radius, and
// a red wireframe box, all from the collision bounds. Drawn unlit here.
void drawSelectionHighlight(const std::string& modelName) {
    float centre[3] = {0, 0, 0}, mn[3] = {-1, -1, -1}, mx[3] = {1, 1, 1}, radius = 1;
    auto it = app.cols.find(modelName);
    if (it != app.cols.end()) {
        const ColModel& c = it->second;
        std::copy(c.boundCenter, c.boundCenter + 3, centre);
        std::copy(c.boundMin, c.boundMin + 3, mn);
        std::copy(c.boundMax, c.boundMax + 3, mx);
        radius = c.boundRadius;
    } else {
        auto m = app.models.find(modelName);
        if (m != app.models.end()) {
            radius = m->second.radius;
            for (int i = 0; i < 3; ++i) { mn[i] = -radius; mx[i] = radius; }
        }
    }
    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_POLYGON_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(1);
    glPushMatrix();
    glTranslatef(centre[0], centre[1], centre[2]);
    glBegin(GL_LINES);
    glColor3f(0, 1, 0);  // Y axis
    glVertex3f(0, -200, 0); glVertex3f(0, 200, 0);
    for (int c = -200; c < 200; c += 10) { glVertex3f(-2, float(c), 0); glVertex3f(2, float(c), 0); }
    glColor3f(1, 0, 0);  // X axis
    glVertex3f(-200, 0, 0); glVertex3f(200, 0, 0);
    for (int c = -200; c < 200; c += 10) { glVertex3f(float(c), -2, 0); glVertex3f(float(c), 2, 0); }
    glColor3f(0, 0, 1);  // Z axis
    glVertex3f(0, 0, -200); glVertex3f(0, 0, 200);
    for (int c = -200; c < 200; c += 10) { glVertex3f(0, -2, float(c)); glVertex3f(0, 2, float(c)); }
    glEnd();
    glBegin(GL_LINES);  // GL_LINES on 360 vertices, so the circle comes out dashed, as in the original
    for (int a = 0; a < 360; ++a) {
        double r = a * 0.0174532925;
        glVertex3f(float(std::cos(r) * radius), float(std::sin(r) * radius), 0);
    }
    glEnd();
    glPopMatrix();
    glColor3f(1, 0, 0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_QUADS);
    glVertex3f(mx[0], mx[1], mx[2]); glVertex3f(mn[0], mx[1], mx[2]); glVertex3f(mn[0], mn[1], mx[2]); glVertex3f(mx[0], mn[1], mx[2]);
    glVertex3f(mx[0], mx[1], mn[2]); glVertex3f(mx[0], mn[1], mn[2]); glVertex3f(mn[0], mn[1], mn[2]); glVertex3f(mn[0], mx[1], mn[2]);
    glVertex3f(mx[0], mn[1], mn[2]); glVertex3f(mx[0], mn[1], mx[2]); glVertex3f(mn[0], mn[1], mx[2]); glVertex3f(mn[0], mn[1], mn[2]);
    glVertex3f(mx[0], mx[1], mn[2]); glVertex3f(mn[0], mx[1], mn[2]); glVertex3f(mn[0], mx[1], mx[2]); glVertex3f(mx[0], mx[1], mx[2]);
    glVertex3f(mn[0], mx[1], mn[2]); glVertex3f(mn[0], mn[1], mn[2]); glVertex3f(mn[0], mn[1], mx[2]); glVertex3f(mn[0], mx[1], mx[2]);
    glVertex3f(mx[0], mx[1], mn[2]); glVertex3f(mx[0], mx[1], mx[2]); glVertex3f(mx[0], mn[1], mx[2]); glVertex3f(mx[0], mn[1], mn[2]);
    glEnd();
    glPopAttrib();
}

void drawRoadOverlay();
void drawPathOverlay();

void render() {
    glViewport(app.viewX, 0, app.width, app.height);
    glClearColor(0, 0, 0, 0);  // MEd clears to black
    glEnable(GL_SCISSOR_TEST);  // clear only the 3D view, not the toolbar strip above it
    glScissor(app.viewX, 0, app.width, app.height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);  // many GTA III meshes are single sided
    // Render state as set up by CMapView::initializeGL in MEd.exe (0x00458ea0):
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);  // placements are scaled
    const GLfloat ambient[4] = {0.4f, 0.4f, 0.4f, 1}, diffuse[4] = {0.7f, 0.7f, 0.7f, 1};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, diffuse);

    // View, as in CMapView::paintGL (0x0045bc40). Yaw and pitch go on the projection matrix; the light
    // position is set while the modelview matrix still holds the previous frame's view, as MEd does.
    // (MEd divides the window width by its height as integers here, which flattens the aspect
    // ratio to 1, 2, ...; the viewer uses the true ratio.)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, double(app.width) / app.height, 1.0, 8000.0);
    glRotatef(app.cam.pitch, 1, 0, 0);
    glRotatef(app.cam.yaw, 0, 1, 0);
    const GLfloat lightPos[4] = {0, -1, -0.5f, 0};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(app.cam.x, app.cam.y - 20, app.cam.z, app.cam.x, app.cam.y, app.cam.z, 0, 0, 1);
    glTranslatef(0, 0, app.cam.h - 20);

    // Where the camera is and which way it faces, for culling. The eye sits at (x, y - 20, z + 20 - h).
    const double yawR = app.cam.yaw * 0.0174532925, pitchR = app.cam.pitch * 0.0174532925;
    const double eyeX = app.cam.x, eyeY = app.cam.y - 20, eyeZ = app.cam.z + 20 - app.cam.h;
    const double fx = std::sin(yawR) * std::cos(pitchR), fy = std::cos(yawR) * std::cos(pitchR), fz = -std::sin(pitchR);

    LARGE_INTEGER q0, q1, q2, qf;
    QueryPerformanceFrequency(&qf);
    QueryPerformanceCounter(&q0);
    collectVisible();
    QueryPerformanceCounter(&q1);
    app.drawn = int(app.visible.size());
    for (int idx : app.visible) {
        const IplInstance& inst = app.instances[idx];
        const ModelDef& def = app.defs.find(inst.id)->second;
        GlModel& gm = getModel(def.model, def.txd);
        glPushMatrix();
        applyPlacement(inst);
        glScaled(inst.scaleX, inst.scaleY, inst.scaleZ);
        drawModel(gm);
        glPopMatrix();
    }
    QueryPerformanceCounter(&q2);
    app.msCollect += 1000.0 * double(q1.QuadPart - q0.QuadPart) / double(qf.QuadPart);
    app.msSubmit += 1000.0 * double(q2.QuadPart - q1.QuadPart) / double(qf.QuadPart);
    drawRoadOverlay();  // road nodes and the previewed piece (road builder)
    if (app.showPaths) drawPathOverlay();  // View > Show traffic paths
    // Selected objects get the bounding box, axes and circle that CMapView draws (FUN_00404410).
    for (int idx : app.selected) {
        const IplInstance& inst = app.instances[idx];
        auto d = app.defs.find(inst.id);
        if (d == app.defs.end()) continue;
        glPushMatrix();
        applyPlacement(inst);
        drawSelectionHighlight(d->second.model);
        glPopMatrix();
    }
}

// ---- window ------------------------------------------------------------------------------

bool writeBmp(const std::string& path) {
    std::vector<unsigned char> px(static_cast<size_t>(app.width) * app.height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, app.width, app.height, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    const int rowBytes = (app.width * 3 + 3) & ~3;
    std::vector<unsigned char> out(static_cast<size_t>(rowBytes) * app.height, 0);
    for (int y = 0; y < app.height; ++y)
        for (int x = 0; x < app.width; ++x)
            for (int c = 0; c < 3; ++c) out[size_t(y) * rowBytes + x * 3 + c] = px[(size_t(y) * app.width + x) * 3 + (2 - c)];
    BITMAPFILEHEADER fh = {};
    BITMAPINFOHEADER ih = {};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof fh + sizeof ih;
    fh.bfSize = fh.bfOffBits + static_cast<DWORD>(out.size());
    ih.biSize = sizeof ih;
    ih.biWidth = app.width;
    ih.biHeight = app.height;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<char*>(&fh), sizeof fh);
    f.write(reinterpret_cast<char*>(&ih), sizeof ih);
    f.write(reinterpret_cast<char*>(out.data()), out.size());
    return static_cast<bool>(f);
}

// ---- Check Map ---------------------------------------------------------------------------------

// Run the map check on the map as it would be saved: files with unsaved edits are rebuilt exactly like
// Save would write them, everything else is read from disk.
CheckResult runCheck() {
    CheckInput in;
    in.root = app.root;
    in.limits = app.limits;
    in.pathLimits = app.pathLimits;
    for (int fileIndex : app.dirty) {
        bool ok;
        std::string original = slurp(app.root + "/" + app.iplPaths[fileIndex], ok);
        if (!ok) continue;
        std::vector<IplInstance> mine;
        for (size_t i = 0; i < app.instances.size(); ++i)
            if (app.instFile[i] == fileIndex) mine.push_back(app.instances[i]);
        std::string text, err;
        if (replaceIplInstSection(original, mine, GameVersion::GTA3, text, err)) in.iplText[toLower(app.iplPaths[fileIndex])] = text;
    }
    return checkMap(in);
}

// Centre the camera on a map position and select the placement there, if there is one.
void goToPosition(double x, double y, double z) {
    int best = -1;
    double bestDist = 1.0;
    for (size_t i = 0; i < app.instances.size(); ++i) {
        const IplInstance& o = app.instances[i];
        double d = std::sqrt((o.x - x) * (o.x - x) + (o.y - y) * (o.y - y) + (o.z - z) * (o.z - z));
        if (d < bestDist) { bestDist = d; best = int(i); }
    }
    if (best >= 0) {
        app.selected.assign(1, best);
        app.history.endSession();
    }
    app.cam.x = float(x);
    app.cam.y = float(y - 20);
    app.cam.z = float(z + 20);
    app.cam.h = 0;
    app.cam.yaw = 0;
    app.cam.pitch = 45;
}

struct CheckUi {
    HWND wnd = nullptr, summary = nullptr, list = nullptr;
    CheckResult result;
} checkUi;

enum { ID_GOTO = 2001, ID_AGAIN, ID_COPY, ID_CLOSE, ID_LIST };

void fillCheckUi() {
    checkUi.result = runCheck();
    const CheckResult& r = checkUi.result;
    char text[512];
    std::snprintf(text, sizeof text,
                  "%d error(s), %d warning(s), %d note(s).  %d placements (%d of %d slots), %d models.  LOD without building %d (original %d), "
                  "building without LOD %d (original %d), duplicates %d (original %d).",
                  r.count(Severity::Error), r.count(Severity::Warning), r.count(Severity::Info), r.summary.placements, r.summary.placements,
                  kPlacementPoolCapacity, r.summary.modelsPlaced, r.summary.lodWithoutBuilding, kOriginalLodWithoutBuilding,
                  r.summary.buildingWithoutLod, kOriginalBuildingWithoutLod, r.summary.duplicates, kOriginalDuplicates);
    SetWindowTextA(checkUi.summary, text);
    SendMessageA(checkUi.list, LB_RESETCONTENT, 0, 0);
    if (r.issues.empty() || (r.count(Severity::Error) == 0 && r.count(Severity::Warning) == 0))
        SendMessageA(checkUi.list, LB_ADDSTRING, 0, (LPARAM)"No problems found. The game should accept this map.");
    for (const CheckIssue& i : r.issues) {
        std::string line = i.severity == Severity::Error ? "[ERROR]   " : i.severity == Severity::Warning ? "[warning] " : "[note]    ";
        line += i.message;
        if (!i.file.empty()) line += "   (" + i.file + (i.line > 0 ? ":" + std::to_string(i.line) : "") + ")";
        SendMessageA(checkUi.list, LB_ADDSTRING, 0, (LPARAM)line.c_str());
    }
}

void checkGoTo(HWND owner) {
    int sel = int(SendMessageA(checkUi.list, LB_GETCURSEL, 0, 0));
    if (sel < 0) return;
    // the list starts with a line that is not an issue when there are no problems
    const bool noProblems = checkUi.result.count(Severity::Error) == 0 && checkUi.result.count(Severity::Warning) == 0;
    if (sel >= int(checkUi.result.issues.size())) return;
    (void)noProblems;
    const CheckIssue& i = checkUi.result.issues[sel];
    if (i.hasPosition) goToPosition(i.x, i.y, i.z);
    else MessageBoxA(owner, "This problem is not tied to a place on the map.", "Check Map", MB_ICONINFORMATION);
}

void copyReport(HWND owner) {
    std::string text = checkReportText(checkUi.result);
    if (!OpenClipboard(owner)) return;
    EmptyClipboard();
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (mem) {
        std::memcpy(GlobalLock(mem), text.c_str(), text.size() + 1);
        GlobalUnlock(mem);
        SetClipboardData(CF_TEXT, mem);
    }
    CloseClipboard();
}

LRESULT CALLBACK checkProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            auto make = [&](const char* cls, const char* text, DWORD style, DWORD ex, int id) {
                HWND c = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, w, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
                SendMessageA(c, WM_SETFONT, (WPARAM)font, TRUE);
                return c;
            };
            checkUi.summary = make("STATIC", "", SS_LEFT, 0, 0);
            checkUi.list = make("LISTBOX", "", WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, WS_EX_CLIENTEDGE, ID_LIST);
            make("BUTTON", "Go to", BS_PUSHBUTTON, 0, ID_GOTO);
            make("BUTTON", "Check again", BS_PUSHBUTTON, 0, ID_AGAIN);
            make("BUTTON", "Copy report", BS_PUSHBUTTON, 0, ID_COPY);
            make("BUTTON", "Close", BS_PUSHBUTTON, 0, ID_CLOSE);
            return 0;
        }
        case WM_SIZE: {
            int cw = LOWORD(lp), ch = HIWORD(lp);
            MoveWindow(checkUi.summary, 8, 6, cw - 16, 34, TRUE);
            MoveWindow(checkUi.list, 8, 44, cw - 16, ch - 44 - 40, TRUE);
            const int by = ch - 32, bw = 100, gap = 8;
            int x = 8;
            for (int id : {ID_GOTO, ID_AGAIN, ID_COPY}) {
                MoveWindow(GetDlgItem(w, id), x, by, bw, 26, TRUE);
                x += bw + gap;
            }
            MoveWindow(GetDlgItem(w, ID_CLOSE), cw - 8 - bw, by, bw, 26, TRUE);
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case ID_GOTO: checkGoTo(w); return 0;
                case ID_AGAIN: fillCheckUi(); return 0;
                case ID_COPY: copyReport(w); return 0;
                case ID_CLOSE: DestroyWindow(w); return 0;
                case ID_LIST:
                    if (HIWORD(wp) == LBN_DBLCLK) checkGoTo(w);
                    return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(w);
            return 0;
        case WM_DESTROY:
            checkUi.wnd = nullptr;
            return 0;
    }
    return DefWindowProcA(w, msg, wp, lp);
}

void showCheckWindow(HWND owner) {
    if (checkUi.wnd) {
        fillCheckUi();
        SetForegroundWindow(checkUi.wnd);
        return;
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc = checkProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "MEdCheckMap";
        RegisterClassA(&wc);
        registered = true;
    }
    RECT r;
    GetWindowRect(owner, &r);
    checkUi.wnd = CreateWindowExA(0, "MEdCheckMap", "Check Map", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VISIBLE | WS_MINIMIZEBOX,
                                  r.left + 40, r.top + 60, 860, 520, owner, nullptr, GetModuleHandle(nullptr), nullptr);
    if (checkUi.wnd) fillCheckUi();
}

// ---- Pools -------------------------------------------------------------------------------------
// The game allocates fixed arrays ("pools") for placed objects when it starts (CPools::Initialise).
// The pool of a placement follows from its model (object.dat, path data), see core/pools.h.

HWND poolsWnd = nullptr;

// ---- the path budget: how full the game's path tables get with the map as it is now (core/paths.h) ----

// The road pieces in the order the game loads them: by IPL file (gta3.dat order), then in each file from the top.
std::vector<PathPlacement> pathPlacementsInLoadOrder() {
    std::vector<size_t> order;
    for (size_t i = 0; i < app.instances.size(); ++i)
        if (app.pathData.count(app.instances[i].id) && app.defs.count(app.instances[i].id)) order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [](size_t a, size_t b) { return app.instFile[a] < app.instFile[b]; });
    std::vector<PathPlacement> out;
    for (size_t i : order) {
        const IplInstance& o = app.instances[i];
        out.push_back({o.id, o.x, o.y, o.z, o.rotX, o.rotY, o.rotZ, o.rotW});
    }
    return out;
}

// Worked out again only when a road piece was added, removed, moved or turned (moving a piece changes what it joins).
PathNetwork gPathNetwork;  // the network of the cached budget (for View > Show traffic paths)

const PathBudget& currentPathBudget() {
    static PathBudget cached;
    static uint64_t cachedKey = 0;
    const std::vector<PathPlacement> roads = pathPlacementsInLoadOrder();
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](const void* p, size_t n) {
        for (size_t k = 0; k < n; ++k) h = (h ^ static_cast<const uint8_t*>(p)[k]) * 1099511628211ull;
    };
    for (const PathPlacement& r : roads) {  // field by field: the struct has padding
        mix(&r.id, sizeof r.id);
        const double v[7] = {r.x, r.y, r.z, r.rx, r.ry, r.rz, r.rw};
        mix(v, sizeof v);
    }
    const size_t n = roads.size();
    mix(&n, sizeof n);
    if (h != cachedKey) {
        cached = simulatePathBudget(app.pathData, app.dynamicIds, roads, &gPathNetwork);
        cachedKey = h;
    }
    return cached;
}

// View > Show traffic paths: the network the game builds from the placed road pieces. Car paths orange, pedestrian
// paths light blue, links inside a piece thinner than joints between pieces, nodes as dots (grey: pedestrian nodes
// the game drops because nothing joins them), and red marks where a path ends with nothing to join (a dead end).
void drawPathOverlay() {
    currentPathBudget();  // brings gPathNetwork up to date
    const PathNetwork& n = gPathNetwork;
    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_POINT_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    const float lift = 0.4f;  // just above the road surface, so the road does not hide it
    for (int pass = 0; pass < 2; ++pass) {  // links inside pieces, then the joints between pieces (thicker)
        glLineWidth(pass == 0 ? 1.5f : 3.0f);
        glBegin(GL_LINES);
        for (const PathNetwork::Link& l : n.links) {
            if (l.betweenPieces != (pass == 1)) continue;
            const PathNetwork::Node &a = n.nodes[size_t(l.a)], &b = n.nodes[size_t(l.b)];
            if (l.car) glColor3f(1.0f, 0.55f, 0.0f);
            else glColor3f(0.35f, 0.8f, 1.0f);
            glVertex3f(a.x, a.y, a.z + lift);
            glVertex3f(b.x, b.y, b.z + lift);
        }
        glEnd();
    }
    glPointSize(5.0f);
    glBegin(GL_POINTS);
    for (const PathNetwork::Node& p : n.nodes) {
        if (!p.kept) glColor3f(0.55f, 0.55f, 0.55f);
        else if (p.car) glColor3f(1.0f, 0.75f, 0.2f);
        else glColor3f(0.6f, 0.9f, 1.0f);
        glVertex3f(p.x, p.y, p.z + lift);
    }
    glEnd();
    glPointSize(9.0f);
    glBegin(GL_POINTS);
    for (const PathNetwork::Point& p : n.openPoints) {
        if (p.car) glColor3f(1.0f, 0.1f, 0.1f);
        else glColor3f(0.85f, 0.2f, 0.6f);
        glVertex3f(p.x, p.y, p.z + lift);
    }
    glEnd();
    glPopAttrib();
}

std::string pathBudgetText() {
    const PathBudget& b = currentPathBudget();
    const PathTableLimits& l = app.pathLimits;
    const PathTableLimits orig;
    struct Row { const char* name; int used, limit, original; };
    const Row rows[] = {
        {"Path nodes", b.nodesPeak(), l.nodes, orig.nodes},
        {"Car links", b.carLinks, l.carLinks, orig.carLinks},
        {"Connections", b.connections, l.connections, orig.connections},
        {"Road objects", b.objects, l.objects, orig.objects},
    };
    std::string t;
    char buf[256];
    for (const Row& r : rows) {
        const int bar = 24, filled = std::min(bar, int(int64_t(r.used) * bar / std::max(r.limit, 1)));
        std::snprintf(buf, sizeof buf, "%-12s %5d / %5d  %5.1f%%  [%s%s]  %d free%s\r\n", r.name, r.used, r.limit, 100.0 * r.used / std::max(r.limit, 1),
                      std::string(filled, '#').c_str(), std::string(bar - filled, '.').c_str(), std::max(r.limit - r.used, 0),
                      r.used > r.limit ? "  OVER: the game crashes while loading" : r.used > r.original ? "  (over the original size)" : "");
        t += buf;
    }
    std::snprintf(buf, sizeof buf, "\r\nPath nodes while loading: %d car + %d pedestrian (the game then drops %d pedestrian nodes with no connection).\r\n", b.carNodes,
                  b.pedNodesCreated, b.pedNodesCreated - b.pedNodesKept);
    t += buf;
    t += "Road pieces with traffic or pedestrian paths (the rd_ road kit and many others) fill these tables. The original\r\n";
    t += "map leaves only 7 nodes and 5 car links free, so a few extra pieces already overflow them and the game crashes\r\n";
    t += "while loading. No limit adjuster raises them for GTA III.\r\n";
    if (l.plugin) {
        std::snprintf(buf, sizeof buf, "The III.PathLimits plugin is in this game folder (%s): the limits above are its settings.\r\n", l.source.c_str());
        t += buf;
    } else {
        t += "The limits above are the original game's (no path limit plugin was found in this game folder).\r\n";
    }
    return t;
}

std::string poolsText() {
    int used[kPoolCount] = {0, 0, 0};
    for (const IplInstance& o : app.instances) {
        auto d = app.defs.find(o.id);
        if (d != app.defs.end()) ++used[int(d->second.pool)];  // an unknown id is ignored by the game and takes no slot
    }
    const char* what[kPoolCount] = {"ordinary map pieces: buildings, terrain, roads",
                                    "pieces with path data (the paths peds and cars follow)",
                                    "objects listed in object.dat (props the player can hit or move)"};
    int total = 0, cap = 0;
    std::string t;
    char b[256];
    for (int k = 0; k < kPoolCount; ++k) {
        total += used[k];
        cap += kPoolCapacity[k];
        const int bar = 24, filled = std::min(bar, used[k] * bar / kPoolCapacity[k]);
        std::snprintf(b, sizeof b, "%-11s %5d / %5d  %5.1f%%  [%s%s]  %d free%s\r\n", poolName(Pool(k)), used[k], kPoolCapacity[k],
                      100.0 * used[k] / kPoolCapacity[k], std::string(filled, '#').c_str(), std::string(bar - filled, '.').c_str(),
                      std::max(kPoolCapacity[k] - used[k], 0), used[k] > kPoolCapacity[k] ? "  over the original size (needs a limit adjuster)" : "");
        t += b;
    }
    std::snprintf(b, sizeof b, "%-11s %5d / %5d  %5.1f%%   (the original map uses 8,689)\r\n\r\n", "Total", total, cap, 100.0 * total / cap);
    t += b;
    t += "Pool        What it holds\r\n";
    for (int k = 0; k < kPoolCount; ++k) {
        std::snprintf(b, sizeof b, "%-11s %s\r\n", poolName(Pool(k)), what[k]);
        t += b;
    }
    t += "\r\nEach pool is full on its own: the game crashes while loading when ANY one has no room left, even if\r\n";
    t += "the others are empty. The original map leaves only 1 treadable free, so a copy of a model with path data\r\n";
    t += "(roads, many buildings) can already overflow it. The model decides the pool, not where it is placed.\r\n\r\n";
    t += "The sizes above are the original game's. They are a suggestion, not a limit of the editor: nothing stops you\r\n";
    t += "going over them if you use a limit adjuster (III.VC.SA.LimitAdjuster raises Buildings, Treadables and Dummys).\r\n";
    if (app.limits.adjusted) {
        std::snprintf(b, sizeof b, "Adjuster file found: %s  (Buildings %lld, Treadables %lld, Dummys %lld).\r\n", app.limits.source.c_str(), app.limits.cap[0],
                      app.limits.cap[1], app.limits.cap[2]);
        t += b;
    } else {
        t += "No adjuster file was found in the game folder, so the editor cannot tell whether you have one.\r\n";
    }
    t += "\r\nPath tables (traffic and pedestrian paths, built by the game at start-up)\r\n";
    t += pathBudgetText();
    return t;
}

LRESULT CALLBACK poolsProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            HWND e = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", poolsText().c_str(), WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                                     0, 0, 10, 10, w, (HMENU)1, GetModuleHandle(nullptr), nullptr);
            SendMessageA(e, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), TRUE);
            CreateWindowExA(0, "BUTTON", "Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 10, 10, w, (HMENU)2, GetModuleHandle(nullptr), nullptr);
            return 0;
        }
        case WM_SIZE:
            MoveWindow(GetDlgItem(w, 1), 8, 8, LOWORD(lp) - 16, HIWORD(lp) - 48, TRUE);
            MoveWindow(GetDlgItem(w, 2), 8, HIWORD(lp) - 34, 100, 26, TRUE);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wp) == 2) SetWindowTextA(GetDlgItem(w, 1), poolsText().c_str());
            return 0;
        case WM_CLOSE: DestroyWindow(w); return 0;
        case WM_DESTROY: poolsWnd = nullptr; return 0;
    }
    return DefWindowProcA(w, msg, wp, lp);
}

void showPoolsWindow(HWND owner) {
    if (poolsWnd) { SetWindowTextA(GetDlgItem(poolsWnd, 1), poolsText().c_str()); SetForegroundWindow(poolsWnd); return; }
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc = poolsProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "MEdPools";
        RegisterClassA(&wc);
        registered = true;
    }
    RECT r;
    GetWindowRect(owner, &r);
    poolsWnd = CreateWindowExA(0, "MEdPools", "Pools", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VISIBLE, r.left + 60, r.top + 80, 960, 400,
                               owner, nullptr, GetModuleHandle(nullptr), nullptr);
}

// ---- toolbar -----------------------------------------------------------------------------------

enum { ID_TB_SAVE = 1001, ID_TB_UNDO, ID_TB_REDO, ID_TB_CENTRE, ID_TB_DELETE, ID_TB_CHECK, ID_TB_POOLS, ID_TB_SAVEMOD, ID_TB_DUPLICATE, ID_TB_OPEN, ID_TB_LODVIEW, ID_TB_NEWOBJ, ID_TB_FILEMENU, ID_TB_REFMENU, ID_TB_COPY, ID_TB_PASTE, ID_TB_BOOKMARKS, ID_TB_VIEWMENU, ID_TB_VIEWEMPTY, ID_TB_ROADATTACH, ID_TB_VIEWFILE, ID_TB_IMPORT, ID_TB_VIEWPATHS };

// Two small arrow icons for Undo (points back) and Redo (points forward), drawn here so no image files are needed.
HIMAGELIST makeArrowIcons() {
    const int sz = 16;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, sz * 2, sz);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);
    HBRUSH bg = CreateSolidBrush(RGB(255, 0, 255));  // this colour becomes transparent
    RECT all = {0, 0, sz * 2, sz};
    FillRect(dc, &all, bg);
    DeleteObject(bg);
    HBRUSH fg = CreateSolidBrush(RGB(40, 100, 190));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(20, 60, 130));
    HGDIOBJ oldBrush = SelectObject(dc, fg), oldPen = SelectObject(dc, pen);
    const POINT back[7] = {{1, 8}, {8, 2}, {8, 6}, {14, 6}, {14, 10}, {8, 10}, {8, 14}};
    POINT forward[7];
    for (int k = 0; k < 7; ++k) forward[k] = {sz + (sz - 1 - back[k].x), back[k].y};  // mirrored, in the second half
    Polygon(dc, back, 7);
    Polygon(dc, forward, 7);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBmp);
    DeleteObject(fg);
    DeleteObject(pen);
    HIMAGELIST il = ImageList_Create(sz, sz, ILC_COLOR32 | ILC_MASK, 2, 0);
    ImageList_AddMasked(il, bmp, RGB(255, 0, 255));
    DeleteObject(bmp);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
    return il;
}

struct ToolDef {
    int id;
    const char* text;
    int icon;     // image list index, or -1
    DWORD style;  // BTNS_DROPDOWN-like extras
};

HWND makeToolbar(HWND parent, int ctrlId, const std::vector<ToolDef>& defs) {
    HWND tb = CreateWindowExA(0, TOOLBARCLASSNAMEA, nullptr,
                              WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS | CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER, 0, 0, 0, 0, parent,
                              (HMENU)(INT_PTR)ctrlId, GetModuleHandle(nullptr), nullptr);
    SendMessageA(tb, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessageA(tb, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DRAWDDARROWS);
    std::vector<TBBUTTON> b(defs.size());
    std::memset(b.data(), 0, b.size() * sizeof(TBBUTTON));
    for (size_t i = 0; i < defs.size(); ++i) {
        if (!defs[i].text) { b[i].fsStyle = BTNS_SEP; continue; }
        b[i].iBitmap = defs[i].icon >= 0 ? defs[i].icon : I_IMAGENONE;
        b[i].idCommand = defs[i].id;
        b[i].fsState = TBSTATE_ENABLED;
        b[i].fsStyle = BTNS_SHOWTEXT | BTNS_AUTOSIZE | defs[i].style;
        b[i].iString = (INT_PTR)defs[i].text;
    }
    SendMessageA(tb, TB_ADDBUTTONSA, int(b.size()), (LPARAM)b.data());
    return tb;
}

// Two rows. Top: File and Reference drop-down menus with Save All between them (files and checks). Below: the editing tools.
void createToolbar(HWND parent) {
    INITCOMMONCONTROLSEX ic = {sizeof ic, ICC_BAR_CLASSES};
    InitCommonControlsEx(&ic);
    OleInitialize(nullptr);  // the folder picker of Save as Mod needs COM
    app.menubar = makeToolbar(parent, 100, {{ID_TB_FILEMENU, "File", -1, BTNS_WHOLEDROPDOWN}, {ID_TB_SAVE, "Save All", -1, BTNS_BUTTON}, {ID_TB_REFMENU, "Reference", -1, BTNS_WHOLEDROPDOWN}, {ID_TB_VIEWMENU, "View", -1, BTNS_WHOLEDROPDOWN}, {ID_TB_BOOKMARKS, "Bookmarks", -1, BTNS_BUTTON}});
    app.toolbar = makeToolbar(parent, 101, {{ID_TB_UNDO, "Undo", 0, BTNS_BUTTON}, {ID_TB_REDO, "Redo", 1, BTNS_BUTTON}, {0, nullptr, -1, 0},
                                            {ID_TB_NEWOBJ, "New Object...", -1, BTNS_BUTTON}, {ID_TB_COPY, "Copy", -1, BTNS_BUTTON}, {ID_TB_PASTE, "Paste", -1, BTNS_BUTTON}, {ID_TB_CENTRE, "Centre on Object", -1, BTNS_BUTTON},
                                            {ID_TB_DUPLICATE, "Duplicate", -1, BTNS_BUTTON}, {ID_TB_DELETE, "Delete", -1, BTNS_BUTTON}, {0, nullptr, -1, 0},
                                            {ID_TB_LODVIEW, "LOD view", -1, BTNS_CHECK}});  // LOD view is a toggle
    SendMessageA(app.toolbar, TB_SETIMAGELIST, 0, (LPARAM)makeArrowIcons());
    auto rowHeight = [](HWND tb) { return int(HIWORD(SendMessageA(tb, TB_GETBUTTONSIZE, 0, 0))) + 6; };
    app.menubarH = rowHeight(app.menubar);
    app.toolbarH = app.menubarH + rowHeight(app.toolbar);
}

// Which buttons can be used now: Save All when something changed, Undo and Redo when there is something to undo or redo
// (as in MEd's setUndo and setRedo), the rest when something is selected.
void updateToolbarState() {
    if (!app.toolbar) return;
    struct Item { HWND bar; int id; int enabled; };
    const Item items[] = {{app.menubar, ID_TB_SAVE, !app.dirty.empty()},
                          {app.toolbar, ID_TB_UNDO, app.history.canUndo()},
                          {app.toolbar, ID_TB_REDO, app.history.canRedo()},
                          {app.toolbar, ID_TB_CENTRE, !app.selected.empty()},
                          {app.toolbar, ID_TB_DELETE, !app.selected.empty()},
                          {app.toolbar, ID_TB_DUPLICATE, !app.selected.empty()},
                          {app.toolbar, ID_TB_COPY, !app.selected.empty()},
                          {app.toolbar, ID_TB_PASTE, !app.clipboard.empty()}};
    static int last[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    for (int i = 0; i < 8; ++i) {
        if (last[i] == items[i].enabled) continue;
        SendMessageA(items[i].bar, TB_ENABLEBUTTON, items[i].id, MAKELONG(items[i].enabled ? TRUE : FALSE, 0));
        last[i] = items[i].enabled;
    }
}


// ---- Save as Mod ---------------------------------------------------------------------------------
// Writes the changed IPL files into a folder you choose (normally a mod folder of the Mod Manager, for example
// mods\ProjectUpstate), in the game's folder layout, instead of the editor's default MEd_saved folder. The folder is
// remembered in med_viewer.ini next to the program. A file that is already there is copied to <file>.med-backup once.
// To keep building on an earlier mod, open the editor on the game folder the mod is applied to, so its files are the
// ones that get re-saved.

std::string modIniPath() {
    char p[MAX_PATH];
    GetModuleFileNameA(nullptr, p, MAX_PATH);
    std::string s = p;
    return s.substr(0, s.find_last_of("\\/")) + "\\med_viewer.ini";
}

int CALLBACK startFolderProc(HWND w, UINT msg, LPARAM, LPARAM data) {
    if (msg == BFFM_INITIALIZED && data) SendMessageA(w, BFFM_SETSELECTIONA, TRUE, data);
    return 0;
}

void saveAsMod(HWND owner) {
    if (app.dirty.empty() && regionsAddedSinceBackup(app.root).empty()) {
        MessageBoxA(owner, "Nothing to save: no file has been changed.", "Save as Mod", MB_ICONINFORMATION);
        return;
    }
    char last[MAX_PATH] = "";
    GetPrivateProfileStringA("SaveAsMod", "folder", "", last, sizeof last, modIniPath().c_str());
    BROWSEINFOA bi = {};
    bi.hwndOwner = owner;
    bi.lpszTitle = "Save the changed files into this folder (for example your mod's folder under the Mod Manager's mods folder):";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = startFolderProc;
    bi.lParam = last[0] ? reinterpret_cast<LPARAM>(last) : 0;
    LPITEMIDLIST id = SHBrowseForFolderA(&bi);
    if (!id) return;
    char path[MAX_PATH];
    const bool got = SHGetPathFromIDListA(id, path) != FALSE;
    CoTaskMemFree(id);
    if (!got) return;
    WritePrivateProfileStringA("SaveAsMod", "folder", path, modIniPath().c_str());

    const std::string oldRoot = app.saveRoot;
    const bool oldInPlace = app.saveInPlace;
    app.saveRoot = path;
    app.saveInPlace = false;
    app.backupTarget = true;
    bool ok;
    std::string r = saveAll(&ok);
    app.saveRoot = oldRoot;
    app.saveInPlace = oldInPlace;
    app.backupTarget = false;
    // Regions made with New region... are part of the mod too: the gta3.dat that lists them, and their IPL files (those
    // with changed objects were written by saveAll just now; an empty one is copied as it is).
    const std::vector<std::string> addedRegions = regionsAddedSinceBackup(app.root);
    if (!addedRegions.empty()) {
        std::error_code ec;
        const fs::path target(path);
        fs::create_directories(target / "data", ec);
        if (fs::exists(target / "data" / "gta3.dat", ec) && !fs::exists(target / "data" / "gta3.dat.med-backup", ec))
            fs::copy_file(target / "data" / "gta3.dat", target / "data" / "gta3.dat.med-backup", ec);
        fs::copy_file(fs::path(app.root) / "data" / "gta3.dat", target / "data" / "gta3.dat", fs::copy_options::overwrite_existing, ec);
        if (ec) { r += "FAILED data/gta3.dat: " + ec.message() + "\n"; ok = false; }
        else r += "saved " + (target / "data" / "gta3.dat").string() + " (lists the new region(s))\n";
        for (const std::string& rel : addedRegions) {
            const fs::path to = target / rel;
            if (fs::exists(to, ec)) continue;  // written by the save above, with its objects
            fs::create_directories(to.parent_path(), ec);
            fs::copy_file(fs::path(app.root) / rel, to, ec);
            if (ec) { r += "FAILED " + rel + ": " + ec.message() + "\n"; ok = false; }
            else r += "saved " + to.string() + "\n";
        }
    }
    if (ok) r += "\nThis only saved the files into the mod folder. To see them in the game (and when you open the game folder here again), press Apply or Launch game in the Mod Manager.";
    MessageBoxA(owner, r.c_str(), ok ? "Save as Mod - saved" : "Save as Mod - failed", ok ? MB_ICONINFORMATION : MB_ICONERROR);
}

// Save All / Ctrl+S: the files are written straight into the folder that was opened (MEd's behaviour), after the user has
// agreed once per session; each original is first kept as <file>.med-backup. Starting the editor with --save-to dir (or
// --shot for tests) writes to that other folder instead and asks nothing.
void saveFromUi(HWND owner) {
    static bool agreed = false;
    if (app.saveInPlace && !agreed && !app.dirty.empty()) {
        const std::string msg = "Save writes your changes directly into this game folder:\n\n" + app.root +
                                "\n\nThe original of each file is kept once next to it as <file>.med-backup. Continue?";
        if (MessageBoxA(owner, msg.c_str(), "Save", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
        agreed = true;
    }
    bool ok;
    std::string r = saveAll(&ok);
    MessageBoxA(owner, r.c_str(), ok ? "MEd 3D viewer - saved" : "MEd 3D viewer - save failed", ok ? MB_ICONINFORMATION : MB_ICONERROR);
}

// Open another game folder. The editor loads everything at start-up, so this remembers the chosen folder and starts a
// new editor window on it, then closes this one (after a warning when there are unsaved changes).
void openGameFolder(HWND owner) {
    if (!app.dirty.empty() &&
        MessageBoxA(owner, "There are unsaved changes. Opening another folder closes this editor window and they are lost.\n\nUse Save All or Save as Mod first if you need them.\n\nOpen another folder anyway?",
                    "Open", MB_YESNO | MB_ICONWARNING) != IDYES)
        return;
    BROWSEINFOA bi = {};
    bi.hwndOwner = owner;
    bi.lpszTitle = "Open the GTA III folder to edit (the one with data\\gta3.dat):";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = startFolderProc;
    bi.lParam = reinterpret_cast<LPARAM>(app.root.c_str());
    LPITEMIDLIST id = SHBrowseForFolderA(&bi);
    if (!id) return;
    char path[MAX_PATH];
    const bool got = SHGetPathFromIDListA(id, path) != FALSE;
    CoTaskMemFree(id);
    if (!got) return;
    if (!fs::exists(std::string(path) + "/data/gta3.dat")) {
        MessageBoxA(owner, "That folder has no data\\gta3.dat. Choose the GTA III folder.", "Open", MB_ICONWARNING);
        return;
    }
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string cmd = std::string("\"") + exe + "\" \"" + path + "\"";
    STARTUPINFOA si = {sizeof si};
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        MessageBoxA(owner, "The editor could not be started on that folder.", "Open", MB_ICONERROR);
        return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    app.dirty.clear();  // already confirmed; do not ask again on the way out
    PostMessageA(owner, WM_CLOSE, 0, 0);
}

void roadAttachFirstOpen(HWND owner);
void viewFileUi(HWND owner);
void importModelUi(HWND owner, const std::string& dffPath = "");
bool openLooseFile(HWND owner, const std::string& path);

std::string importAtStart;  // --import-window dff: the Import Model window opens on this model once the editor is up

void doCommand(HWND h, int id) {
    switch (id) {
        case ID_TB_OPEN: openGameFolder(h); return;
        case ID_TB_BOOKMARKS: showBookmarksWindow(h); return;  // the window keeps the focus
        case ID_TB_VIEWEMPTY: setShowEmptyRegions(!app.showEmptyRegions); break;
        case ID_TB_VIEWPATHS: app.showPaths = !app.showPaths; break;
        case ID_TB_ROADATTACH: roadAttachFirstOpen(h); return;
        case ID_TB_VIEWFILE: viewFileUi(h); return;
        case ID_TB_IMPORT: { const std::string dff = importAtStart; importAtStart.clear(); importModelUi(h, dff); return; }
        case ID_TB_NEWOBJ: newObjectAt(h, app.viewX + app.width / 2, app.toolbarH + app.height / 2); return;
        case ID_TB_COPY: copySelected(); break;
        case ID_TB_PASTE: pasteAt(h, app.viewX + app.width / 2, app.toolbarH + app.height / 2); return;
        case ID_TB_LODVIEW: app.lodView = SendMessageA(app.toolbar, TB_ISBUTTONCHECKED, ID_TB_LODVIEW, 0) != 0; break;
        case ID_TB_SAVEMOD: saveAsMod(h); break;
        case ID_TB_DUPLICATE: duplicateSelected(); break;
        case ID_TB_SAVE: saveFromUi(h); break;
        case ID_TB_UNDO: undoAction(); break;
        case ID_TB_REDO: redoAction(); break;
        case ID_TB_CENTRE: if (!app.selected.empty()) centreOnObject(app.selected[0]); break;
        case ID_TB_DELETE: sinkSelected(); break;
        case ID_TB_CHECK: showCheckWindow(h); return;
        case ID_TB_POOLS: showPoolsWindow(h); return;  // the wizard keeps the focus
    }
    SetFocus(h);  // give the keyboard back to the map view so W/A/S/D keep working
}

// The drop-down menus of the top row: File (Open, Save All, Save as Mod) and Reference (Pools, Check Map).
void showToolbarMenu(HWND owner, int buttonId) {
    HMENU menu = CreatePopupMenu();
    if (buttonId == ID_TB_FILEMENU) {
        AppendMenuA(menu, MF_STRING, ID_TB_OPEN, "Open...");
        AppendMenuA(menu, MF_STRING | (app.dirty.empty() ? MF_GRAYED : MF_ENABLED), ID_TB_SAVE, "Save All" "\tCtrl+S");
        AppendMenuA(menu, MF_STRING, ID_TB_SAVEMOD, "Save as Mod...");
        AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuA(menu, MF_STRING, ID_TB_VIEWFILE, "View a DFF / TXD / COL file...");
        AppendMenuA(menu, MF_STRING, ID_TB_IMPORT, "Import Model (DFF + TXD + COL)...");
    } else if (buttonId == ID_TB_VIEWMENU) {
        AppendMenuA(menu, MF_STRING | (app.showEmptyRegions ? MF_CHECKED : MF_UNCHECKED), ID_TB_VIEWEMPTY, "Show hidden regions");
        AppendMenuA(menu, MF_STRING | (app.showPaths ? MF_CHECKED : MF_UNCHECKED), ID_TB_VIEWPATHS, "Show traffic paths (cars orange, pedestrians blue, dead ends red)");
    } else {
        AppendMenuA(menu, MF_STRING, ID_TB_POOLS, "Pools");
        AppendMenuA(menu, MF_STRING, ID_TB_CHECK, "Check Map" "\tF7");
    }
    RECT r;
    SendMessageA(app.menubar, TB_GETRECT, buttonId, (LPARAM)&r);
    POINT pt = {r.left, r.bottom};
    ClientToScreen(app.menubar, &pt);
    const int cmd = int(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, owner, nullptr));
    DestroyMenu(menu);
    if (cmd) doCommand(owner, cmd);
}

// ---- inspector (properties panel) ---------------------------------------------------------------
// After MEd's "Item Instance" properties panel (CIPLViewer, 0x00447360): the model, Position X / Y / Z and Rotation
// X / Y / Z (angles) of the selected object, each with "<<" "<" ">" ">>" buttons that repeat while held. MEd's steps:
// position 0.1 and 10 units (xplus 0x004425f0, x1plus 0x00442ef0), rotation 0.2 and 10 degrees (xrplus 0x004437f0,
// xr1plus 0x00443f10); MEd 0.32 applies them to every selected object. Kept from this editor: the Rotate -90 / +90
// buttons (the user's request), Reset rotation, Centre, the two deletes. Added: typing a value and pressing Enter (or
// Apply), the model ID with a chooser (MEd: "Allowed File and ID to be changed"), tooltips. A building and its LOD
// stand-in are edited together, as everywhere else in this editor. The boxes are not overwritten while being typed in.

enum {
    ID_INS_INFO = 3001, ID_INS_IDBOX, ID_INS_CHOOSE, ID_INS_APPLY, ID_INS_CENTRE, ID_INS_SINK, ID_INS_HARD, ID_INS_ROTM90, ID_INS_ROTP90, ID_INS_RESET,
    ID_INS_FIELD = 3020,  // + 0..5: X, Y, Z, rotation X, Y, Z boxes
    ID_INS_NUDGE = 3040,  // + field * 4 + k, k = 0..3: <<, <, >, >>
    ID_INS_LABEL = 3080   // + n
};

const double kPosSteps[4] = {-10, -0.1, 0.1, 10};  // MEd's position steps
const double kRotSteps[4] = {-10, -0.2, 0.2, 10};  // MEd's rotation steps, in degrees
const char* kFieldNames[6] = {"X", "Y", "Z", "X", "Y", "Z"};

struct InspectorUi {
    HWND info = nullptr, idBox = nullptr, tips = nullptr;
    HWND field[6] = {};
    std::string shownInfo;
    std::string shownText[7];  // what was last written into the six boxes and the ID box (to know what the user changed)
    int repeatId = 0;          // the nudge button being held
    bool repeatFired = false;  // the held button already acted; skip the BN_CLICKED that follows
    WNDPROC buttonProc = nullptr;
    std::string lastMessage;   // the last warning shown (for the self-test)
    bool quiet = false;        // self-test: record warnings instead of showing them
    bool testHolding = false;  // self-test: the mouse button counts as held (real mouse movement resets a faked key state)
} insp;

std::string num(double v, int digits = 3) {
    char b[64];
    std::snprintf(b, sizeof b, "%.*f", digits, v);
    std::string s = b;
    if (s == "-0" || s.find_first_not_of("-0.") == std::string::npos) s = s[0] == '-' ? s.substr(1) : s;  // no "-0.000"
    return s;
}

void inspWarn(HWND owner, const std::string& text, const char* title) {
    insp.lastMessage = text;
    if (!insp.quiet) MessageBoxA(owner, text.c_str(), title, MB_ICONWARNING);
}

std::string selectionTitle() {
    if (app.selected.empty()) return "";
    if (app.selected.size() > 1) return std::to_string(app.selected.size()) + " objects selected";
    const IplInstance& o = app.instances[app.selected[0]];
    return o.model + " (id " + std::to_string(o.id) + ") selected";
}

std::string inspectorText() {
    if (app.selected.empty())
        return "Nothing selected.\r\n\r\nDouble-click an object in the 3D view to select it (Shift adds more), or pick it in the Placements tab.\r\n\r\n"
               "Right-click an object for its menu. F7 or Reference > Check Map looks for problems in the map.";
    if (app.selected.size() > 1) {
        std::string t = std::to_string(app.selected.size()) + " objects selected. The boxes show the first one; the buttons move and turn them all, "
                        "each about its own position.\r\n\r\n";
        int n = 0;
        for (int idx : app.selected) {
            if (n++ >= 12) { t += "...\r\n"; break; }
            const IplInstance& o = app.instances[idx];
            t += o.model + "  (" + num(o.x, 1) + ", " + num(o.y, 1) + ", " + num(o.z, 1) + ")\r\n";
        }
        return t;
    }
    const int idx = app.selected[0];
    const IplInstance& o = app.instances[idx];
    std::string t = "Object: " + o.model + "\r\n";
    auto d = app.defs.find(o.id);
    if (d != app.defs.end()) {
        t += "Texture dictionary: " + d->second.txd + "   draw distance " + num(d->second.drawDistance, 0) + "\r\n";
        if (toLower(d->second.model) != toLower(o.model)) t += "(the IPL names this model differently; the game uses the ID)\r\n";
    } else {
        t += "No IDE definition for this ID: the game ignores this placement.\r\n";
    }
    t += "File: " + app.iplPaths[app.instFile[idx]] + (app.dirty.count(app.instFile[idx]) ? "  (changed, not saved)" : "") + "\r\n";
    t += "Quaternion: " + num(o.rotX, 4) + ", " + num(o.rotY, 4) + ", " + num(o.rotZ, 4) + ", " + num(o.rotW, 4) + "   scale " + num(o.scaleX, 2) + "\r\n";
    auto c = app.cols.find(toLower(o.model));
    t += c != app.cols.end() ? "Collision radius: " + num(c->second.boundRadius, 1) + "\r\n" : "No collision model found for this name.\r\n";
    const std::vector<size_t> partners = lodPartnersOf(idx);
    if (partners.empty()) t += "LOD partner: none\r\n";
    else {
        t += "LOD partner (moved, turned and removed with it):";
        for (size_t pi : partners) t += " " + app.instances[pi].model;
        t += "\r\n";
    }
    if (o.z <= kSunkZ + 0.5) t += "This object is below the map (moved there instead of being deleted).\r\n";
    if (isRoadPiece(idx)) {
        int open = 0;
        const std::vector<RoadEnd>& e = roadEndsOf(o.id);
        for (const RoadEnd& r : e) open += !roadEndJoined(idx, endInWorld(o, r));
        t += "Road piece: " + std::to_string(e.size()) + " ends, " + std::to_string(open) + " open. Click a green ball in the 3D view to attach the next piece.\r\n";
    }
    {  // original or added: worked out from the whole map, so it is cached briefly
        static std::vector<bool> flags;
        static size_t count = 0;
        static int forIdx = -1;
        static DWORD at = 0;
        if (forIdx != idx || count != app.instances.size() || GetTickCount() - at > 500) {
            flags = originalFlags();
            count = app.instances.size();
            forIdx = idx;
            at = GetTickCount();
        }
        if (size_t(idx) < flags.size())
            t += flags[size_t(idx)] ? "Part of the original GTA III map.\r\n" : "Added by you (not in the original game).\r\n";
    }
    return t;
}

// The six values of an object: position, and the rotation as angles.
void fieldValues(const IplInstance& o, double v[6]) {
    v[0] = o.x;
    v[1] = o.y;
    v[2] = o.z;
    quatToEulerDeg(o.rotX, o.rotY, o.rotZ, o.rotW, app.conjugate, v[3], v[4], v[5]);
}

std::string fieldText(int f, double v) { return num(v, f < 3 ? 3 : 2); }

// The placements an edit applies to: the selection and the LOD partners of it.
std::vector<int> editTargets() { return app.selected.empty() ? std::vector<int>() : withLodPartners(app.selected); }

// Move every target by (dx, dy, dz). `session`: one undo step for a run of nudges until the selection changes (MEd's
// rule for moves); otherwise one undo step for this edit alone.
void shiftTargets(double dx, double dy, double dz, bool session) {
    const std::vector<int> all = editTargets();
    if (all.empty()) return;
    if (!session || app.history.needsSnapshot()) app.history.snapshot(captureState());
    for (int idx : all) {
        app.instances[idx].x += dx;
        app.instances[idx].y += dy;
        app.instances[idx].z += dz;
        app.dirty.insert(app.instFile[idx]);
    }
    if (!session) app.history.endSession();
}

// Change one rotation angle of every target: by `delta`, or set it to `value` (when setTo is true).
void turnTargets(int axis, double amount, bool setTo, bool session) {
    const std::vector<int> all = editTargets();
    if (all.empty()) return;
    if (!session || app.history.needsSnapshot()) app.history.snapshot(captureState());
    for (int idx : all) {
        IplInstance& o = app.instances[idx];
        double e[3];
        quatToEulerDeg(o.rotX, o.rotY, o.rotZ, o.rotW, app.conjugate, e[0], e[1], e[2]);
        e[axis] = setTo ? amount : e[axis] + amount;
        eulerDegToQuat(e[0], e[1], e[2], app.conjugate, o.rotX, o.rotY, o.rotZ, o.rotW);
        app.dirty.insert(app.instFile[idx]);
    }
    if (!session) app.history.endSession();
}

void resetRotation() {
    const std::vector<int> all = editTargets();
    if (all.empty()) return;
    app.history.snapshot(captureState());
    for (int idx : all) {
        IplInstance& o = app.instances[idx];
        o.rotX = o.rotY = o.rotZ = 0;
        o.rotW = 1;
        app.dirty.insert(app.instFile[idx]);
    }
    app.history.endSession();
}

// The id of the LOD stand-in model that goes with a building model ("LOD" + the name without its first three letters,
// core lodModelNameFor), or -1 when the IDE files define none (most objects have no LOD, and a LOD has none itself).
int lodModelIdFor(int buildingId) {
    auto b = app.defs.find(buildingId);
    if (b == app.defs.end() || b->second.farModel) return -1;
    const std::string want = toLower(lodModelNameFor(b->second.name));
    if (want.empty()) return -1;
    for (const auto& kv : app.defs)
        if (kv.second.model == want) return kv.first;
    return -1;
}

// Per placement: true = part of the original GTA III map, false = added by the user (core/original.h). Worked out from
// the current placements, so it is right after saving and reopening as well.
std::vector<bool> originalFlags() {
    std::vector<std::string> files(app.instances.size());
    for (size_t i = 0; i < files.size(); ++i) files[i] = app.iplPaths[size_t(app.instFile[i])];
    size_t n = 0;
    const OriginalEntry* table = gta3OriginalTable(n);
    return classifyOriginal(app.instances, files, table, n);
}

// What a replace did to the LOD stand-ins (for the command line and the self-test).
struct ReplaceReport {
    int replaced = 0;     // placements that got the new model
    int lodSwitched = 0;  // stand-ins that got the new building's LOD model (only those that had another model)
    int lodSunk = 0;      // stand-ins moved below the map because the new building has no LOD model (not those already there)
    int lodAdded = 0;     // stand-ins added because the old building had none and the new one has a LOD model
    int lodRemoved = 0;   // stand-ins the user had added (not original) removed from the file instead of moved below the map
    std::vector<int> removedIdx;  // their indices before the removal
};

// Give the selected placements another model, keeping position and rotation (MEd's CMapView::replaceObject, 0x00460aa0
// with the chooser and 0x0045ce00 for the ID typed in its properties). Returns false (with a warning) for an ID that no
// IDE defines; does nothing (no undo step) when every selected placement already has that model.
// A building and its LOD stand-in are replaced as a pair (the user's rule, 2026-10-12; MEd changes only the selected
// placement and so left the old building's stand-in showing from far away):
//   - the new model has a LOD model: every stand-in of the old building gets it (same place);
//   - it has none: the old stand-ins go. An ORIGINAL stand-in is moved below the map (kSunkZ, the safe delete); one the
//     user added (a duplicate, a paste, ...; originalFlags) is removed from the file, which is safe for something the
//     original game never had (the user's rule, 2026-10-12);
//   - the old building had no stand-in but the new model has a LOD model: one is added at the building's position and
//     rotation, in the same file (it takes a slot of the Building pool).
// A selected stand-in of a selected building is handled as that building's stand-in, not given the building model.
// One undo step. Deliberate difference: MEd takes its undo snapshot before its chooser opens, so a cancelled replace
// still left an undo step; here only a real change takes one.
bool setModelId(HWND owner, int id, ReplaceReport* report = nullptr) {
    ReplaceReport rep;
    if (report) *report = rep;
    if (app.selected.empty()) return false;
    auto d = app.defs.find(id);
    if (d == app.defs.end()) {
        inspWarn(owner, "No object has the model ID " + std::to_string(id) + ". Press Choose... to pick one from the list.", "Model ID");
        return false;
    }
    // pair up before anything changes (the pairing goes by model name)
    std::vector<int> direct;                    // placements that get the new model
    std::vector<std::vector<int>> standInsOf;   // per entry of `direct`: its stand-ins
    std::vector<bool> wasBuilding;              // per entry of `direct`: not a far model before (only a building gets a new stand-in)
    std::set<int> standIns;
    for (int idx : app.selected) {
        if (app.defs[app.instances[idx].id].farModel) continue;
        for (size_t p : lodPartnersOf(idx))
            if (app.defs[app.instances[p].id].farModel) standIns.insert(int(p));
    }
    for (int idx : app.selected) {
        if (standIns.count(idx) || app.instances[idx].id == id) continue;
        direct.push_back(idx);
        wasBuilding.push_back(!app.defs[app.instances[idx].id].farModel);
        std::vector<int> mine;
        if (!app.defs[app.instances[idx].id].farModel)
            for (size_t p : lodPartnersOf(idx))
                if (app.defs[app.instances[p].id].farModel) mine.push_back(int(p));
        standInsOf.push_back(mine);
    }
    if (direct.empty()) return true;
    const int newLod = lodModelIdFor(id);
    const std::vector<bool> original = originalFlags();  // before anything changes
    std::vector<int> toRemove;
    app.history.snapshot(captureState());
    std::set<int> done;  // a stand-in shared by two buildings is changed once
    auto retire = [&](int s) {  // an old stand-in that has no place any more
        if (!original[size_t(s)]) {
            toRemove.push_back(s);
            ++rep.lodRemoved;
        } else {
            rep.lodSunk += app.instances[s].z != kSunkZ;
            app.instances[s].z = kSunkZ;
        }
        app.dirty.insert(app.instFile[s]);
    };
    for (size_t i = 0; i < direct.size(); ++i) {
        const int idx = direct[i];
        app.instances[idx].id = id;
        app.instances[idx].model = d->second.name;
        app.dirty.insert(app.instFile[idx]);
        ++rep.replaced;
        if (d->second.farModel) {  // a building turned into a far model: its old stand-in would show the old building
            for (int s : standInsOf[i])
                if (done.insert(s).second) retire(s);
            continue;
        }
        if (!standInsOf[i].empty()) {
            for (int s : standInsOf[i]) {
                if (!done.insert(s).second) continue;
                if (newLod >= 0) {
                    rep.lodSwitched += app.instances[s].id != newLod;
                    app.instances[s].id = newLod;
                    app.instances[s].model = app.defs[newLod].name;
                    app.dirty.insert(app.instFile[s]);
                } else {
                    retire(s);
                }
            }
        } else if (newLod >= 0 && wasBuilding[i]) {
            IplInstance lod = app.instances[idx];
            lod.id = newLod;
            lod.model = app.defs[newLod].name;
            app.instances.push_back(lod);
            app.instFile.push_back(app.instFile[idx]);
            ++rep.lodAdded;
        }
    }
    if (!toRemove.empty()) {
        rep.removedIdx = toRemove;
        std::sort(rep.removedIdx.begin(), rep.removedIdx.end());
        removePlacements(toRemove);
        app.selected = shiftAfterRemoval(app.selected, toRemove);
    }
    app.history.endSession();
    buildLodPartnerRanges();
    refreshRegions();  // counts change when a stand-in was added
    if (report) *report = rep;
    return true;
}

// Replace Object (right-click menu, the Properties panel's Choose...): pick the new model in the item chooser, which
// starts on the current one. Position and rotation stay; the selection stays on the replaced placements.
void replaceSelectedUi(HWND owner) {
    if (app.selected.empty()) {
        inspWarn(owner, "Select the object to replace first (double-click it).", "Replace Object");
        return;
    }
    const int current = app.instances[app.selected[0]].id;
    const std::string title = app.selected.size() == 1 ? "Replace Object - choose the new model for " + app.instances[app.selected[0]].model + " (type to search)"
                                                       : "Replace Objects - choose the new model for the " + std::to_string(app.selected.size()) + " selected objects (type to search)";
    int chosen = current;
    if (chooseObject(owner, chosen, title.c_str(), "Replace", current)) setModelId(owner, chosen);
}

// A nudge button: field 0..2 moves, 3..5 turns, k = 0..3 is <<, <, >, >>. Runs of nudges are one undo step.
void nudge(int buttonId) {
    const int f = (buttonId - ID_INS_NUDGE) / 4, k = (buttonId - ID_INS_NUDGE) % 4;
    if (f < 0 || f > 5) return;
    if (f < 3) shiftTargets(f == 0 ? kPosSteps[k] : 0, f == 1 ? kPosSteps[k] : 0, f == 2 ? kPosSteps[k] : 0, true);
    else turnTargets(f - 3, kRotSteps[k], false, true);
}

// Apply what was typed: only the boxes whose text differs from what the panel last showed, so an untouched box (rounded
// to 3 or 2 decimals) never nudges anything. Position: the first selected object goes exactly to the typed value and
// the others keep their offsets from it. Rotation: every selected object gets the typed angle on that axis.
void applyTyped(HWND owner) {
    if (app.selected.empty()) return;
    char b[64];
    GetWindowTextA(insp.idBox, b, sizeof b);
    if (trim(b) != insp.shownText[6]) {
        char* end = nullptr;
        const long id = std::strtol(b, &end, 10);
        if (end == b || *end != 0) inspWarn(owner, "The model ID must be a whole number.", "Model ID");
        else setModelId(owner, int(id));
    }
    bool posChanged = false;
    double target[3];
    const IplInstance& first = app.instances[app.selected[0]];
    double cur[6];
    fieldValues(first, cur);
    for (int f = 0; f < 6; ++f) {
        GetWindowTextA(insp.field[f], b, sizeof b);
        const std::string text = trim(b);
        if (f < 3) target[f] = cur[f];
        if (text == insp.shownText[f]) continue;
        char* end = nullptr;
        const double v = std::strtod(text.c_str(), &end);
        if (text.empty() || end == text.c_str() || *end != 0 || !std::isfinite(v)) {
            inspWarn(owner, std::string(f < 3 ? "Position " : "Rotation ") + kFieldNames[f] + " must be a number.", "Properties");
            SetWindowTextA(insp.field[f], insp.shownText[f].c_str());
            continue;
        }
        if (f < 3) { target[f] = v; posChanged = true; }
        else turnTargets(f - 3, v, true, false);
    }
    if (posChanged) {
        const int firstIdx = app.selected[0];
        const double dx = target[0] - app.instances[firstIdx].x, dy = target[1] - app.instances[firstIdx].y, dz = target[2] - app.instances[firstIdx].z;
        shiftTargets(dx, dy, dz, false);
        app.instances[firstIdx].x = target[0];  // exactly what was typed, not x + (typed - x)
        app.instances[firstIdx].y = target[1];
        app.instances[firstIdx].z = target[2];
    }
    for (std::string& s : insp.shownText) s = "\x01";  // show the stored values again
    plRefreshAfterEdit();
}

void refreshInspector() {
    if (!insp.info) return;
    const std::string text = inspectorText();
    if (text != insp.shownInfo) {
        SetWindowTextA(insp.info, text.c_str());
        insp.shownInfo = text;
    }
    const bool any = !app.selected.empty();
    for (HWND e : insp.field) EnableWindow(e, any);
    EnableWindow(insp.idBox, any);
    for (int id : {ID_INS_CHOOSE, ID_INS_APPLY, ID_INS_CENTRE, ID_INS_SINK, ID_INS_HARD, ID_INS_ROTM90, ID_INS_ROTP90, ID_INS_RESET}) EnableWindow(GetDlgItem(app.inspector, id), any);
    for (int i = 0; i < 24; ++i) EnableWindow(GetDlgItem(app.inspector, ID_INS_NUDGE + i), any);
    std::string want[7];
    if (any) {
        const IplInstance& o = app.instances[app.selected[0]];
        double v[6];
        fieldValues(o, v);
        for (int f = 0; f < 6; ++f) want[f] = fieldText(f, v[f]);
        want[6] = std::to_string(o.id);
    }
    const HWND focus = GetFocus();
    for (int f = 0; f < 7; ++f) {
        if (want[f] == insp.shownText[f]) continue;
        HWND e = f < 6 ? insp.field[f] : insp.idBox;
        char b[64];
        GetWindowTextA(e, b, sizeof b);
        if (e == focus && any && trim(b) != insp.shownText[f] && insp.shownText[f] != "\x01") continue;  // the user is typing here
        SetWindowTextA(e, want[f].c_str());
        insp.shownText[f] = want[f];
    }
}

// Enter in a box applies the typed values; Esc puts back what is stored. Called in the main loop before dispatch.
bool inspHandleKey(const MSG& m) {
    if (!insp.info || m.message != WM_KEYDOWN) return false;
    int which = -1;
    for (int f = 0; f < 6; ++f) if (m.hwnd == insp.field[f]) which = f;
    if (m.hwnd == insp.idBox) which = 6;
    if (which < 0) return false;
    if (m.wParam == VK_RETURN) {
        applyTyped(app.inspector);
        refreshInspector();
        SendMessageA(m.hwnd, EM_SETSEL, 0, -1);
        return true;
    }
    if (m.wParam == VK_ESCAPE) {
        SetWindowTextA(m.hwnd, insp.shownText[which].c_str());
        SendMessageA(m.hwnd, EM_SETSEL, 0, -1);
        return true;
    }
    return false;
}

// The nudge buttons repeat while held: one step on press, then after 0.4 s every 60 ms. The list's sort order and the
// rest follow when the button is let go.
LRESULT CALLBACK nudgeButtonProc(HWND b, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            if (IsWindowEnabled(b)) {
                insp.repeatId = GetDlgCtrlID(b);
                insp.repeatFired = true;
                nudge(insp.repeatId);
                SetTimer(b, 1, 400, nullptr);
            }
            break;
        case WM_TIMER:
            if (wp == 1) {
                if (insp.repeatId == GetDlgCtrlID(b) && (insp.testHolding || (GetKeyState(VK_LBUTTON) & 0x8000))) {
                    nudge(insp.repeatId);
                    SetTimer(b, 1, 60, nullptr);
                } else {
                    if (std::getenv("MED_DEBUG")) { std::printf("nudge repeat stops: held id %d, this %d, mouse button %s\n", insp.repeatId, GetDlgCtrlID(b), (GetKeyState(VK_LBUTTON) & 0x8000) ? "down" : "up"); std::fflush(stdout); }
                    KillTimer(b, 1);
                }
                return 0;
            }
            break;
        case WM_LBUTTONUP: {
            const LRESULT r = CallWindowProcA(insp.buttonProc, b, msg, wp, lp);  // sends BN_CLICKED, which repeatFired skips
            KillTimer(b, 1);
            if (insp.repeatId) { insp.repeatId = 0; plRefreshAfterEdit(); }
            insp.repeatFired = false;
            return r;
        }
        case WM_CAPTURECHANGED:
        case WM_KILLFOCUS:
            if (insp.testHolding) break;  // self-test: the window may be in the background, where Windows takes capture and focus away
            if (std::getenv("MED_DEBUG") && insp.repeatId) { std::printf("nudge repeat stops: %s\n", msg == WM_KILLFOCUS ? "focus lost" : "capture lost"); std::fflush(stdout); }
            KillTimer(b, 1);
            if (insp.repeatId) { insp.repeatId = 0; plRefreshAfterEdit(); }
            break;
    }
    return CallWindowProcA(insp.buttonProc, b, msg, wp, lp);
}

void inspAddTip(HWND parent, HWND ctrl, const char* text) {
    if (!insp.tips) return;
    TOOLINFOA ti = {};
    ti.cbSize = sizeof ti;
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd = parent;
    ti.uId = (UINT_PTR)ctrl;
    ti.lpszText = const_cast<char*>(text);
    SendMessageA(insp.tips, TTM_ADDTOOLA, 0, (LPARAM)&ti);
}

LRESULT CALLBACK inspectorProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            auto make = [&](const char* cls, const char* text, DWORD style, DWORD ex, int id) {
                HWND c = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, w, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
                SendMessageA(c, WM_SETFONT, (WPARAM)font, TRUE);
                return c;
            };
            insp.tips = CreateWindowExA(WS_EX_TOPMOST, TOOLTIPS_CLASSA, nullptr, WS_POPUP | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, w,
                                        nullptr, GetModuleHandle(nullptr), nullptr);
            insp.info = make("EDIT", "", ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL, WS_EX_CLIENTEDGE, ID_INS_INFO);
            make("STATIC", "Model ID", SS_LEFT, 0, ID_INS_LABEL + 0);
            insp.idBox = make("EDIT", "", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_INS_IDBOX);
            HWND choose = make("BUTTON", "Choose...", BS_PUSHBUTTON, 0, ID_INS_CHOOSE);
            make("STATIC", "Position", SS_LEFT, 0, ID_INS_LABEL + 1);
            make("STATIC", "Rotation (degrees)", SS_LEFT, 0, ID_INS_LABEL + 2);
            const char* arrows[4] = {"<<", "<", ">", ">>"};
            for (int f = 0; f < 6; ++f) {
                make("STATIC", kFieldNames[f], SS_LEFT, 0, ID_INS_LABEL + 3 + f);
                insp.field[f] = make("EDIT", "", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_INS_FIELD + f);
                inspAddTip(w, insp.field[f], f < 3 ? "Type a position and press Enter (or Apply). Esc puts the stored value back."
                                                   : "Type an angle in degrees and press Enter (or Apply). Every selected object gets this angle.");
                for (int k = 0; k < 4; ++k) {
                    HWND bt = make("BUTTON", arrows[k], BS_PUSHBUTTON, 0, ID_INS_NUDGE + f * 4 + k);
                    WNDPROC old = (WNDPROC)SetWindowLongPtrA(bt, GWLP_WNDPROC, (LONG_PTR)nudgeButtonProc);
                    if (!insp.buttonProc) insp.buttonProc = old;
                    static const char* posTips[4] = {"-10 (hold to repeat)", "-0.1 (hold to repeat)", "+0.1 (hold to repeat)", "+10 (hold to repeat)"};
                    static const char* rotTips[4] = {"-10 degrees (hold to repeat)", "-0.2 degrees (hold to repeat)", "+0.2 degrees (hold to repeat)", "+10 degrees (hold to repeat)"};
                    inspAddTip(w, bt, f < 3 ? posTips[k] : rotTips[k]);
                }
            }
            HWND m90 = make("BUTTON", "Rotate -90", BS_PUSHBUTTON, 0, ID_INS_ROTM90);
            HWND p90 = make("BUTTON", "Rotate +90", BS_PUSHBUTTON, 0, ID_INS_ROTP90);
            HWND reset = make("BUTTON", "Reset", BS_PUSHBUTTON, 0, ID_INS_RESET);
            HWND apply = make("BUTTON", "Apply typed values", BS_PUSHBUTTON, 0, ID_INS_APPLY);
            make("BUTTON", "Centre on Object", BS_PUSHBUTTON, 0, ID_INS_CENTRE);
            HWND sink = make("BUTTON", "Delete (below the map)", BS_PUSHBUTTON, 0, ID_INS_SINK);
            make("BUTTON", "Delete permanently", BS_PUSHBUTTON, 0, ID_INS_HARD);
            inspAddTip(w, insp.idBox, "The model of the selected objects. Type an ID and press Enter, or press Choose...");
            inspAddTip(w, choose, "Replace the model of the selected objects with one from the list");
            inspAddTip(w, m90, "Quarter turn clockwise seen from above");
            inspAddTip(w, p90, "Quarter turn counter-clockwise seen from above");
            inspAddTip(w, reset, "No rotation at all (upright, facing the default way)");
            inspAddTip(w, apply, "Apply what you typed in the boxes (Enter in a box does the same)");
            inspAddTip(w, sink, "Moves it to z = -200 with its LOD stand-in: the safe delete");
            return 0;
        }
        case WM_SIZE: {
            const int cw = LOWORD(lp), ch = HIWORD(lp), m = 6, iw = cw - 2 * m;
            const int rowH = 25, below = 25 + 18 + 3 * rowH + 18 + 3 * rowH + 28 + 28 + 28 + 28 + 8;
            int y = m;
            const int infoH = std::max(90, ch - below - m);
            MoveWindow(insp.info, m, y, iw, infoH, TRUE);
            y += infoH + 6;
            MoveWindow(GetDlgItem(w, ID_INS_LABEL + 0), m, y + 4, 52, 16, TRUE);
            MoveWindow(insp.idBox, m + 54, y, iw - 54 - 78, 22, TRUE);
            MoveWindow(GetDlgItem(w, ID_INS_CHOOSE), m + iw - 74, y, 74, 22, TRUE);
            y += 25;
            const int nb = 26, lab = 12, box = iw - lab - 4 * nb - 4;
            for (int f = 0; f < 6; ++f) {
                if (f == 0 || f == 3) {
                    MoveWindow(GetDlgItem(w, ID_INS_LABEL + (f == 0 ? 1 : 2)), m, y + 2, iw, 16, TRUE);
                    y += 18;
                }
                MoveWindow(GetDlgItem(w, ID_INS_LABEL + 3 + f), m, y + 4, lab, 16, TRUE);
                MoveWindow(insp.field[f], m + lab, y, box, 22, TRUE);
                for (int k = 0; k < 4; ++k) MoveWindow(GetDlgItem(w, ID_INS_NUDGE + f * 4 + k), m + lab + box + 4 + k * nb, y, nb, 22, TRUE);
                y += rowH;
            }
            const int third = (iw - 8) / 3;
            MoveWindow(GetDlgItem(w, ID_INS_ROTM90), m, y, third, 24, TRUE);
            MoveWindow(GetDlgItem(w, ID_INS_ROTP90), m + third + 4, y, third, 24, TRUE);
            MoveWindow(GetDlgItem(w, ID_INS_RESET), m + 2 * (third + 4), y, third, 24, TRUE);
            y += 28;
            MoveWindow(GetDlgItem(w, ID_INS_APPLY), m, y, iw, 24, TRUE); y += 28;
            MoveWindow(GetDlgItem(w, ID_INS_CENTRE), m, y, iw, 24, TRUE); y += 28;
            const int half = (iw - 4) / 2;
            MoveWindow(GetDlgItem(w, ID_INS_SINK), m, y, half, 24, TRUE);
            MoveWindow(GetDlgItem(w, ID_INS_HARD), m + half + 4, y, half, 24, TRUE);
            return 0;
        }
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            if (HIWORD(wp) != BN_CLICKED) return 0;
            if (id >= ID_INS_NUDGE && id < ID_INS_NUDGE + 24) {
                if (insp.repeatFired) return 0;  // already done on press (mouse); this one is the keyboard (Space)
                nudge(id);
                plRefreshAfterEdit();
                return 0;  // keep the focus on the button so Space can repeat
            }
            if (id == ID_INS_APPLY) applyTyped(w);
            else if (id == ID_INS_CHOOSE && !app.selected.empty()) {
                replaceSelectedUi(GetParent(w));
            } else if (id == ID_INS_CENTRE && !app.selected.empty()) centreOnObject(app.selected[0]);
            else if (id == ID_INS_SINK) sinkSelected();
            else if (id == ID_INS_HARD) {
                if (MessageBoxA(w, "Remove the selected object(s) and their LOD partners from the IPL file when you save?\n\nThe game can crash when objects are removed (see the notes); moving them below the map is safer.",
                                "Delete permanently", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) == IDYES)
                    deleteSelected();
            } else if (id == ID_INS_ROTP90 || id == ID_INS_ROTM90) rotateSelected(id == ID_INS_ROTP90 ? 90 : -90);
            else if (id == ID_INS_RESET) resetRotation();
            SetFocus(GetParent(w));  // the keyboard goes back to the map view
            return 0;
        }
    }
    return DefWindowProcA(w, msg, wp, lp);
}

void createInspector(HWND parent) {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = inspectorProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "MEdInspector";
    RegisterClassA(&wc);
    app.inspectorW = 300;
    app.inspector = CreateWindowExA(0, "MEdInspector", "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, app.inspectorW, 400, parent, nullptr,
                                    GetModuleHandle(nullptr), nullptr);
}

// ---- Scene panel: which regions (IPL files) are drawn ----------------------------------------------
// MEd's Scene tab lists the item placement files with a check box each ("Render"); only checked files are
// built into the scene and picked (CMapView, the visible flag at +0x19 of each file in paintGL, picking and
// the display list builder 0x00459d70). Double-clicking a file centres the map on it, and Select/Deselect all
// are there too.

struct Region {
    int file = 0;  // index into app.iplPaths
    std::string name;
    int count = 0;
    double minX = 1e30, maxX = -1e30, minY = 1e30, maxY = -1e30, minZ = 1e30, maxZ = -1e30;
};
std::vector<Region> regions;

// Regions that held no placements when the map was loaded (GTA3.ZON and CULL.IPL in GTA III: zones and cull boxes, no
// objects) are left out of the Scene and Placements lists unless View > Show hidden regions is ticked. A region added with
// New region..., or one emptied by moving its objects away, is always listed.
bool regionListed(int file) { return app.showEmptyRegions || file < 0 || file >= int(app.regionHideable.size()) || !app.regionHideable[file]; }

enum { ID_SC_LIST = 4001, ID_SC_ALL, ID_SC_NONE, ID_SC_ONLY, ID_SC_INFO, ID_SC_LODSHOW, ID_SC_LODLABEL, ID_SC_LODSLIDER, ID_SC_NEW, ID_SC_MOVE };

struct SceneUi {
    HWND list = nullptr, info = nullptr;
    bool populating = false;
    int shownVisible = -1, shownTotal = -1;
} scene;

// For every far stand-in that has a building partner (same name tail, same place: findLodPartners), remember
// how far that building is drawn. The LOD is shown only once the camera is farther away than that.
void buildLodPartnerRanges() {
    app.lodPartnerMin.assign(app.instances.size(), 0.0f);
    std::unordered_map<std::string, std::vector<int>> byTail;
    for (size_t i = 0; i < app.instances.size(); ++i) {
        const std::string& n = app.instances[i].model;
        if (n.size() > 3) byTail[toLower(n.substr(3))].push_back(int(i));
    }
    for (size_t i = 0; i < app.instances.size(); ++i) {
        const IplInstance& lod = app.instances[i];
        auto d = app.defs.find(lod.id);
        if (d == app.defs.end() || !d->second.farModel || lod.model.size() <= 3) continue;
        auto it = byTail.find(toLower(lod.model.substr(3)));
        if (it == byTail.end()) continue;
        for (int j : it->second) {
            if (j == int(i)) continue;
            auto hd = app.defs.find(app.instances[j].id);
            if (hd == app.defs.end() || hd->second.farModel) continue;  // the partner is the normal model
            const IplInstance& b = app.instances[j];
            const double dist = std::sqrt((lod.x - b.x) * (lod.x - b.x) + (lod.y - b.y) * (lod.y - b.y) + (lod.z - b.z) * (lod.z - b.z));
            if (dist <= 5.0) app.lodPartnerMin[i] = std::max<float>(app.lodPartnerMin[i], float(hd->second.drawDistance));
        }
    }
}

// Count the placements and measure the extent of every region again (after placements moved between files).
void recomputeRegionStats() {
    std::vector<int> index(app.iplPaths.size(), -1);
    for (size_t k = 0; k < regions.size(); ++k) {
        Region& r = regions[k];
        index[r.file] = int(k);
        r.count = 0;
        r.minX = r.minY = r.minZ = 1e30;
        r.maxX = r.maxY = r.maxZ = -1e30;
    }
    for (size_t i = 0; i < app.instances.size(); ++i) {
        const int f = app.instFile[i];
        Region& r = regions[index[f]];
        const IplInstance& o = app.instances[i];
        ++r.count;
        r.minX = std::min(r.minX, o.x); r.maxX = std::max(r.maxX, o.x);
        r.minY = std::min(r.minY, o.y); r.maxY = std::max(r.maxY, o.y);
        r.minZ = std::min(r.minZ, o.z); r.maxZ = std::max(r.maxZ, o.z);
    }
}

// Every IPL file listed in gta3.dat is a region, also an empty one (a new region has no objects yet). Which
// regions are switched on is kept when the list is built again.
void initRegions() {
    const std::vector<char> old = app.regionOn;
    app.regionHideable.resize(app.iplPaths.size(), 0);  // a region added later is never hidden
    app.regionOn.assign(app.iplPaths.size(), 1);
    for (size_t i = 0; i < old.size() && i < app.regionOn.size(); ++i) app.regionOn[i] = old[i];
    regions.clear();
    for (size_t f = 0; f < app.iplPaths.size(); ++f) {
        Region r;
        r.file = int(f);
        const std::string& path = app.iplPaths[f];
        r.name = path.substr(path.find_last_of('/') == std::string::npos ? 0 : path.find_last_of('/') + 1);
        regions.push_back(r);
    }
    recomputeRegionStats();
}

void setRegion(int file, bool on) {
    if (file < 0 || file >= int(app.regionOn.size())) return;
    app.regionOn[file] = on ? 1 : 0;
    if (!on) {  // objects of a hidden region cannot stay selected
        std::vector<int> keep;
        for (int idx : app.selected) if (app.instFile[idx] != file) keep.push_back(idx);
        if (keep.size() != app.selected.size()) { app.selected = keep; app.history.endSession(); }
    }
}

void centreOnRegion(const Region& r) {
    if (r.count == 0) return;  // an empty region has no position
    app.cam.x = float((r.minX + r.maxX) / 2);
    app.cam.y = float((r.minY + r.maxY) / 2 - 100);
    app.cam.z = float(std::max(r.maxZ, 0.0) + 250);
    app.cam.h = 0;
    app.cam.yaw = 0;
    app.cam.pitch = 45;
}

std::vector<int> sceneRows;  // the regions[] index shown in each row of the Scene list (hidden regions are left out)

int sceneRowRegion(int row) { return row >= 0 && row < int(sceneRows.size()) ? sceneRows[row] : -1; }

void fillRegionList() {
    scene.populating = true;
    ListView_DeleteAllItems(scene.list);
    sceneRows.clear();
    for (size_t r = 0; r < regions.size(); ++r) {
        if (!regionListed(regions[r].file)) continue;
        const size_t i = sceneRows.size();
        sceneRows.push_back(int(r));
        LVITEMA it = {};
        it.mask = LVIF_TEXT | LVIF_PARAM;
        it.iItem = int(i);
        it.pszText = const_cast<char*>(regions[r].name.c_str());
        it.lParam = regions[r].file;
        SendMessageA(scene.list, LVM_INSERTITEMA, 0, (LPARAM)&it);
        std::string c = std::to_string(regions[r].count);
        LVITEMA sub = {};
        sub.iSubItem = 1;
        sub.pszText = const_cast<char*>(c.c_str());
        SendMessageA(scene.list, LVM_SETITEMTEXTA, i, (LPARAM)&sub);
        ListView_SetCheckState(scene.list, int(i), app.regionOn[regions[r].file] ? TRUE : FALSE);
    }
    scene.populating = false;
}

// After placements changed file or count (undo, redo, delete, duplicate, move to a region).
void plRefreshAfterEdit();

void refreshRegions() {
    recomputeRegionStats();
    if (scene.list) fillRegionList();
    plRefreshAfterEdit();
}

void refreshScene() {
    if (!scene.info) return;
    int visible = 0, total = 0;
    for (size_t i = 0; i < app.instances.size(); ++i) {
        ++total;
        if (app.regionOn[app.instFile[i]]) ++visible;
    }
    if (visible == scene.shownVisible && total == scene.shownTotal) return;
    char t[200];
    std::snprintf(t, sizeof t, "%d of %d placements are in checked regions.\nOnly checked regions are drawn and can be picked.", visible, total);
    SetWindowTextA(scene.info, t);
    scene.shownVisible = visible;
    scene.shownTotal = total;
}

void setAllRegions(bool on) {
    for (const Region& r : regions) setRegion(r.file, on);
    fillRegionList();
    {
        char t[96];
        std::snprintf(t, sizeof t, "LOD models appear beyond %d units", int(app.lodNear));
        SetWindowTextA(GetDlgItem(app.scenePanel, ID_SC_LODLABEL), t);
    }
}

// ---- a one-line text prompt (modal) ------------------------------------------------------------------

struct PromptState {
    HWND edit = nullptr;
    bool done = false, ok = false;
    std::string text;
};

LRESULT CALLBACK promptProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    PromptState* p = reinterpret_cast<PromptState*>(GetWindowLongPtrA(w, GWLP_USERDATA));
    if (msg == WM_COMMAND && p && (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL)) {
        char b[256] = "";
        GetWindowTextA(p->edit, b, sizeof b);
        p->text = trim(b);
        p->ok = LOWORD(wp) == IDOK;
        p->done = true;
        return 0;
    }
    if (msg == WM_CLOSE && p) {
        p->done = true;
        return 0;
    }
    return DefWindowProcA(w, msg, wp, lp);
}

bool askText(HWND owner, const char* title, const char* label, std::string& value) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc = promptProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "MEdPrompt";
        RegisterClassA(&wc);
        registered = true;
    }
    PromptState p;
    RECT r;
    GetWindowRect(owner, &r);
    HWND w = CreateWindowExA(WS_EX_DLGMODALFRAME, "MEdPrompt", title, WS_POPUP | WS_CAPTION | WS_SYSMENU, r.left + 60, r.top + 120, 420, 130, owner, nullptr,
                             GetModuleHandle(nullptr), nullptr);
    SetWindowLongPtrA(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&p));
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    auto make = [&](const char* cls, const char* text, DWORD style, int x, int y, int cw, int ch, int id) {
        HWND c = CreateWindowExA(style & WS_BORDER ? WS_EX_CLIENTEDGE : 0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, cw, ch, w, (HMENU)(INT_PTR)id,
                                 GetModuleHandle(nullptr), nullptr);
        SendMessageA(c, WM_SETFONT, (WPARAM)font, TRUE);
        return c;
    };
    make("STATIC", label, SS_LEFT, 12, 10, 390, 18, 0);
    p.edit = make("EDIT", value.c_str(), WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 12, 32, 385, 22, 101);  // not 1: that is IDOK, and edit notifications would count as pressing OK
    make("BUTTON", "OK", BS_DEFPUSHBUTTON | WS_TABSTOP, 236, 66, 76, 26, IDOK);
    make("BUTTON", "Cancel", BS_PUSHBUTTON | WS_TABSTOP, 320, 66, 76, 26, IDCANCEL);
    EnableWindow(owner, FALSE);
    ShowWindow(w, SW_SHOW);
    SetFocus(p.edit);
    SendMessageA(p.edit, EM_SETSEL, 0, -1);
    MSG m;
    while (!p.done && GetMessageA(&m, nullptr, 0, 0) > 0) {
        if (m.message == WM_KEYDOWN && m.wParam == VK_RETURN) { SendMessageA(w, WM_COMMAND, IDOK, 0); continue; }
        if (m.message == WM_KEYDOWN && m.wParam == VK_ESCAPE) { SendMessageA(w, WM_COMMAND, IDCANCEL, 0); continue; }
        if (!IsDialogMessageA(w, &m)) {
            TranslateMessage(&m);
            DispatchMessageA(&m);
        }
    }
    EnableWindow(owner, TRUE);
    DestroyWindow(w);
    SetForegroundWindow(owner);
    if (!p.ok) return false;
    value = p.text;
    return true;
}

// "New region...": make an empty IPL file in the opened folder, list it in gta3.dat (core/region.h) and add it to the
// Scene list right away; the list row is selected so "Move selected objects here" can fill it.
bool createRegionNamed(const std::string& name, std::string& rel, std::string& err) {
    if (!addIplRegion(app.root, name, rel, err)) return false;
    app.iplPaths.push_back(rel);
    initRegions();
    fillRegionList();
    plRefreshAfterEdit();
    ListView_SetItemState(scene.list, int(sceneRows.size()) - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);  // the new region is the last row
    ListView_EnsureVisible(scene.list, int(sceneRows.size()) - 1, FALSE);
    return true;
}

void createRegionUi(HWND owner) {
    std::string name;
    if (!askText(owner, "New region", "Name of the new region (letters, digits, _ and -):", name) || name.empty()) return;
    std::string rel, err;
    if (!createRegionNamed(name, rel, err)) {
        MessageBoxA(owner, err.c_str(), "New region", MB_ICONWARNING);
        return;
    }
    const std::string msg = "Created the region " + rel + " and listed it in data/gta3.dat (the original gta3.dat is kept as gta3.dat.med-backup)." +
                            std::string(" Select objects, then press Move selected objects here to put them into it, and Save.");
    MessageBoxA(owner, msg.c_str(), "New region", MB_ICONINFORMATION);
}

// ---- the item chooser (MEd's CIDEChooser): a searchable list of every IDE object -----------------------------

struct ChooserState {
    HWND list = nullptr, search = nullptr;
    std::vector<int> ids;  // the ids shown, in list order
    bool done = false, ok = false;
    int chosen = -1;
    int preselect = -1;  // the model to highlight (Replace: the current one), when the search shows it
};

void fillChooser(ChooserState& c) {
    char b[128] = "";
    GetWindowTextA(c.search, b, sizeof b);
    const std::string q = toLower(trim(b));
    std::vector<std::pair<std::string, int>> rows;
    for (const auto& kv : app.defs) {
        const ModelDef& d = kv.second;
        if (!q.empty() && d.model.find(q) == std::string::npos && toLower(d.txd).find(q) == std::string::npos && std::to_string(kv.first) != q) continue;
        rows.push_back({d.model, kv.first});
    }
    std::sort(rows.begin(), rows.end());
    SendMessageA(c.list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(c.list);
    c.ids.clear();
    for (const auto& r : rows) {
        const ModelDef& d = app.defs[r.second];
        LVITEMA it = {};
        it.mask = LVIF_TEXT;
        it.iItem = int(c.ids.size());
        std::string name = d.name;
        it.pszText = const_cast<char*>(name.c_str());
        ListView_InsertItem(c.list, &it);
        const std::string id = std::to_string(r.second), dist = std::to_string(int(d.drawDistance));
        ListView_SetItemText(c.list, it.iItem, 1, const_cast<char*>(id.c_str()));
        ListView_SetItemText(c.list, it.iItem, 2, const_cast<char*>(d.txd.c_str()));
        ListView_SetItemText(c.list, it.iItem, 3, const_cast<char*>(dist.c_str()));
        c.ids.push_back(r.second);
    }
    // highlight: the model typed in full, else the first whose name starts with the text, else the preselected one
    // (Replace: the current model), else the first row. A search also finds texture dictionaries, so without this
    // "supasave" would highlight ind_land098 (texture indsupasave) instead of SupaSave.
    int row = -1, starts = -1, pre = -1;
    for (size_t i = 0; i < c.ids.size(); ++i) {
        const std::string& m = app.defs[c.ids[i]].model;
        if (!q.empty() && m == q && row < 0) row = int(i);
        if (!q.empty() && m.compare(0, q.size(), q) == 0 && starts < 0) starts = int(i);
        if (c.ids[i] == c.preselect) pre = int(i);
    }
    if (row < 0) row = starts;
    if (row < 0) row = pre;
    if (row < 0) row = 0;
    if (!c.ids.empty()) ListView_SetItemState(c.list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    SendMessageA(c.list, WM_SETREDRAW, TRUE, 0);
    if (!c.ids.empty()) ListView_EnsureVisible(c.list, row, FALSE);
}

LRESULT CALLBACK chooserProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    ChooserState* c = reinterpret_cast<ChooserState*>(GetWindowLongPtrA(w, GWLP_USERDATA));
    if (!c) return DefWindowProcA(w, msg, wp, lp);
    auto accept = [&]() {
        const int sel = ListView_GetNextItem(c->list, -1, LVNI_SELECTED);
        if (sel < 0 || sel >= int(c->ids.size())) return;
        c->chosen = c->ids[sel];
        c->ok = true;
        c->done = true;
    };
    switch (msg) {
        case WM_SIZE: {
            const int cw = LOWORD(lp), ch = HIWORD(lp);
            MoveWindow(c->search, 8, 8, cw - 16, 22, TRUE);
            MoveWindow(c->list, 8, 38, cw - 16, ch - 38 - 40, TRUE);
            MoveWindow(GetDlgItem(w, IDOK), cw - 8 - 76 - 84, ch - 32, 76, 26, TRUE);
            MoveWindow(GetDlgItem(w, IDCANCEL), cw - 8 - 76, ch - 32, 76, 26, TRUE);
            return 0;
        }
        case WM_COMMAND:
            if (std::getenv("MED_DEBUG")) { std::printf("chooser WM_COMMAND id %d code %d\n", int(LOWORD(wp)), int(HIWORD(wp))); std::fflush(stdout); }
            if (LOWORD(wp) == IDOK) accept();
            else if (LOWORD(wp) == IDCANCEL) c->done = true;
            else if (reinterpret_cast<HWND>(lp) == c->search && HIWORD(wp) == EN_CHANGE) fillChooser(*c);
            return 0;
        case WM_NOTIFY: {
            NMHDR* h = reinterpret_cast<NMHDR*>(lp);
            if (std::getenv("MED_DEBUG") && h->hwndFrom == c->list) { std::printf("chooser list notify code %d\n", int(h->code)); std::fflush(stdout); }
            if (h->hwndFrom == c->list && h->code == NM_DBLCLK) accept();
            return 0;
        }
        case WM_GETMINMAXINFO:
            reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize = {420, 300};
            return 0;
        case WM_CLOSE:
            c->done = true;
            return 0;
    }
    return DefWindowProcA(w, msg, wp, lp);
}

// Choose an object from the IDE files (search by name, texture dictionary or id). False when cancelled.
bool chooseObject(HWND owner, int& id, const char* title, const char* okText, int preselect) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc = chooserProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "MEdChooser";
        RegisterClassA(&wc);
        registered = true;
    }
    ChooserState c;
    c.preselect = preselect;
    RECT r;
    GetWindowRect(owner, &r);
    HWND w = CreateWindowExA(WS_EX_DLGMODALFRAME, "MEdChooser", title ? title : "New Object - choose an object (type to search)", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, r.left + 80,
                             r.top + 80, 560, 520, owner, nullptr, GetModuleHandle(nullptr), nullptr);
    SetWindowLongPtrA(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&c));
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    auto make = [&](const char* cls, const char* text, DWORD style, DWORD ex, int id2) {
        HWND h = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, w, (HMENU)(INT_PTR)id2, GetModuleHandle(nullptr), nullptr);
        SendMessageA(h, WM_SETFONT, (WPARAM)font, TRUE);
        return h;
    };
    c.search = make("EDIT", "", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, 101);  // not 1 (IDOK)
    c.list = make(WC_LISTVIEWA, "", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP, WS_EX_CLIENTEDGE, 102);  // not 2 (IDCANCEL)
    ListView_SetExtendedListViewStyle(c.list, LVS_EX_FULLROWSELECT);
    const char* cols[4] = {"Model", "ID", "Texture dictionary", "Draw distance"};
    const int widths[4] = {170, 60, 170, 90};
    for (int i = 0; i < 4; ++i) {
        LVCOLUMNA col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = const_cast<char*>(cols[i]);
        col.cx = widths[i];
        ListView_InsertColumn(c.list, i, &col);
    }
    make("BUTTON", okText ? okText : "Place", BS_DEFPUSHBUTTON | WS_TABSTOP, 0, IDOK);
    make("BUTTON", "Cancel", BS_PUSHBUTTON | WS_TABSTOP, 0, IDCANCEL);
    RECT cr;
    GetClientRect(w, &cr);
    SendMessageA(w, WM_SIZE, 0, MAKELPARAM(cr.right, cr.bottom));
    fillChooser(c);
    EnableWindow(owner, FALSE);
    ShowWindow(w, SW_SHOW);
    SetFocus(c.search);
    MSG m;
    while (!c.done && GetMessageA(&m, nullptr, 0, 0) > 0) {
        if (m.message == WM_KEYDOWN && (m.hwnd == c.search || m.hwnd == c.list)) {
            if (m.wParam == VK_RETURN) { SendMessageA(w, WM_COMMAND, IDOK, 0); continue; }
            if (m.wParam == VK_ESCAPE) { c.done = true; continue; }
            if (m.hwnd == c.search && (m.wParam == VK_DOWN || m.wParam == VK_UP)) {  // arrow keys move through the list from the search box
                const int sel = ListView_GetNextItem(c.list, -1, LVNI_SELECTED), to = sel + (m.wParam == VK_DOWN ? 1 : -1);
                if (to >= 0 && to < int(c.ids.size())) {
                    ListView_SetItemState(c.list, to, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                    ListView_EnsureVisible(c.list, to, FALSE);
                }
                continue;
            }
        }
        if (!IsDialogMessageA(w, &m)) {
            TranslateMessage(&m);
            DispatchMessageA(&m);
        }
    }
    EnableWindow(owner, TRUE);
    DestroyWindow(w);
    SetForegroundWindow(owner);
    if (!c.ok) return false;
    id = c.chosen;
    return true;
}

// MEd puts a new object into the IPL chosen in its Placements window and refuses when none is chosen or when that file is
// hidden. Here that is the region selected in the Scene list; without one, the file of the selected object.
int placementsRegion();

bool newObjectTargetFile(int& file, std::string& problem) {
    int sel = scene.list ? ListView_GetNextItem(scene.list, -1, LVNI_SELECTED) : -1;
    if (placementsRegion() >= 0) file = placementsRegion();
    else if (sceneRowRegion(sel) >= 0) file = regions[sceneRowRegion(sel)].file;
    else if (!app.selected.empty()) file = app.instFile[app.selected[0]];
    else {
        problem = "Pick the region it should go into first: in the Placements tab (Region), or click it in the Scene list (New region... makes one). Or select an object, and its region is used.";
        return false;
    }
    if (!app.regionOn[file]) {
        problem = "That region is not visible. Tick it in the Scene list first, so you can see what you add.";
        return false;
    }
    return true;
}

LRESULT CALLBACK sceneProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            auto make = [&](const char* cls, const char* text, DWORD style, DWORD ex, int id) {
                HWND c = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, w, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
                SendMessageA(c, WM_SETFONT, (WPARAM)font, TRUE);
                return c;
            };
            scene.list = make(WC_LISTVIEWA, "", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE, ID_SC_LIST);
            ListView_SetExtendedListViewStyle(scene.list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
            LVCOLUMNA c1 = {};
            c1.mask = LVCF_TEXT | LVCF_WIDTH;
            c1.pszText = const_cast<char*>("Region (IPL file)");
            c1.cx = 140;
            SendMessageA(scene.list, LVM_INSERTCOLUMNA, 0, (LPARAM)&c1);
            LVCOLUMNA c2 = {};
            c2.mask = LVCF_TEXT | LVCF_WIDTH;
            c2.pszText = const_cast<char*>("Objects");
            c2.cx = 60;
            SendMessageA(scene.list, LVM_INSERTCOLUMNA, 1, (LPARAM)&c2);
            make("BUTTON", "Select all", BS_PUSHBUTTON, 0, ID_SC_ALL);
            make("BUTTON", "Deselect all", BS_PUSHBUTTON, 0, ID_SC_NONE);
            make("BUTTON", "Only the selected row", BS_PUSHBUTTON, 0, ID_SC_ONLY);
            make("BUTTON", "New region...", BS_PUSHBUTTON, 0, ID_SC_NEW);
            make("BUTTON", "Move selected objects here", BS_PUSHBUTTON, 0, ID_SC_MOVE);
            scene.info = make("STATIC", "", SS_LEFT, 0, ID_SC_INFO);
            HWND show = make("BUTTON", "Show LOD models (far stand-ins)", BS_AUTOCHECKBOX, 0, ID_SC_LODSHOW);
            SendMessageA(show, BM_SETCHECK, app.showLod ? BST_CHECKED : BST_UNCHECKED, 0);
            make("STATIC", "", SS_LEFT, 0, ID_SC_LODLABEL);
            HWND slider = make(TRACKBAR_CLASSA, "", TBS_HORZ | TBS_AUTOTICKS, 0, ID_SC_LODSLIDER);
            SendMessageA(slider, TBM_SETRANGE, TRUE, MAKELONG(0, 1500));
            SendMessageA(slider, TBM_SETTICFREQ, 250, 0);
            SendMessageA(slider, TBM_SETPOS, TRUE, int(app.lodNear));
            return 0;
        }
        case WM_SIZE: {
            const int cw = LOWORD(lp), ch = HIWORD(lp), m = 6;
            const int below = 6 * 28 + 44 + 96;
            MoveWindow(scene.list, m, m, cw - 2 * m, std::max(80, ch - below - m), TRUE);
            int y = ch - below + 4;
            const int hb = (cw - 2 * m - 4) / 2;
            MoveWindow(GetDlgItem(w, ID_SC_ALL), m, y, hb, 24, TRUE);
            MoveWindow(GetDlgItem(w, ID_SC_NONE), m + hb + 4, y, hb, 24, TRUE);
            y += 28;
            MoveWindow(GetDlgItem(w, ID_SC_ONLY), m, y, cw - 2 * m, 24, TRUE);
            y += 28;
            MoveWindow(GetDlgItem(w, ID_SC_NEW), m, y, cw - 2 * m, 24, TRUE);
            y += 28;
            MoveWindow(GetDlgItem(w, ID_SC_MOVE), m, y, cw - 2 * m, 24, TRUE);
            y += 32;
            MoveWindow(scene.info, m, y, cw - 2 * m, 40, TRUE);
            y += 46;
            MoveWindow(GetDlgItem(w, ID_SC_LODSHOW), m, y, cw - 2 * m, 20, TRUE);
            y += 22;
            MoveWindow(GetDlgItem(w, ID_SC_LODLABEL), m, y, cw - 2 * m, 16, TRUE);
            y += 18;
            MoveWindow(GetDlgItem(w, ID_SC_LODSLIDER), m, y, cw - 2 * m, 30, TRUE);
            return 0;
        }
        case WM_HSCROLL:
            if ((HWND)lp == GetDlgItem(w, ID_SC_LODSLIDER)) {
                app.lodNear = float(SendMessageA((HWND)lp, TBM_GETPOS, 0, 0));
                char t[96];
                std::snprintf(t, sizeof t, "LOD models appear beyond %d units", int(app.lodNear));
                SetWindowTextA(GetDlgItem(w, ID_SC_LODLABEL), t);
            }
            return 0;
        case WM_COMMAND:
            if (LOWORD(wp) == ID_SC_LODSHOW && HIWORD(wp) == BN_CLICKED) {
                app.showLod = SendMessageA((HWND)lp, BM_GETCHECK, 0, 0) == BST_CHECKED;
                SetFocus(GetParent(w));
                return 0;
            }
            if (HIWORD(wp) == BN_CLICKED) {
                if (LOWORD(wp) == ID_SC_ALL) setAllRegions(true);
                else if (LOWORD(wp) == ID_SC_NONE) setAllRegions(false);
                else if (LOWORD(wp) == ID_SC_NEW) createRegionUi(w);
                else if (LOWORD(wp) == ID_SC_MOVE) {
                    const int sel = sceneRowRegion(ListView_GetNextItem(scene.list, -1, LVNI_SELECTED));
                    if (sel < 0) MessageBoxA(w, "Click a region in the list first (the one the objects should go into).", "Move selected objects", MB_ICONINFORMATION);
                    else if (app.selected.empty()) MessageBoxA(w, "Select objects in the 3D view first (double-click).", "Move selected objects", MB_ICONINFORMATION);
                    else {
                        const size_t n = moveSelectedToRegion(regions[sel].file);
                        char t[200];
                        if (n) std::snprintf(t, sizeof t, "Moved %zu placement(s) (including LOD partners) to %s. They are saved into that file with Save.", n, regions[sel].name.c_str());
                        else std::snprintf(t, sizeof t, "The selected objects are already in %s.", regions[sel].name.c_str());
                        MessageBoxA(w, t, "Move selected objects", MB_ICONINFORMATION);
                    }
                } else if (LOWORD(wp) == ID_SC_ONLY) {
                    const int sel = sceneRowRegion(ListView_GetNextItem(scene.list, -1, LVNI_SELECTED));
                    if (sel >= 0) {
                        for (size_t i = 0; i < regions.size(); ++i) setRegion(regions[i].file, int(i) == sel);
                        fillRegionList();
                    }
                }
                SetFocus(GetParent(w));
            }
            return 0;
        case WM_NOTIFY: {
            NMHDR* h = (NMHDR*)lp;
            if (h->idFrom != ID_SC_LIST) break;
            if (h->code == LVN_ITEMCHANGED && !scene.populating) {
                NMLISTVIEW* n = (NMLISTVIEW*)lp;
                if ((n->uChanged & LVIF_STATE) && ((n->uOldState ^ n->uNewState) & LVIS_STATEIMAGEMASK) && sceneRowRegion(n->iItem) >= 0)
                    setRegion(regions[sceneRowRegion(n->iItem)].file, ((n->uNewState & LVIS_STATEIMAGEMASK) >> 12) == 2);
            } else if (h->code == NM_DBLCLK) {
                const int sel = sceneRowRegion(ListView_GetNextItem(scene.list, -1, LVNI_SELECTED));
                if (sel >= 0) centreOnRegion(regions[sel]);  // MEd: double-click centres the map on the file
            }
            return 0;
        }
    }
    return DefWindowProcA(w, msg, wp, lp);
}

void createScenePanel(HWND parent) {
    INITCOMMONCONTROLSEX ic = {sizeof ic, ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&ic);
    WNDCLASSA wc = {};
    wc.lpfnWndProc = sceneProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "MEdScene";
    RegisterClassA(&wc);
    app.leftW = 310;
    app.viewX = app.leftW;
    app.scenePanel = CreateWindowExA(0, "MEdScene", "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, app.leftW, 400, parent, nullptr, GetModuleHandle(nullptr), nullptr);
    fillRegionList();
}

// ---- the Placements tab (after MEd's IPL tab, CIPLTree 0x0042a700) ----------------------------------------------------
// MEd lists the IPL files, and the placements of the file chosen there; choosing a file also makes it the destination of
// New / Paste (setContext). Its search is a separate Find window (name or ID, Find Next). Here the same is one panel, made
// easier to use: a region picker (the destination), a search box that filters while typing (name, or ID when a number is
// typed) over this region or all regions, a sortable list (click a column header), Previous / Next match buttons (F3 /
// Shift+F3) that select and fly to each match, and two-way sync: clicking rows selects the objects in 3D, double-click or
// Enter flies there, and selecting in 3D highlights the rows (switching region when needed).

enum {
    ID_PL_SEARCH = 5001, ID_PL_SCOPE_REGION, ID_PL_SCOPE_ALL, ID_PL_REGION, ID_PL_LIST, ID_PL_COUNT, ID_PL_PREV, ID_PL_NEXT,
    ID_PL_NEW, ID_PL_DUP, ID_PL_DEL, ID_PL_LBL_SEARCH, ID_PL_LBL_REGION
};

struct PlacementsUi {
    HWND panel = nullptr, search = nullptr, regionBox = nullptr, list = nullptr, count = nullptr, scopeAll = nullptr, tips = nullptr;
    std::vector<int> rows;          // the placements shown, in list order
    int region = -1;                // the region (IPL file) picked: shown in "this region" mode, and the destination of New / Paste
    int sortCol = -1;               // -1: file order
    bool sortDesc = false;
    bool syncing = false;           // true while the list is being changed from code (ignore its notifications)
    std::vector<int> lastSelected;  // app.selected as last mirrored into the list
    char text[64] = "";             // buffer for LVN_GETDISPINFO
} pl;

const char* kPlColumns[6] = {"Name", "ID", "X", "Y", "Z", "Region"};
const int kPlWidths[6] = {92, 38, 56, 56, 46, 0};  // Region is shown only in All regions mode

void plAddTip(HWND ctrl, const char* text) {
    if (!pl.tips) return;
    TOOLINFOA ti = {};
    ti.cbSize = sizeof ti;
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd = pl.panel;
    ti.uId = (UINT_PTR)ctrl;
    ti.lpszText = const_cast<char*>(text);
    SendMessageA(pl.tips, TTM_ADDTOOLA, 0, (LPARAM)&ti);
}

std::string regionLabel(const Region& r) { return r.name + "  (" + std::to_string(r.count) + ")"; }

// The region picker: every region with its object count; keeps the picked one.
void plFillRegions() {
    if (!pl.regionBox) return;
    pl.syncing = true;
    SendMessageA(pl.regionBox, CB_RESETCONTENT, 0, 0);
    int pick = -1;
    if (pl.region >= 0 && !regionListed(pl.region)) pl.region = -1;
    for (const Region& r : regions) {
        if (!regionListed(r.file)) continue;
        const int at = int(SendMessageA(pl.regionBox, CB_ADDSTRING, 0, (LPARAM)regionLabel(r).c_str()));
        SendMessageA(pl.regionBox, CB_SETITEMDATA, at, r.file);
        if (r.file == pl.region) pick = at;
    }
    SendMessageA(pl.regionBox, CB_SETCURSEL, pick, 0);
    pl.syncing = false;
}

// Mirror app.selected into the list (rows highlighted, the first one scrolled into view).
void plMirrorSelection() {
    if (!pl.list) return;
    pl.syncing = true;
    ListView_SetItemState(pl.list, -1, 0, LVIS_SELECTED);
    int first = -1;
    for (int idx : app.selected) {
        auto it = std::find(pl.rows.begin(), pl.rows.end(), idx);
        if (it == pl.rows.end()) continue;
        const int row = int(it - pl.rows.begin());
        ListView_SetItemState(pl.list, row, LVIS_SELECTED, LVIS_SELECTED);
        if (first < 0) first = row;
    }
    if (first >= 0) {
        ListView_SetItemState(pl.list, first, LVIS_FOCUSED, LVIS_FOCUSED);
        ListView_EnsureVisible(pl.list, first, FALSE);
    }
    pl.lastSelected = app.selected;
    pl.syncing = false;
}

// Build the rows again: the search text, the scope and the sort order decide what is shown.
void plRefill() {
    if (!pl.list) return;
    char b[128] = "";
    GetWindowTextA(pl.search, b, sizeof b);
    const std::string q = toLower(trim(b));
    const bool numeric = !q.empty() && std::all_of(q.begin(), q.end(), [](char c) { return c >= '0' && c <= '9'; });
    const int num = numeric ? std::atoi(q.c_str()) : -1;
    const bool all = SendMessageA(pl.scopeAll, BM_GETCHECK, 0, 0) == BST_CHECKED;
    pl.rows.clear();
    size_t inScope = 0;
    for (size_t i = 0; i < app.instances.size(); ++i) {
        if (!all && app.instFile[i] != pl.region) continue;
        ++inScope;
        const IplInstance& o = app.instances[i];
        if (!q.empty() && !(numeric && o.id == num) && toLower(o.model).find(q) == std::string::npos) continue;
        pl.rows.push_back(int(i));
    }
    if (pl.sortCol >= 0) {
        auto key = [](int i, int col) -> double {
            const IplInstance& o = app.instances[i];
            switch (col) {
                case 1: return o.id;
                case 2: return o.x;
                case 3: return o.y;
                case 4: return o.z;
                case 5: return app.instFile[i];
            }
            return 0;
        };
        const int col = pl.sortCol;
        const bool desc = pl.sortDesc;
        std::stable_sort(pl.rows.begin(), pl.rows.end(), [&](int a, int c) {
            if (col == 0) {
                const std::string x = toLower(app.instances[a].model), y = toLower(app.instances[c].model);
                return desc ? y < x : x < y;
            }
            return desc ? key(c, col) < key(a, col) : key(a, col) < key(c, col);
        });
    }
    ListView_SetColumnWidth(pl.list, 5, all ? 84 : 0);  // which region each row is in, only needed when listing all regions
    pl.syncing = true;
    ListView_SetItemCountEx(pl.list, int(pl.rows.size()), LVSICF_NOSCROLL);
    InvalidateRect(pl.list, nullptr, TRUE);
    pl.syncing = false;
    std::string c;
    if (!all && pl.region < 0) c = "Pick a region above to see its objects, or choose All regions to search everywhere.";
    else if (pl.rows.empty()) c = q.empty() ? "This region has no objects yet. New Object, Paste or Move selected objects here (Scene tab) fill it." : "Nothing matches. Try part of a name, or a model ID.";
    else c = std::to_string(pl.rows.size()) + " of " + std::to_string(inScope) + " shown" + (q.empty() ? "" : " (search: " + q + ")") +
             ". Double-click or Enter flies there.";
    SetWindowTextA(pl.count, c.c_str());
    plMirrorSelection();
}

// Pick the region shown and used as destination (from code: also updates the picker).
void plSetRegion(int file) {
    pl.region = file;
    plFillRegions();
    plRefill();
}

// After placements were added, removed or moved between regions (called from refreshRegions).
// View > Show hidden regions: list the regions that were empty at load, or leave them out again.
void setShowEmptyRegions(bool on) {
    app.showEmptyRegions = on;
    if (scene.list) fillRegionList();
    plRefreshAfterEdit();
}

void plRefreshAfterEdit() {
    itRefreshAfterEdit();
    if (!pl.list) return;
    plFillRegions();
    plRefill();
}

// Objects picked in the list are shown: a hidden region (unticked in the Scene tab) is switched on, otherwise the object
// could not be seen, and setRegion would drop it from the selection as soon as the region is hidden.
void plShowRegionsOf(const std::vector<int>& placements) {
    bool changed = false;
    for (int idx : placements)
        if (idx >= 0 && idx < int(app.instances.size()) && !app.regionOn[app.instFile[idx]]) {
            setRegion(app.instFile[idx], true);
            changed = true;
        }
    if (changed && scene.list) fillRegionList();
}

// Select one row (and only it), which selects the object in 3D, and fly there.
void plGoToRow(int row) {
    if (row < 0 || row >= int(pl.rows.size())) return;
    plShowRegionsOf({pl.rows[row]});
    app.selected.assign(1, pl.rows[row]);
    app.history.endSession();
    centreOnObject(pl.rows[row]);
    plMirrorSelection();
}

void plStep(int dir) {
    if (pl.rows.empty()) return;
    const int cur = ListView_GetNextItem(pl.list, -1, LVNI_FOCUSED);
    const int n = int(pl.rows.size());
    plGoToRow(cur < 0 ? (dir > 0 ? 0 : n - 1) : (cur + dir + n) % n);
}

// The list's own selection changed (by the user): select the same placements in 3D.
void plSelectionFromList() {
    if (pl.syncing) return;
    std::vector<int> sel;
    for (int row = ListView_GetNextItem(pl.list, -1, LVNI_SELECTED); row >= 0; row = ListView_GetNextItem(pl.list, row, LVNI_SELECTED))
        if (row < int(pl.rows.size())) sel.push_back(pl.rows[row]);
    plShowRegionsOf(sel);
    app.selected = sel;
    app.history.endSession();
    pl.lastSelected = sel;
}

// Every frame: when the selection changed in 3D, show it in the list (switching region if it is in another one).
void plSyncFromSelection() {
    if (!pl.list || app.selected == pl.lastSelected) return;
    const bool all = SendMessageA(pl.scopeAll, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (!app.selected.empty() && !all && app.instFile[app.selected[0]] != pl.region) {
        plSetRegion(app.instFile[app.selected[0]]);  // also mirrors the selection
        return;
    }
    plMirrorSelection();
}

// Keys for the panel, handled before dispatch: F3 / Shift+F3 step through the matches, Enter in the search box goes to the
// next match, Enter in the list flies to the focused row, Esc in the search box clears it.
bool plHandleKey(const MSG& m) {
    if (!pl.list || m.message != WM_KEYDOWN || (m.hwnd != pl.search && m.hwnd != pl.list)) return false;
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (m.wParam == VK_F3) { plStep(shift ? -1 : 1); return true; }
    if (m.wParam == VK_RETURN && m.hwnd == pl.search) { plStep(shift ? -1 : 1); return true; }
    if (m.wParam == VK_RETURN && m.hwnd == pl.list) { plGoToRow(ListView_GetNextItem(pl.list, -1, LVNI_FOCUSED)); return true; }
    if (m.wParam == VK_ESCAPE && m.hwnd == pl.search) { SetWindowTextA(pl.search, ""); return true; }
    return false;
}

LRESULT CALLBACK placementsProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            pl.panel = w;
            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            auto make = [&](const char* cls, const char* text, DWORD style, DWORD ex, int id) {
                HWND c = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, w, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
                SendMessageA(c, WM_SETFONT, (WPARAM)font, TRUE);
                return c;
            };
            pl.tips = CreateWindowExA(WS_EX_TOPMOST, TOOLTIPS_CLASSA, nullptr, WS_POPUP | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, w,
                                      nullptr, GetModuleHandle(nullptr), nullptr);
            make("STATIC", "Region (new objects and pastes go here):", SS_LEFT, 0, ID_PL_LBL_REGION);
            pl.regionBox = make("COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, ID_PL_REGION);
            make("STATIC", "Search (name, or model ID):", SS_LEFT, 0, ID_PL_LBL_SEARCH);
            pl.search = make("EDIT", "", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_PL_SEARCH);
            HWND prev = make("BUTTON", "<", BS_PUSHBUTTON, 0, ID_PL_PREV);
            HWND next = make("BUTTON", ">", BS_PUSHBUTTON, 0, ID_PL_NEXT);
            HWND inRegion = make("BUTTON", "This region", BS_AUTORADIOBUTTON | WS_GROUP, 0, ID_PL_SCOPE_REGION);
            pl.scopeAll = make("BUTTON", "All regions", BS_AUTORADIOBUTTON, 0, ID_PL_SCOPE_ALL);
            SendMessageA(inRegion, BM_SETCHECK, BST_CHECKED, 0);
            pl.list = make(WC_LISTVIEWA, "", LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_PL_LIST);
            ListView_SetExtendedListViewStyle(pl.list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
            for (int i = 0; i < 6; ++i) {
                LVCOLUMNA c = {};
                c.mask = LVCF_TEXT | LVCF_WIDTH | (i >= 1 && i <= 4 ? LVCF_FMT : 0);
                c.fmt = LVCFMT_RIGHT;
                c.pszText = const_cast<char*>(kPlColumns[i]);
                c.cx = kPlWidths[i];
                ListView_InsertColumn(pl.list, i, &c);
            }
            pl.count = make("STATIC", "", SS_LEFT, 0, ID_PL_COUNT);
            HWND bNew = make("BUTTON", "New...", BS_PUSHBUTTON, 0, ID_PL_NEW);
            HWND bDup = make("BUTTON", "Duplicate", BS_PUSHBUTTON, 0, ID_PL_DUP);
            HWND bDel = make("BUTTON", "Delete", BS_PUSHBUTTON, 0, ID_PL_DEL);
            plAddTip(pl.regionBox, "The region whose objects are listed. New Object and Paste put things into this region.");
            plAddTip(pl.search, "Type part of a name (for example cone) or a model ID. Enter or F3: next match, Shift+F3: previous, Esc: clear.");
            plAddTip(prev, "Previous match: select it and fly there (Shift+F3)");
            plAddTip(next, "Next match: select it and fly there (F3)");
            plAddTip(inRegion, "List the objects of the region picked above");
            plAddTip(pl.scopeAll, "List and search the objects of every region");
            plAddTip(pl.list, "Click to select in 3D (Ctrl or Shift for several). Double-click or Enter: fly there. Click a column header to sort.");
            plAddTip(bNew, "Add a new object to the picked region (in the middle of the view)");
            plAddTip(bDup, "Duplicate the selected objects (with their LOD stand-ins)");
            plAddTip(bDel, "Delete the selected objects (moves them below the map, the safe delete)");
            return 0;
        }
        case WM_SIZE: {
            const int cw = LOWORD(lp), ch = HIWORD(lp), m = 6, iw = cw - 2 * m;
            int y = m;
            MoveWindow(GetDlgItem(w, ID_PL_LBL_REGION), m, y, iw, 16, TRUE); y += 17;
            MoveWindow(pl.regionBox, m, y, iw, 300, TRUE); y += 28;
            MoveWindow(GetDlgItem(w, ID_PL_LBL_SEARCH), m, y, iw, 16, TRUE); y += 17;
            MoveWindow(pl.search, m, y, iw - 2 * 26, 22, TRUE);
            MoveWindow(GetDlgItem(w, ID_PL_PREV), m + iw - 50, y, 24, 22, TRUE);
            MoveWindow(GetDlgItem(w, ID_PL_NEXT), m + iw - 24, y, 24, 22, TRUE); y += 26;
            MoveWindow(GetDlgItem(w, ID_PL_SCOPE_REGION), m, y, iw / 2, 20, TRUE);
            MoveWindow(pl.scopeAll, m + iw / 2, y, iw / 2, 20, TRUE); y += 24;
            const int bottom = ch - m - 26 - 6 - 44;
            MoveWindow(pl.list, m, y, iw, std::max(60, bottom - y), TRUE);
            y = std::max(y + 60, bottom) + 4;
            MoveWindow(pl.count, m, y, iw, 42, TRUE); y += 44;
            const int bw = (iw - 8) / 3;
            MoveWindow(GetDlgItem(w, ID_PL_NEW), m, y, bw, 26, TRUE);
            MoveWindow(GetDlgItem(w, ID_PL_DUP), m + bw + 4, y, bw, 26, TRUE);
            MoveWindow(GetDlgItem(w, ID_PL_DEL), m + 2 * (bw + 4), y, bw, 26, TRUE);
            return 0;
        }
        case WM_COMMAND: {
            const int id = LOWORD(wp), code = HIWORD(wp);
            if (id == ID_PL_SEARCH && code == EN_CHANGE) plRefill();
            else if (id == ID_PL_REGION && code == CBN_SELCHANGE && !pl.syncing) {
                const int at = int(SendMessageA(pl.regionBox, CB_GETCURSEL, 0, 0));
                if (at >= 0) { pl.region = int(SendMessageA(pl.regionBox, CB_GETITEMDATA, at, 0)); plRefill(); }
            } else if ((id == ID_PL_SCOPE_REGION || id == ID_PL_SCOPE_ALL) && code == BN_CLICKED) plRefill();
            else if (id == ID_PL_PREV && code == BN_CLICKED) plStep(-1);
            else if (id == ID_PL_NEXT && code == BN_CLICKED) plStep(1);
            else if (id == ID_PL_NEW && code == BN_CLICKED) newObjectAt(GetParent(w), app.viewX + app.width / 2, app.toolbarH + app.height / 2);
            else if (id == ID_PL_DUP && code == BN_CLICKED) duplicateSelected();
            else if (id == ID_PL_DEL && code == BN_CLICKED) sinkSelected();
            return 0;
        }
        case WM_NOTIFY: {
            NMHDR* h = reinterpret_cast<NMHDR*>(lp);
            if (h->hwndFrom != pl.list) break;
            if (h->code == LVN_GETDISPINFOA) {
                NMLVDISPINFOA* d = reinterpret_cast<NMLVDISPINFOA*>(lp);
                if (!(d->item.mask & LVIF_TEXT) || d->item.iItem < 0 || d->item.iItem >= int(pl.rows.size())) return 0;
                const int i = pl.rows[d->item.iItem];
                const IplInstance& o = app.instances[i];
                switch (d->item.iSubItem) {
                    case 0: std::snprintf(pl.text, sizeof pl.text, "%s", o.model.c_str()); break;
                    case 1: std::snprintf(pl.text, sizeof pl.text, "%d", o.id); break;
                    case 2: std::snprintf(pl.text, sizeof pl.text, "%.2f", o.x); break;
                    case 3: std::snprintf(pl.text, sizeof pl.text, "%.2f", o.y); break;
                    case 4: std::snprintf(pl.text, sizeof pl.text, "%.2f", o.z); break;
                    default: {
                        const std::string& p = app.iplPaths[app.instFile[i]];
                        std::snprintf(pl.text, sizeof pl.text, "%s", p.substr(p.find_last_of('/') + 1).c_str());
                    }
                }
                d->item.pszText = pl.text;
                return 0;
            }
            if (h->code == LVN_ITEMCHANGED || h->code == LVN_ODSTATECHANGED) { plSelectionFromList(); return 0; }
            if (h->code == NM_DBLCLK) { plGoToRow(reinterpret_cast<NMITEMACTIVATE*>(lp)->iItem); return 0; }
            if (h->code == LVN_COLUMNCLICK) {
                const int col = reinterpret_cast<NMLISTVIEW*>(lp)->iSubItem;
                pl.sortDesc = pl.sortCol == col ? !pl.sortDesc : false;
                pl.sortCol = col;
                plRefill();
                return 0;
            }
            return 0;
        }
    }
    return DefWindowProcA(w, msg, wp, lp);
}

void showLeftTab(int tab);
void roadSync();
void renderRoadThumbnails();
bool roadClick(int mx, int my);
bool roadStripOpen();
void closeRoadStrip();

// ---- the Item tab (after MEd's "Item" tab: CIDETree with CIDEFinder, "Find Item Definition", search by name, ID or
// texture) ------------------------------------------------------------------------------------------------------------------
// Every object definition of the IDE files: name, ID, how often it is placed, the game pool its placements take, texture
// dictionary, draw distance, IDE file. Search while typing (name, texture dictionary, or ID when a number is typed), show
// all / placed / not placed, sort by any column. "Add to map" places the item in the region picked in the Placements tab
// (as New Object, without the chooser); "Show where it is placed" (also double-click / Enter) opens the Placements tab
// filtered to that model in all regions. Selecting an object in 3D highlights its definition here.

enum { ID_IT_SEARCH = 6001, ID_IT_SHOW, ID_IT_LIST, ID_IT_COUNT, ID_IT_ADD, ID_IT_WHERE, ID_IT_LBL_SEARCH, ID_IT_LBL_SHOW, ID_IT_EXTRACT, ID_IT_DFF, ID_IT_TXD, ID_IT_COL };

struct ItemUi {
    HWND panel = nullptr, search = nullptr, show = nullptr, list = nullptr, count = nullptr, tips = nullptr;
    std::vector<int> rows;                  // model ids shown, in list order
    std::unordered_map<int, int> placed;    // model id -> number of placements
    int sortCol = 0;                        // by name
    bool sortDesc = false;
    bool syncing = false;
    int lastSelectedId = -2;                // the model id last mirrored from the 3D selection
    int lastSelectedIdx = -2;               // and the placement it belonged to (another placement of the same model, or a Replace, mirrors again)
    char text[96] = "";
} it;

const char* kItColumns[7] = {"Name", "ID", "Placed", "Pool", "Texture", "Draw", "IDE file"};
const int kItWidths[7] = {104, 40, 46, 62, 84, 40, 84};

const char* poolShortName(Pool p) { return p == Pool::Building ? "Building" : p == Pool::Treadable ? "Treadable" : "Dummy"; }

void itAddTip(HWND ctrl, const char* text) {
    if (!it.tips) return;
    TOOLINFOA ti = {};
    ti.cbSize = sizeof ti;
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd = it.panel;
    ti.uId = (UINT_PTR)ctrl;
    ti.lpszText = const_cast<char*>(text);
    SendMessageA(it.tips, TTM_ADDTOOLA, 0, (LPARAM)&ti);
}

void itCountPlacements() {
    it.placed.clear();
    for (const IplInstance& o : app.instances) ++it.placed[o.id];
}

int itPlacedCount(int id) {
    auto f = it.placed.find(id);
    return f == it.placed.end() ? 0 : f->second;
}

// Highlight the definition of the first selected placement (when it is listed).
void itMirrorSelection() {
    if (!it.list) return;
    const int id = app.selected.empty() ? -1 : app.instances[app.selected[0]].id;
    it.lastSelectedId = id;
    it.lastSelectedIdx = app.selected.empty() ? -1 : app.selected[0];
    auto f = std::find(it.rows.begin(), it.rows.end(), id);
    if (f == it.rows.end()) return;  // keep whatever the user picked in the list
    it.syncing = true;
    const int row = int(f - it.rows.begin());
    ListView_SetItemState(it.list, -1, 0, LVIS_SELECTED);
    ListView_SetItemState(it.list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(it.list, row, FALSE);
    it.syncing = false;
}

void itRefill() {
    if (!it.list) return;
    const int keepId = it.rows.empty() ? -1 : [] {  // the item picked in the list stays picked when the list is rebuilt
        const int row = ListView_GetNextItem(it.list, -1, LVNI_SELECTED);
        return row >= 0 && row < int(it.rows.size()) ? it.rows[row] : -1;
    }();
    char b[128] = "";
    GetWindowTextA(it.search, b, sizeof b);
    const std::string q = toLower(trim(b));
    const bool numeric = !q.empty() && std::all_of(q.begin(), q.end(), [](char c) { return c >= '0' && c <= '9'; });
    const int num = numeric ? std::atoi(q.c_str()) : -1;
    const int mode = int(SendMessageA(it.show, CB_GETCURSEL, 0, 0));  // 0 all, 1 placed, 2 not placed
    it.rows.clear();
    for (const auto& kv : app.defs) {
        const ModelDef& d = kv.second;
        if (!q.empty() && !(numeric && kv.first == num) && d.model.find(q) == std::string::npos && d.txd.find(q) == std::string::npos) continue;
        const int n = itPlacedCount(kv.first);
        if ((mode == 1 && n == 0) || (mode == 2 && n > 0)) continue;
        it.rows.push_back(kv.first);
    }
    const int col = it.sortCol;
    const bool desc = it.sortDesc;
    auto text = [](int id, int c) -> std::string {
        const ModelDef& d = app.defs[id];
        return c == 0 ? d.model : c == 3 ? poolShortName(d.pool) : c == 4 ? d.txd : toLower(d.ide);
    };
    auto value = [](int id, int c) -> double { return c == 1 ? id : c == 2 ? itPlacedCount(id) : app.defs[id].drawDistance; };
    std::stable_sort(it.rows.begin(), it.rows.end(), [&](int a, int c) {
        if (col == 0 || col == 3 || col == 4 || col == 6) {
            const std::string x = text(a, col), y = text(c, col);
            return desc ? y < x : x < y;
        }
        return desc ? value(c, col) < value(a, col) : value(a, col) < value(c, col);
    });
    it.syncing = true;
    ListView_SetItemCountEx(it.list, int(it.rows.size()), LVSICF_NOSCROLL);
    InvalidateRect(it.list, nullptr, TRUE);
    ListView_SetItemState(it.list, -1, 0, LVIS_SELECTED);
    it.syncing = false;
    std::string c;
    if (it.rows.empty()) c = "Nothing matches. Try part of a name or texture dictionary, or a model ID.";
    else c = std::to_string(it.rows.size()) + " of " + std::to_string(app.defs.size()) + " items shown. Double-click: show where it is placed.";
    SetWindowTextA(it.count, c.c_str());
    auto f = std::find(it.rows.begin(), it.rows.end(), keepId);
    if (f != it.rows.end()) {
        it.syncing = true;
        const int row = int(f - it.rows.begin());
        ListView_SetItemState(it.list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(it.list, row, FALSE);
        it.syncing = false;
    } else {
        itMirrorSelection();
    }
}

void itRefreshAfterEdit() {
    if (!it.list) return;
    itCountPlacements();
    itRefill();
}

int itSelectedId() {
    const int row = ListView_GetNextItem(it.list, -1, LVNI_SELECTED);
    return row >= 0 && row < int(it.rows.size()) ? it.rows[row] : -1;
}

// "Show where it is placed": the Placements tab, all regions, filtered to this model ID.
void itShowPlacements(int id) {
    if (id < 0 || !pl.list) return;
    showLeftTab(1);
    SendMessageA(GetDlgItem(pl.panel, ID_PL_SCOPE_REGION), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessageA(pl.scopeAll, BM_SETCHECK, BST_CHECKED, 0);
    SetWindowTextA(pl.search, std::to_string(id).c_str());  // sends EN_CHANGE: the list refills
    plRefill();
    SetFocus(pl.list);
}

// "Add to map": as New Object, with this item instead of the chooser.
void itAddToMap(HWND owner) {
    const int id = itSelectedId();
    if (id < 0) {
        inspWarn(owner, "Click an item in the list first.", "Add to map");
        return;
    }
    newObjectAt(owner, app.viewX + app.width / 2, app.toolbarH + app.height / 2, id);
}

// Every frame: when the 3D selection changed, highlight the definition of the first selected object.
void itSyncFromSelection() {
    if (!it.list) return;
    const int id = app.selected.empty() ? -1 : app.instances[app.selected[0]].id;
    const int idx = app.selected.empty() ? -1 : app.selected[0];
    if (id != it.lastSelectedId || idx != it.lastSelectedIdx) itMirrorSelection();
}

bool itHandleKey(const MSG& m) {
    if (!it.list || m.message != WM_KEYDOWN || (m.hwnd != it.search && m.hwnd != it.list)) return false;
    if (m.wParam == VK_RETURN && m.hwnd == it.list) { itShowPlacements(itSelectedId()); return true; }
    if (m.wParam == VK_RETURN && m.hwnd == it.search) {  // to the list, first item selected
        if (!it.rows.empty()) {
            ListView_SetItemState(it.list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            SetFocus(it.list);
        }
        return true;
    }
    if (m.wParam == VK_ESCAPE && m.hwnd == it.search) { SetWindowTextA(it.search, ""); return true; }
    return false;
}

LRESULT CALLBACK itemProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            it.panel = w;
            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            auto make = [&](const char* cls, const char* text, DWORD style, DWORD ex, int id) {
                HWND c = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, w, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
                SendMessageA(c, WM_SETFONT, (WPARAM)font, TRUE);
                return c;
            };
            it.tips = CreateWindowExA(WS_EX_TOPMOST, TOOLTIPS_CLASSA, nullptr, WS_POPUP | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, w,
                                      nullptr, GetModuleHandle(nullptr), nullptr);
            make("STATIC", "Search (name, texture or model ID):", SS_LEFT, 0, ID_IT_LBL_SEARCH);
            it.search = make("EDIT", "", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_IT_SEARCH);
            make("STATIC", "Show:", SS_LEFT, 0, ID_IT_LBL_SHOW);
            it.show = make("COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, ID_IT_SHOW);
            for (const char* s : {"All items", "Placed on the map", "Not placed yet"}) SendMessageA(it.show, CB_ADDSTRING, 0, (LPARAM)s);
            SendMessageA(it.show, CB_SETCURSEL, 0, 0);
            it.list = make(WC_LISTVIEWA, "", LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_IT_LIST);
            ListView_SetExtendedListViewStyle(it.list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
            for (int i = 0; i < 7; ++i) {
                LVCOLUMNA c = {};
                c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
                c.fmt = (i == 1 || i == 2 || i == 5) ? LVCFMT_RIGHT : LVCFMT_LEFT;
                c.pszText = const_cast<char*>(kItColumns[i]);
                c.cx = kItWidths[i];
                ListView_InsertColumn(it.list, i, &c);
            }
            it.count = make("STATIC", "", SS_LEFT, 0, ID_IT_COUNT);
            HWND add = make("BUTTON", "Add to map", BS_PUSHBUTTON, 0, ID_IT_ADD);
            HWND where = make("BUTTON", "Show placements", BS_PUSHBUTTON, 0, ID_IT_WHERE);
            HWND extract = make("BUTTON", "Extract...", BS_PUSHBUTTON, 0, ID_IT_EXTRACT);
            HWND dffb = make("BUTTON", "DFF Properties", BS_PUSHBUTTON, 0, ID_IT_DFF);
            HWND txdb = make("BUTTON", "View TXD", BS_PUSHBUTTON, 0, ID_IT_TXD);
            HWND colb = make("BUTTON", "View Collision", BS_PUSHBUTTON, 0, ID_IT_COL);
            itAddTip(dffb, "View Information about the 3D Model: the model on its own (right button turns, left button moves, wheel zooms) and its details");
            itAddTip(txdb, "View Texture Dictionary: every texture of this item's .txd with its size and format");
            itAddTip(colb, "View this model's collision file: its boxes, spheres and mesh, coloured by surface");
            itAddTip(it.search, "Type part of a name (tree), a texture dictionary (dyncones) or a model ID (1360). Enter: to the list. Esc: clear.");
            itAddTip(it.show, "All definitions, only those placed somewhere on the map, or only the ones not used yet");
            itAddTip(it.list, "Click a column header to sort. Placed: how many placements use it. Pool: the game pool its placements take "
                              "(Treadable is nearly full on the original map). Double-click or Enter: show where it is placed.");
            itAddTip(add, "Place the selected item in the region picked in the Placements tab, in the middle of the view");
            itAddTip(where, "Open the Placements tab with every placement of this item (all regions)");
            itAddTip(extract, "Copy this item's model, textures and collision (and an info file) into a folder; the game's files are not changed");
            return 0;
        }
        case WM_SIZE: {
            const int cw = LOWORD(lp), ch = HIWORD(lp), m = 6, iw = cw - 2 * m;
            int y = m;
            MoveWindow(GetDlgItem(w, ID_IT_LBL_SEARCH), m, y, iw, 16, TRUE); y += 17;
            MoveWindow(it.search, m, y, iw, 22, TRUE); y += 28;
            MoveWindow(GetDlgItem(w, ID_IT_LBL_SHOW), m, y + 4, 36, 16, TRUE);
            MoveWindow(it.show, m + 38, y, iw - 38, 200, TRUE); y += 30;
            const int bottom = ch - m - 26 - 6 - 30 - 30;
            MoveWindow(it.list, m, y, iw, std::max(60, bottom - y), TRUE);
            y = std::max(y + 60, bottom) + 4;
            MoveWindow(it.count, m, y, iw, 30, TRUE); y += 32;
            const int third = (iw - 8) / 3;
            MoveWindow(GetDlgItem(w, ID_IT_ADD), m, y, third, 26, TRUE);
            MoveWindow(GetDlgItem(w, ID_IT_WHERE), m + third + 4, y, third, 26, TRUE);
            MoveWindow(GetDlgItem(w, ID_IT_EXTRACT), m + 2 * (third + 4), y, third, 26, TRUE);
            y += 30;
            MoveWindow(GetDlgItem(w, ID_IT_DFF), m, y, third, 26, TRUE);
            MoveWindow(GetDlgItem(w, ID_IT_TXD), m + third + 4, y, third, 26, TRUE);
            MoveWindow(GetDlgItem(w, ID_IT_COL), m + 2 * (third + 4), y, third, 26, TRUE);
            return 0;
        }
        case WM_COMMAND: {
            const int id = LOWORD(wp), code = HIWORD(wp);
            if (id == ID_IT_SEARCH && code == EN_CHANGE) itRefill();
            else if (id == ID_IT_SHOW && code == CBN_SELCHANGE) itRefill();
            else if (id == ID_IT_ADD && code == BN_CLICKED) itAddToMap(GetParent(w));
            else if (id == ID_IT_DFF && code == BN_CLICKED) {
                if (itSelectedId() >= 0) openDffViewer(GetParent(w), itSelectedId());
                else inspWarn(GetParent(w), "Pick an item in the list first.", "DFF Properties");
            } else if (id == ID_IT_TXD && code == BN_CLICKED) {
                if (itSelectedId() >= 0) openTxdViewer(GetParent(w), app.defs[itSelectedId()].txd);
                else inspWarn(GetParent(w), "Pick an item in the list first.", "TXD Properties");
            } else if (id == ID_IT_COL && code == BN_CLICKED) {
                if (itSelectedId() >= 0) openColViewer(GetParent(w), itSelectedId());
                else inspWarn(GetParent(w), "Pick an item in the list first.", "Collision Object Properties");
            } else if (id == ID_IT_EXTRACT && code == BN_CLICKED) extractObjectsUi(GetParent(w), itSelectedId() >= 0 ? std::vector<int>{itSelectedId()} : std::vector<int>());
            else if (id == ID_IT_WHERE && code == BN_CLICKED) {
                if (itSelectedId() < 0) inspWarn(GetParent(w), "Click an item in the list first.", "Show where it is placed");
                else itShowPlacements(itSelectedId());
            }
            return 0;
        }
        case WM_NOTIFY: {
            NMHDR* h = reinterpret_cast<NMHDR*>(lp);
            if (h->hwndFrom != it.list) break;
            if (h->code == LVN_GETDISPINFOA) {
                NMLVDISPINFOA* d = reinterpret_cast<NMLVDISPINFOA*>(lp);
                if (!(d->item.mask & LVIF_TEXT) || d->item.iItem < 0 || d->item.iItem >= int(it.rows.size())) return 0;
                const int mid = it.rows[d->item.iItem];
                const ModelDef& def = app.defs[mid];
                switch (d->item.iSubItem) {
                    case 0: std::snprintf(it.text, sizeof it.text, "%s", def.name.c_str()); break;
                    case 1: std::snprintf(it.text, sizeof it.text, "%d", mid); break;
                    case 2: std::snprintf(it.text, sizeof it.text, "%d", itPlacedCount(mid)); break;
                    case 3: std::snprintf(it.text, sizeof it.text, "%s", poolShortName(def.pool)); break;
                    case 4: std::snprintf(it.text, sizeof it.text, "%s", def.txd.c_str()); break;
                    case 5: std::snprintf(it.text, sizeof it.text, "%.0f", def.drawDistance); break;
                    default: std::snprintf(it.text, sizeof it.text, "%s", def.ide.c_str());
                }
                d->item.pszText = it.text;
                return 0;
            }
            if (h->code == NM_DBLCLK) {
                const int row = reinterpret_cast<NMITEMACTIVATE*>(lp)->iItem;
                if (row >= 0 && row < int(it.rows.size())) itShowPlacements(it.rows[row]);
                return 0;
            }
            if (h->code == LVN_COLUMNCLICK) {
                const int col = reinterpret_cast<NMLISTVIEW*>(lp)->iSubItem;
                it.sortDesc = it.sortCol == col ? !it.sortDesc : false;
                it.sortCol = col;
                itRefill();
                return 0;
            }
            return 0;
        }
    }
    return DefWindowProcA(w, msg, wp, lp);
}

void createItemPanel(HWND parent) {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = itemProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "MEdItem";
    RegisterClassA(&wc);
    CreateWindowExA(0, "MEdItem", "", WS_CHILD | WS_CLIPCHILDREN, 0, 0, app.leftW, 400, parent, nullptr, GetModuleHandle(nullptr), nullptr);
}

// ---- Extract Object (after MEd's CMapView::extractObject, 0x0045c7f0) -----------------------------------------------
// Copies an object's model, texture dictionary and collision, plus an info file, into <folder>/<model>/ (core/extract.h).
// MEd copied the DFF and TXD only and asked "Please choose a location" each time; here the folder is remembered, several
// objects can be extracted at once (each model once), existing files are only overwritten after asking, and the result
// is summed up with an offer to open the folder. The game's files are only read.

// The IDE line of a definition, written the way the IDE files hold it (objs: id, model, txd, meshes, draw distances,
// flags; tobj: the same plus time on and off).
std::string ideLineOf(const IdeObject& o) {
    std::string s = std::to_string(o.id) + ", " + o.model + ", " + o.txd + ", " + std::to_string(o.drawDistances.size());
    for (double d : o.drawDistances) {
        char b[32];
        std::snprintf(b, sizeof b, "%g", d);
        s += std::string(", ") + b;
    }
    s += ", " + std::to_string(o.flags);
    if (o.timed) s += ", " + std::to_string(o.timeOn) + ", " + std::to_string(o.timeOff);
    return s;
}

ExtractRequest extractRequestFor(int id) {
    ExtractRequest r;
    auto d = app.defs.find(id);
    if (d == app.defs.end()) return r;
    const ModelDef& def = d->second;
    r.model = def.name;
    r.txd = def.txd;
    r.colFiles = app.colFiles;
    r.genericTxdPath = app.root + "/models/generic.txd";
    int placed = 0, sharing = 0;
    for (const IplInstance& o : app.instances) placed += o.id == id;
    for (const auto& kv : app.defs) sharing += kv.second.txd == def.txd;
    r.info = "Extracted with the MEd revival map editor from " + app.root + "\r\n"
             "Model: " + def.name + " (ID " + std::to_string(id) + ")\r\n"
             "IDE line: " + def.ideLine + "\r\n"
             "IDE file: " + def.ide + " (section " + (def.timed ? "tobj" : "objs") + ")\r\n"
             "Texture dictionary: " + def.txd + ".txd, used by " + std::to_string(sharing) + " object definition(s)\r\n"
             "Game pool of its placements: " + poolShortName(def.pool) + "\r\n"
             "Placed on the map: " + std::to_string(placed) + " time(s)\r\n"
             "Model and textures copied from: " + app.img.path() + "\r\n";
    return r;
}

// The models of a selection, each once, in selection order.
std::vector<int> modelIdsOf(const std::vector<int>& placements) {
    std::vector<int> ids;
    for (int idx : placements)
        if (idx >= 0 && idx < int(app.instances.size()) && std::find(ids.begin(), ids.end(), app.instances[idx].id) == ids.end()) ids.push_back(app.instances[idx].id);
    return ids;
}

std::vector<ExtractResult> extractModelsTo(const std::string& folder, const std::vector<int>& ids) {
    std::vector<ExtractResult> results;
    for (int id : ids) {
        const ExtractRequest req = extractRequestFor(id);
        if (req.model.empty()) continue;
        results.push_back(extractObject(app.img, folder, req));
    }
    return results;
}

void extractObjectsUi(HWND owner, const std::vector<int>& ids) {
    if (ids.empty()) {
        inspWarn(owner, "Select an object (or pick an item in the Item tab) first.", "Extract Object");
        return;
    }
    char last[MAX_PATH] = "";
    GetPrivateProfileStringA("Extract", "folder", "", last, sizeof last, modIniPath().c_str());
    BROWSEINFOA bi = {};
    bi.hwndOwner = owner;
    bi.lpszTitle = "Copy the model, textures and collision into this folder (a subfolder per object is made):";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = startFolderProc;
    bi.lParam = last[0] ? reinterpret_cast<LPARAM>(last) : 0;
    LPITEMIDLIST pid = SHBrowseForFolderA(&bi);
    if (!pid) return;
    char path[MAX_PATH];
    const bool got = SHGetPathFromIDListA(pid, path) != FALSE;
    CoTaskMemFree(pid);
    if (!got) return;
    WritePrivateProfileStringA("Extract", "folder", path, modIniPath().c_str());
    size_t existing = 0;
    for (int id : ids) existing += extractTargetsThatExist(path, extractRequestFor(id)).size();
    if (existing > 0) {
        const std::string q = std::to_string(existing) + " of the files are already in that folder (from an earlier extract). Replace them?";
        if (MessageBoxA(owner, q.c_str(), "Extract Object", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    }
    const std::vector<ExtractResult> results = extractModelsTo(path, ids);
    std::string text;
    size_t files = 0;
    for (const ExtractResult& r : results) {
        files += r.written.size();
        text += r.folder.substr(r.folder.find_last_of("\\/") + 1) + ": " + std::to_string(r.written.size()) + " file(s)\r\n";
        for (const std::string& w : r.warnings) text += "    " + w + "\r\n";
    }
    text = "Copied " + std::to_string(files) + " file(s) of " + std::to_string(results.size()) + " object(s) into " + path + ". The game's files were not changed.\r\n\r\n" +
           text + "\r\nOpen the folder?";
    if (MessageBoxA(owner, text.c_str(), "Extract Object", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
        const std::string open = results.size() == 1 ? results[0].folder : std::string(path);
        ShellExecuteA(owner, "open", open.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

// ---- Road builder: a road piece's ends as nodes, a strip of pieces that fit, a live preview, attach on click ------------
// Not in MEd (it only places and moves objects freely); added for map building on the user's request (2026-10-12).
// The ends of a road model are measured from its mesh (core/roadkit.h, checked against Rockstar's map). Selecting one road
// piece shows a sphere on each of its ends (bright: open, dim: already joined to another piece). Clicking an open
// sphere opens the strip along the top of the 3D view: every road piece with an end of the same width, once per
// distinct way of joining (a slope climbing or descending, a corner turning left or right, a T-junction by its stem or
// an arm), each with a thumbnail drawn so its joining end points down. Hovering a thumbnail shows the piece attached in
// the 3D view; clicking places it (with its LOD stand-in if the model has one), selects it and moves the strip on to
// the new piece's far end, so a road is built by clicking on. One undo step per piece. Esc closes the strip.

constexpr int kThumbW = 120, kThumbH = 90, kStripH = 160, kStripItemW = kThumbW + 12;

struct RoadVariant {
    int id = -1;    // model id
    int end = -1;   // which of its ends joins the target
    std::string name, label;
    int order = 0;  // for sorting: kind, then size
    HBITMAP thumb = nullptr;
};

struct RoadBuilderUi {
    std::map<int, std::vector<RoadEnd>> ends;  // model id -> ends in its own coordinates (only rd_ models have any)
    HWND strip = nullptr, closeButton = nullptr;
    int target = -1, targetEnd = -1;           // the placement and the end the strip attaches to
    std::vector<RoadVariant> variants;
    int hover = -1;                            // the variant under the mouse (previewed), -1 none
    int scroll = 0;                            // strip scroll, pixels
    bool tracking = false;
    bool thumbsPending = false;
    bool forcePreview = false;  // the command line's --preview-attach (no strip window there)
    std::map<std::pair<int, int>, HBITMAP> thumbCache;  // (model id, end) -> thumbnail
    GLUquadric* sphere = nullptr;
    std::string lastMessage;
} rb;

const std::vector<RoadEnd>& roadEndsOf(int id) {
    auto c = rb.ends.find(id);
    if (c != rb.ends.end()) return c->second;
    std::vector<RoadEnd>& out = rb.ends[id];
    auto d = app.defs.find(id);
    if (d == app.defs.end() || d->second.model.rfind("rd_", 0) != 0) return out;
    GlModel& gm = getModel(d->second.model, d->second.txd);
    std::vector<float> pos;
    for (const GlGeometry& g : gm.geoms)  // in the model's own coordinates: through each atomic's frame
        for (size_t i = 0; i + 2 < g.pos.size(); i += 3) {
            const float x = g.pos[i], y = g.pos[i + 1], z = g.pos[i + 2];
            pos.push_back(g.matrix[0] * x + g.matrix[4] * y + g.matrix[8] * z + g.matrix[12]);
            pos.push_back(g.matrix[1] * x + g.matrix[5] * y + g.matrix[9] * z + g.matrix[13]);
            pos.push_back(g.matrix[2] * x + g.matrix[6] * y + g.matrix[10] * z + g.matrix[14]);
        }
    out = roadEndsFor(d->second.name, pos);
    return out;
}

bool isRoadPiece(int idx) { return idx >= 0 && idx < int(app.instances.size()) && !roadEndsOf(app.instances[idx].id).empty(); }

// An end of a placed piece is joined when another placed road piece has an end meeting it.
bool roadEndJoined(int idx, const RoadEnd& w) {
    for (size_t j = 0; j < app.instances.size(); ++j) {
        if (int(j) == idx) continue;
        const IplInstance& o = app.instances[j];
        if (std::fabs(o.x - w.x) > 80 || std::fabs(o.y - w.y) > 80) continue;
        if (!app.regionOn.empty() && !app.regionOn[app.instFile[j]]) continue;
        for (const RoadEnd& e : roadEndsOf(o.id))
            if (endsMeet(w, endInWorld(o, e))) return true;
    }
    return false;
}

// The piece whose nodes are shown: exactly one road piece selected (or the strip's target).
int roadNodePiece() {
    if (((rb.strip && IsWindowVisible(rb.strip)) || rb.forcePreview) && isRoadPiece(rb.target)) return rb.target;
    return app.selected.size() == 1 && isRoadPiece(app.selected[0]) ? app.selected[0] : -1;
}

// What joining by end `k` of model `id` does, seen from that end: the other ends straight on, to the left or right, and
// how far they climb. Gives the label, a sort key, and a key that is the same for two ends that give the same result.
std::string roadVariantShape(int id, int k, std::string& label, int& order) {
    const std::vector<RoadEnd>& e = roadEndsOf(id);
    const RoadEnd& j = e[size_t(k)];
    const double tx = -j.dx, ty = -j.dy;  // into the piece
    const double lx = -ty, ly = tx;       // to the left
    std::vector<std::string> parts;
    std::string key;
    int straight = 0, left = 0, right = 0;
    double climb = 0, shift = 0, length = 0, turn = 0;
    for (size_t o = 0; o < e.size(); ++o) {
        if (int(o) == k) continue;
        const double rx = e[o].x - j.x, ry = e[o].y - j.y, rz = e[o].z - j.z;
        const double along = rx * tx + ry * ty, side = rx * lx + ry * ly;
        const double dl = e[o].dx * lx + e[o].dy * ly, ds = e[o].dx * tx + e[o].dy * ty;
        auto r1 = [](double v) { return std::round(v * 10) / 10 + 0.0; };  // + 0.0: no "-0.0" next to "0.0"
        char b[96];
        auto r2 = [](double v) { return std::round(v * 100) / 100 + 0.0; };
        std::snprintf(b, sizeof b, "%.1f,%.1f,%.1f,%.2f,%.2f;", r1(along), r1(side), r1(rz), r2(dl), r2(ds));
        parts.push_back(b);
        turn = std::atan2(dl, ds) * 57.29577951308232;  // where the other end leads: 0 straight on, + to the left
        climb = rz;
        if (ds > 0.5) { ++straight; shift = side; length = along; }
        else if (dl > 0.5) ++left;
        else ++right;
    }
    const char* w = j.width < 25 ? "narrow road" : j.width < 35 ? "road" : "wide road";
    if (e.size() == 2 && std::fabs(turn) > 1 && std::fabs(std::fabs(turn) - 90) > 1) {  // a bend that is not a quarter turn
        char b[96];
        std::snprintf(b, sizeof b, "bends %s %.1f%s", turn > 0 ? "left" : "right", std::fabs(turn), "\xb0");  // the degree sign in the ANSI code page
        label = b;
        if (std::fabs(climb) > 0.05) {
            std::snprintf(b, sizeof b, ", %s %.2f", climb > 0 ? "climbs" : "descends", std::fabs(climb));
            label += b;
        }
        order = 2500 + int(std::fabs(turn) * 10) + (turn > 0 ? 0 : 1);
    } else if (e.size() == 2 && straight == 1) {
        char b[96];
        if (std::fabs(climb) > 0.05) std::snprintf(b, sizeof b, "%s %.2f over %.0f", climb > 0 ? "climbs" : "descends", std::fabs(climb), length);
        else if (std::fabs(shift) > 0.5) std::snprintf(b, sizeof b, "%.0f long, shifts %s %.0f", length, shift > 0 ? "left" : "right", std::fabs(shift));
        else std::snprintf(b, sizeof b, "straight, %.0f long", length);
        label = b;
        order = (std::fabs(climb) > 0.05 ? 1000 : std::fabs(shift) > 0.5 ? 2000 : 0) + int(length);
    } else if (e.size() == 2) {
        label = left ? "turns left" : "turns right";
        order = 3000 + (left ? 0 : 1);
    } else if (e.size() == 3) {
        label = straight ? (std::string("T-junction: on, branch ") + (left ? "left" : "right")) : "T-junction: left and right";
        order = 4000 + (straight ? (left ? 1 : 2) : 0);
    } else if (e.size() == 4) {
        label = "crossroads";
        order = 5000;
    } else {
        label = "dead end";
        order = 6000;
    }
    (void)w;  // every piece in the strip fits the end's width, so the width is not repeated in the label
    std::sort(parts.begin(), parts.end());  // the same ends in any order are the same shape
    for (const std::string& part : parts) key += part;
    return key;
}

// Every road piece that has an end of this width, once per distinct way of joining.
std::vector<RoadVariant> roadVariantsFor(double width) {
    std::vector<RoadVariant> out;
    for (const auto& kv : app.defs) {
        if (kv.second.model.rfind("rd_", 0) != 0) continue;
        const std::vector<RoadEnd>& e = roadEndsOf(kv.first);
        if (e.size() < 2) continue;  // a piece with one measurable end cannot be built on
        std::set<std::string> seen;
        for (size_t k = 0; k < e.size(); ++k) {
            if (std::fabs(e[k].width - width) > 0.6) continue;
            RoadVariant v;
            v.id = kv.first;
            v.end = int(k);
            v.name = kv.second.name.size() > 3 ? kv.second.name.substr(3) : kv.second.name;
            if (!seen.insert(roadVariantShape(kv.first, int(k), v.label, v.order)).second) continue;  // the same result as another end
            out.push_back(v);
        }
    }
    std::sort(out.begin(), out.end(), [](const RoadVariant& a, const RoadVariant& b) { return a.order != b.order ? a.order < b.order : a.name < b.name; });
    return out;
}

// Where a variant goes when it is attached to the strip's target end.
bool roadVariantPlacement(const RoadVariant& v, IplInstance& out) {
    if (!isRoadPiece(rb.target)) return false;
    const std::vector<RoadEnd>& te = roadEndsOf(app.instances[rb.target].id);
    if (rb.targetEnd < 0 || rb.targetEnd >= int(te.size())) return false;
    const std::vector<RoadEnd>& ve = roadEndsOf(v.id);
    if (v.end < 0 || v.end >= int(ve.size())) return false;
    const RoadEnd target = endInWorld(app.instances[rb.target], te[size_t(rb.targetEnd)]);
    out = IplInstance();
    out.id = v.id;
    out.model = app.defs[v.id].name;
    return attachPlacement(target, ve[size_t(v.end)], out.x, out.y, out.z, out.rotX, out.rotY, out.rotZ, out.rotW);
}

void roadStripRefresh();

// Point the strip at an end of a placed piece.
void roadSetTarget(int idx, int end) {
    rb.target = idx;
    rb.targetEnd = end;
    rb.hover = -1;
    rb.scroll = 0;
    const std::vector<RoadEnd>& e = roadEndsOf(app.instances[idx].id);
    rb.variants = roadVariantsFor(e[size_t(end)].width);
    for (RoadVariant& v : rb.variants) {
        auto t = rb.thumbCache.find({v.id, v.end});
        v.thumb = t == rb.thumbCache.end() ? nullptr : t->second;
        if (!v.thumb) rb.thumbsPending = true;
    }
    roadStripRefresh();
}

LRESULT CALLBACK roadStripProc(HWND w, UINT msg, WPARAM wp, LPARAM lp);

void openRoadStrip(int idx, int end) {
    if (!isRoadPiece(idx)) return;
    if (!rb.strip) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc = roadStripProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_HAND);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "MEdRoadStrip";
        RegisterClassA(&wc);
        rb.strip = CreateWindowExA(WS_EX_CLIENTEDGE, "MEdRoadStrip", "", WS_CHILD | WS_CLIPCHILDREN, app.viewX, app.toolbarH, app.width, kStripH, app.hwnd, nullptr,
                                   GetModuleHandle(nullptr), nullptr);
        rb.closeButton = CreateWindowA("BUTTON", "Close (Esc)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 90, 22, rb.strip, (HMENU)1, GetModuleHandle(nullptr), nullptr);
        SendMessageA(rb.closeButton, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
    roadSetTarget(idx, end);
    MoveWindow(rb.strip, app.viewX, app.toolbarH, app.width, kStripH, TRUE);
    MoveWindow(rb.closeButton, app.width - 100, 2, 90, 22, TRUE);
    ShowWindow(rb.strip, SW_SHOW);
}

void closeRoadStrip() {
    if (rb.strip) ShowWindow(rb.strip, SW_HIDE);
    rb.hover = -1;
    rb.target = -1;
    rb.targetEnd = -1;
}

bool roadStripOpen() { return rb.strip && IsWindowVisible(rb.strip); }

// Place a variant at the target end: the piece (and its LOD stand-in when the model has one) into the target's file,
// one undo step, selected; the strip moves on to the new piece's end straight ahead (else its first open end).
int placeRoadVariant(int vi) {
    if (vi < 0 || vi >= int(rb.variants.size())) return -1;
    const RoadVariant v = rb.variants[size_t(vi)];
    IplInstance p;
    if (!roadVariantPlacement(v, p)) {
        rb.lastMessage = "That end is tilted, so a level piece cannot be attached to it.";
        if (!insp.quiet) MessageBoxA(app.hwnd, rb.lastMessage.c_str(), "Attach road piece", MB_ICONINFORMATION);
        return -1;
    }
    const int file = app.instFile[size_t(rb.target)];
    app.history.snapshot(captureState());
    app.instances.push_back(p);
    app.instFile.push_back(file);
    const int idx = int(app.instances.size()) - 1;
    const int lod = lodModelIdFor(v.id);
    if (lod >= 0) {
        IplInstance l = p;
        l.id = lod;
        l.model = app.defs[lod].name;
        app.instances.push_back(l);
        app.instFile.push_back(file);
    }
    app.dirty.insert(file);
    app.selected.assign(1, idx);
    app.history.endSession();
    buildLodPartnerRanges();
    refreshRegions();
    // carry on from the new piece: the end straight ahead of the joining one, else the first open one
    const std::vector<RoadEnd>& e = roadEndsOf(v.id);
    const RoadEnd& j = e[size_t(v.end)];
    int next = -1;
    for (size_t k = 0; k < e.size(); ++k)
        if (int(k) != v.end && e[k].dx * j.dx + e[k].dy * j.dy < -0.5 && !roadEndJoined(idx, endInWorld(app.instances[idx], e[k]))) next = int(k);
    for (size_t k = 0; k < e.size() && next < 0; ++k)
        if (int(k) != v.end && !roadEndJoined(idx, endInWorld(app.instances[idx], e[k]))) next = int(k);
    if (next >= 0 && roadStripOpen()) roadSetTarget(idx, next);
    else closeRoadStrip();
    return idx;
}

// The node under the mouse (client coordinates of the 3D view), -1 for none; `open` says whether it is free.
int roadNodeAt(int mx, int my, bool* open = nullptr) {
    const int piece = roadNodePiece();
    if (piece < 0) return -1;
    glViewport(app.viewX, 0, app.width, app.height);
    setViewMatrices(-1, -1);
    GLdouble mv[16], pr[16];
    GLint vp[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, pr);
    glGetIntegerv(GL_VIEWPORT, vp);
    const std::vector<RoadEnd>& e = roadEndsOf(app.instances[piece].id);
    int best = -1;
    double bestD = 18;
    for (size_t k = 0; k < e.size(); ++k) {
        const RoadEnd w = endInWorld(app.instances[piece], e[k]);
        GLdouble sx, sy, sz;
        if (!gluProject(w.x, w.y, w.z + 1.5, mv, pr, vp, &sx, &sy, &sz) || sz < 0 || sz > 1) continue;
        const double d = std::hypot(sx - mx, (vp[3] - sy) - my);
        if (d < bestD) { bestD = d; best = int(k); }
    }
    if (best >= 0 && open) *open = !roadEndJoined(piece, endInWorld(app.instances[piece], e[size_t(best)]));
    return best;
}

// A click (no drag) in the 3D view: on an open node it opens the strip there. True when it was a node.
bool roadClick(int mx, int my) {
    bool open = false;
    const int k = roadNodeAt(mx, my, &open);
    if (k < 0) return false;
    if (!open) {
        rb.lastMessage = "That end is already joined to another road piece.";
        return true;
    }
    openRoadStrip(roadNodePiece(), k);
    return true;
}

// Drawn after the map: the nodes of the selected road piece and the previewed piece.
void drawRoadOverlay() {
    if (!rb.sphere) rb.sphere = gluNewQuadric();
    // the preview: the hovered piece where it would go, with a cyan box round it
    if ((roadStripOpen() || rb.forcePreview) && rb.hover >= 0 && rb.hover < int(rb.variants.size())) {
        IplInstance p;
        if (roadVariantPlacement(rb.variants[size_t(rb.hover)], p)) {
            const ModelDef& def = app.defs[p.id];
            GlModel& gm = getModel(def.model, def.txd);
            glPushMatrix();
            applyPlacement(p);
            drawModel(gm);
            glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_POLYGON_BIT);
            glDisable(GL_LIGHTING);
            glDisable(GL_TEXTURE_2D);
            glColor3f(0, 1, 1);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            double lo[2] = {1e9, 1e9}, hi[2] = {-1e9, -1e9};  // the model's own extent
            for (const GlGeometry& g : gm.geoms)
                for (size_t i = 0; i + 2 < g.pos.size(); i += 3)
                    for (int k = 0; k < 2; ++k) { lo[k] = std::min(lo[k], double(g.pos[i + k])); hi[k] = std::max(hi[k], double(g.pos[i + k])); }
            glBegin(GL_LINE_LOOP);
            glVertex3d(lo[0], lo[1], 0.3); glVertex3d(hi[0], lo[1], 0.3); glVertex3d(hi[0], hi[1], 0.3); glVertex3d(lo[0], hi[1], 0.3);
            glEnd();
            glPopAttrib();
            glPopMatrix();
        }
    }
    const int piece = roadNodePiece();
    if (piece < 0) return;
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_TEXTURE_2D);
    const std::vector<RoadEnd>& e = roadEndsOf(app.instances[piece].id);
    for (size_t k = 0; k < e.size(); ++k) {
        const RoadEnd w = endInWorld(app.instances[piece], e[k]);
        const bool joined = roadEndJoined(piece, w);
        const bool isTarget = (roadStripOpen() || rb.forcePreview) && piece == rb.target && int(k) == rb.targetEnd;
        if (isTarget) glColor3f(1, 0.85f, 0);        // the end the strip attaches to: yellow
        else if (joined) glColor3f(0.35f, 0.35f, 0.35f);  // joined: dim
        else glColor3f(0.1f, 0.9f, 0.2f);            // open: green
        glPushMatrix();
        glTranslated(w.x, w.y, w.z + 1.5);
        gluSphere(rb.sphere, isTarget ? 1.8 : 1.4, 16, 12);
        glPopMatrix();
    }
    glPopAttrib();
}

// Thumbnails are drawn into the back buffer before a frame and read back (the frame then clears over them): the piece
// turned so its joining end points down, seen from above and behind that end.
void renderRoadThumbnails() {
    if (!rb.thumbsPending) return;
    rb.thumbsPending = false;
    for (RoadVariant& v : rb.variants) {
        if (v.thumb) continue;
        auto c = rb.thumbCache.find({v.id, v.end});
        if (c != rb.thumbCache.end()) { v.thumb = c->second; continue; }
        const ModelDef& def = app.defs[v.id];
        GlModel& gm = getModel(def.model, def.txd);
        const RoadEnd& j = roadEndsOf(v.id)[size_t(v.end)];
        double lo[3] = {1e9, 1e9, 1e9}, hi[3] = {-1e9, -1e9, -1e9};
        for (const GlGeometry& g : gm.geoms)
            for (size_t i = 0; i + 2 < g.pos.size(); i += 3)
                for (int k = 0; k < 3; ++k) { lo[k] = std::min(lo[k], double(g.pos[i + k])); hi[k] = std::max(hi[k], double(g.pos[i + k])); }
        const double cx = (lo[0] + hi[0]) / 2, cy = (lo[1] + hi[1]) / 2, cz = (lo[2] + hi[2]) / 2;
        const double r = std::max(5.0, std::hypot(hi[0] - lo[0], hi[1] - lo[1]) / 2);
        glViewport(app.viewX, 0, kThumbW, kThumbH);
        glEnable(GL_SCISSOR_TEST);
        glScissor(app.viewX, 0, kThumbW, kThumbH);
        glClearColor(0.18f, 0.2f, 0.24f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(40, double(kThumbW) / kThumbH, 1, 4000);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(0, -r * 1.9, r * 1.7, 0, 0, 0, 0, 0, 1);  // from the south, above
        const GLfloat lightPos[4] = {0.3f, -1, 1, 0};
        glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
        glRotated(std::atan2(-1.0, 0.0) * 57.29577951 - std::atan2(j.dy, j.dx) * 57.29577951, 0, 0, 1);  // joining end towards the camera
        glTranslated(-cx, -cy, -cz);
        glEnable(GL_DEPTH_TEST);
        drawModel(gm);
        std::vector<unsigned char> px(size_t(kThumbW) * kThumbH * 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(app.viewX, 0, kThumbW, kThumbH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof bi.bmiHeader;
        bi.bmiHeader.biWidth = kThumbW;
        bi.bmiHeader.biHeight = kThumbH;  // bottom-up, as glReadPixels gives it
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        void* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (bmp && bits) {
            unsigned char* d = static_cast<unsigned char*>(bits);
            for (size_t i = 0; i < size_t(kThumbW) * kThumbH; ++i) {  // RGBA -> BGRA
                d[i * 4] = px[i * 4 + 2];
                d[i * 4 + 1] = px[i * 4 + 1];
                d[i * 4 + 2] = px[i * 4];
                d[i * 4 + 3] = 255;
            }
        }
        rb.thumbCache[{v.id, v.end}] = bmp;
        v.thumb = bmp;
    }
    glClearColor(0, 0, 0, 0);
    glViewport(app.viewX, 0, app.width, app.height);
    setViewMatrices(-1, -1);  // the frame's light is set under this view, as every frame
    if (rb.strip) InvalidateRect(rb.strip, nullptr, FALSE);
}

int roadStripItemAt(int x, int y) {
    if (y < 26 || y > 26 + kThumbH + 34) return -1;
    const int i = (x + rb.scroll - 6) / kStripItemW;
    if (x + rb.scroll - 6 < 0 || i < 0 || i >= int(rb.variants.size())) return -1;
    return i;
}

void roadStripRefresh() {
    if (rb.strip) InvalidateRect(rb.strip, nullptr, TRUE);
}

LRESULT CALLBACK roadStripProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(w, &ps);
            RECT cr;
            GetClientRect(w, &cr);
            HDC mem = CreateCompatibleDC(dc);  // drawn off screen, so scrolling does not flicker
            HBITMAP back = CreateCompatibleBitmap(dc, cr.right, cr.bottom);
            HGDIOBJ oldBack = SelectObject(mem, back);
            FillRect(mem, &cr, GetSysColorBrush(COLOR_BTNFACE));
            SelectObject(mem, GetStockObject(DEFAULT_GUI_FONT));
            SetBkMode(mem, TRANSPARENT);
            std::string head;
            if (isRoadPiece(rb.target)) {
                const RoadEnd& e = roadEndsOf(app.instances[rb.target].id)[size_t(rb.targetEnd)];
                head = "Attach to the yellow end of " + app.instances[rb.target].model + " (" + std::to_string(int(std::round(e.width))) + " wide): " +
                       std::to_string(rb.variants.size()) + " pieces fit. Hover to preview, click to place. Mouse wheel scrolls.";
                // the path budget: every kit piece brings traffic and pedestrian paths, and the game's tables are small
                const PathBudget& pb = currentPathBudget();
                const PathTableLimits& pl = app.pathLimits;
                head += "   Paths: " + std::to_string(std::max(pl.nodes - pb.nodesPeak(), 0)) + " nodes and " + std::to_string(std::max(pl.carLinks - pb.carLinks, 0)) +
                        " car links left";
                if (pb.nodesPeak() > pl.nodes || pb.carLinks > pl.carLinks || pb.connections > pl.connections || pb.objects > pl.objects) head += " (ALREADY OVER: see Check Map)";
                if (rb.hover >= 0 && rb.hover < int(rb.variants.size())) {
                    auto pd = app.pathData.find(rb.variants[size_t(rb.hover)].id);
                    if (pd != app.pathData.end()) {
                        const PathCost c = pathCostOf(pd->second);
                        head += "; this piece adds " + std::to_string(c.carNodes) + " car + " + std::to_string(c.pedNodes) + " pedestrian nodes";
                    } else {
                        head += "; this piece has no paths";
                    }
                }
            }
            TextOutA(mem, 8, 6, head.c_str(), int(head.size()));
            HDC tdc = CreateCompatibleDC(dc);
            for (size_t i = 0; i < rb.variants.size(); ++i) {
                const int x = 6 + int(i) * kStripItemW - rb.scroll, y = 26;
                if (x + kStripItemW < 0 || x > cr.right) continue;
                const RoadVariant& v = rb.variants[i];
                RECT box = {x - 2, y - 2, x + kThumbW + 2, y + kThumbH + 34};
                if (int(i) == rb.hover) FillRect(mem, &box, GetSysColorBrush(COLOR_HIGHLIGHT));
                if (v.thumb) {
                    HGDIOBJ o = SelectObject(tdc, v.thumb);
                    BitBlt(mem, x, y, kThumbW, kThumbH, tdc, 0, 0, SRCCOPY);
                    SelectObject(tdc, o);
                } else {
                    RECT t = {x, y, x + kThumbW, y + kThumbH};
                    FillRect(mem, &t, GetSysColorBrush(COLOR_3DSHADOW));
                }
                SetTextColor(mem, GetSysColor(int(i) == rb.hover ? COLOR_HIGHLIGHTTEXT : COLOR_BTNTEXT));
                RECT t1 = {x, y + kThumbH + 2, x + kThumbW, y + kThumbH + 16};
                DrawTextA(mem, v.name.c_str(), -1, &t1, DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                RECT t2 = {x - 4, y + kThumbH + 16, x + kThumbW + 4, y + kThumbH + 32};
                DrawTextA(mem, v.label.c_str(), -1, &t2, DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            DeleteDC(tdc);
            BitBlt(dc, 0, 0, cr.right, cr.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBack);
            DeleteObject(back);
            DeleteDC(mem);
            EndPaint(w, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE: {
            if (!rb.tracking) {
                TRACKMOUSEEVENT t = {sizeof t, TME_LEAVE, w, 0};
                TrackMouseEvent(&t);
                rb.tracking = true;
            }
            const int i = roadStripItemAt(int(short(LOWORD(lp))), int(short(HIWORD(lp))));
            if (i != rb.hover) {
                rb.hover = i;
                InvalidateRect(w, nullptr, FALSE);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            rb.tracking = false;
            if (rb.hover != -1) {
                rb.hover = -1;
                InvalidateRect(w, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP: {
            const int i = roadStripItemAt(int(short(LOWORD(lp))), int(short(HIWORD(lp))));
            if (i >= 0) placeRoadVariant(i);
            InvalidateRect(w, nullptr, FALSE);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            const int total = int(rb.variants.size()) * kStripItemW + 12;
            RECT cr;
            GetClientRect(w, &cr);
            rb.scroll = std::max(0, std::min(rb.scroll - GET_WHEEL_DELTA_WPARAM(wp) / 120 * kStripItemW, std::max(0, total - int(cr.right))));
            const POINT pt = {int(short(LOWORD(lp))), int(short(HIWORD(lp)))};
            POINT c = pt;
            ScreenToClient(w, &c);
            rb.hover = roadStripItemAt(c.x, c.y);
            InvalidateRect(w, nullptr, FALSE);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == 1) closeRoadStrip();
            return 0;
        case WM_USER + 1:  // tests: hover item wp (-1 none) without the real mouse, which would report leaving at once
            rb.hover = int(wp) >= 0 && int(wp) < int(rb.variants.size()) ? int(wp) : -1;
            InvalidateRect(w, nullptr, FALSE);
            return 0;
    }
    return DefWindowProcA(w, msg, wp, lp);
}

// Ctrl+R: the strip on the selected road piece's first open end (the same as clicking that node).
void roadAttachFirstOpen(HWND owner) {
    const int piece = app.selected.size() == 1 ? app.selected[0] : -1;
    if (!isRoadPiece(piece)) {
        rb.lastMessage = "Select one road piece (a model named rd_...) first; its ends show as balls you can click.";
        if (!insp.quiet) MessageBoxA(owner, rb.lastMessage.c_str(), "Attach road piece", MB_ICONINFORMATION);
        return;
    }
    const std::vector<RoadEnd>& e = roadEndsOf(app.instances[piece].id);
    for (size_t k = 0; k < e.size(); ++k)
        if (!roadEndJoined(piece, endInWorld(app.instances[piece], e[k]))) {
            openRoadStrip(piece, int(k));
            return;
        }
    rb.lastMessage = "Every end of this road piece is already joined to another piece.";
    if (!insp.quiet) MessageBoxA(owner, rb.lastMessage.c_str(), "Attach road piece", MB_ICONINFORMATION);
}

// Once a frame: the strip closes when its target is no longer there (undo, delete, another selection).
void roadSync() {
    if (!roadStripOpen()) return;
    const bool targetOk = isRoadPiece(rb.target) && std::find(app.selected.begin(), app.selected.end(), rb.target) != app.selected.end() && app.selected.size() == 1;
    if (!targetOk) closeRoadStrip();
}

// ---- Model viewers: MEd's "DFF Properties" (CDFFView) around its 3D widget CGLView -----------------------------------
// CGLView (constructor 0x004144e0, initializeGL 0x00414160, paintGL 0x00414900, slots via 0x004662a0): its own
// window and GL context; view state rotation X -45, Z 0, Y 0 degrees and translation (0, 0, -52) (reset 0x004140b0);
// projection gluPerspective(45, aspect, 1, 5000), then glTranslatef(x, y, z), glRotatef(X, 1,0,0), glRotatef(Z, 0,0,1),
// glRotatef(Y, 0,1,0) and the light at (0, 0, 200, 0) set on the projection matrix; the model is drawn under an
// identity modelview. Lighting is off for models (drawn with their material colours and textures). Right button drags
// turn (1 degree per pixel: Z by x, X by y), left button drags move (0.25 per pixel), the wheel moves in by 1 (Shift: 10),
// Zoom In / Zoom Out by 3 (0x004143c0 / 0x004143e0), "Automatically Rotate" adds Z 1, X 0.5, Y 0.4 every 40 ms (QTimer
// 40, slot 0x004140e0; an angle past 360 restarts at 1), Wireframe draws GL_LINE. The dialog (0x00415610) has Show
// Wireframe, Show 2DFX Lighting, Show Night Vertex Colours, "Show Object Number" and an information table.
// MEd's "Collision Object Properties" (CColView, 0x00407d80) uses the same widget in its second mode (CGLView 0x00414460 /
// 0x00414730): lighting on with the light at (0, -1, -0.5, 0), then (0x004054e0) the boxes as solid boxes, the spheres
// as 5 x 5 GLU spheres and the mesh as lit triangles, each coloured by its surface material (0x004040b0), then the
// selection axes, circle and red bounds box (0x00404ff0). Check boxes: Show Boxes, Show Spheres, Show Collision Mesh,
// Show Shadow Mesh (COL3 only) and Wireframe; table: file format, boxes, spheres, mesh vertices and faces.
// Deliberate differences: MEd's projection uses an aspect of 1 whatever the window's shape (models stretch); here the
// true ratio. The view turns about the middle of the mesh (MEd: the model's origin, often a building's corner) and starts
// at a distance fitted to the model's size (MEd: always 52, so a cone is a dot and a building does not fit).

constexpr int kViewerPanelW = 270;

struct GlViewState {  // CGLView's view fields (+0x24 .. +0x3c)
    float rotX = -45, rotZ = 0, rotY = 0, tx = 0, ty = 0, tz = -52;
};

struct ModelViewer {
    HWND wnd = nullptr, gl = nullptr, info = nullptr, objectBox = nullptr;
    HDC dc = nullptr;
    HGLRC rc = nullptr;
    int id = -1;               // model id
    std::string dff, txd;      // model and texture dictionary
    DffModel model;            // as read from the archive (for the information table)
    GlViewState view, home;    // current and Reset View
    bool autoRotate = false, wireframe = false, night = false, fx = false;
    int object = 0;            // Show Object Number: 0 = all, else 1-based
    POINT last = {0, 0};
    DWORD lastTick = 0;
    bool closed = false;
    float centre[3] = {0, 0, 0};  // the middle of the model's mesh: the view turns about it
    int kind = 0;                 // 0 DFF Properties, 1 Collision Object Properties
    std::string file;             // a loose file from disk ("" = a model of the game)
    std::string modelKey;         // a loose model's entry in app.models
    std::vector<ColModel> colModels;  // every collision model of a loose .col (Model in file picks one)
    ColModel col;                 // the collision model (kind 1), its record as stored in the COL file
    std::string colSource;        // the COL file it came from
    bool showBoxes = true, showSpheres = true, showMesh = true;
    // the Edit tab (collision only; ceXxx below)
    HWND tabs = nullptr;                // View | Edit
    bool editTab = false;
    std::vector<HWND> viewCtrls;        // the View tab's own controls
    std::map<int, HWND> ce;             // the Edit tab's controls by id
    std::vector<HWND> ceLabels;         // its labels
    ColModel loaded;                    // as opened or last saved (unsaved = different from this)
    std::vector<ColModel> undo, redo;
    bool dirty = false;
    std::set<int> selBoxes, selSpheres;
    std::set<size_t> selFaces;
    int selBoxFace = -1;                // the side of the one selected box that push / pull moves
    int newMaterial = 0;                // the material Add Box / Add Sphere give
    bool showRef = true;                // the model's outline
    bool dragging = false, dragMoved = false;
    int dragMode = 0;                   // 1 click or pan, 2 push / pull, 3 selection rectangle
    POINT down = {0, 0}, cur = {0, 0};
    ColModel dragStart;
    double dragP[3] = {0, 0, 0}, dragN[3] = {0, 0, 1};
    float dragD = 0;
    double proj[16] = {}, mv[16] = {};  // the 3D view's matrices as last drawn (for picking)
    GLint vp[4] = {0, 0, 1, 1};
    int rev = 0, openEdgesRev = -1;
    std::vector<std::array<uint32_t, 2>> openEdgesCache;
    std::string shownShapeKey, shownInfo, lastMessage;
};
std::vector<std::unique_ptr<ModelViewer>> viewers;

enum { ID_MV_ZOOMIN = 9001, ID_MV_ZOOMOUT, ID_MV_RESET, ID_MV_ROTATE, ID_MV_WIRE, ID_MV_2DFX, ID_MV_NIGHT, ID_MV_OBJECT, ID_MV_INFO, ID_MV_CLOSE,
       ID_MV_LBL_OBJECT, ID_MV_BOXES, ID_MV_SPHERES, ID_MV_MESH, ID_MV_SHADOW };
enum { ID_CE_TABS = 9301, ID_CE_ADDBOX, ID_CE_ADDSPHERE, ID_CE_SELSIDE, ID_CE_SELALL, ID_CE_SELNONE, ID_CE_DELETE, ID_CE_DIST, ID_CE_OUT, ID_CE_IN,
       ID_CE_MX, ID_CE_MY, ID_CE_MZ, ID_CE_MOVE, ID_CE_MAT, ID_CE_F0, ID_CE_F1, ID_CE_F2, ID_CE_F3, ID_CE_F4, ID_CE_F5, ID_CE_APPLY,
       ID_CE_SHOWREF, ID_CE_UNDO, ID_CE_REDO, ID_CE_STATUS, ID_CE_REGION, ID_CE_NEWREGION, ID_CE_SAVE, ID_CE_LBL };

ModelViewer* viewerOf(HWND w) {
    for (auto& v : viewers)
        if (v->wnd == w || v->gl == w) return v.get();
    return nullptr;
}

// The information table of MEd's DFF Properties (its rows, in plain text).
std::string dffInfoText(const ModelViewer& v) {
    const DffModel& m = v.model;
    size_t verts = 0, faces = 0, mats = 0, texMats = 0, splits = 0;
    bool colours = false, uv = false, normals = false, strips = false, night = false, fx = false;
    for (const DffGeometry& g : m.geometries) {
        verts += g.vertexCount();
        faces += g.triangles.size() / 3;
        mats += g.materials.size();
        for (const DffMaterial& mt : g.materials) texMats += mt.textured;
        splits += g.splits.size();
        colours = colours || !g.colors.empty();
        uv = uv || !g.uvs.empty();
        normals = normals || !g.normals.empty();
        strips = strips || g.triangleStrips;
        night = night || g.nightColors;
        fx = fx || g.has2dfx;
    }
    auto yn = [](bool b) { return std::string(b ? "Yes" : "No"); };
    char ver[32];
    std::snprintf(ver, sizeof ver, "%X", m.version);
    std::string t = v.file.empty() ? "Model: " + v.dff + ".dff\r\nTextures: " + v.txd + ".txd\r\n"
                                   : "File: " + v.file + "\r\nTextures: " + v.txd + "\r\n";
    t += "RenderWare version: " + std::string(ver) + "\r\n\r\n";
    t += "Number of Objects: " + std::to_string(m.atomics.size()) + "\r\n";
    t += "Number of Vertices: " + std::to_string(verts) + "\r\n";
    t += "Number of Faces: " + std::to_string(faces) + "\r\n";
    t += "Colours: " + yn(colours) + "\r\n";
    t += "UV Mapping: " + yn(uv) + "\r\n";
    t += "Normals: " + yn(normals) + "\r\n";
    t += "Number of Materials: " + std::to_string(mats) + "\r\n";
    t += "Number of Textured Materials: " + std::to_string(texMats) + "\r\n";
    t += "Number of Triangle Splits: " + std::to_string(splits) + "\r\n";
    t += std::string("Split Format: ") + (splits == 0 ? "none" : strips ? "Triangle Strips" : "Triangle Lists") + "\r\n";
    t += "Night Vertex Colours: " + yn(night) + "\r\n";
    t += "Contains 2DFX: " + yn(fx) + "\r\n";
    if (!m.geometries.empty()) {
        t += "\r\nTextures used:\r\n";
        std::set<std::string> seen;
        for (const DffGeometry& g : m.geometries)
            for (const DffMaterial& mt : g.materials)
                if (mt.textured && seen.insert(toLower(mt.texture)).second) t += "  " + mt.texture + "\r\n";
    }
    return t;
}

// MEd's colour for a surface material (0x004040b0): grey for most, green grass and hedges, blue water and glass, sandy
// sand, brown wood.
void colMaterialColour(int material, unsigned char rgb[3]) {
    unsigned int c;
    switch (material) {
        case 0: c = 0x646464; break;
        case 1: c = 0x969696; break;
        case 2: case 0x22: c = 0x00ff00; break;
        case 3: case 0x10: c = 0x408080; break;
        case 4: c = 0x1b3636; break;
        case 5: c = 0x403636; break;
        case 6: case 8: case 10: case 11: case 12: case 13: case 14: case 15: c = 0x808080; break;
        case 7: c = 0xff8080; break;
        case 9: c = 0x000080; break;
        case 0x12: case 0x21: c = 0x9dceff; break;
        case 0x13: c = 0xff0000; break;
        case 0x14: case 0x15: case 0x16: c = 0x0039b0; break;
        case 0x17: case 0x18: case 0x1b: case 0x1c: case 0x1e: c = 0xc0c0c0; break;
        case 0x19: c = 0x36d700; break;
        case 0x1a: c = 0x337533; break;
        case 0x20: c = 0x0000ff; break;
        default: c = 0x646464; break;
    }
    rgb[0] = (unsigned char)(c & 0xff);  // red is the low byte (grass 0x00ff00 green, water 0xff0000 blue)
    rgb[1] = (unsigned char)((c >> 8) & 0xff);
    rgb[2] = (unsigned char)((c >> 16) & 0xff);
}

// The table of MEd's Collision Object Properties.
std::string colInfoText(const ModelViewer& v) {
    const ColModel& c = v.col;
    std::string t = "Model: " + v.dff + "\r\nCollision file: " + v.colSource + "\r\n\r\n";
    t += "File Format: COLL (version " + std::to_string(c.version) + ", GTA III / Vice City)\r\n";
    t += "Number of Boxes: " + std::to_string(c.boxes.size()) + "\r\n";
    t += "Number of Spheres: " + std::to_string(c.spheres.size()) + "\r\n";
    t += "Collision Mesh Vertices: " + std::to_string(c.vertices.size() / 3) + "\r\n";
    t += "Collision Mesh Face Count: " + std::to_string(c.faces.size()) + "\r\n";
    t += "Shadow Mesh: only in COL3 (San Andreas)\r\n";
    char b[160];
    std::snprintf(b, sizeof b, "\r\nBounding sphere: radius %.2f at %.2f, %.2f, %.2f\r\nBounding box: %.2f, %.2f, %.2f to %.2f, %.2f, %.2f\r\n", c.boundRadius,
                  c.boundCenter[0], c.boundCenter[1], c.boundCenter[2], c.boundMin[0], c.boundMin[1], c.boundMin[2], c.boundMax[0], c.boundMax[1], c.boundMax[2]);
    t += b;
    std::map<int, int> used;
    for (const ColBox& x : c.boxes) ++used[x.surface.material];
    for (const ColSphere& x : c.spheres) ++used[x.surface.material];
    for (const ColFace& x : c.faces) ++used[x.material];
    if (!used.empty()) {
        t += "\r\nSurface materials (number, name, grip: shapes):\r\n";
        for (const auto& kv : used)
            t += "  " + std::to_string(kv.first) + " " + colMaterialName(kv.first) + " (" + gripGroupName(colMaterialGrip(kv.first)) + " grip): " + std::to_string(kv.second) + "\r\n";
    }
    return t;
}

// A solid box (MEd draws them through 0x00404280 with the polygon mode filled).
void drawSolidBox(const float* mn, const float* mx) {
    const float x0 = mn[0], y0 = mn[1], z0 = mn[2], x1 = mx[0], y1 = mx[1], z1 = mx[2];
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);  glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glNormal3f(0, 0, -1); glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y0, z0);
    glNormal3f(1, 0, 0);  glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1); glVertex3f(x1, y0, z1);
    glNormal3f(-1, 0, 0); glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
    glNormal3f(0, 1, 0);  glVertex3f(x0, y1, z0); glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0);
    glNormal3f(0, -1, 0); glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1);
    glEnd();
}

void drawColEditOverlay(ModelViewer& v);

// The collision model as MEd draws it (0x004054e0, then the bounds 0x00404ff0).
void drawColModel(ModelViewer& v) {
    const ColModel& c = v.col;
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    const GLfloat ambient[4] = {0.4f, 0.4f, 0.4f, 1}, diffuse[4] = {0.7f, 0.7f, 0.7f, 1};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, diffuse);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    unsigned char rgb[3];
    if (v.showBoxes)
        for (const ColBox& x : c.boxes) {
            colMaterialColour(x.surface.material, rgb);
            glColor3ub(rgb[0], rgb[1], rgb[2]);
            drawSolidBox(x.min, x.max);
        }
    if (v.showSpheres) {
        static GLUquadric* q = nullptr;
        if (!q) q = gluNewQuadric();
        gluQuadricDrawStyle(q, GLU_FILL);
        gluQuadricNormals(q, GLU_SMOOTH);
        for (const ColSphere& x : c.spheres) {
            colMaterialColour(x.surface.material, rgb);
            glColor3ub(rgb[0], rgb[1], rgb[2]);
            glPushMatrix();
            glTranslatef(x.center[0], x.center[1], x.center[2]);
            gluSphere(q, x.radius, 5, 5);  // MEd: 5 slices, 5 stacks
            glPopMatrix();
        }
    }
    if (v.showMesh) {
        glBegin(GL_TRIANGLES);
        const size_t nv = c.vertices.size() / 3;
        for (const ColFace& f : c.faces) {
            if (f.a >= nv || f.b >= nv || f.c >= nv) continue;
            const float* a = &c.vertices[f.a * 3];
            const float* b = &c.vertices[f.b * 3];
            const float* d = &c.vertices[f.c * 3];
            const float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2], wx = d[0] - a[0], wy = d[1] - a[1], wz = d[2] - a[2];
            colMaterialColour(f.material, rgb);
            glColor3ub(rgb[0], rgb[1], rgb[2]);
            glNormal3f(uy * wz - uz * wy, uz * wx - ux * wz, ux * wy - uy * wx);
            glVertex3fv(a);
            glVertex3fv(b);
            glVertex3fv(d);
        }
        glEnd();
    }
    // the bounds: green Y, red X and blue Z axes (+-200, a tick every 10), the bounding circle, the red bounds box
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
    glPushMatrix();
    glTranslatef(c.boundCenter[0], c.boundCenter[1], c.boundCenter[2]);
    glBegin(GL_LINES);
    glColor3f(0, 1, 0);
    glVertex3f(0, -200, 0); glVertex3f(0, 200, 0);
    for (int k = -200; k < 200; k += 10) { glVertex3f(-2, float(k), 0); glVertex3f(2, float(k), 0); }
    glColor3f(1, 0, 0);
    glVertex3f(-200, 0, 0); glVertex3f(200, 0, 0);
    for (int k = -200; k < 200; k += 10) { glVertex3f(float(k), -2, 0); glVertex3f(float(k), 2, 0); }
    glColor3f(0, 0, 1);
    glVertex3f(0, 0, -200); glVertex3f(0, 0, 200);
    for (int k = -200; k < 200; k += 10) { glVertex3f(0, -2, float(k)); glVertex3f(0, 2, float(k)); }
    glEnd();
    glBegin(GL_LINES);
    for (int a = 0; a < 360; ++a) glVertex3f(float(std::cos(a * 0.0174532925) * c.boundRadius), float(std::sin(a * 0.0174532925) * c.boundRadius), 0);
    glEnd();
    glPopMatrix();
    glColor3f(1, 0, 0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    drawSolidBox(c.boundMin, c.boundMax);
    glPolygonMode(GL_FRONT_AND_BACK, v.wireframe ? GL_LINE : GL_FILL);
    if (v.editTab) drawColEditOverlay(v);
}

// ---- the collision editor: the Edit tab of Collision Object Properties (core: coledit.h; not in MEd, whose COL
// support was view-only) ------------------------------------------------------------------------------------------------
// Click a shape to select it (Shift adds or removes), double-click a mesh face for its whole flat side, Ctrl+drag a
// rectangle; drag a selected face, box side or sphere to push or pull it; Del deletes, Ctrl+Z / Ctrl+Y undo and redo.
// Buttons do the same with typed numbers. Save writes the record into the user's own COL file (a loose file: back
// into it), never into Rockstar's: an edited copy of a Rockstar model goes into <region>.col (level 0) or
// <region>_lvl<N>.col of the user's region, listed in gta3.dat after Rockstar's file of the same level (coledit.h).

bool ceQuiet = false;  // self-test: no message boxes; closing with unsaved edits discards them

const char* kCeLabels[5] = {"Push / pull the selection by:", "Move the selection by X, Y, Z:", "Surface material:", "", "Save into region:"};

void ceWarn(ModelViewer& v, const std::string& text) {
    v.lastMessage = text;
    if (!ceQuiet) MessageBoxA(v.wnd, text.c_str(), "Collision editor", MB_ICONWARNING);
}

bool ceHasSelection(const ModelViewer& v) { return !v.selBoxes.empty() || !v.selSpheres.empty() || !v.selFaces.empty(); }

void ceClearSelection(ModelViewer& v) {
    v.selBoxes.clear();
    v.selSpheres.clear();
    v.selFaces.clear();
    v.selBoxFace = -1;
}

void ceFixSelection(ModelViewer& v) {  // drop what no longer exists
    for (auto it = v.selBoxes.begin(); it != v.selBoxes.end();) it = *it >= int(v.col.boxes.size()) ? v.selBoxes.erase(it) : std::next(it);
    for (auto it = v.selSpheres.begin(); it != v.selSpheres.end();) it = *it >= int(v.col.spheres.size()) ? v.selSpheres.erase(it) : std::next(it);
    for (auto it = v.selFaces.begin(); it != v.selFaces.end();) it = *it >= v.col.faces.size() ? v.selFaces.erase(it) : std::next(it);
    if (v.selBoxes.size() != 1) v.selBoxFace = -1;
}

void cePushUndo(ModelViewer& v) {
    v.undo.push_back(v.col);
    if (v.undo.size() > 50) v.undo.erase(v.undo.begin());
    v.redo.clear();
}

std::string ceLowerSlashes(std::string s) {
    for (char& c : s) {
        if (c == char(92)) c = '/';
        c = char(std::tolower((unsigned char)c));
    }
    return s;
}

std::vector<std::string> userRegionNames();

// The user's region whose own COL file `rel` is (<region>.col or <region>_lvl<N>.col), else "".
std::string ceOwnRegionOf(const std::string& rel) {
    const std::string want = ceLowerSlashes(rel);
    for (const std::string& r : userRegionNames())
        for (int level = 0; level <= 9; ++level)
            if (ceLowerSlashes(colSaveTarget(r, level).relPath) == want) return r;
    return "";
}

// Where Save writes.
struct CeTarget {
    bool ok = false, loose = false, own = false;
    std::string rel, abs, datLine, region, why;
    int level = 0;
};

CeTarget ceTarget(const ModelViewer& v) {
    CeTarget t;
    if (!v.file.empty()) {  // a loose .col from disk: back into it
        t.ok = t.loose = true;
        t.rel = t.abs = v.file;
        return t;
    }
    const std::string own = ceOwnRegionOf(v.colSource);
    if (!own.empty()) {  // already the user's own file (an imported model, or a copy saved before)
        t.ok = t.own = true;
        t.region = own;
        t.rel = v.colSource;
        std::replace(t.rel.begin(), t.rel.end(), char(92), '/');
        t.abs = app.root + "/" + t.rel;
        return t;
    }
    bool ok = false;
    const std::string dat = slurp(app.root + "/data/gta3.dat", ok);
    const int level = ok ? colFileLevel(dat, v.colSource) : -1;
    if (level < 0) {
        t.why = v.colSource + " is not listed in data/gta3.dat, so the editor cannot tell which level a copy needs.";
        return t;
    }
    const int sel = v.ce.count(ID_CE_REGION) ? int(SendMessageA(v.ce.at(ID_CE_REGION), CB_GETCURSEL, 0, 0)) : -1;
    if (sel < 0) {
        t.why = "Rockstar's files are never changed: the edited copy goes into a COL file of a region of your own. Make one with New region... first.";
        return t;
    }
    char b[256] = "";
    SendMessageA(v.ce.at(ID_CE_REGION), CB_GETLBTEXT, WPARAM(sel), (LPARAM)b);
    const ColSaveTarget s = colSaveTarget(b, level);
    t.ok = true;
    t.region = b;
    t.level = level;
    t.rel = s.relPath;
    t.abs = app.root + "/" + s.relPath;
    t.datLine = s.datLine;
    return t;
}

std::string ceSelectionText(const ModelViewer& v) {
    std::vector<std::string> parts;
    auto plural = [](size_t n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); };
    if (!v.selFaces.empty()) parts.push_back(plural(v.selFaces.size(), "mesh face", "mesh faces"));
    if (!v.selBoxes.empty()) {
        static const char* sides[6] = {"-X", "+X", "-Y", "+Y", "-Z", "+Z"};
        std::string s = plural(v.selBoxes.size(), "box", "boxes");
        if (v.selBoxes.size() == 1) s += " (no. " + std::to_string(*v.selBoxes.begin() + 1) + (v.selBoxFace >= 0 ? std::string(", side ") + sides[v.selBoxFace] : std::string()) + ")";
        parts.push_back(s);
    }
    if (!v.selSpheres.empty()) parts.push_back(plural(v.selSpheres.size(), "sphere", "spheres"));
    if (parts.empty()) return "nothing (click a shape)";
    std::string t;
    for (size_t i = 0; i < parts.size(); ++i) t += (i ? ", " : "") + parts[i];
    return t;
}

const std::vector<std::array<uint32_t, 2>>& ceOpenEdges(ModelViewer& v) {
    if (v.openEdgesRev != v.rev) {
        v.openEdgesCache = openEdges(v.col);
        v.openEdgesRev = v.rev;
    }
    return v.openEdgesCache;
}

void ceSetText(HWND h, const std::string& s) {
    char b[512] = "";
    GetWindowTextA(h, b, sizeof b);
    if (s != b) SetWindowTextA(h, s.c_str());
}

// Everything the Edit tab shows: status, material, the selected box or sphere, which buttons work, the title.
void ceRefresh(ModelViewer& v) {
    if (v.kind != 1 || v.ce.empty()) return;
    ceFixSelection(v);
    const bool sel = ceHasSelection(v);
    const bool oneBox = v.selBoxes.size() == 1 && v.selSpheres.empty() && v.selFaces.empty();
    const bool oneSphere = v.selSpheres.size() == 1 && v.selBoxes.empty() && v.selFaces.empty();
    const bool canPush = !v.selFaces.empty() || (oneBox && v.selBoxFace >= 0) || oneSphere;
    EnableWindow(v.ce[ID_CE_DELETE], sel);
    EnableWindow(v.ce[ID_CE_SELNONE], sel);
    EnableWindow(v.ce[ID_CE_SELSIDE], !v.selFaces.empty());
    EnableWindow(v.ce[ID_CE_SELALL], !v.col.faces.empty());
    EnableWindow(v.ce[ID_CE_OUT], canPush);
    EnableWindow(v.ce[ID_CE_IN], canPush);
    EnableWindow(v.ce[ID_CE_MOVE], sel);
    EnableWindow(v.ce[ID_CE_UNDO], !v.undo.empty());
    EnableWindow(v.ce[ID_CE_REDO], !v.redo.empty());
    EnableWindow(v.ce[ID_CE_APPLY], oneBox || oneSphere);
    // the material of the selection (blank when mixed), else the one new shapes get
    std::set<int> mats;
    for (int i : v.selBoxes) mats.insert(v.col.boxes[size_t(i)].surface.material);
    for (int i : v.selSpheres) mats.insert(v.col.spheres[size_t(i)].surface.material);
    for (size_t i : v.selFaces) mats.insert(v.col.faces[i].material);
    const int mat = sel ? (mats.size() == 1 ? *mats.begin() : -1) : v.newMaterial;
    if (int(SendMessageA(v.ce[ID_CE_MAT], CB_GETCURSEL, 0, 0)) != mat) SendMessageA(v.ce[ID_CE_MAT], CB_SETCURSEL, WPARAM(mat), 0);
    // the selected box or sphere in numbers
    const std::string shapeLabel = oneBox ? "Selected box: Min X, Y, Z / Max X, Y, Z" : oneSphere ? "Selected sphere: Centre X, Y, Z / Radius" : "Select one box or sphere to type its size:";
    ceSetText(v.ceLabels[3], shapeLabel);
    float f[6] = {0, 0, 0, 0, 0, 0};
    int nf = 0;
    if (oneBox) {
        const ColBox& b = v.col.boxes[size_t(*v.selBoxes.begin())];
        for (int k = 0; k < 3; ++k) { f[k] = b.min[k]; f[3 + k] = b.max[k]; }
        nf = 6;
    } else if (oneSphere) {
        const ColSphere& s = v.col.spheres[size_t(*v.selSpheres.begin())];
        for (int k = 0; k < 3; ++k) f[k] = s.center[k];
        f[3] = s.radius;
        nf = 4;
    }
    const std::string shapeKey = std::to_string(nf) + ":" + std::to_string(v.rev) + ":" + ceSelectionText(v);
    if (shapeKey != v.shownShapeKey) {  // only when the shape or the selection changed, so typing is not overwritten
        v.shownShapeKey = shapeKey;
        for (int k = 0; k < 6; ++k) {
            HWND h = v.ce[ID_CE_F0 + k];
            EnableWindow(h, k < nf);
            SetWindowTextA(h, k < nf ? num(f[k], 3).c_str() : "");
        }
    }
    // status
    const CeTarget t = ceTarget(v);
    std::string s = "Selected: " + ceSelectionText(v) + "\r\n";
    auto count = [](size_t n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); };
    s += "Shapes: " + count(v.col.boxes.size(), "box", "boxes") + ", " + count(v.col.spheres.size(), "sphere", "spheres") + ", " +
         count(v.col.faces.size(), "mesh face", "mesh faces") + "\r\n";
    const size_t open = ceOpenEdges(v).size();
    s += open ? "Open edges (red): " + std::to_string(open) + " - the mesh has holes or borders there\r\n" : std::string(v.col.faces.empty() ? "" : "No open edges: the mesh is closed\r\n");
    if (v.col.boxes.empty() && v.col.spheres.empty() && v.col.faces.empty()) s += "Nothing is solid yet: Add Box gives a box around the model.\r\n";
    if (v.dragging) s += "Push / pull: " + num(v.dragD, 2) + "\r\n";
    s += v.dirty ? "Unsaved changes.\r\n" : "No unsaved changes.\r\n";
    if (t.ok) {
        if (t.loose) s += "Save writes back into " + t.rel + " (a backup is kept once).";
        else if (t.own) s += "Save writes into your own " + t.rel + ".";
        else
            s += "Save writes a copy into " + t.rel + " (" + t.datLine.substr(0, 9) + ", loaded after Rockstar's file, so the game uses it on that island). Rockstar's " +
                 v.colSource + " is not changed. Every placement of " + v.col.name + " gets the new collision.";
    } else {
        s += t.why;
    }
    ceSetText(v.ce[ID_CE_STATUS], s);
    EnableWindow(v.ce[ID_CE_SAVE], t.ok && v.dirty);
    const bool fixedRegion = !v.file.empty() || !ceOwnRegionOf(v.colSource).empty();
    EnableWindow(v.ce[ID_CE_REGION], !fixedRegion);
    EnableWindow(v.ce[ID_CE_NEWREGION], !fixedRegion);
    // the window title says when something is unsaved; the View tab's table follows the edits
    char title[512] = "";
    GetWindowTextA(v.wnd, title, sizeof title);
    std::string tt = title;
    if (tt.size() > 2 && tt.substr(tt.size() - 2) == " *") tt.resize(tt.size() - 2);
    if (v.dirty) tt += " *";
    ceSetText(v.wnd, tt);
    const std::string info = colInfoText(v);
    if (info != v.shownInfo) {
        v.shownInfo = info;
        SetWindowTextA(v.info, info.c_str());
    }
}

// After any change to v.col: bounds, dirty (compared with what was loaded or last saved), redraw caches.
// `bounds`: an edit sets the bounds from the shapes; undo and redo give back a record exactly as it was (Rockstar's
// own bounds are not the tight box, so recomputing them would change an untouched record).
void ceChanged(ModelViewer& v, bool bounds = true) {
    if (bounds) computeColBounds(v.col);
    ++v.rev;
    v.dirty = writeColl(v.col) != writeColl(v.loaded);
    ceRefresh(v);
}

void ceUndo(ModelViewer& v) {
    if (v.undo.empty()) return;
    v.redo.push_back(v.col);
    v.col = v.undo.back();
    v.undo.pop_back();
    ceClearSelection(v);
    ceChanged(v, false);
}

void ceRedo(ModelViewer& v) {
    if (v.redo.empty()) return;
    v.undo.push_back(v.col);
    v.col = v.redo.back();
    v.redo.pop_back();
    ceClearSelection(v);
    ceChanged(v, false);
}

// Push or pull the selection by d on v.col (no undo step; callers take it).
bool ceApplyPushPull(ModelViewer& v, float d, std::string& why) {
    if (!v.selFaces.empty()) return pushPullFaces(v.col, v.selFaces, d, why);
    const bool oneBox = v.selBoxes.size() == 1 && v.selSpheres.empty();
    const bool oneSphere = v.selSpheres.size() == 1 && v.selBoxes.empty();
    if (oneBox && v.selBoxFace >= 0) {
        if (pushPullBoxFace(v.col.boxes[size_t(*v.selBoxes.begin())], v.selBoxFace, d)) return true;
        why = "The box would get thinner than " + num(kMinBoxSize, 2) + ".";
        return false;
    }
    if (oneSphere) {
        ColSphere& s = v.col.spheres[size_t(*v.selSpheres.begin())];
        if (!(s.radius + d >= kMinSphereRadius - 1e-6f)) {
            why = "The sphere would get smaller than radius " + num(kMinSphereRadius, 2) + ".";
            return false;
        }
        s.radius += d;
        return true;
    }
    why = oneBox ? "Click the side of the box you want to move first." : "Select mesh faces, one side of a box or one sphere to push or pull.";
    return false;
}

bool cePushPull(ModelViewer& v, float d) {
    std::string why;
    const ColModel before = v.col;
    if (!ceApplyPushPull(v, d, why)) {
        v.col = before;
        ceWarn(v, why);
        return false;
    }
    ColModel after = v.col;
    v.col = before;
    cePushUndo(v);
    v.col = std::move(after);
    ceChanged(v);
    return true;
}

bool ceMove(ModelViewer& v, float dx, float dy, float dz) {
    if (!ceHasSelection(v)) return false;
    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz)) return false;
    if (dx == 0 && dy == 0 && dz == 0) return false;
    cePushUndo(v);
    const float d[3] = {dx, dy, dz};
    for (int i : v.selBoxes)
        for (int k = 0; k < 3; ++k) { v.col.boxes[size_t(i)].min[k] += d[k]; v.col.boxes[size_t(i)].max[k] += d[k]; }
    for (int i : v.selSpheres)
        for (int k = 0; k < 3; ++k) v.col.spheres[size_t(i)].center[k] += d[k];
    std::set<uint32_t> verts;
    for (size_t i : v.selFaces) for (uint32_t x : {v.col.faces[i].a, v.col.faces[i].b, v.col.faces[i].c}) verts.insert(x);
    for (uint32_t x : verts)
        for (int k = 0; k < 3; ++k) v.col.vertices[size_t(x) * 3 + size_t(k)] += d[k];
    ceChanged(v);
    return true;
}

bool ceDelete(ModelViewer& v) {
    if (!ceHasSelection(v)) return false;
    cePushUndo(v);
    for (auto it = v.selBoxes.rbegin(); it != v.selBoxes.rend(); ++it) v.col.boxes.erase(v.col.boxes.begin() + *it);
    for (auto it = v.selSpheres.rbegin(); it != v.selSpheres.rend(); ++it) v.col.spheres.erase(v.col.spheres.begin() + *it);
    deleteFaces(v.col, v.selFaces);
    ceClearSelection(v);
    ceChanged(v);
    return true;
}

void ceSetMaterial(ModelViewer& v, int mat) {
    if (mat < 0 || mat > 255) return;
    if (!ceHasSelection(v)) {  // nothing selected: the material new boxes and spheres get
        v.newMaterial = mat;
        ceRefresh(v);
        return;
    }
    bool change = false;
    for (int i : v.selBoxes) change = change || v.col.boxes[size_t(i)].surface.material != mat;
    for (int i : v.selSpheres) change = change || v.col.spheres[size_t(i)].surface.material != mat;
    for (size_t i : v.selFaces) change = change || v.col.faces[i].material != mat;
    if (!change) return;
    cePushUndo(v);
    for (int i : v.selBoxes) v.col.boxes[size_t(i)].surface.material = uint8_t(mat);
    for (int i : v.selSpheres) v.col.spheres[size_t(i)].surface.material = uint8_t(mat);
    for (size_t i : v.selFaces) v.col.faces[i].material = uint8_t(mat);
    ceChanged(v);
}

void ceAddBox(ModelViewer& v) {
    cePushUndo(v);
    ColBox b = newBoxFor(v.col);
    b.surface.material = uint8_t(v.newMaterial);
    v.col.boxes.push_back(b);
    ceClearSelection(v);
    v.selBoxes.insert(int(v.col.boxes.size()) - 1);
    v.selBoxFace = 5;  // the top: push / pull makes it taller or lower
    ceChanged(v);
}

void ceAddSphere(ModelViewer& v) {
    cePushUndo(v);
    ColSphere s = newSphereFor(v.col);
    s.surface.material = uint8_t(v.newMaterial);
    v.col.spheres.push_back(s);
    ceClearSelection(v);
    v.selSpheres.insert(int(v.col.spheres.size()) - 1);
    ceChanged(v);
}

// The typed numbers of the one selected box (min, max) or sphere (centre, radius).
bool ceApplyFields(ModelViewer& v) {
    const bool oneBox = v.selBoxes.size() == 1 && v.selSpheres.empty() && v.selFaces.empty();
    const bool oneSphere = v.selSpheres.size() == 1 && v.selBoxes.empty() && v.selFaces.empty();
    if (!oneBox && !oneSphere) return false;
    float f[6];
    const int n = oneBox ? 6 : 4;
    for (int k = 0; k < n; ++k) {
        char b[64] = "";
        GetWindowTextA(v.ce[ID_CE_F0 + k], b, sizeof b);
        char* end = nullptr;
        const double x = std::strtod(b, &end);
        if (!b[0] || !end || *end || !std::isfinite(x) || std::fabs(x) > 1e5) {
            ceWarn(v, std::string("'") + b + "' is not a number the editor can use.");
            v.shownShapeKey.clear();
            ceRefresh(v);
            return false;
        }
        f[k] = float(x);
    }
    if (oneBox)
        for (int k = 0; k < 3; ++k)
            if (f[3 + k] - f[k] < kMinBoxSize - 1e-6f) {
                ceWarn(v, "Each Max must be at least " + num(kMinBoxSize, 2) + " more than its Min.");
                return false;
            }
    if (oneSphere && f[3] < kMinSphereRadius - 1e-6f) {
        ceWarn(v, "The radius must be at least " + num(kMinSphereRadius, 2) + ".");
        return false;
    }
    ColModel after = v.col;
    if (oneBox) {
        ColBox& b = after.boxes[size_t(*v.selBoxes.begin())];
        for (int k = 0; k < 3; ++k) { b.min[k] = f[k]; b.max[k] = f[3 + k]; }
    } else {
        ColSphere& s = after.spheres[size_t(*v.selSpheres.begin())];
        for (int k = 0; k < 3; ++k) s.center[k] = f[k];
        s.radius = f[3];
    }
    if (writeColl(after) == writeColl(v.col)) return false;  // nothing changed
    cePushUndo(v);
    v.col = after;
    ceChanged(v);
    return true;
}

// Save: the record into its target file (backup once), gta3.dat lists the file (backup once).
bool ceSave(ModelViewer& v) {
    namespace fs = std::filesystem;
    const CeTarget t = ceTarget(v);
    if (!t.ok) {
        ceWarn(v, t.why);
        return false;
    }
    std::error_code ec;
    bool existed = fs::exists(t.abs, ec), ok = false;
    std::vector<unsigned char> bytes;
    if (existed) {
        const std::string s = slurp(t.abs, ok);
        if (!ok) {
            ceWarn(v, "Could not read " + t.rel + ".");
            return false;
        }
        bytes.assign(s.begin(), s.end());
    }
    std::string err;
    if (!putColRecord(bytes, v.col, err)) {
        ceWarn(v, "Could not update " + t.rel + ": " + err);
        return false;
    }
    if (existed && !fs::exists(t.abs + ".med-backup", ec)) fs::copy_file(t.abs, t.abs + ".med-backup", ec);
    fs::create_directories(fs::path(t.abs).parent_path(), ec);
    {
        std::ofstream out(t.abs, std::ios::binary | std::ios::trunc);
        out.write((const char*)bytes.data(), std::streamsize(bytes.size()));
        if (!out) {
            ceWarn(v, "Could not write " + t.rel + " (is it read-only?).");
            return false;
        }
    }
    if (!t.datLine.empty()) {  // gta3.dat must list the file, after Rockstar's of the same level
        const std::string datPath = app.root + "/data/gta3.dat";
        std::string dat = slurp(datPath, ok);
        if (ok && addDatLine(dat, "COLFILE", t.datLine)) {
            if (!fs::exists(datPath + ".med-backup", ec)) fs::copy_file(datPath, datPath + ".med-backup", ec);
            std::ofstream out(datPath, std::ios::binary | std::ios::trunc);
            out.write(dat.data(), std::streamsize(dat.size()));
            if (!out) {
                ceWarn(v, "The collision was saved into " + t.rel + ", but data/gta3.dat could not be written, so the game will not load it. Add this line after the COLFILE lines:\n" + t.datLine);
                return false;
            }
        }
    }
    if (t.loose) {
        for (ColModel& c : v.colModels)
            if (toLower(c.name) == toLower(v.col.name)) c = v.col;
    } else {
        app.cols[toLower(v.col.name)] = v.col;  // the map's selection bounds follow
        bool known = false;
        for (const std::string& c : app.colFiles) known = known || ceLowerSlashes(c) == ceLowerSlashes(t.abs);
        if (!known) app.colFiles.push_back(t.abs);  // gta3.dat lists it after the others, as here
        v.colSource = t.rel;
    }
    v.loaded = v.col;
    v.dirty = false;
    v.lastMessage.clear();
    ceRefresh(v);
    return true;
}

// Close asks about unsaved edits. False = stay open.
bool ceConfirmClose(ModelViewer& v) {
    if (v.kind != 1 || !v.dirty || ceQuiet) return true;
    const int a = MessageBoxA(v.wnd, ("Save the changes to the collision of " + v.col.name + "?").c_str(), "Collision editor", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (a == IDCANCEL) return false;
    if (a == IDYES) return ceSave(v);
    return true;
}

// ---- the mouse in the 3D view ----

bool ceRay(const ModelViewer& v, int x, int y, float o[3], float d[3]) {
    double a[3], b[3];
    const double wy = double(v.vp[3]) - y;
    if (gluUnProject(x, wy, 0, v.mv, v.proj, v.vp, &a[0], &a[1], &a[2]) != GL_TRUE) return false;
    if (gluUnProject(x, wy, 1, v.mv, v.proj, v.vp, &b[0], &b[1], &b[2]) != GL_TRUE) return false;
    for (int k = 0; k < 3; ++k) { o[k] = float(a[k]); d[k] = float(b[k] - a[k]); }
    return true;
}

ColHit cePick(const ModelViewer& v, int x, int y, float o[3], float d[3]) {
    if (!ceRay(v, x, y, o, d)) return ColHit();
    return pickCol(v.col, o, d, v.showBoxes, v.showSpheres, v.showMesh);
}

// A click: select what is under the mouse (Shift: add or remove it); `side` = the whole flat side of a mesh face.
void ceClick(ModelViewer& v, int x, int y, bool add, bool side) {
    float o[3], d[3];
    const ColHit h = cePick(v, x, y, o, d);
    if (!add) ceClearSelection(v);
    if (h.kind == ColHit::Face) {
        const std::set<size_t> faces = side ? coplanarNeighbours(v.col, size_t(h.index)) : std::set<size_t>{size_t(h.index)};
        const bool all = std::all_of(faces.begin(), faces.end(), [&](size_t f) { return v.selFaces.count(f) > 0; });
        for (size_t f : faces) {
            if (add && all) v.selFaces.erase(f);
            else v.selFaces.insert(f);
        }
    } else if (h.kind == ColHit::Box) {
        if (add && v.selBoxes.count(h.index)) v.selBoxes.erase(h.index);
        else {
            v.selBoxes.insert(h.index);
            v.selBoxFace = h.boxFace;
        }
    } else if (h.kind == ColHit::Sphere) {
        if (add && v.selSpheres.count(h.index)) v.selSpheres.erase(h.index);
        else v.selSpheres.insert(h.index);
    }
    ceRefresh(v);
}

// Ctrl+drag: everything whose points all fall inside the rectangle (boxes: all corners, spheres: the centre).
void ceRectSelect(ModelViewer& v, RECT r, bool add) {
    if (r.left > r.right) std::swap(r.left, r.right);
    if (r.top > r.bottom) std::swap(r.top, r.bottom);
    if (!add) ceClearSelection(v);
    auto inside = [&](double x, double y, double z) {
        double sx, sy, sz;
        if (gluProject(x, y, z, v.mv, v.proj, v.vp, &sx, &sy, &sz) != GL_TRUE || sz < 0 || sz > 1) return false;
        sy = v.vp[3] - sy;
        return sx >= r.left && sx <= r.right && sy >= r.top && sy <= r.bottom;
    };
    if (v.showMesh)
        for (size_t i = 0; i < v.col.faces.size(); ++i) {
            const ColFace& f = v.col.faces[i];
            bool in = true;
            for (uint32_t k : {f.a, f.b, f.c})
                if (k * 3 + 2 >= v.col.vertices.size() || !inside(v.col.vertices[k * 3], v.col.vertices[k * 3 + 1], v.col.vertices[k * 3 + 2])) in = false;
            if (in) v.selFaces.insert(i);
        }
    if (v.showBoxes)
        for (size_t i = 0; i < v.col.boxes.size(); ++i) {
            const ColBox& b = v.col.boxes[i];
            bool in = true;
            for (int c = 0; c < 8; ++c)
                if (!inside(c & 1 ? b.max[0] : b.min[0], c & 2 ? b.max[1] : b.min[1], c & 4 ? b.max[2] : b.min[2])) in = false;
            if (in) v.selBoxes.insert(int(i));
        }
    if (v.showSpheres)
        for (size_t i = 0; i < v.col.spheres.size(); ++i)
            if (inside(v.col.spheres[i].center[0], v.col.spheres[i].center[1], v.col.spheres[i].center[2])) v.selSpheres.insert(int(i));
    if (v.selBoxes.size() != 1) v.selBoxFace = -1;
    ceRefresh(v);
}

// Button down on a selected shape: get ready to push or pull it (the direction and the point it is held at).
bool ceStartDrag(ModelViewer& v, int x, int y) {
    float o[3], d[3];
    const ColHit h = cePick(v, x, y, o, d);
    if (h.kind == ColHit::None) return false;
    const bool onSelected = (h.kind == ColHit::Face && v.selFaces.count(size_t(h.index))) || (h.kind == ColHit::Box && v.selBoxes.count(h.index) && v.selBoxes.size() == 1) ||
                            (h.kind == ColHit::Sphere && v.selSpheres.count(h.index) && v.selSpheres.size() == 1);
    if (!onSelected) return false;
    for (int k = 0; k < 3; ++k) v.dragP[k] = o[k] + double(d[k]) * h.t;
    if (h.kind == ColHit::Face) {
        double s[3] = {0, 0, 0};
        for (size_t i : v.selFaces) {
            float n[3];
            if (!faceNormal(v.col, i, n)) continue;
            const ColFace& f = v.col.faces[i];
            const float *a = &v.col.vertices[f.a * 3], *b = &v.col.vertices[f.b * 3], *c = &v.col.vertices[f.c * 3];
            const double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2], wx = c[0] - a[0], wy = c[1] - a[1], wz = c[2] - a[2];
            const double area = std::sqrt((uy * wz - uz * wy) * (uy * wz - uz * wy) + (uz * wx - ux * wz) * (uz * wx - ux * wz) + (ux * wy - uy * wx) * (ux * wy - uy * wx));
            for (int k = 0; k < 3; ++k) s[k] += n[k] * area;
        }
        const double len = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
        if (len < 1e-9) return false;
        for (int k = 0; k < 3; ++k) v.dragN[k] = s[k] / len;
    } else if (h.kind == ColHit::Box) {
        if (v.selSpheres.size() || v.selFaces.size()) return false;
        v.selBoxFace = h.boxFace;
        float n[3];
        boxFaceNormal(h.boxFace, n);
        for (int k = 0; k < 3; ++k) v.dragN[k] = n[k];
    } else {
        if (v.selBoxes.size() || v.selFaces.size()) return false;
        const ColSphere& s = v.col.spheres[size_t(h.index)];
        double n[3], len = 0;
        for (int k = 0; k < 3; ++k) { n[k] = v.dragP[k] - s.center[k]; len += n[k] * n[k]; }
        len = std::sqrt(len);
        if (len < 1e-9) return false;
        for (int k = 0; k < 3; ++k) v.dragN[k] = n[k] / len;
    }
    v.dragStart = v.col;
    v.dragD = 0;
    return true;
}

// While dragging: the distance along the push direction to the point of that line nearest the mouse ray, in
// hundredths (Shift: tenths).
void ceDragTo(ModelViewer& v, int x, int y, bool coarse) {
    float o[3], d[3];
    if (!ceRay(v, x, y, o, d)) return;
    const double* n = v.dragN;
    const double w0[3] = {v.dragP[0] - o[0], v.dragP[1] - o[1], v.dragP[2] - o[2]};
    const double b = n[0] * d[0] + n[1] * d[1] + n[2] * d[2], c = double(d[0]) * d[0] + double(d[1]) * d[1] + double(d[2]) * d[2];
    const double dn = n[0] * w0[0] + n[1] * w0[1] + n[2] * w0[2], e = d[0] * w0[0] + d[1] * w0[1] + d[2] * w0[2];
    const double denom = c - b * b;  // n is a unit vector
    if (std::fabs(denom) < 1e-9 * c) return;  // looking straight along the push direction
    double s = (b * e - c * dn) / denom;
    const double step = coarse ? 0.1 : 0.01;
    s = std::round(s / step) * step;
    if (!std::isfinite(s) || std::fabs(s) > 1e4) return;
    if (float(s) == v.dragD) return;
    ColModel keep = v.col;
    v.col = v.dragStart;
    std::string why;
    if (s != 0 && !ceApplyPushPull(v, float(s), why)) {
        v.col = keep;  // past what is allowed: stay at the last good distance
        return;
    }
    v.dragD = float(s);
    computeColBounds(v.col);
    ++v.rev;
    ceRefresh(v);
}

// ---- drawing the Edit tab's extras (inside drawColModel, in the model's coordinates) ----

void drawGlModelWire(const GlModel& m) {
    for (const GlGeometry& g : m.geoms) {
        glPushMatrix();
        glMultMatrixf(g.matrix);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, g.pos.data());
        for (const GlBatch& b : g.batches) glDrawElements(GL_TRIANGLES, b.count, GL_UNSIGNED_INT, g.idx.data() + b.first);
        glDisableClientState(GL_VERTEX_ARRAY);
        glPopMatrix();
    }
}

GlModel* viewerGl(ModelViewer& v);

void drawColEditOverlay(ModelViewer& v) {
    const ColModel& c = v.col;
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // the model itself as a grey outline, to build the collision against
    if (v.showRef && v.id >= 0) {
        if (GlModel* gm = viewerGl(v)) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDisable(GL_DEPTH_TEST);  // seen through the collision shapes, faintly
            glColor4f(0.8f, 0.8f, 0.8f, 0.3f);
            drawGlModelWire(*gm);
            glEnable(GL_DEPTH_TEST);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
    // the selection in yellow
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1, -1);
    glColor4f(1, 0.9f, 0, 0.55f);
    glBegin(GL_TRIANGLES);
    const size_t nv = c.vertices.size() / 3;
    for (size_t i : v.selFaces) {
        if (i >= c.faces.size()) continue;
        const ColFace& f = c.faces[i];
        if (f.a >= nv || f.b >= nv || f.c >= nv) continue;
        glVertex3fv(&c.vertices[f.a * 3]);
        glVertex3fv(&c.vertices[f.b * 3]);
        glVertex3fv(&c.vertices[f.c * 3]);
    }
    glEnd();
    if (v.selBoxes.size() == 1 && v.selBoxFace >= 0 && *v.selBoxes.begin() < int(c.boxes.size())) {  // the side push / pull moves
        const ColBox& b = c.boxes[size_t(*v.selBoxes.begin())];
        float mn[3] = {b.min[0], b.min[1], b.min[2]}, mx[3] = {b.max[0], b.max[1], b.max[2]};
        const int axis = v.selBoxFace / 2;
        if (v.selBoxFace % 2) mn[axis] = mx[axis];
        else mx[axis] = mn[axis];
        drawSolidBox(mn, mx);
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glLineWidth(2);
    glColor4f(1, 0.9f, 0, 1);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    for (int i : v.selBoxes)
        if (i < int(c.boxes.size())) drawSolidBox(c.boxes[size_t(i)].min, c.boxes[size_t(i)].max);
    static GLUquadric* q = nullptr;
    if (!q) q = gluNewQuadric();
    gluQuadricDrawStyle(q, GLU_LINE);
    for (int i : v.selSpheres)
        if (i < int(c.spheres.size())) {
            glPushMatrix();
            glTranslatef(c.spheres[size_t(i)].center[0], c.spheres[size_t(i)].center[1], c.spheres[size_t(i)].center[2]);
            gluSphere(q, c.spheres[size_t(i)].radius * 1.01, 12, 8);
            glPopMatrix();
        }
    gluQuadricDrawStyle(q, GLU_FILL);
    glPolygonMode(GL_FRONT_AND_BACK, v.wireframe ? GL_LINE : GL_FILL);
    // open edges in red, on top
    glDisable(GL_DEPTH_TEST);
    glColor4f(1, 0.1f, 0.1f, 1);
    glBegin(GL_LINES);
    for (const auto& e : ceOpenEdges(v)) {
        if (e[0] >= nv || e[1] >= nv) continue;
        glVertex3fv(&c.vertices[e[0] * 3]);
        glVertex3fv(&c.vertices[e[1] * 3]);
    }
    glEnd();
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1);
    // the selection rectangle
    if (v.dragging && v.dragMode == 3) {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, v.vp[2], v.vp[3], 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        glColor4f(1, 1, 1, 0.9f);
        glBegin(GL_LINE_LOOP);
        glVertex2i(v.down.x, v.down.y);
        glVertex2i(v.cur.x, v.down.y);
        glVertex2i(v.cur.x, v.cur.y);
        glVertex2i(v.down.x, v.cur.y);
        glEnd();
        glEnable(GL_DEPTH_TEST);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
    }
}

// The mouse in the 3D view while the Edit tab is open. True = handled.
bool ceMouse(ModelViewer& v, HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    const int x = int(short(LOWORD(lp))), y = int(short(HIWORD(lp)));
    switch (msg) {
        case WM_LBUTTONDOWN:
            SetFocus(w);
            SetCapture(w);
            v.last = v.down = v.cur = {x, y};
            v.dragging = true;
            v.dragMoved = false;
            v.dragMode = (wp & MK_CONTROL) ? 3 : ceStartDrag(v, x, y) ? 2 : 1;
            return true;
        case WM_MOUSEMOVE: {
            if (!v.dragging || !(wp & MK_LBUTTON)) return false;  // the right button turns the view as in View
            v.cur = {x, y};
            if (std::abs(x - v.down.x) > 3 || std::abs(y - v.down.y) > 3) {
                if (!v.dragMoved && v.dragMode == 2) {
                    std::swap(v.col, v.dragStart);  // the undo step holds the shape before the drag
                    cePushUndo(v);
                    std::swap(v.col, v.dragStart);
                }
                v.dragMoved = true;
            }
            if (v.dragMode == 1 && v.dragMoved) {  // pan, as in View
                v.view.tx += float((x - v.last.x) * 0.25);
                v.view.ty -= float((y - v.last.y) * 0.25);
            } else if (v.dragMode == 2 && v.dragMoved) {
                ceDragTo(v, x, y, (wp & MK_SHIFT) != 0);
            }
            v.last = {x, y};
            return true;
        }
        case WM_LBUTTONUP: {
            if (!v.dragging) return false;
            ReleaseCapture();
            const int mode = v.dragMode;
            const bool moved = v.dragMoved;
            v.dragging = false;
            v.dragMode = 0;
            if (!moved) ceClick(v, x, y, (wp & MK_SHIFT) != 0, false);
            else if (mode == 3) ceRectSelect(v, RECT{v.down.x, v.down.y, x, y}, (wp & MK_SHIFT) != 0);
            else if (mode == 2) {
                if (v.dragD == 0 && !v.undo.empty()) {  // let go where it started: no change, no undo step
                    v.col = v.undo.back();
                    v.undo.pop_back();
                    ceChanged(v, false);
                } else {
                    ceChanged(v);
                }
            }
            return true;
        }
        case WM_LBUTTONDBLCLK:
            ceClick(v, x, y, (wp & MK_SHIFT) != 0, true);  // the whole flat side
            return true;
    }
    return false;
}

// Keys in the 3D view while the Edit tab is open. True = handled.
bool ceKey(ModelViewer& v, WPARAM key) {
    const bool ctrl = GetKeyState(VK_CONTROL) < 0;
    if (key == VK_DELETE) { ceDelete(v); return true; }
    if (ctrl && key == 'Z') { ceUndo(v); return true; }
    if (ctrl && key == 'Y') { ceRedo(v); return true; }
    if (ctrl && key == 'S') { ceSave(v); return true; }
    if (key == VK_ESCAPE && ceHasSelection(v)) { ceClearSelection(v); ceRefresh(v); return true; }
    return false;
}

// ---- the Edit tab's controls ----

void ceFillRegions(ModelViewer& v, const std::string& select = "") {
    HWND box = v.ce[ID_CE_REGION];
    char cur[256] = "";
    const int was = int(SendMessageA(box, CB_GETCURSEL, 0, 0));
    if (was >= 0) SendMessageA(box, CB_GETLBTEXT, WPARAM(was), (LPARAM)cur);
    const std::string want = select.empty() ? std::string(cur) : select;
    SendMessageA(box, CB_RESETCONTENT, 0, 0);
    int pick = 0, k = 0;
    const std::string own = ceOwnRegionOf(v.colSource);
    for (const std::string& r : userRegionNames()) {
        SendMessageA(box, CB_ADDSTRING, 0, (LPARAM)r.c_str());
        if (toLower(r) == toLower(own.empty() ? want : own)) pick = k;
        ++k;
    }
    SendMessageA(box, CB_SETCURSEL, k ? WPARAM(pick) : WPARAM(-1), 0);
}

void ceShowTab(ModelViewer& v, bool edit) {
    v.editTab = edit;
    for (HWND h : v.viewCtrls) ShowWindow(h, edit ? SW_HIDE : SW_SHOW);
    for (auto& kv : v.ce) ShowWindow(kv.second, edit ? SW_SHOW : SW_HIDE);
    for (HWND h : v.ceLabels) ShowWindow(h, edit ? SW_SHOW : SW_HIDE);
    if (v.tabs && int(SendMessageA(v.tabs, TCM_GETCURSEL, 0, 0)) != (edit ? 1 : 0)) SendMessageA(v.tabs, TCM_SETCURSEL, edit ? 1 : 0, 0);
    if (edit) {
        ceFillRegions(v);
        ceRefresh(v);
    }
}

void ceCreateControls(ModelViewer& v, HFONT font) {
    auto make = [&](const char* cls, const char* text, DWORD style, DWORD ex, int cid) {
        HWND h = CreateWindowExA(ex, cls, text, WS_CHILD | style, 0, 0, 10, 10, v.wnd, (HMENU)(INT_PTR)cid, GetModuleHandle(nullptr), nullptr);
        SendMessageA(h, WM_SETFONT, (WPARAM)font, TRUE);
        if (cid != ID_CE_LBL) v.ce[cid] = h;
        return h;
    };
    v.tabs = CreateWindowExA(0, WC_TABCONTROLA, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 10, 10, v.wnd, (HMENU)(INT_PTR)ID_CE_TABS, GetModuleHandle(nullptr), nullptr);
    SendMessageA(v.tabs, WM_SETFONT, (WPARAM)font, TRUE);
    TCITEMA ti = {};
    ti.mask = TCIF_TEXT;
    ti.pszText = const_cast<char*>("View");
    SendMessageA(v.tabs, TCM_INSERTITEMA, 0, (LPARAM)&ti);
    ti.pszText = const_cast<char*>("Edit");
    SendMessageA(v.tabs, TCM_INSERTITEMA, 1, (LPARAM)&ti);
    make("BUTTON", "Add Box", BS_PUSHBUTTON, 0, ID_CE_ADDBOX);
    make("BUTTON", "Add Sphere", BS_PUSHBUTTON, 0, ID_CE_ADDSPHERE);
    make("BUTTON", "Select flat side", BS_PUSHBUTTON, 0, ID_CE_SELSIDE);
    make("BUTTON", "Select all faces", BS_PUSHBUTTON, 0, ID_CE_SELALL);
    make("BUTTON", "Clear selection", BS_PUSHBUTTON, 0, ID_CE_SELNONE);
    make("BUTTON", "Delete selected", BS_PUSHBUTTON, 0, ID_CE_DELETE);
    for (const char* t : kCeLabels) v.ceLabels.push_back(make("STATIC", t, SS_LEFT, 0, ID_CE_LBL));
    make("EDIT", "0.5", ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, ID_CE_DIST);
    make("BUTTON", "Pull out", BS_PUSHBUTTON, 0, ID_CE_OUT);
    make("BUTTON", "Push in", BS_PUSHBUTTON, 0, ID_CE_IN);
    for (int id : {ID_CE_MX, ID_CE_MY, ID_CE_MZ}) make("EDIT", "0", ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, id);
    make("BUTTON", "Move", BS_PUSHBUTTON, 0, ID_CE_MOVE);
    HWND mat = make("COMBOBOX", "", CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL, 0, ID_CE_MAT);
    for (int k = 0; k <= kColMaterialCount - 1; ++k) SendMessageA(mat, CB_ADDSTRING, 0, (LPARAM)(std::to_string(k) + " " + colMaterialName(k)).c_str());
    for (int k = 0; k < 6; ++k) make("EDIT", "", ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, ID_CE_F0 + k);
    make("BUTTON", "Apply typed size", BS_PUSHBUTTON, 0, ID_CE_APPLY);
    HWND ref = make("BUTTON", "Show the model's outline", BS_AUTOCHECKBOX, 0, ID_CE_SHOWREF);
    SendMessageA(ref, BM_SETCHECK, BST_CHECKED, 0);
    make("BUTTON", "Undo", BS_PUSHBUTTON, 0, ID_CE_UNDO);
    make("BUTTON", "Redo", BS_PUSHBUTTON, 0, ID_CE_REDO);
    make("EDIT", "", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, WS_EX_CLIENTEDGE, ID_CE_STATUS);
    make("COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 0, ID_CE_REGION);
    make("BUTTON", "New region...", BS_PUSHBUTTON, 0, ID_CE_NEWREGION);
    make("BUTTON", "Save", BS_PUSHBUTTON, 0, ID_CE_SAVE);
    if (v.id < 0) EnableWindow(v.ce[ID_CE_SHOWREF], FALSE);  // a loose .col has no model to show
}

// Lay the Edit tab out in the right-hand panel from `y` down.
void ceLayout(ModelViewer& v, int px, int pw, int y, int ch) {
    auto put = [&](int id, int x, int yy, int w, int h) { MoveWindow(v.ce[id], x, yy, w, h, TRUE); };
    const int half = (pw - 4) / 2, third = (pw - 8) / 3;
    put(ID_CE_ADDBOX, px, y, half, 24);
    put(ID_CE_ADDSPHERE, px + half + 4, y, half, 24);
    y += 28;
    put(ID_CE_SELSIDE, px, y, half, 24);
    put(ID_CE_SELALL, px + half + 4, y, half, 24);
    y += 28;
    put(ID_CE_SELNONE, px, y, half, 24);
    put(ID_CE_DELETE, px + half + 4, y, half, 24);
    y += 32;
    MoveWindow(v.ceLabels[0], px, y, pw, 16, TRUE);
    y += 18;
    const int bw = (pw - 64 - 8) / 2;
    put(ID_CE_DIST, px, y, 60, 22);
    put(ID_CE_OUT, px + 64, y, bw, 22);
    put(ID_CE_IN, px + 68 + bw, y, bw, 22);
    y += 28;
    MoveWindow(v.ceLabels[1], px, y, pw, 16, TRUE);
    y += 18;
    const int ew = (pw - 60 - 12) / 3;
    put(ID_CE_MX, px, y, ew, 22);
    put(ID_CE_MY, px + ew + 4, y, ew, 22);
    put(ID_CE_MZ, px + 2 * (ew + 4), y, ew, 22);
    put(ID_CE_MOVE, px + 3 * (ew + 4), y, pw - 3 * (ew + 4), 22);
    y += 30;
    MoveWindow(v.ceLabels[2], px, y + 4, 96, 16, TRUE);
    put(ID_CE_MAT, px + 98, y, pw - 98, 320);
    y += 30;
    MoveWindow(v.ceLabels[3], px, y, pw, 16, TRUE);
    y += 18;
    for (int k = 0; k < 6; ++k) put(ID_CE_F0 + k, px + (k % 3) * (third + 4), y + (k / 3) * 26, third, 22);
    y += 52;
    put(ID_CE_APPLY, px, y, pw, 22);
    y += 28;
    put(ID_CE_SHOWREF, px, y, pw, 20);
    y += 24;
    put(ID_CE_UNDO, px, y, half, 24);
    put(ID_CE_REDO, px + half + 4, y, half, 24);
    y += 30;
    const int bottom = ch - 32 - 30 - 28 - 6;  // Close, Save, the region row
    put(ID_CE_STATUS, px, y, pw, std::max(40, bottom - y));
    y = std::max(y + 40, bottom) + 4;
    MoveWindow(v.ceLabels[4], px, y + 4, 96, 16, TRUE);
    put(ID_CE_REGION, px + 98, y, pw - 98 - 84, 200);
    put(ID_CE_NEWREGION, px + pw - 80, y, 80, 22);
    y += 28;
    put(ID_CE_SAVE, px, y, pw, 26);
}

float ceNumber(HWND h, bool& ok) {
    char b[64] = "";
    GetWindowTextA(h, b, sizeof b);
    std::string s = trim(b);
    if (s.empty()) { ok = true; return 0; }
    char* end = nullptr;
    const double x = std::strtod(s.c_str(), &end);
    ok = end && !*end && std::isfinite(x) && std::fabs(x) <= 1e4;
    return ok ? float(x) : 0;
}

// WM_COMMAND from the Edit tab. True = handled.
bool ceCommand(ModelViewer& v, int id, int code) {
    switch (id) {
        case ID_CE_ADDBOX: ceAddBox(v); return true;
        case ID_CE_ADDSPHERE: ceAddSphere(v); return true;
        case ID_CE_SELSIDE: {
            std::set<size_t> all;
            for (size_t f : v.selFaces)
                for (size_t g : coplanarNeighbours(v.col, f)) all.insert(g);
            v.selFaces.insert(all.begin(), all.end());
            ceRefresh(v);
            return true;
        }
        case ID_CE_SELALL:
            for (size_t i = 0; i < v.col.faces.size(); ++i) v.selFaces.insert(i);
            ceRefresh(v);
            return true;
        case ID_CE_SELNONE: ceClearSelection(v); ceRefresh(v); return true;
        case ID_CE_DELETE: ceDelete(v); return true;
        case ID_CE_OUT:
        case ID_CE_IN: {
            bool ok;
            const float d = ceNumber(v.ce[ID_CE_DIST], ok);
            if (!ok || !(d > 0)) {
                ceWarn(v, "Type how far to push or pull, a number above 0 (for example 0.5).");
                return true;
            }
            cePushPull(v, id == ID_CE_OUT ? d : -d);
            return true;
        }
        case ID_CE_MOVE: {
            bool ox, oy, oz;
            const float dx = ceNumber(v.ce[ID_CE_MX], ox), dy = ceNumber(v.ce[ID_CE_MY], oy), dz = ceNumber(v.ce[ID_CE_MZ], oz);
            if (!ox || !oy || !oz) {
                ceWarn(v, "Type the move in X, Y and Z as numbers (empty counts as 0).");
                return true;
            }
            ceMove(v, dx, dy, dz);
            return true;
        }
        case ID_CE_MAT:
            if (code == CBN_SELCHANGE) ceSetMaterial(v, int(SendMessageA(v.ce[ID_CE_MAT], CB_GETCURSEL, 0, 0)));
            return true;
        case ID_CE_APPLY: ceApplyFields(v); return true;
        case ID_CE_SHOWREF: v.showRef = SendMessageA(v.ce[ID_CE_SHOWREF], BM_GETCHECK, 0, 0) == BST_CHECKED; return true;
        case ID_CE_UNDO: ceUndo(v); return true;
        case ID_CE_REDO: ceRedo(v); return true;
        case ID_CE_REGION:
            if (code == CBN_SELCHANGE) ceRefresh(v);
            return true;
        case ID_CE_NEWREGION: {
            const std::vector<std::string> before = userRegionNames();
            createRegionUi(v.wnd);
            const std::vector<std::string> after = userRegionNames();
            ceFillRegions(v, after.size() > before.size() ? after.back() : "");
            ceRefresh(v);
            return true;
        }
        case ID_CE_SAVE: ceSave(v); return true;
    }
    return false;
}

// The material list: MEd's colour beside each number, and how many shapes of this model use it.
void ceDrawMaterialItem(ModelViewer& v, const DRAWITEMSTRUCT* di) {
    if (di->itemID == UINT(-1)) return;
    const int k = int(di->itemID);
    const bool sel = (di->itemState & ODS_SELECTED) != 0;
    FillRect(di->hDC, &di->rcItem, GetSysColorBrush(sel ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    unsigned char rgb[3];
    colMaterialColour(k, rgb);
    HBRUSH b = CreateSolidBrush(RGB(rgb[0], rgb[1], rgb[2]));
    RECT sw = {di->rcItem.left + 3, di->rcItem.top + 3, di->rcItem.left + 19, di->rcItem.bottom - 3};
    FillRect(di->hDC, &sw, b);
    DeleteObject(b);
    FrameRect(di->hDC, &sw, (HBRUSH)GetStockObject(BLACK_BRUSH));
    int used = 0;
    for (const ColBox& x : v.col.boxes) used += x.surface.material == k;
    for (const ColSphere& x : v.col.spheres) used += x.surface.material == k;
    for (const ColFace& x : v.col.faces) used += x.material == k;
    std::string text = std::to_string(k) + "  " + colMaterialName(k) + (k <= 32 ? std::string("  - ") + gripGroupName(colMaterialGrip(k)) + " grip" : std::string()) +
                       (used ? "  (" + std::to_string(used) + " here)" : std::string());
    SetBkMode(di->hDC, TRANSPARENT);
    SetTextColor(di->hDC, GetSysColor(sel ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    RECT tr = di->rcItem;
    tr.left += 24;
    DrawTextA(di->hDC, text.c_str(), -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
}

// The GL model a viewer shows: a loose file's own, or the game model's.
GlModel* viewerGl(ModelViewer& v) {
    if (!v.modelKey.empty()) {
        auto m = app.models.find(v.modelKey);
        return m == app.models.end() ? nullptr : &m->second;
    }
    auto d = app.defs.find(v.id);
    return d == app.defs.end() ? nullptr : &getModel(d->second.model, d->second.txd);
}

void drawViewer(ModelViewer& v) {
    if (!v.gl || !v.rc) return;
    RECT r;
    GetClientRect(v.gl, &r);
    const int w = std::max<int>(r.right, 1), h = std::max<int>(r.bottom, 1);
    wglMakeCurrent(v.dc, v.rc);
    // CGLView::initializeGL: black, depth, alpha test > 0, blending, textures, smooth lines, nicest hints
    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 0);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glDisable(GL_CULL_FACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_LIGHTING);  // CGLView::setModel: models are drawn unlit
    glPolygonMode(GL_FRONT_AND_BACK, v.wireframe ? GL_LINE : GL_FILL);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, double(w) / h, 1.0, 5000.0);  // MEd: aspect 1 (models stretched); the true ratio here
    if (v.kind == 1) {  // 0x00414730: the collision light, set before the view's turn
        const GLfloat colLight[4] = {0, -1, -0.5f, 0};
        glLightfv(GL_LIGHT0, GL_POSITION, colLight);
    }
    glTranslatef(v.view.tx, v.view.ty, v.view.tz);
    glRotatef(v.view.rotX, 1, 0, 0);
    glRotatef(v.view.rotZ, 0, 0, 1);
    glRotatef(v.view.rotY, 0, 1, 0);
    if (v.kind == 0) {
        const GLfloat lightPos[4] = {0, 0, 200, 0};
        glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    }
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(-v.centre[0], -v.centre[1], -v.centre[2]);  // MEd turns about the model's origin, often a corner of a building
    glGetDoublev(GL_PROJECTION_MATRIX, v.proj);  // for picking in the Edit tab
    glGetDoublev(GL_MODELVIEW_MATRIX, v.mv);
    glGetIntegerv(GL_VIEWPORT, v.vp);
    if (v.kind == 1) drawColModel(v);
    else if (GlModel* gmp = viewerGl(v)) {
        GlModel& gm = *gmp;
        if (v.object <= 0) drawModel(gm);
        else if (v.object <= int(gm.geoms.size())) {
            GlModel one;
            one.geoms.push_back(gm.geoms[size_t(v.object - 1)]);
            drawModel(one);
        }
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    SwapBuffers(v.dc);
    wglMakeCurrent(app.dc, app.rc);
}

// Once a frame from the main loop: auto-rotation (CGLView's 40 ms timer) and drawing of every open viewer.
void updateViewers() {
    for (size_t i = 0; i < viewers.size();) {
        ModelViewer& v = *viewers[i];
        if (v.closed) {
            if (v.rc) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(v.rc); wglMakeCurrent(app.dc, app.rc); }
            viewers.erase(viewers.begin() + std::ptrdiff_t(i));
            continue;
        }
        const DWORD now = GetTickCount();
        if (v.autoRotate)
            while (now - v.lastTick >= 40) {  // 0x004140e0: Z +1, X +0.5, Y +0.4; past 360 restarts at 1
                v.lastTick += 40;
                v.view.rotZ += 1.0f;
                v.view.rotX += 0.5f;
                v.view.rotY += 0.4f;
                if (v.view.rotZ > 360) v.view.rotZ = 1;
                if (v.view.rotX > 360) v.view.rotX = 1;
                if (v.view.rotY > 360) v.view.rotY = 1;
            }
        else v.lastTick = now;
        if (IsWindowVisible(v.wnd) && !IsIconic(v.wnd)) drawViewer(v);
        ++i;
    }
}

LRESULT CALLBACK viewerGlProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    ModelViewer* v = viewerOf(w);
    if (!v) return DefWindowProcA(w, msg, wp, lp);
    if (v->kind == 1 && v->editTab) {
        if (msg == WM_KEYDOWN && ceKey(*v, wp)) return 0;
        if (ceMouse(*v, w, msg, wp, lp)) return 0;
    }
    switch (msg) {
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) SendMessageA(GetParent(w), WM_CLOSE, 0, 0);
            return 0;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
            SetFocus(w);
            SetCapture(w);
            v->last = {int(short(LOWORD(lp))), int(short(HIWORD(lp)))};
            return 0;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
            if (!(wp & (MK_LBUTTON | MK_RBUTTON))) ReleaseCapture();
            return 0;
        case WM_MOUSEMOVE: {  // 0x00414680: right button turns, left button moves
            const int x = int(short(LOWORD(lp))), y = int(short(HIWORD(lp)));
            const double dx = x - v->last.x, dy = y - v->last.y;
            if (wp & MK_RBUTTON) {
                v->view.rotZ += float(dx);
                v->view.rotX += float(dy);
            }
            if (wp & MK_LBUTTON) {
                v->view.tx += float(dx * 0.25);
                v->view.ty -= float(dy * 0.25);
            }
            v->last = {x, y};
            return 0;
        }
        case WM_MOUSEWHEEL: {  // 0x00414610: 1 per notch, 10 with Shift
            const float step = (GET_KEYSTATE_WPARAM(wp) & MK_SHIFT) ? 10.0f : 1.0f;
            v->view.tz += GET_WHEEL_DELTA_WPARAM(wp) > 0 ? step : -step;
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(w, &ps);
            EndPaint(w, &ps);
            return 0;
        }
    }
    return DefWindowProcA(w, msg, wp, lp);
}

void viewerLayout(ModelViewer& v) {
    RECT r;
    GetClientRect(v.wnd, &r);
    const int cw = r.right, ch = r.bottom, m = 6, px = cw - kViewerPanelW + m, pw = kViewerPanelW - 2 * m;
    MoveWindow(v.gl, 0, 0, std::max(cw - kViewerPanelW, 50), ch, TRUE);
    int y = m;
    if (v.tabs) {  // Collision Object Properties: View | Edit
        MoveWindow(v.tabs, px, y, pw, 24, TRUE);
        y += 28;
    }
    const int third = (pw - 8) / 3;
    MoveWindow(GetDlgItem(v.wnd, ID_MV_ZOOMIN), px, y, third, 24, TRUE);
    MoveWindow(GetDlgItem(v.wnd, ID_MV_ZOOMOUT), px + third + 4, y, third, 24, TRUE);
    MoveWindow(GetDlgItem(v.wnd, ID_MV_RESET), px + 2 * (third + 4), y, third, 24, TRUE);
    y += 30;
    if (v.kind == 1 && v.editTab) {
        ceLayout(v, px, pw, y, ch);
        MoveWindow(GetDlgItem(v.wnd, ID_MV_CLOSE), px, ch - 32, pw, 26, TRUE);
        return;
    }
    for (int id : {ID_MV_ROTATE, ID_MV_WIRE, ID_MV_2DFX, ID_MV_NIGHT, ID_MV_BOXES, ID_MV_SPHERES, ID_MV_MESH, ID_MV_SHADOW}) {
        HWND c = GetDlgItem(v.wnd, id);
        if (!c) continue;  // each kind of viewer has its own set
        MoveWindow(c, px, y, pw, 20, TRUE);
        y += 22;
    }
    if (v.objectBox) {
        MoveWindow(GetDlgItem(v.wnd, ID_MV_LBL_OBJECT), px, y + 4, 120, 16, TRUE);
        MoveWindow(v.objectBox, px + 122, y, pw - 122, 200, TRUE);
        y += 30;
    } else {
        y += 6;
    }
    MoveWindow(v.info, px, y, pw, std::max(60, ch - y - 40), TRUE);
    MoveWindow(GetDlgItem(v.wnd, ID_MV_CLOSE), px, ch - 32, pw, 26, TRUE);
}

// Point the view at the middle of a collision model, far enough away to see all of it.
void viewerFitCol(ModelViewer& v) {
    double half = 1;
    for (int k = 0; k < 3; ++k) {
        v.centre[k] = (v.col.boundMin[k] + v.col.boundMax[k]) / 2;
        half = std::max(half, double(v.col.boundMax[k] - v.col.boundMin[k]) / 2);
    }
    v.home = GlViewState();
    v.home.tz = -float(std::max(3.0, half * 4.0));
    v.view = v.home;
}

void viewerPickColModel(ModelViewer& v, int k) {
    if (k < 0 || k >= int(v.colModels.size())) return;
    v.col = v.colModels[size_t(k)];
    v.dff = v.col.name;
    v.loaded = v.col;
    v.undo.clear();
    v.redo.clear();
    ceClearSelection(v);
    v.dirty = false;
    ++v.rev;
    viewerFitCol(v);
    SetWindowTextA(v.info, colInfoText(v).c_str());
    SetWindowTextA(v.wnd, ("Collision Object Properties - " + v.col.name + " (" + v.file.substr(v.file.find_last_of("/" + std::string(1, char(92))) + 1) + ")").c_str());
}

LRESULT CALLBACK viewerProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    ModelViewer* v = viewerOf(w);
    switch (msg) {
        case WM_SIZE:
            if (v) viewerLayout(*v);
            return 0;
        case WM_GETMINMAXINFO:
            reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize = {kViewerPanelW + 260, v && v->kind == 1 ? 660 : 420};
            return 0;
        case WM_NOTIFY:
            if (v && reinterpret_cast<NMHDR*>(lp)->idFrom == ID_CE_TABS && reinterpret_cast<NMHDR*>(lp)->code == TCN_SELCHANGE) {
                ceShowTab(*v, SendMessageA(v->tabs, TCM_GETCURSEL, 0, 0) == 1);
                viewerLayout(*v);
            }
            return 0;
        case WM_MEASUREITEM:
            if (reinterpret_cast<MEASUREITEMSTRUCT*>(lp)->CtlID == ID_CE_MAT) {
                reinterpret_cast<MEASUREITEMSTRUCT*>(lp)->itemHeight = 20;
                return TRUE;
            }
            break;
        case WM_DRAWITEM:
            if (v && reinterpret_cast<DRAWITEMSTRUCT*>(lp)->CtlID == ID_CE_MAT) {
                ceDrawMaterialItem(*v, reinterpret_cast<DRAWITEMSTRUCT*>(lp));
                return TRUE;
            }
            break;
        case WM_COMMAND: {
            if (!v) return 0;
            const int id = LOWORD(wp), code = HIWORD(wp);
            if (v->kind == 1 && ceCommand(*v, id, code)) return 0;
            if (id == ID_MV_ZOOMIN) v->view.tz += 3;   // 0x004143c0
            else if (id == ID_MV_ZOOMOUT) v->view.tz -= 3;  // 0x004143e0
            else if (id == ID_MV_RESET) v->view = v->home;
            else if (id == ID_MV_ROTATE) v->autoRotate = SendMessageA(GetDlgItem(w, id), BM_GETCHECK, 0, 0) == BST_CHECKED;
            else if (id == ID_MV_WIRE) v->wireframe = SendMessageA(GetDlgItem(w, id), BM_GETCHECK, 0, 0) == BST_CHECKED;
            else if (id == ID_MV_2DFX) v->fx = SendMessageA(GetDlgItem(w, id), BM_GETCHECK, 0, 0) == BST_CHECKED;
            else if (id == ID_MV_NIGHT) v->night = SendMessageA(GetDlgItem(w, id), BM_GETCHECK, 0, 0) == BST_CHECKED;
            else if (id == ID_MV_BOXES) v->showBoxes = SendMessageA(GetDlgItem(w, id), BM_GETCHECK, 0, 0) == BST_CHECKED;
            else if (id == ID_MV_SPHERES) v->showSpheres = SendMessageA(GetDlgItem(w, id), BM_GETCHECK, 0, 0) == BST_CHECKED;
            else if (id == ID_MV_MESH) v->showMesh = SendMessageA(GetDlgItem(w, id), BM_GETCHECK, 0, 0) == BST_CHECKED;
            else if (id == ID_MV_OBJECT && code == CBN_SELCHANGE) {
                const int k = int(SendMessageA(v->objectBox, CB_GETCURSEL, 0, 0));
                if (v->kind == 1 && v->dirty) {  // the edits belong to the model shown: keep them until saved or undone
                    for (size_t i = 0; i < v->colModels.size(); ++i)
                        if (v->colModels[i].name == v->col.name) SendMessageA(v->objectBox, CB_SETCURSEL, WPARAM(i), 0);
                    ceWarn(*v, "Save or undo the changes to " + v->col.name + " before picking another model.");
                }
                else if (v->kind == 1) viewerPickColModel(*v, k);  // a loose .col: Model in file
                else v->object = k;
            }
            else if (id == ID_MV_CLOSE) SendMessageA(w, WM_CLOSE, 0, 0);
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) SendMessageA(w, WM_CLOSE, 0, 0);
            return 0;
        case WM_CLOSE:
            if (v && !ceConfirmClose(*v)) return 0;  // unsaved collision edits: Save / Discard / Cancel
            DestroyWindow(w);
            return 0;
        case WM_DESTROY:
            if (v) v->closed = true;
            return 0;
    }
    return DefWindowProcA(w, msg, wp, lp);
}

// Open MEd's "DFF Properties" (kind 0) or "Collision Object Properties" (kind 1) for a model definition. Several can
// be open at once; opening one that is already open brings it to the front.
ModelViewer* openModelViewer(HWND owner, int id, int kind, const std::string& file) {
    auto d = app.defs.find(id);
    if (file.empty() && d == app.defs.end()) return nullptr;
    for (auto& v : viewers)
        if (v->kind == kind && !v->closed && (file.empty() ? v->id == id : toLower(v->file) == toLower(file))) {
            ShowWindow(v->wnd, SW_RESTORE);
            SetForegroundWindow(v->wnd);
            return v.get();
        }
    auto v = std::make_unique<ModelViewer>();
    v->id = file.empty() ? id : -1;
    v->kind = kind;
    v->file = file;
    std::vector<unsigned char> data;
    std::string err;
    const std::string title0 = "View " + std::string(kind == 1 ? "Collision" : "DFF");
    if (!file.empty()) {  // a loose file from disk
        bool ok;
        const std::string bytes = slurp(file, ok);
        if (!ok) {
            inspWarn(owner, "Could not open " + file + ".", title0.c_str());
            return nullptr;
        }
        data.assign(bytes.begin(), bytes.end());
        const size_t slash = file.find_last_of("/" + std::string(1, char(92)));
        const std::string folder = slash == std::string::npos ? "." : file.substr(0, slash);
        std::string stem = file.substr(slash == std::string::npos ? 0 : slash + 1);
        if (stem.size() > 4) stem = stem.substr(0, stem.size() - 4);
        if (kind == 1) {
            if (!parseCol(data, v->colModels, err) && v->colModels.empty()) {
                inspWarn(owner, file + " is not a collision file this editor can read" + (err.empty() ? "." : ": " + err), title0.c_str());
                return nullptr;
            }
            if (v->colModels.empty()) {
                inspWarn(owner, file + " holds no collision models.", title0.c_str());
                return nullptr;
            }
            v->col = v->colModels[0];
            v->dff = v->col.name;
            v->colSource = file;
        } else {
            if (!parseDff(data, v->model, err)) {
                inspWarn(owner, file + " is not a model this editor can read" + (err.empty() ? "." : ": " + err), title0.c_str());
                return nullptr;
            }
            v->dff = stem;
            // its textures: <name>.txd beside it, else the only .txd in that folder
            std::string txdPath;
            std::error_code ec;
            if (std::filesystem::exists(folder + "/" + stem + ".txd", ec)) txdPath = folder + "/" + stem + ".txd";
            else {
                std::vector<std::string> found;
                for (const auto& e : std::filesystem::directory_iterator(folder, ec))
                    if (toLower(e.path().extension().string()) == ".txd") found.push_back(e.path().string());
                if (found.size() == 1) txdPath = found[0];
            }
            const std::string txdKey = "file:" + toLower(txdPath);
            if (!txdPath.empty()) {
                const std::string tb = slurp(txdPath, ok);
                TxdFile tf;
                std::string terr;
                if (ok && parseTxd(std::vector<unsigned char>(tb.begin(), tb.end()), tf, terr)) {
                    auto old = app.textures.find(txdKey);  // a re-opened file is read again (it may have changed)
                    if (old != app.textures.end()) {
                        for (auto& kv : old->second) glDeleteTextures(1, &kv.second);
                        app.textures.erase(old);
                    }
                    uploadTxd(txdKey, tf);
                    app.loadedTxds.insert(txdKey);
                    v->txd = txdPath.substr(txdPath.find_last_of("/" + std::string(1, char(92))) + 1);
                } else {
                    v->txd = "(could not read " + txdPath + ")";
                }
            } else {
                v->txd = "(none found: put " + stem + ".txd next to the model)";
            }
            v->modelKey = "file:" + toLower(file);
            app.models.erase(v->modelKey);
            buildGlModel(app.models[v->modelKey], v->model, txdPath.empty() ? std::string("file:none") : txdKey);
        }
    } else if (kind == 1) {  // the model's own collision record, from the COL files of gta3.dat (the last that has it: the game's choice)
        std::vector<unsigned char> record;
        std::vector<ColModel> models;
        if (!findCollisionRecord(app.colFiles, d->second.model, record, v->colSource) || !parseCol(record, models, err) || models.empty()) {
            inspWarn(owner, "No collision model named " + d->second.name + " was found in the game's COL files.", "Collision Object Properties");
            return nullptr;
        }
        v->col = models[0];
        v->dff = d->second.name;
        const std::string root = app.root + "/";
        if (v->colSource.rfind(root, 0) == 0) v->colSource = v->colSource.substr(root.size());
    } else if (!app.img.extract(d->second.model + ".dff", data, err) || !parseDff(data, v->model, err)) {
        inspWarn(owner, "Could not read " + d->second.name + ".dff from " + app.img.path() + (err.empty() ? "." : ": " + err), "DFF Properties");
        return nullptr;
    }
    if (file.empty()) {
        v->dff = d->second.name;
        v->txd = d->second.txd;
        getModel(d->second.model, d->second.txd);  // load it (and its textures) under the map's context, which the viewer shares
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc = viewerProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "MEdModelViewer";
        RegisterClassA(&wc);
        WNDCLASSA gc = {};
        gc.style = CS_OWNDC | CS_DBLCLKS;
        gc.lpfnWndProc = viewerGlProc;
        gc.hInstance = GetModuleHandle(nullptr);
        gc.hCursor = LoadCursor(nullptr, IDC_SIZEALL);
        gc.lpszClassName = "MEdModelViewerGL";
        RegisterClassA(&gc);
        registered = true;
    }
    RECT r;
    GetWindowRect(owner, &r);
    const int n = int(viewers.size());
    const std::string fileName = file.empty() ? std::string() : file.substr(file.find_last_of("/" + std::string(1, char(92))) + 1);
    const std::string title = kind == 1 ? "Collision Object Properties - " + v->dff + (file.empty() ? "" : " (" + fileName + ")")
                                        : "DFF Properties - " + (file.empty() ? v->dff + ".dff" : file);
    v->wnd = CreateWindowExA(0, "MEdModelViewer", title.c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, r.left + 120 + 30 * n, r.top + 60 + 30 * n, 900, kind == 1 ? 720 : 600, owner,
                             nullptr, GetModuleHandle(nullptr), nullptr);
    v->gl = CreateWindowExA(0, "MEdModelViewerGL", "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 10, 10, v->wnd, nullptr, GetModuleHandle(nullptr), nullptr);
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    auto make = [&](const char* cls, const char* text, DWORD style, DWORD ex, int cid) {
        HWND h = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, v->wnd, (HMENU)(INT_PTR)cid, GetModuleHandle(nullptr), nullptr);
        SendMessageA(h, WM_SETFONT, (WPARAM)font, TRUE);
        return h;
    };
    make("BUTTON", "Zoom In", BS_PUSHBUTTON, 0, ID_MV_ZOOMIN);
    make("BUTTON", "Zoom Out", BS_PUSHBUTTON, 0, ID_MV_ZOOMOUT);
    make("BUTTON", "Reset View", BS_PUSHBUTTON, 0, ID_MV_RESET);
    make("BUTTON", "Automatically Rotate", BS_AUTOCHECKBOX, 0, ID_MV_ROTATE);
    make("BUTTON", kind == 1 ? "Wireframe" : "Show Wireframe", BS_AUTOCHECKBOX, 0, ID_MV_WIRE);
    if (kind == 1) {
        for (int cid : {ID_MV_BOXES, ID_MV_SPHERES, ID_MV_MESH}) {
            HWND c = make("BUTTON", cid == ID_MV_BOXES ? "Show Boxes" : cid == ID_MV_SPHERES ? "Show Spheres" : "Show Collision Mesh", BS_AUTOCHECKBOX, 0, cid);
            SendMessageA(c, BM_SETCHECK, BST_CHECKED, 0);
        }
        HWND shadow = make("BUTTON", "Show Shadow Mesh (COL3 only)", BS_AUTOCHECKBOX, 0, ID_MV_SHADOW);
        EnableWindow(shadow, FALSE);  // GTA III and Vice City collision has no shadow mesh
        if (v->colModels.size() > 1) {  // a loose .col with several models: pick one
            make("STATIC", "Model in file:", SS_LEFT, 0, ID_MV_LBL_OBJECT);
            v->objectBox = make("COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 0, ID_MV_OBJECT);
            for (const ColModel& c : v->colModels) SendMessageA(v->objectBox, CB_ADDSTRING, 0, (LPARAM)c.name.c_str());
            SendMessageA(v->objectBox, CB_SETCURSEL, 0, 0);
        }
        v->info = make("EDIT", "", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, WS_EX_CLIENTEDGE, ID_MV_INFO);
        make("BUTTON", "Close", BS_PUSHBUTTON, 0, ID_MV_CLOSE);
        SetWindowTextA(v->info, colInfoText(*v).c_str());
        for (int cid : {ID_MV_ROTATE, ID_MV_WIRE, ID_MV_BOXES, ID_MV_SPHERES, ID_MV_MESH, ID_MV_SHADOW, ID_MV_LBL_OBJECT, ID_MV_OBJECT, ID_MV_INFO})
            if (HWND c = GetDlgItem(v->wnd, cid)) v->viewCtrls.push_back(c);
        v->loaded = v->col;
        ceCreateControls(*v, font);  // the Edit tab
    } else {
        HWND fx = make("BUTTON", "Show 2DFX Lighting", BS_AUTOCHECKBOX, 0, ID_MV_2DFX);
        HWND night = make("BUTTON", "Show Night Vertex Colours", BS_AUTOCHECKBOX, 0, ID_MV_NIGHT);
        make("STATIC", "Show Object Number:", SS_LEFT, 0, ID_MV_LBL_OBJECT);
        v->objectBox = make("COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 0, ID_MV_OBJECT);
        v->info = make("EDIT", "", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, WS_EX_CLIENTEDGE, ID_MV_INFO);
        make("BUTTON", "Close", BS_PUSHBUTTON, 0, ID_MV_CLOSE);
        SendMessageA(v->objectBox, CB_ADDSTRING, 0, (LPARAM) "All objects");
        for (size_t k = 0; k < v->model.atomics.size(); ++k) SendMessageA(v->objectBox, CB_ADDSTRING, 0, (LPARAM)std::to_string(k + 1).c_str());
        SendMessageA(v->objectBox, CB_SETCURSEL, 0, 0);
        bool hasNight = false, hasFx = false;
        for (const DffGeometry& g : v->model.geometries) { hasNight = hasNight || g.nightColors; hasFx = hasFx || g.has2dfx; }
        EnableWindow(fx, hasFx);        // the GTA III models have neither; the check boxes are for Vice City and San Andreas
        EnableWindow(night, hasNight);
        SetWindowTextA(v->info, dffInfoText(*v).c_str());
    }
    // the GL context of the 3D view, sharing textures with the map's
    v->dc = GetDC(v->gl);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 24;
    const int fmt = ChoosePixelFormat(v->dc, &pfd);
    if (fmt) SetPixelFormat(v->dc, fmt, &pfd);
    v->rc = wglCreateContext(v->dc);
    if (v->rc) wglShareLists(app.rc, v->rc);
    wglMakeCurrent(app.dc, app.rc);
    // turn about the middle of the mesh and start far enough away to see all of it (MEd: the origin and always 52)
    if (kind == 1) {  // the collision bounds
        viewerFitCol(*v);
    } else {
        GlModel& gm = *viewerGl(*v);
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        for (const GlGeometry& g : gm.geoms)
            for (size_t i = 0; i + 2 < g.pos.size(); i += 3)
                for (int k = 0; k < 3; ++k) {
                    const float c = g.matrix[k] * g.pos[i] + g.matrix[4 + k] * g.pos[i + 1] + g.matrix[8 + k] * g.pos[i + 2] + g.matrix[12 + k];
                    lo[k] = std::min(lo[k], c);
                    hi[k] = std::max(hi[k], c);
                }
        double half = 1;
        for (int k = 0; k < 3; ++k)
            if (hi[k] >= lo[k]) {
                v->centre[k] = (lo[k] + hi[k]) / 2;
                half = std::max(half, double(hi[k] - lo[k]) / 2);
            }
        v->home.tz = -float(std::max(3.0, half * 4.0));  // the whole model fills the view, a cone as well as a building
    }
    v->view = v->home;
    v->lastTick = GetTickCount();
    ModelViewer* raw = v.get();
    viewers.push_back(std::move(v));
    viewerLayout(*raw);
    ShowWindow(raw->wnd, SW_SHOW);
    return raw;
}

ModelViewer* openDffViewer(HWND owner, int id) { return openModelViewer(owner, id, 0, ""); }
ModelViewer* openColViewer(HWND owner, int id) { return openModelViewer(owner, id, 1, ""); }

// ---- MEd's "TXD Properties" (CTexView, "TextureView", constructor 0x00465270): a texture dictionary's textures ------------
// MEd: a list of the texture names (itemSelectionChanged shows the picked one), the picture, and a table with the
// dictionary, Texture Name, Alpha Name, MipMaps and AlphaFlags; Close ("Close TXD Properties Window"). Here also the size,
// the pixel format, the platform and which object definitions use the dictionary; the picture is drawn over a
// checkerboard so transparent parts show ("Show alpha"), at its own size or fitted to the window.

struct TxdViewer {
    HWND wnd = nullptr, list = nullptr, preview = nullptr, info = nullptr;
    std::string txd;           // dictionary name (without .txd)
    std::string source;        // where it was read from
    TxdFile file;
    int sel = -1;              // the texture shown
    bool fit = true, alpha = true;
    std::vector<uint8_t> bgra; // the shown texture, ready for StretchDIBits
    bool closed = false;
};
std::vector<std::unique_ptr<TxdViewer>> txdViewers;

enum { ID_TV_LIST = 9101, ID_TV_PREVIEW, ID_TV_INFO, ID_TV_FIT, ID_TV_ALPHA, ID_TV_CLOSE };

TxdViewer* txdViewerOf(HWND w) {
    for (auto& v : txdViewers)
        if (v->wnd == w || v->preview == w) return v.get();
    return nullptr;
}

// RenderWare raster format in words: the pixel layout (bits 8..11) and palette / mipmap flags.
std::string rasterFormatText(const TxdTexture& t) {
    std::string s;
    switch (t.rasterFormat & 0xF00) {
        case 0x100: s = "1555 (16-bit, 1-bit alpha)"; break;
        case 0x200: s = "565 (16-bit)"; break;
        case 0x300: s = "4444 (16-bit, 4-bit alpha)"; break;
        case 0x400: s = "LUM8 (8-bit grey)"; break;
        case 0x500: s = "8888 (32-bit with alpha)"; break;
        case 0x600: s = "888 (24-bit)"; break;
        case 0xA00: s = "555 (16-bit)"; break;
        default: s = "unknown"; break;
    }
    if (t.rasterFormat & 0x2000) s = "8-bit palette of " + s;
    if (t.rasterFormat & 0x4000) s = "4-bit palette of " + s;
    if (t.compressed) s += ", DXT compressed";
    return s;
}

std::string txdInfoText(const TxdViewer& v) {
    std::string t = "Texture Dictionary: " + v.txd + ".txd\r\nFrom: " + v.source + "\r\nTextures: " + std::to_string(v.file.textures.size()) + "\r\n";
    std::vector<std::string> users;
    for (const auto& kv : app.defs)
        if (kv.second.txd == toLower(v.txd)) users.push_back(kv.second.name);
    t += "Used by " + std::to_string(users.size()) + " object definition(s)";
    for (size_t i = 0; i < users.size() && i < 8; ++i) t += (i ? ", " : ": ") + users[i];
    if (users.size() > 8) t += ", ...";
    t += "\r\n";
    if (v.sel >= 0 && v.sel < int(v.file.textures.size())) {
        const TxdTexture& x = v.file.textures[size_t(v.sel)];
        t += "\r\nTexture Name: " + x.name + "\r\n";
        t += "Alpha Name: " + (x.mask.empty() ? std::string("(none)") : x.mask) + "\r\n";
        t += "Size: " + std::to_string(x.width) + " x " + std::to_string(x.height) + "\r\n";
        t += "Format: " + rasterFormatText(x) + ", " + std::to_string(x.depth) + " bits stored\r\n";
        t += "MipMaps: " + std::to_string(x.mipLevels) + "\r\n";
        t += std::string("AlphaFlags: ") + (x.hasAlpha ? "has alpha" : "no alpha") + "\r\n";
        t += std::string("Platform: ") + (x.platform == 8 ? "Direct3D 8 (GTA III / Vice City PC)" : x.platform == 9 ? "Direct3D 9 (San Andreas PC)" : std::to_string(x.platform)) + "\r\n";
    }
    return t;
}

void txdShow(TxdViewer& v, int index) {
    v.sel = index;
    v.bgra.clear();
    if (index >= 0 && index < int(v.file.textures.size())) {
        const TxdTexture& x = v.file.textures[size_t(index)];
        v.bgra.resize(x.rgba.size());
        for (size_t i = 0; i + 3 < x.rgba.size(); i += 4) {
            v.bgra[i] = x.rgba[i + 2];
            v.bgra[i + 1] = x.rgba[i + 1];
            v.bgra[i + 2] = x.rgba[i];
            v.bgra[i + 3] = x.rgba[i + 3];
        }
    }
    SetWindowTextA(v.info, txdInfoText(v).c_str());
    InvalidateRect(v.preview, nullptr, FALSE);
}

// The picture: over a grey checkerboard, alpha blended (Show alpha) or opaque, at its size or fitted (keeping its shape).
void txdPaintPreview(TxdViewer& v, HDC dc, const RECT& r) {
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP back = CreateCompatibleBitmap(dc, r.right, r.bottom);
    HGDIOBJ old = SelectObject(mem, back);
    HBRUSH light = CreateSolidBrush(RGB(204, 204, 204)), dark = CreateSolidBrush(RGB(153, 153, 153));
    for (int y = 0; y < r.bottom; y += 12)
        for (int x = 0; x < r.right; x += 12) {
            RECT c = {x, y, x + 12, y + 12};
            FillRect(mem, &c, ((x / 12 + y / 12) % 2) ? dark : light);
        }
    DeleteObject(light);
    DeleteObject(dark);
    if (v.sel >= 0 && v.sel < int(v.file.textures.size()) && !v.bgra.empty()) {
        const TxdTexture& x = v.file.textures[size_t(v.sel)];
        int w = x.width, h = x.height;
        if (v.fit) {
            const double s = std::min(double(r.right - 16) / w, double(r.bottom - 16) / h);
            w = std::max(1, int(w * s));
            h = std::max(1, int(h * s));
        }
        const int ox = std::max(0, int(r.right - w) / 2), oy = std::max(0, int(r.bottom - h) / 2);
        // premultiplied copy for AlphaBlend (Show alpha), or the colours alone
        std::vector<uint8_t> px = v.bgra;
        for (size_t i = 0; i + 3 < px.size(); i += 4) {
            if (v.alpha) {
                const unsigned a = px[i + 3];
                px[i] = uint8_t(px[i] * a / 255);
                px[i + 1] = uint8_t(px[i + 1] * a / 255);
                px[i + 2] = uint8_t(px[i + 2] * a / 255);
            } else {
                px[i + 3] = 255;
            }
        }
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof bi.bmiHeader;
        bi.bmiHeader.biWidth = x.width;
        bi.bmiHeader.biHeight = -int(x.height);  // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        void* bits = nullptr;
        HBITMAP tex = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (tex && bits) {
            std::memcpy(bits, px.data(), px.size());
            HDC tdc = CreateCompatibleDC(dc);
            HGDIOBJ o = SelectObject(tdc, tex);
            SetStretchBltMode(mem, COLORONCOLOR);  // nearest pixel: texels stay sharp when enlarged
            BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
            AlphaBlend(mem, ox, oy, w, h, tdc, 0, 0, x.width, x.height, bf);
            SelectObject(tdc, o);
            DeleteDC(tdc);
        }
        if (tex) DeleteObject(tex);
    }
    BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(back);
    DeleteDC(mem);
}

LRESULT CALLBACK txdPreviewProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    TxdViewer* v = txdViewerOf(w);
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT && v) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(w, &ps);
        RECT r;
        GetClientRect(w, &r);
        txdPaintPreview(*v, dc, r);
        EndPaint(w, &ps);
        return 0;
    }
    return DefWindowProcA(w, msg, wp, lp);
}

void txdLayout(TxdViewer& v) {
    RECT r;
    GetClientRect(v.wnd, &r);
    const int m = 6, listW = 220, infoH = 150;
    MoveWindow(v.list, m, m, listW, r.bottom - 2 * m, TRUE);
    const int x = m + listW + m, w = std::max(60, int(r.right) - x - m);
    MoveWindow(v.preview, x, m, w, std::max(60, int(r.bottom) - 3 * m - infoH - 28), TRUE);
    const int y = r.bottom - m - infoH - 28;
    MoveWindow(GetDlgItem(v.wnd, ID_TV_FIT), x, y, 120, 22, TRUE);
    MoveWindow(GetDlgItem(v.wnd, ID_TV_ALPHA), x + 126, y, 120, 22, TRUE);
    MoveWindow(GetDlgItem(v.wnd, ID_TV_CLOSE), x + w - 90, y, 90, 24, TRUE);
    MoveWindow(v.info, x, y + 28, w, infoH, TRUE);
}

LRESULT CALLBACK txdViewerProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    TxdViewer* v = txdViewerOf(w);
    switch (msg) {
        case WM_SIZE:
            if (v) txdLayout(*v);
            return 0;
        case WM_GETMINMAXINFO:
            reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize = {560, 420};
            return 0;
        case WM_NOTIFY: {
            const NMHDR* h = reinterpret_cast<const NMHDR*>(lp);
            if (v && h->hwndFrom == v->list && h->code == LVN_ITEMCHANGED) {
                const NMLISTVIEW* n = reinterpret_cast<const NMLISTVIEW*>(lp);
                if ((n->uNewState & LVIS_SELECTED) && n->iItem != v->sel) txdShow(*v, n->iItem);
            }
            return 0;
        }
        case WM_COMMAND:
            if (!v) return 0;
            if (LOWORD(wp) == ID_TV_FIT) { v->fit = SendMessageA(GetDlgItem(w, ID_TV_FIT), BM_GETCHECK, 0, 0) == BST_CHECKED; InvalidateRect(v->preview, nullptr, FALSE); }
            else if (LOWORD(wp) == ID_TV_ALPHA) { v->alpha = SendMessageA(GetDlgItem(w, ID_TV_ALPHA), BM_GETCHECK, 0, 0) == BST_CHECKED; InvalidateRect(v->preview, nullptr, FALSE); }
            else if (LOWORD(wp) == ID_TV_CLOSE) DestroyWindow(w);
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) DestroyWindow(w);
            return 0;
        case WM_CLOSE:
            DestroyWindow(w);
            return 0;
        case WM_DESTROY:
            if (v) v->closed = true;
            return 0;
    }
    return DefWindowProcA(w, msg, wp, lp);
}

// Read a texture dictionary the way the game finds it: an entry of gta3.img, or models/generic.txd for "generic".
bool readTxd(const std::string& name, TxdFile& out, std::string& source, std::string& err) {
    std::vector<unsigned char> data;
    if (app.img.extract(name + ".txd", data, err)) {
        source = app.img.path();
        std::replace(source.begin(), source.end(), '\\', '/');
        std::string root = app.root + "/";
        std::replace(root.begin(), root.end(), '\\', '/');
        if (toLower(source).rfind(toLower(root), 0) == 0) source = source.substr(root.size());  // models/gta3.img
    }
    else if (toLower(name) == "generic") {
        bool ok;
        const std::string path = app.root + "/models/generic.txd";
        const std::string bytes = slurp(path, ok);
        if (!ok) return false;
        data.assign(bytes.begin(), bytes.end());
        source = "models/generic.txd";
    } else {
        return false;
    }
    return parseTxd(data, out, err);
}

TxdViewer* openTxdViewer(HWND owner, const std::string& txdName, const std::string& file) {
    for (auto& v : txdViewers)
        if (!v->closed && (file.empty() ? toLower(v->txd) == toLower(txdName) && v->source.find(':') == std::string::npos : toLower(v->source) == toLower(file))) {
            ShowWindow(v->wnd, SW_RESTORE);
            SetForegroundWindow(v->wnd);
            return v.get();
        }
    auto v = std::make_unique<TxdViewer>();
    v->txd = txdName;
    std::string err;
    if (!file.empty()) {  // a loose .txd from disk
        bool ok;
        const std::string bytes = slurp(file, ok);
        if (!ok || !parseTxd(std::vector<unsigned char>(bytes.begin(), bytes.end()), v->file, err)) {
            inspWarn(owner, ok ? file + " is not a texture dictionary this editor can read" + (err.empty() ? "." : ": " + err) : "Could not open " + file + ".", "View TXD");
            return nullptr;
        }
        v->source = file;
    } else if (!readTxd(txdName, v->file, v->source, err)) {
        inspWarn(owner, "Could not read " + txdName + ".txd" + (err.empty() ? " (it is not in gta3.img)." : ": " + err), "TXD Properties");
        return nullptr;
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc = txdViewerProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "MEdTxdViewer";
        RegisterClassA(&wc);
        WNDCLASSA pc = {};
        pc.lpfnWndProc = txdPreviewProc;
        pc.hInstance = GetModuleHandle(nullptr);
        pc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        pc.lpszClassName = "MEdTxdPreview";
        RegisterClassA(&pc);
        registered = true;
    }
    RECT r;
    GetWindowRect(owner, &r);
    const int n = int(txdViewers.size());
    const std::string title = "TXD Properties - " + (file.empty() ? txdName + ".txd" : file);
    v->wnd = CreateWindowExA(0, "MEdTxdViewer", title.c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, r.left + 140 + 30 * n, r.top + 110 + 30 * n, 860, 620, owner,
                             nullptr, GetModuleHandle(nullptr), nullptr);
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    auto make = [&](const char* cls, const char* text, DWORD style, DWORD ex, int cid) {
        HWND h = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, v->wnd, (HMENU)(INT_PTR)cid, GetModuleHandle(nullptr), nullptr);
        SendMessageA(h, WM_SETFONT, (WPARAM)font, TRUE);
        return h;
    };
    v->list = make(WC_LISTVIEWA, "", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE, ID_TV_LIST);
    ListView_SetExtendedListViewStyle(v->list, LVS_EX_FULLROWSELECT);
    const char* cols[2] = {"Texture Name", "Size"};
    for (int i = 0; i < 2; ++i) {
        LVCOLUMNA c = {};
        c.mask = LVCF_TEXT | LVCF_WIDTH;
        c.pszText = const_cast<char*>(cols[i]);
        c.cx = i == 0 ? 130 : 66;
        ListView_InsertColumn(v->list, i, &c);
    }
    for (size_t i = 0; i < v->file.textures.size(); ++i) {
        const TxdTexture& x = v->file.textures[i];
        LVITEMA it = {};
        it.mask = LVIF_TEXT;
        it.iItem = int(i);
        it.pszText = const_cast<char*>(x.name.c_str());
        ListView_InsertItem(v->list, &it);
        const std::string size = std::to_string(x.width) + "x" + std::to_string(x.height);
        ListView_SetItemText(v->list, int(i), 1, const_cast<char*>(size.c_str()));
    }
    v->preview = CreateWindowExA(WS_EX_CLIENTEDGE, "MEdTxdPreview", "", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, v->wnd, (HMENU)ID_TV_PREVIEW, GetModuleHandle(nullptr), nullptr);
    HWND fit = make("BUTTON", "Fit to window", BS_AUTOCHECKBOX, 0, ID_TV_FIT);
    HWND alpha = make("BUTTON", "Show alpha", BS_AUTOCHECKBOX, 0, ID_TV_ALPHA);
    SendMessageA(fit, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageA(alpha, BM_SETCHECK, BST_CHECKED, 0);
    make("BUTTON", "Close", BS_PUSHBUTTON, 0, ID_TV_CLOSE);
    v->info = make("EDIT", "", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, WS_EX_CLIENTEDGE, ID_TV_INFO);
    TxdViewer* raw = v.get();
    txdViewers.push_back(std::move(v));
    txdLayout(*raw);
    if (!raw->file.textures.empty()) {
        ListView_SetItemState(raw->list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);  // shows the first
    } else {
        txdShow(*raw, -1);
    }
    ShowWindow(raw->wnd, SW_SHOW);
    return raw;
}

// View a loose .dff, .txd or .col from disk (File > View a DFF / TXD / COL file..., or dropped on the window).
bool openLooseFile(HWND owner, const std::string& path) {
    std::string ext = toLower(path.size() > 4 ? path.substr(path.size() - 4) : path);
    if (ext == ".dff") return openModelViewer(owner, -1, 0, path) != nullptr;
    if (ext == ".col") return openModelViewer(owner, -1, 1, path) != nullptr;
    if (ext == ".txd") {
        const size_t slash = path.find_last_of("/" + std::string(1, char(92)));
        std::string stem = path.substr(slash == std::string::npos ? 0 : slash + 1);
        return openTxdViewer(owner, stem.substr(0, stem.size() - 4), path) != nullptr;
    }
    inspWarn(owner, path + " is not a .dff, .txd or .col file.", "View a file");
    return false;
}

// File > View a DFF / TXD / COL file...: a file picker that remembers its folder (med_viewer.ini, [Viewer] folder).
void viewFileUi(HWND owner) {
    char last[MAX_PATH] = "";
    GetPrivateProfileStringA("Viewer", "folder", "", last, sizeof last, modIniPath().c_str());
    char name[MAX_PATH * 8] = "";
    OPENFILENAMEA o = {};
    o.lStructSize = sizeof o;
    o.hwndOwner = owner;
    o.lpstrFilter = "Models, textures and collision (*.dff;*.txd;*.col)\0*.dff;*.txd;*.col\0Models (*.dff)\0*.dff\0Texture dictionaries (*.txd)\0*.txd\0Collision (*.col)\0*.col\0";
    o.lpstrFile = name;
    o.nMaxFile = sizeof name;
    o.lpstrInitialDir = last[0] ? last : nullptr;
    o.lpstrTitle = "View a model, texture dictionary or collision file";
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&o)) return;
    // one file: the full path; several: the folder, then each name, separated by zeros
    std::string first = name;
    std::vector<std::string> files;
    const char* p = name + first.size() + 1;
    if (!*p) files.push_back(first);
    while (*p) {
        files.push_back(first + "\\" + p);
        p += std::strlen(p) + 1;
    }
    const std::string folder = files.size() == 1 ? first.substr(0, first.find_last_of("\\/")) : first;
    WritePrivateProfileStringA("Viewer", "folder", folder.c_str(), modIniPath().c_str());
    for (const std::string& f : files) openLooseFile(owner, f);
}

// Once a frame: drop closed texture viewers.
void updateTxdViewers() {
    txdViewers.erase(std::remove_if(txdViewers.begin(), txdViewers.end(), [](const std::unique_ptr<TxdViewer>& v) { return v->closed; }), txdViewers.end());
}

// ---- Import Model (rules: CLAUDE.md "Import rules"; core: core/importer.h) ---------------------------------------------
// File > Import Model...: pick the .dff; the .txd and .col with the same name beside it are filled in. The window checks
// everything as you change it (planImport, nothing written) and lists the problems and notes; Import is only enabled
// when there are no problems. The three files can be looked at in the viewers first. The destination is one of the
// user's own regions (made with New region): its own .ide and .col get the definition and the collision, gta3.dat lists
// them, the model and textures are appended to gta3.img. Afterwards the model is in the Item tab, ready to place.

struct ImportUi {
    HWND wnd = nullptr, dff = nullptr, txd = nullptr, col = nullptr, name = nullptr, id = nullptr, draw = nullptr, flags = nullptr, region = nullptr, checks = nullptr;
    HWND startEmpty = nullptr, startBox = nullptr, startMesh = nullptr, importBtn = nullptr;
    bool done = false, imported = false;
    std::string lastError;
    int importedId = -1;
    bool quiet = false;
    std::set<int> ids;               // what the game uses (gameIdsAndNames), read when the window opens
    std::set<std::string> names;
    bool scanned = false;
} imp;

enum { ID_IM_DFF = 9201, ID_IM_DFF_BROWSE, ID_IM_DFF_VIEW, ID_IM_TXD, ID_IM_TXD_BROWSE, ID_IM_TXD_VIEW, ID_IM_COL, ID_IM_COL_BROWSE, ID_IM_COL_CLEAR,
       ID_IM_COL_VIEW, ID_IM_EMPTY, ID_IM_BOX, ID_IM_MESH, ID_IM_NAME, ID_IM_ID, ID_IM_SUGGEST, ID_IM_DRAW, ID_IM_FLAGS, ID_IM_REGION, ID_IM_NEWREGION,
       ID_IM_CHECKS, ID_IM_IMPORT, ID_IM_CANCEL, ID_IM_LABEL };

std::string imText(HWND h) {
    char b[1024] = "";
    GetWindowTextA(h, b, sizeof b);
    return trim(b);
}

// The user's own regions (made with New region): they get the model's .ide and .col.
std::vector<std::string> userRegionNames() {
    std::vector<std::string> out;
    for (const std::string& rel : regionsAddedSinceBackup(app.root)) {  // "DATA/MAPS/<name>/<name>.IPL"
        std::string s = rel;
        std::replace(s.begin(), s.end(), '\\', '/');
        const size_t a = s.find_last_of('/');
        std::string file = a == std::string::npos ? s : s.substr(a + 1);
        if (file.size() > 4) out.push_back(file.substr(0, file.size() - 4));
    }
    return out;
}

void imFillRegions(const std::string& select = "") {
    SendMessageA(imp.region, CB_RESETCONTENT, 0, 0);
    int pick = 0, k = 0;
    for (const std::string& r : userRegionNames()) {
        SendMessageA(imp.region, CB_ADDSTRING, 0, (LPARAM)r.c_str());
        if (toLower(r) == toLower(select)) pick = k;
        ++k;
    }
    SendMessageA(imp.region, CB_SETCURSEL, k ? pick : -1, 0);
}

ImportRequest imRequest() {
    ImportRequest r;
    r.gameRoot = app.root;
    r.dffPath = imText(imp.dff);
    r.txdPath = imText(imp.txd);
    r.colPath = imText(imp.col);
    r.name = imText(imp.name);
    const std::string idText = imText(imp.id), drawText = imText(imp.draw), flagText = imText(imp.flags);
    char* end = nullptr;
    const long id = std::strtol(idText.c_str(), &end, 10);
    r.id = (!idText.empty() && end && *end == 0) ? int(id) : -1;
    const double dd = std::strtod(drawText.c_str(), &end);
    r.drawDistance = (!drawText.empty() && end && *end == 0) ? dd : -1;
    const long fl = std::strtol(flagText.c_str(), &end, 10);
    r.flags = (!flagText.empty() && end && *end == 0) ? int(fl) : 0;
    r.start = SendMessageA(imp.startBox, BM_GETCHECK, 0, 0) == BST_CHECKED ? CollisionStart::Box
            : SendMessageA(imp.startMesh, BM_GETCHECK, 0, 0) == BST_CHECKED ? CollisionStart::Mesh
                                                                            : CollisionStart::Empty;
    const int sel = int(SendMessageA(imp.region, CB_GETCURSEL, 0, 0));
    if (sel >= 0) {
        char b[256] = "";
        SendMessageA(imp.region, CB_GETLBTEXT, WPARAM(sel), (LPARAM)b);
        r.region = b;
    }
    return r;
}

// Check everything again (nothing written) and show the result.
// `rescan`: read the game's IDE files again for the ids and names in use (about 150 ms on GTA III, so it is done when the
// window opens and right before importing, not on every key press).
ImportPlan imRecheck(bool rescan = false) {
    const DWORD t0 = GetTickCount();
    if (rescan || !imp.scanned) {
        imp.ids.clear();
        imp.names.clear();
        gameIdsAndNames(app.root, imp.ids, imp.names);
        imp.scanned = true;
    }
    const DWORD t1 = GetTickCount();
    ImportRequest r = imRequest();
    ImportPlan p = planImport(r, app.img, imp.ids, imp.names);
    if (std::getenv("MED_DEBUG")) std::printf("import check: game ids %lu ms, plan %lu ms\n", t1 - t0, GetTickCount() - t1), std::fflush(stdout);
    if (imText(imp.id).size() && r.id < 0) p.problems.insert(p.problems.begin(), "The model ID must be a whole number.");
    if (imText(imp.flags).size() && r.flags == 0 && imText(imp.flags) != "0") p.problems.insert(p.problems.begin(), "The flags must be a whole number.");
    const bool noCol = r.colPath.empty();
    for (HWND h : {imp.startEmpty, imp.startBox, imp.startMesh}) EnableWindow(h, noCol);
    std::string text;
    if (p.problems.empty()) text = "Ready to import " + r.name + " (ID " + std::to_string(r.id) + ") into region " + r.region + ".\r\n";
    else {
        text = "Not ready yet:\r\n";
        for (const std::string& s : p.problems) text += "  - " + s + "\r\n";
    }
    if (p.problems.empty() || !p.notes.empty()) {
        if (!p.notes.empty()) text += "\r\nWorth knowing:\r\n";
        for (const std::string& s : p.notes) text += "  - " + s + "\r\n";
        if (p.problems.empty()) {
            text += "\r\nCollision: ";
            text += p.collisionFromFile ? "from " + r.colPath : r.start == CollisionStart::Box ? std::string("one box around the model")
                                                              : r.start == CollisionStart::Mesh ? "the model's own triangles (" + std::to_string(p.collision.faces.size()) + " faces)"
                                                                                                : std::string("empty (a clean slate)");
            text += "\r\nThe model and textures go into models/gta3.img (gta3.dir is backed up first); the definition into data/maps/" + r.region + "/" + r.region +
                    ".ide; the collision into data/maps/" + r.region + "/" + r.region + ".col (level 0, always loaded).\r\n";
        }
    }
    SetWindowTextA(imp.checks, text.c_str());
    EnableWindow(imp.importBtn, p.problems.empty());
    return p;
}

// A file beside the model with the same name and this extension, else "".
std::string imBeside(const std::string& dff, const std::string& ext) {
    std::error_code ec;
    const std::filesystem::path p(dff);
    const std::filesystem::path c = p.parent_path() / (p.stem().string() + ext);
    return std::filesystem::exists(c, ec) ? c.string() : std::string();
}

void imSetModel(const std::string& dff) {
    SetWindowTextA(imp.dff, dff.c_str());
    std::string txd = imBeside(dff, ".txd");
    if (txd.empty()) {  // the only .txd in that folder
        std::error_code ec;
        std::vector<std::string> found;
        for (const auto& e : std::filesystem::directory_iterator(std::filesystem::path(dff).parent_path(), ec))
            if (toLower(e.path().extension().string()) == ".txd") found.push_back(e.path().string());
        if (found.size() == 1) txd = found[0];
    }
    SetWindowTextA(imp.txd, txd.c_str());
    SetWindowTextA(imp.col, imBeside(dff, ".col").c_str());
    SetWindowTextA(imp.name, std::filesystem::path(dff).stem().string().c_str());
}

std::string imPickFile(HWND owner, const char* filter, const char* title) {
    char last[MAX_PATH] = "";
    GetPrivateProfileStringA("Import", "folder", "", last, sizeof last, modIniPath().c_str());
    char name[MAX_PATH] = "";
    OPENFILENAMEA o = {};
    o.lStructSize = sizeof o;
    o.hwndOwner = owner;
    o.lpstrFilter = filter;
    o.lpstrFile = name;
    o.nMaxFile = sizeof name;
    o.lpstrInitialDir = last[0] ? last : nullptr;
    o.lpstrTitle = title;
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&o)) return "";
    const std::string folder = std::filesystem::path(name).parent_path().string();
    WritePrivateProfileStringA("Import", "folder", folder.c_str(), modIniPath().c_str());
    return name;
}

// After a successful import: the new definition, its collision and the region's COL file are known at once.
void imLoadImported(const ImportRequest& r, const ImportPlan& p) {
    ModelDef d;
    d.model = toLower(r.name);
    d.name = r.name;
    d.txd = toLower(p.txdName);
    d.ide = r.region + ".ide";
    d.ideLine = ideObjsLine(r.id, r.name, p.txdName, r.drawDistance, r.flags);
    d.drawDistance = r.drawDistance;
    d.farModel = isFarModel(d.model, d.drawDistance);
    d.pool = Pool::Building;  // not in object.dat and no path data: a building slot
    app.defs[r.id] = d;
    app.cols[d.model] = p.collision;
    const std::string colPath = app.root + "/data/maps/" + r.region + "/" + r.region + ".col";
    bool known = false;
    for (const std::string& c : app.colFiles) known = known || toLower(c) == toLower(colPath);
    if (!known) app.colFiles.push_back(colPath);
    rb.ends.erase(r.id);
    itRefill();
}

bool imDoImport(HWND w) {
    ImportPlan p = imRecheck(true);  // the game's files as they are now
    if (!p.problems.empty()) return false;
    const ImportRequest r = imRequest();
    std::vector<std::string> written;
    std::string err;
    if (!applyImport(r, p, app.img, written, err)) {
        imp.lastError = err;
        if (!imp.quiet) MessageBoxA(w, ("The import stopped: " + err + "\n\nmodels/med-imports.txt and the backups say how to undo what was written.").c_str(), "Import Model", MB_ICONERROR);
        return false;
    }
    imLoadImported(r, p);
    imp.imported = true;
    imp.importedId = r.id;
    if (!imp.quiet)
        MessageBoxA(w, (r.name + " is imported (ID " + std::to_string(r.id) + ") and is in the Item tab, ready to place in region " + r.region +
                        ".\n\nFiles changed in the game folder:\n" + [&] {
                            std::string s;
                            for (std::string f : written) {
                                std::replace(f.begin(), f.end(), '\\', '/');
                                std::string root = app.root;
                                std::replace(root.begin(), root.end(), '\\', '/');
                                if (toLower(f).rfind(toLower(root) + "/", 0) == 0) f = f.substr(root.size() + 1);
                                s += "  " + f + "\n";
                            }
                            return s;
                        }() + "\nHow to undo it is written in models/med-imports.txt.")
                           .c_str(),
                    "Import Model", MB_ICONINFORMATION);
    return true;
}

LRESULT CALLBACK importProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND: {
            const int id = LOWORD(wp), code = HIWORD(wp);
            if (id == ID_IM_DFF_BROWSE) {
                const std::string f = imPickFile(w, "Models (*.dff)\0*.dff\0", "Import a model: pick its .dff");
                if (!f.empty()) imSetModel(f);
            } else if (id == ID_IM_TXD_BROWSE) {
                const std::string f = imPickFile(w, "Texture dictionaries (*.txd)\0*.txd\0", "The model's texture dictionary");
                if (!f.empty()) SetWindowTextA(imp.txd, f.c_str());
            } else if (id == ID_IM_COL_BROWSE) {
                const std::string f = imPickFile(w, "Collision (*.col)\0*.col\0", "The model's collision");
                if (!f.empty()) SetWindowTextA(imp.col, f.c_str());
            } else if (id == ID_IM_COL_CLEAR) {
                SetWindowTextA(imp.col, "");
            } else if (id == ID_IM_DFF_VIEW || id == ID_IM_TXD_VIEW || id == ID_IM_COL_VIEW) {
                const std::string f = imText(id == ID_IM_DFF_VIEW ? imp.dff : id == ID_IM_TXD_VIEW ? imp.txd : imp.col);
                if (!f.empty()) openLooseFile(w, f);
            } else if (id == ID_IM_SUGGEST) {
                imRecheck(true);
                SetWindowTextA(imp.id, std::to_string(proposeModelId(imp.ids)).c_str());
            } else if (id == ID_IM_NEWREGION) {
                const std::vector<std::string> before = userRegionNames();
                createRegionUi(w);
                const std::vector<std::string> after = userRegionNames();
                imFillRegions(after.size() > before.size() ? after.back() : "");
            } else if (id == ID_IM_IMPORT) {
                if (imDoImport(w)) imp.done = true;
            } else if (id == ID_IM_CANCEL) {
                imp.done = true;
            }
            if (code == EN_CHANGE || code == CBN_SELCHANGE || code == BN_CLICKED) {
                if (!imp.done) imRecheck();
            }
            return 0;
        }
        case WM_CLOSE:
            imp.done = true;
            return 0;
    }
    return DefWindowProcA(w, msg, wp, lp);
}

// File > Import Model... (`dffPath` given: start with that model, for tests and drag and drop).
void importModelUi(HWND owner, const std::string& dffPath) {
    std::string dff = dffPath;
    if (dff.empty()) {
        dff = imPickFile(owner, "Models (*.dff)\0*.dff\0", "Import a model: pick its .dff (its .txd and .col beside it come along)");
        if (dff.empty()) return;
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc = importProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "MEdImport";
        RegisterClassA(&wc);
        registered = true;
    }
    const bool quiet = imp.quiet;
    imp = ImportUi();
    imp.quiet = quiet;
    RECT r;
    GetWindowRect(owner, &r);
    imp.wnd = CreateWindowExA(WS_EX_DLGMODALFRAME, "MEdImport", "Import Model", WS_POPUP | WS_CAPTION | WS_SYSMENU, r.left + 100, r.top + 80, 720, 620, owner, nullptr,
                              GetModuleHandle(nullptr), nullptr);
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    auto make = [&](const char* cls, const char* text, DWORD style, DWORD ex, int id, int x, int y, int w, int h) {
        HWND c = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, imp.wnd, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
        SendMessageA(c, WM_SETFONT, (WPARAM)font, TRUE);
        return c;
    };
    int y = 12;
    auto fileRow = [&](const char* label, int editId, int browseId, int viewId, HWND& edit) {
        make("STATIC", label, SS_LEFT, 0, ID_IM_LABEL, 12, y + 4, 110, 16);
        edit = make("EDIT", "", ES_AUTOHSCROLL | ES_READONLY, WS_EX_CLIENTEDGE, editId, 124, y, 400, 22);
        make("BUTTON", "Browse...", BS_PUSHBUTTON, 0, browseId, 530, y, 80, 22);
        make("BUTTON", "View", BS_PUSHBUTTON, 0, viewId, 616, y, 80, 22);
        y += 28;
    };
    fileRow("Model (.dff):", ID_IM_DFF, ID_IM_DFF_BROWSE, ID_IM_DFF_VIEW, imp.dff);
    fileRow("Textures (.txd):", ID_IM_TXD, ID_IM_TXD_BROWSE, ID_IM_TXD_VIEW, imp.txd);
    fileRow("Collision (.col):", ID_IM_COL, ID_IM_COL_BROWSE, ID_IM_COL_VIEW, imp.col);
    make("BUTTON", "No collision file", BS_PUSHBUTTON, 0, ID_IM_COL_CLEAR, 530, y, 166, 22);
    make("STATIC", "Without a collision file it starts as:", SS_LEFT, 0, ID_IM_LABEL, 12, y + 4, 300, 16);
    y += 26;
    imp.startEmpty = make("BUTTON", "Empty (a clean slate: nothing solid until you build it)", BS_AUTORADIOBUTTON | WS_GROUP, 0, ID_IM_EMPTY, 24, y, 660, 20);
    y += 22;
    imp.startBox = make("BUTTON", "One box around the model (solid at once; good for simple props)", BS_AUTORADIOBUTTON, 0, ID_IM_BOX, 24, y, 660, 20);
    y += 22;
    imp.startMesh = make("BUTTON", "Copy the model's triangles (solid in its exact shape; good for roads and buildings)", BS_AUTORADIOBUTTON, 0, ID_IM_MESH, 24, y, 660, 20);
    SendMessageA(imp.startEmpty, BM_SETCHECK, BST_CHECKED, 0);
    y += 32;
    make("STATIC", "Name:", SS_LEFT, 0, ID_IM_LABEL, 12, y + 4, 60, 16);
    imp.name = make("EDIT", "", ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, ID_IM_NAME, 74, y, 170, 22);
    make("STATIC", "Model ID:", SS_LEFT, 0, ID_IM_LABEL, 258, y + 4, 60, 16);
    imp.id = make("EDIT", "", ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, ID_IM_ID, 320, y, 70, 22);
    make("BUTTON", "Suggest", BS_PUSHBUTTON, 0, ID_IM_SUGGEST, 394, y, 70, 22);
    make("STATIC", "Draw distance:", SS_LEFT, 0, ID_IM_LABEL, 476, y + 4, 80, 16);
    imp.draw = make("EDIT", "200", ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, ID_IM_DRAW, 556, y, 50, 22);
    make("STATIC", "Flags:", SS_LEFT, 0, ID_IM_LABEL, 616, y + 4, 34, 16);
    imp.flags = make("EDIT", "0", ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, ID_IM_FLAGS, 652, y, 44, 22);
    y += 32;
    make("STATIC", "Region:", SS_LEFT, 0, ID_IM_LABEL, 12, y + 4, 60, 16);
    imp.region = make("COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 0, ID_IM_REGION, 74, y, 300, 200);
    make("BUTTON", "New region...", BS_PUSHBUTTON, 0, ID_IM_NEWREGION, 380, y, 100, 22);
    make("STATIC", "Your own regions only", SS_LEFT, 0, ID_IM_LABEL, 488, y + 4, 208, 16);
    y += 34;
    imp.checks = make("EDIT", "", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, WS_EX_CLIENTEDGE, ID_IM_CHECKS, 12, y, 684, 560 - y - 30);
    imp.importBtn = make("BUTTON", "Import", BS_DEFPUSHBUTTON, 0, ID_IM_IMPORT, 520, 560 - 18, 84, 26);
    make("BUTTON", "Cancel", BS_PUSHBUTTON, 0, ID_IM_CANCEL, 612, 560 - 18, 84, 26);
    gameIdsAndNames(app.root, imp.ids, imp.names);  // once; every check while typing uses this
    imp.scanned = true;
    SetWindowTextA(imp.id, std::to_string(proposeModelId(imp.ids)).c_str());
    imFillRegions();
    imSetModel(dff);
    imRecheck();
    if (imp.quiet) return;  // tests drive the window themselves and close it
    EnableWindow(owner, FALSE);
    ShowWindow(imp.wnd, SW_SHOW);
    MSG m;
    while (!imp.done && GetMessageA(&m, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageA(imp.wnd, &m)) {
            TranslateMessage(&m);
            DispatchMessageA(&m);
        }
    }
    EnableWindow(owner, TRUE);
    DestroyWindow(imp.wnd);
    imp.wnd = nullptr;
    SetForegroundWindow(owner);
}

// The tabs at the top of the left column: Scene (which regions are shown), Placements and Item.
void showLeftTab(int tab) {
    if (app.leftTabs) TabCtrl_SetCurSel(app.leftTabs, tab);
    if (app.scenePanel) ShowWindow(app.scenePanel, tab == 0 ? SW_SHOW : SW_HIDE);
    if (pl.panel) ShowWindow(pl.panel, tab == 1 ? SW_SHOW : SW_HIDE);
    if (it.panel) ShowWindow(it.panel, tab == 2 ? SW_SHOW : SW_HIDE);
}

void createPlacementsPanel(HWND parent) {
    INITCOMMONCONTROLSEX ic = {sizeof ic, ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&ic);
    WNDCLASSA wc = {};
    wc.lpfnWndProc = placementsProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "MEdPlacements";
    RegisterClassA(&wc);
    app.leftTabs = CreateWindowExA(0, WC_TABCONTROLA, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, app.leftW, 24, parent, (HMENU)102, GetModuleHandle(nullptr), nullptr);
    SendMessageA(app.leftTabs, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    const char* names[3] = {"Scene", "Placements", "Item"};
    for (int i = 0; i < 3; ++i) {
        TCITEMA t = {};
        t.mask = TCIF_TEXT;
        t.pszText = const_cast<char*>(names[i]);
        TabCtrl_InsertItem(app.leftTabs, i, &t);
    }
    app.leftTabH = 24;
    CreateWindowExA(0, "MEdPlacements", "", WS_CHILD | WS_CLIPCHILDREN, 0, 0, app.leftW, 400, parent, nullptr, GetModuleHandle(nullptr), nullptr);
    createItemPanel(parent);
    plFillRegions();
    plRefill();
    showLeftTab(0);
}

int placementsRegion() { return pl.panel ? pl.region : -1; }

// ---- the Bookmarks window (after MEd's CBookMarks, 0x0040a870) ----------------------------------------------------------
// MEd: a list of saved camera positions with Add... (asks for a name), Remove (asks first), Move Up, Move Down, Move to
// location, OK (save and close) and Cancel (discard). Here the window stays open while you fly around and every change is
// saved at once (no OK / Cancel to forget), Rename... is added, double-click or Enter flies there, Delete removes. The file
// keeps MEd's format, so it can be exchanged with MEd: bookmarks.ini next to the editor, started from MEd's own file the
// first time (MEdv032/bookmarks.ini itself is never written).

enum { ID_BM_LIST = 7001, ID_BM_ADD, ID_BM_RENAME, ID_BM_REMOVE, ID_BM_UP, ID_BM_DOWN, ID_BM_GO, ID_BM_CLOSE, ID_BM_STATUS };

struct BookmarksUi {
    HWND wnd = nullptr, list = nullptr, status = nullptr, tips = nullptr;
    bool quiet = false;      // self-test: no dialogs; Remove counts as confirmed
    std::string lastMessage;
} bm;

bool bmSave() {
    std::ofstream f(app.bookmarkFile, std::ios::binary | std::ios::trunc);
    f << writeBookmarks(app.bookmarks);
    f.close();
    if (!f) {
        bm.lastMessage = "Could not save " + app.bookmarkFile + " (read-only?). The change is kept until the editor closes.";
        if (!bm.quiet) MessageBoxA(bm.wnd ? bm.wnd : app.hwnd, bm.lastMessage.c_str(), "Bookmarks", MB_ICONWARNING);
        return false;
    }
    return true;
}

int bmSelected() {
    if (!bm.list) return -1;
    const int row = ListView_GetNextItem(bm.list, -1, LVNI_SELECTED);
    return row >= 0 && row < int(app.bookmarks.size()) ? row : -1;
}

void bmFill(int select) {
    if (!bm.list) return;
    ListView_DeleteAllItems(bm.list);
    for (size_t i = 0; i < app.bookmarks.size(); ++i) {
        const Bookmark& b = app.bookmarks[i];
        LVITEMA it2 = {};
        it2.mask = LVIF_TEXT;
        it2.iItem = int(i);
        it2.pszText = const_cast<char*>(b.name.c_str());
        ListView_InsertItem(bm.list, &it2);
        const std::string where = num(b.x, 1) + ", " + num(b.y, 1) + ", " + num(b.z, 1);
        ListView_SetItemText(bm.list, int(i), 1, const_cast<char*>(where.c_str()));
    }
    if (select >= int(app.bookmarks.size())) select = int(app.bookmarks.size()) - 1;
    if (select >= 0) {
        ListView_SetItemState(bm.list, select, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(bm.list, select, FALSE);
    }
    const std::string s = app.bookmarks.empty() ? "No bookmarks yet. Fly somewhere and press Add... to keep the place."
                                                : std::to_string(app.bookmarks.size()) + " bookmark(s). Double-click or Enter flies there. Saved in " + app.bookmarkFile;
    SetWindowTextA(bm.status, s.c_str());
}

// The camera, as MEd's bookmark holds it.
Bookmark bmFromCamera(const std::string& name) {
    Bookmark b;
    b.name = name;
    b.x = app.cam.x;
    b.y = app.cam.y;
    b.z = app.cam.z;
    b.h = app.cam.h;
    b.yaw = app.cam.yaw;
    b.pitch = app.cam.pitch;
    return b;
}

void bmGoTo(int i) {
    if (i < 0 || i >= int(app.bookmarks.size())) return;
    const Bookmark& b = app.bookmarks[i];
    app.cam.x = float(b.x);
    app.cam.y = float(b.y);
    app.cam.z = float(b.z);
    app.cam.h = float(b.h);
    app.cam.yaw = float(b.yaw);
    app.cam.pitch = float(b.pitch);
}

// A suggested name in MEd's style, "GTA III - <region> - ", the region being the one of the placement nearest to where
// the camera looks.
std::string bmSuggestedName() {
    const double tx = app.cam.x, ty = app.cam.y;
    int best = -1;
    double bestD = 1e30;
    for (size_t i = 0; i < app.instances.size(); ++i) {
        const double d = (app.instances[i].x - tx) * (app.instances[i].x - tx) + (app.instances[i].y - ty) * (app.instances[i].y - ty);
        if (d < bestD) { bestD = d; best = int(i); }
    }
    std::string region = "";
    if (best >= 0) {
        const std::string& p = app.iplPaths[app.instFile[best]];
        region = p.substr(p.find_last_of('/') + 1);
        if (region.size() > 4 && toLower(region.substr(region.size() - 4)) == ".ipl") region = region.substr(0, region.size() - 4);
    }
    return "GTA III - " + (region.empty() ? std::string("") : region + " - ");
}

bool bmAdd(const std::string& name) {
    if (!validBookmarkName(name)) {
        bm.lastMessage = "A bookmark name cannot be empty or contain < > or line breaks.";
        if (!bm.quiet) MessageBoxA(bm.wnd, bm.lastMessage.c_str(), "Bookmarks", MB_ICONWARNING);
        return false;
    }
    app.bookmarks.push_back(bmFromCamera(trim(name)));
    bmSave();
    bmFill(int(app.bookmarks.size()) - 1);
    return true;
}

bool bmRename(int i, const std::string& name) {
    if (i < 0 || i >= int(app.bookmarks.size())) return false;
    if (!validBookmarkName(name)) {
        bm.lastMessage = "A bookmark name cannot be empty or contain < > or line breaks.";
        if (!bm.quiet) MessageBoxA(bm.wnd, bm.lastMessage.c_str(), "Bookmarks", MB_ICONWARNING);
        return false;
    }
    app.bookmarks[i].name = trim(name);
    bmSave();
    bmFill(i);
    return true;
}

bool bmRemove(int i) {
    if (i < 0 || i >= int(app.bookmarks.size())) return false;
    if (!bm.quiet) {
        const std::string q = "Are you sure you want to remove this item from your bookmarks list?\n\n" + app.bookmarks[i].name;  // MEd's question
        if (MessageBoxA(bm.wnd, q.c_str(), "Remove bookmark", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return false;
    }
    app.bookmarks.erase(app.bookmarks.begin() + i);
    bmSave();
    bmFill(i);
    return true;
}

bool bmMove(int i, int dir) {
    const int j = i + dir;
    if (i < 0 || j < 0 || i >= int(app.bookmarks.size()) || j >= int(app.bookmarks.size())) return false;
    std::swap(app.bookmarks[i], app.bookmarks[j]);
    bmSave();
    bmFill(j);
    return true;
}

void bmCommand(int id) {
    const int sel = bmSelected();
    switch (id) {
        case ID_BM_ADD: {
            std::string name = bmSuggestedName();
            if (askText(bm.wnd, "Add bookmark", "Please choose a name for this location.", name)) bmAdd(name);  // MEd's prompt
            break;
        }
        case ID_BM_RENAME: {
            if (sel < 0) break;
            std::string name = app.bookmarks[sel].name;
            if (askText(bm.wnd, "Rename bookmark", "New name for this location:", name)) bmRename(sel, name);
            break;
        }
        case ID_BM_REMOVE: bmRemove(sel); break;
        case ID_BM_UP: bmMove(sel, -1); break;
        case ID_BM_DOWN: bmMove(sel, 1); break;
        case ID_BM_GO: bmGoTo(sel); break;
        case ID_BM_CLOSE: DestroyWindow(bm.wnd); break;
    }
}

// Enter flies to the selected bookmark, Delete removes it (after asking). Called in the main loop before dispatch.
bool bmHandleKey(const MSG& m) {
    if (!bm.list || m.message != WM_KEYDOWN || m.hwnd != bm.list) return false;
    if (m.wParam == VK_RETURN) { bmGoTo(bmSelected()); return true; }
    if (m.wParam == VK_DELETE) { bmRemove(bmSelected()); return true; }
    return false;
}

LRESULT CALLBACK bookmarksProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            bm.wnd = w;
            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            auto make = [&](const char* cls, const char* text, DWORD style, DWORD ex, int id) {
                HWND c = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, w, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
                SendMessageA(c, WM_SETFONT, (WPARAM)font, TRUE);
                return c;
            };
            bm.tips = CreateWindowExA(WS_EX_TOPMOST, TOOLTIPS_CLASSA, nullptr, WS_POPUP | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, w,
                                      nullptr, GetModuleHandle(nullptr), nullptr);
            auto tip = [&](HWND c, const char* text) {
                TOOLINFOA ti = {};
                ti.cbSize = sizeof ti;
                ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
                ti.hwnd = w;
                ti.uId = (UINT_PTR)c;
                ti.lpszText = const_cast<char*>(text);
                SendMessageA(bm.tips, TTM_ADDTOOLA, 0, (LPARAM)&ti);
            };
            bm.list = make(WC_LISTVIEWA, "", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_BM_LIST);
            ListView_SetExtendedListViewStyle(bm.list, LVS_EX_FULLROWSELECT);
            const char* cols[2] = {"Name", "Position (x, y, z)"};
            const int widths[2] = {260, 150};
            for (int i = 0; i < 2; ++i) {
                LVCOLUMNA c = {};
                c.mask = LVCF_TEXT | LVCF_WIDTH;
                c.pszText = const_cast<char*>(cols[i]);
                c.cx = widths[i];
                ListView_InsertColumn(bm.list, i, &c);
            }
            const struct { const char* text; int id; const char* help; } buttons[] = {
                {"&Add...", ID_BM_ADD, "Add the current camera position as a bookmark"},
                {"Re&name...", ID_BM_RENAME, "Give the selected bookmark another name"},
                {"&Remove", ID_BM_REMOVE, "Remove the selected bookmark (asks first; Delete key too)"},
                {"Move &Up", ID_BM_UP, "Move the selected bookmark up the list"},
                {"Move &Down", ID_BM_DOWN, "Move the selected bookmark down the list"},
                {"&Move to location", ID_BM_GO, "Fly to the selected bookmark (double-click or Enter too)"},
                {"&Close", ID_BM_CLOSE, "Close this window (everything is already saved)"}};
            for (const auto& b : buttons) tip(make("BUTTON", b.text, BS_PUSHBUTTON | WS_TABSTOP, 0, b.id), b.help);
            bm.status = make("STATIC", "", SS_LEFT, 0, ID_BM_STATUS);
            tip(bm.list, "Double-click or Enter: fly there. Delete: remove.");
            bmFill(0);
            return 0;
        }
        case WM_SIZE: {
            const int cw = LOWORD(lp), ch = HIWORD(lp), m = 8, bw = 120;
            MoveWindow(bm.list, m, m, cw - 3 * m - bw, ch - 2 * m - 34, TRUE);
            int y = m;
            for (int id = ID_BM_ADD; id <= ID_BM_CLOSE; ++id) {
                if (id == ID_BM_UP || id == ID_BM_GO || id == ID_BM_CLOSE) y += 8;
                MoveWindow(GetDlgItem(w, id), cw - m - bw, y, bw, 26, TRUE);
                y += 30;
            }
            MoveWindow(bm.status, m, ch - m - 28, cw - 2 * m, 28, TRUE);
            return 0;
        }
        case WM_COMMAND:
            if (HIWORD(wp) == BN_CLICKED) bmCommand(LOWORD(wp));
            return 0;
        case WM_NOTIFY: {
            NMHDR* h = reinterpret_cast<NMHDR*>(lp);
            if (h->hwndFrom == bm.list && h->code == NM_DBLCLK) bmGoTo(reinterpret_cast<NMITEMACTIVATE*>(lp)->iItem);
            return 0;
        }
        case WM_GETMINMAXINFO:
            reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize = {420, 320};
            return 0;
        case WM_CLOSE:
            DestroyWindow(w);
            return 0;
        case WM_DESTROY:
            bm.wnd = bm.list = bm.status = bm.tips = nullptr;
            return 0;
    }
    return DefWindowProcA(w, msg, wp, lp);
}

void showBookmarksWindow(HWND owner) {
    if (bm.wnd) {
        SetForegroundWindow(bm.wnd);
        return;
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc = bookmarksProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "MEdBookmarks";
        RegisterClassA(&wc);
        registered = true;
    }
    RECT r;
    GetWindowRect(owner, &r);
    CreateWindowExA(WS_EX_TOOLWINDOW, "MEdBookmarks", "Bookmarks", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VISIBLE, r.left + 120, r.top + 120, 580,
                    360, owner, nullptr, GetModuleHandle(nullptr), nullptr);
}

// ---- self-test of the Placements tab: med_viewer <game copy> --selftest ---------------------------------------------
// Drives the tab inside the real window through the same messages a user's clicks and keys produce (WM_SETTEXT on the
// search box, CBN_SELCHANGE, BN_CLICKED, LVN_COLUMNCLICK, NM_DBLCLK, list selection changes, F3 / Enter / Esc), and the
// editing actions (undo, redo, delete, duplicate, copy and paste, new object, move to region, new region), checking after
// every step that the list is consistent with the map; then a long random sequence of all of them. It edits the map in
// memory and creates a region on disk, so run it on a throwaway copy of the game; it never saves.

struct SelfTest {
    int checks = 0, failures = 0;
    void check(bool ok, const std::string& what, int line) {
        ++checks;
        if (!ok) {
            ++failures;
            std::printf("FAIL (line %d): %s\n", line, what.c_str());
            std::fflush(stdout);
        }
    }
};
#define ST_CHECK(t, c, what) (t).check((c), (what), __LINE__)

namespace stui {
void search(const std::string& text) { SetWindowTextA(pl.search, text.c_str()); }  // the edit control sends EN_CHANGE
void region(int file) {
    const int n = int(SendMessageA(pl.regionBox, CB_GETCOUNT, 0, 0));
    for (int i = 0; i < n; ++i)
        if (int(SendMessageA(pl.regionBox, CB_GETITEMDATA, i, 0)) == file) {
            SendMessageA(pl.regionBox, CB_SETCURSEL, i, 0);
            SendMessageA(pl.panel, WM_COMMAND, MAKEWPARAM(ID_PL_REGION, CBN_SELCHANGE), (LPARAM)pl.regionBox);
        }
}
void scope(bool all) {
    HWND inRegion = GetDlgItem(pl.panel, ID_PL_SCOPE_REGION);
    SendMessageA(inRegion, BM_SETCHECK, all ? BST_UNCHECKED : BST_CHECKED, 0);
    SendMessageA(pl.scopeAll, BM_SETCHECK, all ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(pl.panel, WM_COMMAND, MAKEWPARAM(all ? ID_PL_SCOPE_ALL : ID_PL_SCOPE_REGION, BN_CLICKED), (LPARAM)(all ? pl.scopeAll : inRegion));
}
void header(int col) {
    NMLISTVIEW nm = {};
    nm.hdr.hwndFrom = pl.list;
    nm.hdr.idFrom = ID_PL_LIST;
    nm.hdr.code = LVN_COLUMNCLICK;
    nm.iItem = -1;
    nm.iSubItem = col;
    SendMessageA(pl.panel, WM_NOTIFY, ID_PL_LIST, (LPARAM)&nm);
}
void clickRows(const std::vector<int>& rows) {  // as a click / Ctrl+click: the list's own selection changes, which sends LVN_ITEMCHANGED
    ListView_SetItemState(pl.list, -1, 0, LVIS_SELECTED);
    for (int r : rows) ListView_SetItemState(pl.list, r, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}
void doubleClick(int row) {
    NMITEMACTIVATE nm = {};
    nm.hdr.hwndFrom = pl.list;
    nm.hdr.idFrom = ID_PL_LIST;
    nm.hdr.code = NM_DBLCLK;
    nm.iItem = row;
    SendMessageA(pl.panel, WM_NOTIFY, ID_PL_LIST, (LPARAM)&nm);
}
bool key(HWND target, WPARAM vk, bool shift = false) {
    BYTE ks[256];
    GetKeyboardState(ks);
    const BYTE oldShift = ks[VK_SHIFT];
    ks[VK_SHIFT] = shift ? 0x80 : 0;
    SetKeyboardState(ks);
    MSG m = {};
    m.hwnd = target;
    m.message = WM_KEYDOWN;
    m.wParam = vk;
    const bool handled = plHandleKey(m);
    ks[VK_SHIFT] = oldShift;
    SetKeyboardState(ks);
    return handled;
}
std::string text(HWND w) {
    char b[512] = "";
    GetWindowTextA(w, b, sizeof b);
    return b;
}
std::vector<int> listSelection() {
    std::vector<int> rows;
    for (int r = ListView_GetNextItem(pl.list, -1, LVNI_SELECTED); r >= 0; r = ListView_GetNextItem(pl.list, r, LVNI_SELECTED)) rows.push_back(r);
    return rows;
}
bool allScope() { return SendMessageA(pl.scopeAll, BM_GETCHECK, 0, 0) == BST_CHECKED; }
}  // namespace stui

// Everything that must hold whatever happened before.
void stInvariants(SelfTest& t, const std::string& when) {
    plSyncFromSelection();  // the main loop does this every frame, before the user can look at the list
    const std::string q = toLower(trim(stui::text(pl.search)));
    const bool numeric = !q.empty() && std::all_of(q.begin(), q.end(), [](char c) { return c >= '0' && c <= '9'; });
    const int num = numeric ? std::atoi(q.c_str()) : -1;
    const bool all = stui::allScope();
    // the expected rows, computed here independently of plRefill
    std::vector<int> want;
    for (size_t i = 0; i < app.instances.size(); ++i) {
        if (!all && app.instFile[i] != pl.region) continue;
        const IplInstance& o = app.instances[i];
        if (!q.empty() && !(numeric && o.id == num) && toLower(o.model).find(q) == std::string::npos) continue;
        want.push_back(int(i));
    }
    std::vector<int> got = pl.rows;
    for (int r : got) ST_CHECK(t, r >= 0 && r < int(app.instances.size()), when + ": a row points outside the placements");
    std::vector<int> a = want, b = got;
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    ST_CHECK(t, a == b, when + ": the rows are not exactly the placements that match (" + std::to_string(got.size()) + " shown, " + std::to_string(want.size()) + " expected)");
    if (pl.sortCol < 0) ST_CHECK(t, got == want, when + ": without sorting the rows must be in file order");
    ST_CHECK(t, ListView_GetItemCount(pl.list) == int(got.size()), when + ": the list's item count differs from the rows");
    // sorted correctly
    if (pl.sortCol >= 1 && pl.sortCol <= 5) {
        for (size_t k = 1; k < got.size(); ++k) {
            const IplInstance &p = app.instances[got[k - 1]], &c = app.instances[got[k]];
            double kp = 0, kc = 0;
            switch (pl.sortCol) {
                case 1: kp = p.id; kc = c.id; break;
                case 2: kp = p.x; kc = c.x; break;
                case 3: kp = p.y; kc = c.y; break;
                case 4: kp = p.z; kc = c.z; break;
                case 5: kp = app.instFile[got[k - 1]]; kc = app.instFile[got[k]]; break;
            }
            if (pl.sortDesc ? kp < kc : kp > kc) {
                ST_CHECK(t, false, when + ": rows are not sorted by column " + std::to_string(pl.sortCol));
                break;
            }
        }
    }
    // the selection: valid, and highlighted in the list exactly where it is listed
    for (int idx : app.selected) ST_CHECK(t, idx >= 0 && idx < int(app.instances.size()), when + ": the selection points outside the placements");
    plSyncFromSelection();  // what the main loop does every frame
    std::vector<int> wantRows;
    for (int idx : app.selected) {
        auto it = std::find(got.begin(), got.end(), idx);
        if (it != got.end()) wantRows.push_back(int(it - got.begin()));
    }
    std::sort(wantRows.begin(), wantRows.end());
    ST_CHECK(t, stui::listSelection() == wantRows, when + ": the highlighted rows differ from the selection");
    // the region picker holds every region, with the right count
    {
        int listed = 0;
        for (const Region& r : regions) listed += regionListed(r.file);
        ST_CHECK(t, int(SendMessageA(pl.regionBox, CB_GETCOUNT, 0, 0)) == listed, when + ": the region picker does not list every listed region");
        ST_CHECK(t, !scene.list || int(sceneRows.size()) == listed, when + ": the Scene list does not show every listed region");
    }
    const std::string status = stui::text(pl.count);
    ST_CHECK(t, !status.empty(), when + ": the status line is empty");
    if (!got.empty()) ST_CHECK(t, status.find(std::to_string(got.size()) + " of ") == 0, when + ": the status line does not say how many are shown");
}

// ---- self-test of the properties panel (runs inside --selftest) ------------------------------------------------------

namespace stinsp {
void type(int field, const std::string& text) {  // type into a box (field 0..5, 6 = model ID) and press Enter
    HWND e = field < 6 ? insp.field[field] : insp.idBox;
    SetFocus(e);
    SetWindowTextA(e, text.c_str());
    MSG m = {};
    m.hwnd = e;
    m.message = WM_KEYDOWN;
    m.wParam = VK_RETURN;
    inspHandleKey(m);
}
void click(int id) { SendMessageA(app.inspector, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)GetDlgItem(app.inspector, id)); }
int nudgeId(int field, int k) { return ID_INS_NUDGE + field * 4 + k; }
std::string box(int field) {
    char b[64] = "";
    GetWindowTextA(field < 6 ? insp.field[field] : insp.idBox, b, sizeof b);
    return b;
}
// Hold a nudge button down for `ms` milliseconds with real mouse messages, letting its timer run.
void hold(int id, DWORD ms) {
    HWND b = GetDlgItem(app.inspector, id);
    insp.testHolding = true;  // the button checks that the mouse button is still down
    SendMessageA(b, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(5, 5));
    const DWORD start = GetTickCount();
    MSG m;
    while (GetTickCount() - start < ms)
        while (PeekMessageA(&m, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageA(&m);
        }
    insp.testHolding = false;
    SendMessageA(b, WM_LBUTTONUP, 0, MAKELPARAM(5, 5));
}
double angle(int idx, int axis) {
    double e[3];
    const IplInstance& o = app.instances[idx];
    quatToEulerDeg(o.rotX, o.rotY, o.rotZ, o.rotW, app.conjugate, e[0], e[1], e[2]);
    return e[axis];
}
double angleDiff(double a, double b) {  // a - b, folded into (-180, 180]
    double d = std::fmod(a - b, 360.0);
    if (d > 180) d -= 360;
    if (d <= -180) d += 360;
    return d;
}
}  // namespace stinsp

// What must hold for the panel whatever happened.
void stInspectorInvariants(SelfTest& t, const std::string& when) {
    for (size_t i = 0; i < app.instances.size(); ++i) {
        const IplInstance& o = app.instances[i];
        const double n = o.rotX * o.rotX + o.rotY * o.rotY + o.rotZ * o.rotZ + o.rotW * o.rotW;
        // the shipped files round quaternions to 6 decimals, so |q|^2 is off by up to about 1.3e-6 there
        if (std::fabs(n - 1) > 1e-5 || !std::isfinite(o.x) || !std::isfinite(o.y) || !std::isfinite(o.z)) {
            ST_CHECK(t, false, when + ": placement " + std::to_string(i) + " has a broken position or rotation");
            break;
        }
    }
    insp.shownText[0] = "\x01";  // make the next refresh write every box (no box has the focus in the test)
    for (std::string& s : insp.shownText) s = "\x01";
    SetFocus(app.hwnd);
    refreshInspector();
    if (app.selected.empty()) {
        for (int f = 0; f < 7; ++f) ST_CHECK(t, stinsp::box(f).empty(), when + ": with nothing selected the boxes are empty");
        return;
    }
    double v[6];
    fieldValues(app.instances[app.selected[0]], v);
    for (int f = 0; f < 6; ++f) ST_CHECK(t, stinsp::box(f) == fieldText(f, v[f]), when + ": box " + std::to_string(f) + " does not show the first selected object");
    ST_CHECK(t, stinsp::box(6) == std::to_string(app.instances[app.selected[0]].id), when + ": the ID box does not show the model ID");
}

void runInspectorSelfTest(SelfTest& t) {
    insp.quiet = true;
    auto fileOf = [](const std::string& name) {
        for (const Region& r : regions) if (toLower(r.name) == toLower(name)) return r.file;
        return -1;
    };
    // a cone of props, nothing else selected
    int cone = -1, hospital = -1, lodHospital = -1, coneId = -1, barrierId = -1;
    for (size_t i = 0; i < app.instances.size(); ++i) {
        if (cone < 0 && app.instances[i].model == "trafficcone" && app.instFile[i] == fileOf("props.IPL")) cone = int(i);
        if (hospital < 0 && app.instances[i].model == "hospitaland") hospital = int(i);
    }
    for (const auto& kv : app.defs) {
        if (kv.second.model == "trafficcone") coneId = kv.first;
        if (kv.second.model == "bar_barrier10") barrierId = kv.first;
    }
    ST_CHECK(t, cone >= 0 && hospital >= 0 && coneId >= 0 && barrierId >= 0, "the test needs a cone, hospitaland and bar_barrier10");
    if (hospital >= 0) {
        const std::vector<size_t> p = findLodPartners(app.instances, size_t(hospital));
        if (!p.empty()) lodHospital = int(p[0]);
    }
    ST_CHECK(t, lodHospital >= 0, "hospitaland has its LOD partner");

    // 1. nothing selected
    app.selected.clear();
    stInspectorInvariants(t, "nothing selected");

    // 2. one object: the boxes show it; typed position goes exactly there; only changed boxes act
    app.selected.assign(1, cone);
    app.history.endSession();
    stInspectorInvariants(t, "a cone selected");
    const double y0 = app.instances[cone].y, z0 = app.instances[cone].z;
    stinsp::type(0, "1234.5678");
    ST_CHECK(t, app.instances[cone].x == 1234.5678, "typed X goes exactly there");
    ST_CHECK(t, app.instances[cone].y == y0 && app.instances[cone].z == z0, "untouched boxes do not move it (no rounding drift)");
    stInspectorInvariants(t, "typed X");
    stinsp::type(2, "-12.25");
    ST_CHECK(t, app.instances[cone].z == -12.25, "typed Z goes exactly there");
    // bad input: a warning, nothing changes, the box shows the stored value again
    const double x1 = app.instances[cone].x;
    insp.lastMessage.clear();
    stinsp::type(0, "abc");
    ST_CHECK(t, app.instances[cone].x == x1 && !insp.lastMessage.empty(), "a word in X: warning, nothing moves");
    stinsp::type(0, "");
    ST_CHECK(t, app.instances[cone].x == x1, "an empty X box moves nothing");
    stinsp::type(0, "1e400");
    ST_CHECK(t, app.instances[cone].x == x1, "an infinite number is refused");
    stinsp::type(0, "5,5");
    ST_CHECK(t, app.instances[cone].x == x1, "a comma is refused (not silently read as 5)");
    stInspectorInvariants(t, "bad input");
    // Esc puts the stored value back without changing anything
    SetFocus(insp.field[1]);
    SetWindowTextA(insp.field[1], "999");
    {
        MSG m = {};
        m.hwnd = insp.field[1];
        m.message = WM_KEYDOWN;
        m.wParam = VK_ESCAPE;
        inspHandleKey(m);
    }
    ST_CHECK(t, stinsp::box(1) == insp.shownText[1] && app.instances[cone].y == y0, "Esc restores the box and changes nothing");
    // typing is not overwritten by the per-frame refresh
    SetFocus(insp.field[0]);
    SetWindowTextA(insp.field[0], "77");
    refreshInspector();
    ST_CHECK(t, stinsp::box(0) == "77", "the refresh does not overwrite what is being typed");
    SetWindowTextA(insp.field[0], insp.shownText[0].c_str());
    SetFocus(app.hwnd);

    // 3. nudges: every button of every position axis, exact steps
    for (int f = 0; f < 3; ++f)
        for (int k = 0; k < 4; ++k) {
            const double before = f == 0 ? app.instances[cone].x : f == 1 ? app.instances[cone].y : app.instances[cone].z;
            stinsp::click(stinsp::nudgeId(f, k));
            const double after = f == 0 ? app.instances[cone].x : f == 1 ? app.instances[cone].y : app.instances[cone].z;
            ST_CHECK(t, std::fabs((after - before) - kPosSteps[k]) < 1e-9, "position button " + std::to_string(f) + "/" + std::to_string(k) + " moves by its step");
        }
    stInspectorInvariants(t, "position nudges");

    // 4. rotation: typed angles, nudges, Rotate -90 / +90, Reset
    stinsp::type(5, "45");
    ST_CHECK(t, std::fabs(stinsp::angle(cone, 2) - 45) < 1e-9 && std::fabs(stinsp::angle(cone, 0)) < 1e-9, "typed heading 45 (and no tilt)");
    stInspectorInvariants(t, "typed heading");
    for (int f = 3; f < 6; ++f)
        for (int k = 0; k < 4; ++k) {
            const double before = stinsp::angle(cone, f - 3);
            stinsp::click(stinsp::nudgeId(f, k));
            const double after = stinsp::angle(cone, f - 3);
            ST_CHECK(t, std::fabs(stinsp::angleDiff(after, before) - kRotSteps[k]) < 1e-7, "rotation button " + std::to_string(f) + "/" + std::to_string(k) + " turns by its step");
        }
    stInspectorInvariants(t, "rotation nudges");
    stinsp::click(ID_INS_RESET);
    const IplInstance& c = app.instances[cone];
    ST_CHECK(t, c.rotX == 0 && c.rotY == 0 && c.rotZ == 0 && c.rotW == 1, "Reset gives no rotation");
    stinsp::click(ID_INS_ROTP90);
    ST_CHECK(t, std::fabs(stinsp::angle(cone, 2) - 90) < 1e-9, "Rotate +90 from none gives heading 90");
    stinsp::click(ID_INS_ROTM90);
    stinsp::click(ID_INS_ROTM90);
    ST_CHECK(t, std::fabs(stinsp::angle(cone, 2) + 90) < 1e-9, "Rotate -90 twice gives heading -90");
    stinsp::type(5, "180");
    stinsp::click(stinsp::nudgeId(5, 3));
    ST_CHECK(t, std::fabs(stinsp::angle(cone, 2) + 170) < 1e-7, "180 + 10 wraps to -170");
    stinsp::type(3, "30");
    stinsp::type(4, "-20");
    ST_CHECK(t, std::fabs(stinsp::angle(cone, 0) - 30) < 1e-7 && std::fabs(stinsp::angle(cone, 1) + 20) < 1e-7 && std::fabs(stinsp::angle(cone, 2) + 170) < 1e-7,
             "typed tilts keep the heading");
    stinsp::type(4, "89.9");
    stinsp::click(stinsp::nudgeId(4, 3));
    stInspectorInvariants(t, "tilt past 90 (gimbal) stays a valid rotation");
    stinsp::click(ID_INS_RESET);

    // 5. undo: a run of nudges on one selection is one undo step; typed values are their own steps
    {
        app.selected.assign(1, cone);
        app.history.endSession();
        const double xs = app.instances[cone].x;
        const size_t undoBefore = app.history.undoCount();
        for (int k = 0; k < 5; ++k) stinsp::click(stinsp::nudgeId(0, 2));
        ST_CHECK(t, app.history.undoCount() == std::min(undoBefore + 1, kUndoLimit), "five nudges in a row are one undo step");
        undoAction();
        ST_CHECK(t, app.instances[cone].x == xs, "undo puts the object back before the nudges");
        app.selected.assign(1, cone);
        app.history.endSession();
        stinsp::type(0, "10");
        stinsp::type(0, "20");
        undoAction();
        ST_CHECK(t, app.instances[cone].x == 10, "each typed value is its own undo step");
        redoAction();
        ST_CHECK(t, app.instances[cone].x == 20, "redo brings it back");
    }

    // 6. holding a button repeats (real mouse down and up, the button's own timer)
    {
        app.selected.assign(1, cone);
        app.history.endSession();
        const double xs = app.instances[cone].x;
        const size_t undoBefore = app.history.undoCount();
        stinsp::hold(stinsp::nudgeId(0, 2), 1200);
        const double moved = app.instances[cone].x - xs;
        const double steps = moved / 0.1;
        ST_CHECK(t, steps >= 2 && std::fabs(steps - std::round(steps)) < 1e-6, "holding > repeats whole steps ("  /* at least the press and one repeat: a busy PC delays the timer */ + num(steps, 2) + " steps)");
        ST_CHECK(t, app.history.undoCount() == std::min(undoBefore + 1, kUndoLimit), "a held button is one undo step");
        const double after = app.instances[cone].x;
        stinsp::click(stinsp::nudgeId(0, 2));
        ST_CHECK(t, std::fabs(app.instances[cone].x - after - 0.1) < 1e-9, "after a hold, a click is one step again (no stuck state)");
    }

    // 7. several objects: buttons move and turn them all; typed X moves the group so the first lands there
    {
        int other = -1;
        for (size_t i = 0; i < app.instances.size(); ++i)
            if (int(i) != cone && app.instances[i].model == "trafficcone") { other = int(i); break; }
        app.selected = {cone, other};
        app.history.endSession();
        const double dx = app.instances[other].x - app.instances[cone].x, oy = app.instances[other].y;
        stinsp::click(stinsp::nudgeId(1, 3));
        ST_CHECK(t, std::fabs(app.instances[other].y - oy - 10) < 1e-9, "a button moves every selected object");
        stinsp::type(0, "500");
        ST_CHECK(t, app.instances[cone].x == 500 && std::fabs(app.instances[other].x - (500 + dx)) < 1e-9, "typed X: the first goes there, the others keep their offset");
        stinsp::type(5, "33");
        ST_CHECK(t, std::fabs(stinsp::angle(cone, 2) - 33) < 1e-9 && std::fabs(stinsp::angle(other, 2) - 33) < 1e-9, "typed heading applies to every selected object");
        stInspectorInvariants(t, "two selected");
    }

    // 8. a building and its LOD stand-in move and turn together (panel, keys and drag all use the same rule)
    if (hospital >= 0 && lodHospital >= 0) {
        app.selected.assign(1, hospital);
        app.history.endSession();
        const double lx = app.instances[lodHospital].x;
        stinsp::click(stinsp::nudgeId(0, 3));
        ST_CHECK(t, std::fabs(app.instances[lodHospital].x - lx - 10) < 1e-9, "the LOD stand-in moves with its building");
        stinsp::type(5, "12");
        ST_CHECK(t, std::fabs(stinsp::angle(lodHospital, 2) - 12) < 1e-9, "the LOD stand-in turns with its building");
        moveSelected(1, 0, 0);
        ST_CHECK(t, std::fabs(app.instances[lodHospital].x - lx - 11) < 1e-9, "Alt+drag / Alt+keys move the LOD stand-in too");
        ST_CHECK(t, findLodPartners(app.instances, size_t(hospital)).size() == 1, "after moving, the pair is still a pair");
    }

    // 9. the model ID: typed, chosen, unknown
    {
        app.selected.assign(1, cone);
        app.history.endSession();
        stinsp::type(6, std::to_string(barrierId));
        ST_CHECK(t, app.instances[cone].id == barrierId && toLower(app.instances[cone].model) == "bar_barrier10", "typing a model ID replaces the model and its name");
        stInspectorInvariants(t, "replaced model");
        insp.lastMessage.clear();
        stinsp::type(6, "999999");
        ST_CHECK(t, app.instances[cone].id == barrierId && !insp.lastMessage.empty(), "an unknown ID is refused with a warning");
        stinsp::type(6, "12x");
        ST_CHECK(t, app.instances[cone].id == barrierId, "a non-number ID is refused");
        undoAction();
        ST_CHECK(t, app.instances[cone].id == coneId, "undo gives the old model back");
        stInspectorInvariants(t, "ID undo");
    }

    // 10. random use of everything in the panel
    unsigned seed = 4242u + (std::getenv("MED_SELFTEST_SEED") ? unsigned(std::atoi(std::getenv("MED_SELFTEST_SEED"))) : 0u);
    auto rnd = [&](int n) { seed = seed * 1664525u + 1013904223u; return n <= 0 ? 0 : int((seed >> 8) % unsigned(n)); };
    const char* typed[] = {"0", "10.5", "-3", "180", "-180", "359", "1e3", "abc", "", "0.0001", "-0.0001", "90"};
    for (int step = 0; step < 1500; ++step) {
        const int op = rnd(9);
        if (rnd(10) == 0 || app.selected.empty()) {  // change the selection now and then
            const int a = rnd(int(app.instances.size())), b = rnd(int(app.instances.size()));
            app.selected = rnd(3) == 0 ? std::vector<int>{a, b} : std::vector<int>{a};
            if (app.selected.size() == 2 && a == b) app.selected.pop_back();
            app.history.endSession();
        }
        std::string what;
        switch (op) {
            case 0: what = "nudge"; stinsp::click(stinsp::nudgeId(rnd(6), rnd(4))); break;
            case 1: what = "type"; stinsp::type(rnd(6), typed[rnd(12)]); break;
            case 2: what = "rotate 90"; stinsp::click(rnd(2) ? ID_INS_ROTP90 : ID_INS_ROTM90); break;
            case 3: what = "reset"; stinsp::click(ID_INS_RESET); break;
            case 4: what = "model id"; stinsp::type(6, std::to_string(rnd(2) ? coneId : barrierId)); break;
            case 5: what = "undo"; undoAction(); break;
            case 6: what = "redo"; redoAction(); break;
            case 7: what = "duplicate"; duplicateSelected(); break;
            case 8: what = "delete below the map"; sinkSelected(); break;
        }
        const int failuresBefore = t.failures;
        stInspectorInvariants(t, "panel random step " + std::to_string(step) + " (" + what + ")");
        stInvariants(t, "panel random step " + std::to_string(step) + " (" + what + ", list)");
        if (t.failures > failuresBefore + 20) break;
    }
    insp.quiet = false;
}

// ---- self-test of the Item tab (runs inside --selftest) ---------------------------------------------------------------

namespace stitem {
void search(const std::string& text) { SetWindowTextA(it.search, text.c_str()); }
void show(int mode) {
    SendMessageA(it.show, CB_SETCURSEL, mode, 0);
    SendMessageA(it.panel, WM_COMMAND, MAKEWPARAM(ID_IT_SHOW, CBN_SELCHANGE), (LPARAM)it.show);
}
void header(int col) {
    NMLISTVIEW nm = {};
    nm.hdr.hwndFrom = it.list;
    nm.hdr.idFrom = ID_IT_LIST;
    nm.hdr.code = LVN_COLUMNCLICK;
    nm.iItem = -1;
    nm.iSubItem = col;
    SendMessageA(it.panel, WM_NOTIFY, ID_IT_LIST, (LPARAM)&nm);
}
void pick(int row) {  // a click on a row
    ListView_SetItemState(it.list, -1, 0, LVIS_SELECTED);
    if (row >= 0) ListView_SetItemState(it.list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}
void doubleClick(int row) {
    NMITEMACTIVATE nm = {};
    nm.hdr.hwndFrom = it.list;
    nm.hdr.idFrom = ID_IT_LIST;
    nm.hdr.code = NM_DBLCLK;
    nm.iItem = row;
    SendMessageA(it.panel, WM_NOTIFY, ID_IT_LIST, (LPARAM)&nm);
}
void button(int id) { SendMessageA(it.panel, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)GetDlgItem(it.panel, id)); }
int rowOf(int id) {
    auto f = std::find(it.rows.begin(), it.rows.end(), id);
    return f == it.rows.end() ? -1 : int(f - it.rows.begin());
}
}  // namespace stitem

void stItemInvariants(SelfTest& t, const std::string& when) {
    // counts computed here independently
    std::unordered_map<int, int> placed;
    for (const IplInstance& o : app.instances) ++placed[o.id];
    for (const auto& kv : app.defs) {
        const int a = placed.count(kv.first) ? placed[kv.first] : 0;
        if (a != itPlacedCount(kv.first)) {
            ST_CHECK(t, false, when + ": the Placed count of model " + std::to_string(kv.first) + " is wrong (" + std::to_string(itPlacedCount(kv.first)) + " instead of " + std::to_string(a) + ")");
            break;
        }
    }
    char b[128] = "";
    GetWindowTextA(it.search, b, sizeof b);
    const std::string q = toLower(trim(b));
    const bool numeric = !q.empty() && std::all_of(q.begin(), q.end(), [](char c) { return c >= '0' && c <= '9'; });
    const int num = numeric ? std::atoi(q.c_str()) : -1;
    const int mode = int(SendMessageA(it.show, CB_GETCURSEL, 0, 0));
    std::vector<int> want;
    for (const auto& kv : app.defs) {
        const ModelDef& d = kv.second;
        if (!q.empty() && !(numeric && kv.first == num) && d.model.find(q) == std::string::npos && d.txd.find(q) == std::string::npos) continue;
        const int n = placed.count(kv.first) ? placed[kv.first] : 0;
        if ((mode == 1 && n == 0) || (mode == 2 && n > 0)) continue;
        want.push_back(kv.first);
    }
    std::vector<int> a = want, g = it.rows;
    std::sort(a.begin(), a.end());
    std::sort(g.begin(), g.end());
    ST_CHECK(t, a == g, when + ": the item rows are not exactly the matching definitions (" + std::to_string(it.rows.size()) + " shown, " + std::to_string(want.size()) + " expected)");
    ST_CHECK(t, ListView_GetItemCount(it.list) == int(it.rows.size()), when + ": the item list's count differs from its rows");
    for (size_t k = 1; k < it.rows.size(); ++k) {
        const int p = it.rows[k - 1], c = it.rows[k];
        const ModelDef &dp = app.defs[p], &dc = app.defs[c];
        bool bad = false;
        switch (it.sortCol) {
            case 0: bad = it.sortDesc ? dp.model < dc.model : dc.model < dp.model; break;
            case 1: bad = it.sortDesc ? p < c : c < p; break;
            case 2: bad = it.sortDesc ? itPlacedCount(p) < itPlacedCount(c) : itPlacedCount(c) < itPlacedCount(p); break;
            case 3: bad = it.sortDesc ? std::string(poolShortName(dp.pool)) < poolShortName(dc.pool) : std::string(poolShortName(dc.pool)) < poolShortName(dp.pool); break;
            case 4: bad = it.sortDesc ? dp.txd < dc.txd : dc.txd < dp.txd; break;
            case 5: bad = it.sortDesc ? dp.drawDistance < dc.drawDistance : dc.drawDistance < dp.drawDistance; break;
            case 6: bad = it.sortDesc ? toLower(dp.ide) < toLower(dc.ide) : toLower(dc.ide) < toLower(dp.ide); break;
        }
        if (bad) {
            ST_CHECK(t, false, when + ": the items are not sorted by column " + std::to_string(it.sortCol));
            break;
        }
    }
    // at most one row picked, and it is a listed row
    int picked = 0;
    for (int r = ListView_GetNextItem(it.list, -1, LVNI_SELECTED); r >= 0; r = ListView_GetNextItem(it.list, r, LVNI_SELECTED)) ++picked;
    ST_CHECK(t, picked <= 1, when + ": more than one item picked");
    char st[256] = "";
    GetWindowTextA(it.count, st, sizeof st);
    if (!it.rows.empty()) ST_CHECK(t, std::string(st).find(std::to_string(it.rows.size()) + " of ") == 0, when + ": the item status line is wrong");
}

void runItemSelfTest(SelfTest& t) {
    insp.quiet = true;
    showLeftTab(2);
    stitem::search("");
    stitem::show(0);
    stItemInvariants(t, "item start");
    ST_CHECK(t, it.rows.size() == app.defs.size(), "every definition is listed at first (" + std::to_string(it.rows.size()) + " of " + std::to_string(app.defs.size()) + ")");

    int coneId = -1;
    for (const auto& kv : app.defs) if (kv.second.model == "trafficcone") coneId = kv.first;
    ST_CHECK(t, coneId >= 0, "the test needs trafficcone");
    ST_CHECK(t, app.defs[coneId].pool == Pool::Dummy && std::string(poolShortName(app.defs[coneId].pool)) == "Dummy", "a cone (object.dat) is a Dummy");
    ST_CHECK(t, !app.defs[coneId].ide.empty(), "the IDE file of a definition is known");

    // searches
    for (const char* q : {"cone", "CONE", "  cone ", "dyncones", "1360", "zzzz_nothing", "99999", "%s\\'\"", "lod", "a"}) {
        stitem::search(q);
        stItemInvariants(t, std::string("item search '") + q + "'");
    }
    stitem::search("dyncones");
    ST_CHECK(t, stitem::rowOf(coneId) >= 0, "a texture dictionary name finds the cone");
    stitem::search("1360");
    ST_CHECK(t, stitem::rowOf(coneId) >= 0, "the ID finds the cone");
    stitem::search(std::string(300, 'q'));
    ST_CHECK(t, it.rows.empty(), "a very long search finds nothing and does not crash");
    stitem::search("");

    // show modes
    stitem::show(1);
    stItemInvariants(t, "placed only");
    const size_t placedKinds = it.rows.size();
    stitem::show(2);
    stItemInvariants(t, "not placed only");
    ST_CHECK(t, placedKinds + it.rows.size() == app.defs.size(), "placed + not placed = every definition");
    stitem::show(0);

    // sorting, every column both ways
    for (int col = 0; col < 7; ++col)
        for (int twice = 0; twice < 2; ++twice) {
            stitem::header(col);
            stItemInvariants(t, "item sort column " + std::to_string(col) + (twice ? " descending" : " ascending"));
        }
    stitem::header(0);
    if (it.sortDesc) stitem::header(0);

    // Show where it is placed (button, double-click, Enter)
    stitem::search("trafficcone");
    stitem::pick(stitem::rowOf(coneId));
    stitem::button(ID_IT_WHERE);
    ST_CHECK(t, TabCtrl_GetCurSel(app.leftTabs) == 1 && stui::allScope(), "Show where it is placed opens the Placements tab in All regions");
    {
        int n = 0;
        for (int r : pl.rows) n += app.instances[r].id == coneId;
        ST_CHECK(t, n == itPlacedCount(coneId) && int(pl.rows.size()) == n, "it lists exactly the placements of that model (" + std::to_string(pl.rows.size()) + ")");
    }
    stInvariants(t, "after Show where it is placed");
    showLeftTab(2);
    stitem::doubleClick(stitem::rowOf(coneId));
    ST_CHECK(t, TabCtrl_GetCurSel(app.leftTabs) == 1, "a double-click on an item does the same");
    showLeftTab(2);
    stitem::doubleClick(-1);
    stitem::doubleClick(100000);
    ST_CHECK(t, TabCtrl_GetCurSel(app.leftTabs) == 2, "a double-click outside the rows does nothing");
    stitem::pick(-1);
    insp.lastMessage.clear();
    stitem::button(ID_IT_WHERE);
    ST_CHECK(t, !insp.lastMessage.empty(), "Show where it is placed with nothing picked asks to pick an item");
    insp.lastMessage.clear();
    stitem::button(ID_IT_ADD);
    ST_CHECK(t, !insp.lastMessage.empty(), "Add to map with nothing picked asks to pick an item");

    // Add to map: into the region picked in the Placements tab; the count follows
    {
        int props = -1;
        for (const Region& r : regions) if (toLower(r.name) == "props.ipl") props = r.file;
        stui::scope(false);
        stui::region(props);
        showLeftTab(2);
        stitem::search("trafficcone");
        stitem::pick(stitem::rowOf(coneId));
        const int before = itPlacedCount(coneId);
        const size_t total = app.instances.size();
        stitem::button(ID_IT_ADD);
        ST_CHECK(t, app.instances.size() == total + 1 && itPlacedCount(coneId) == before + 1, "Add to map adds one placement of that item");
        ST_CHECK(t, app.instFile.back() == props && app.instances.back().id == coneId, "into the region picked in the Placements tab");
        ST_CHECK(t, app.selected == std::vector<int>{int(app.instances.size()) - 1}, "and selects it");
        ST_CHECK(t, itSelectedId() == coneId, "the item stays picked in the list after adding");
        stItemInvariants(t, "after Add to map");
        undoAction();
        ST_CHECK(t, itPlacedCount(coneId) == before, "undo takes the count back");
        stItemInvariants(t, "after undo of Add to map");
    }

    // a selection in 3D highlights its definition
    {
        stitem::search("");
        int barrier = -1;
        for (size_t i = 0; i < app.instances.size(); ++i) if (app.instances[i].model == "bar_barrier10") { barrier = int(i); break; }
        if (barrier < 0) barrier = 0;  // earlier random edits may have replaced every barrier: any placement will do
        app.selected.clear();          // a NEW selection (the same one again would rightly leave the list alone)
        itSyncFromSelection();
        app.selected.assign(1, barrier);
        itSyncFromSelection();
        ST_CHECK(t, itSelectedId() == app.instances[barrier].id, "selecting in 3D highlights the item");
        stItemInvariants(t, "3D selection");
    }

    // keys: Enter in the search box goes to the list, Enter in the list shows the placements, Esc clears
    {
        showLeftTab(2);
        stitem::search("trafficcone");
        MSG m = {};
        m.message = WM_KEYDOWN;
        m.hwnd = it.search;
        m.wParam = VK_RETURN;
        itHandleKey(m);
        ST_CHECK(t, itSelectedId() == it.rows[0], "Enter in the search box picks the first item");
        m.hwnd = it.list;
        itHandleKey(m);
        ST_CHECK(t, TabCtrl_GetCurSel(app.leftTabs) == 1, "Enter in the list shows where it is placed");
        showLeftTab(2);
        m.hwnd = it.search;
        m.wParam = VK_ESCAPE;
        itHandleKey(m);
        char b[64] = "";
        GetWindowTextA(it.search, b, sizeof b);
        ST_CHECK(t, b[0] == 0, "Esc clears the item search");
    }

    // random use, with the edits that change the counts
    unsigned seed = 777u + (std::getenv("MED_SELFTEST_SEED") ? unsigned(std::atoi(std::getenv("MED_SELFTEST_SEED"))) : 0u);
    auto rnd = [&](int n) { seed = seed * 22695477u + 1u; return n <= 0 ? 0 : int((seed >> 8) % unsigned(n)); };
    const char* terms[] = {"", "cone", "lod", "1360", "tree", "x", "dyncones", "zzz", "938", "a"};
    for (int step = 0; step < 1200; ++step) {
        const int op = rnd(12);
        std::string what;
        switch (op) {
            case 0: what = "search"; stitem::search(terms[rnd(10)]); break;
            case 1: what = "show"; stitem::show(rnd(3)); break;
            case 2: what = "sort"; stitem::header(rnd(7)); break;
            case 3: what = "pick"; stitem::pick(it.rows.empty() ? -1 : rnd(int(it.rows.size()))); break;
            case 4: what = "add to map"; if (pl.region >= 0 && itSelectedId() >= 0) stitem::button(ID_IT_ADD); break;
            case 5: what = "show where"; if (itSelectedId() >= 0) stitem::button(ID_IT_WHERE); showLeftTab(2); break;
            case 6: what = "select in 3D"; if (!app.instances.empty()) app.selected.assign(1, rnd(int(app.instances.size()))); itSyncFromSelection(); break;
            case 7: what = "undo"; undoAction(); break;
            case 8: what = "redo"; redoAction(); break;
            case 9: what = "duplicate"; duplicateSelected(); break;
            case 10: what = "delete permanently"; deleteSelected(); break;
            case 11: what = "region"; stui::region(regions[rnd(int(regions.size()))].file); break;
        }
        const int failuresBefore = t.failures;
        stItemInvariants(t, "item random step " + std::to_string(step) + " (" + what + ")");
        stInvariants(t, "item random step " + std::to_string(step) + " (" + what + ", list)");
        if (t.failures > failuresBefore + 20) break;
    }
    insp.quiet = false;
}

// ---- self-test of the Bookmarks window (runs inside --selftest, on a temporary bookmarks file) ------------------------

void stBookmarkInvariants(SelfTest& t, const std::string& when) {
    bool ok;
    const std::string onDisk = slurp(app.bookmarkFile, ok);
    const std::vector<Bookmark> back = parseBookmarks(onDisk);
    bool same = ok && back.size() == app.bookmarks.size();
    for (size_t i = 0; same && i < back.size(); ++i) {
        const Bookmark &a = back[i], &b = app.bookmarks[i];
        auto near6 = [](double x, double y) { return std::fabs(x - y) <= 1e-5 * std::max(1.0, std::fabs(y)); };  // 6 significant digits on disk
        same = a.name == b.name && near6(a.x, b.x) && near6(a.y, b.y) && near6(a.z, b.z) && near6(a.h, b.h) && near6(a.yaw, b.yaw) && near6(a.pitch, b.pitch) && a.flag == b.flag;
    }
    ST_CHECK(t, same, when + ": the file on disk is not the list (" + std::to_string(back.size()) + " on disk, " + std::to_string(app.bookmarks.size()) + " in the list)");
    ST_CHECK(t, writeBookmarks(back) == onDisk, when + ": the file is not in MEd's exact format");
    if (bm.list) {
        ST_CHECK(t, ListView_GetItemCount(bm.list) == int(app.bookmarks.size()), when + ": the window's list count differs");
        for (int i = 0; i < ListView_GetItemCount(bm.list); ++i) {
            char b[300] = "";
            ListView_GetItemText(bm.list, i, 0, b, sizeof b);
            if (app.bookmarks[i].name != b) {
                ST_CHECK(t, false, when + ": row " + std::to_string(i) + " shows another name");
                break;
            }
        }
        const int sel = bmSelected();
        ST_CHECK(t, sel >= -1 && sel < int(app.bookmarks.size()), when + ": the selected row is outside the list");
    }
}

// Extract Object: copies only. Every file written must equal what the game holds, the map and the game's files must not
// change, and a selection gives each model once.
void runExtractSelfTest(SelfTest& t) {
    namespace fs = std::filesystem;
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    const std::string out = std::string(tmp) + "med_selftest_extract_" + std::to_string(GetCurrentProcessId());  // own folder per run (runs may go in parallel)
    std::error_code ec;
    fs::remove_all(out, ec);
    auto fingerprint = [](const std::string& p) {
        std::error_code e;
        return std::to_string(fs::file_size(p, e)) + "@" + std::to_string(fs::last_write_time(p, e).time_since_epoch().count());
    };
    std::vector<std::string> gameFiles = app.colFiles;
    gameFiles.push_back(app.img.path());
    std::vector<std::string> before;
    for (const std::string& f : gameFiles) before.push_back(fingerprint(f));
    auto mapState = [] {
        std::string s = std::to_string(app.instances.size()) + "/" + std::to_string(app.history.undoCount()) + "/" + std::to_string(app.dirty.size());
        for (const IplInstance& o : app.instances) s += std::to_string(o.id) + "," + std::to_string(o.x) + "," + std::to_string(o.y) + "," + std::to_string(o.z) + "," + std::to_string(o.rotW) + ";";
        return s;
    };
    const std::string mapBefore = mapState();

    // a spread of models: every 25th definition plus the ones the guide talks about
    std::vector<int> ids;
    int n = 0;
    for (const auto& kv : app.defs)
        if (n++ % 25 == 0) ids.push_back(kv.first);
    for (const auto& kv : app.defs)
        for (const char* name : {"trafficcone", "hospitaland", "lodpitaland"})
            if (kv.second.model == name) ids.push_back(kv.first);
    const std::vector<ExtractResult> results = extractModelsTo(out, ids);
    ST_CHECK(t, results.size() == ids.size(), "one result per model (" + std::to_string(ids.size()) + ")");
    int withCol = 0;
    for (size_t i = 0; i < results.size() && i < ids.size(); ++i) {
        const ModelDef& def = app.defs[ids[i]];
        const ExtractResult& r = results[i];
        const std::string dir = out + "/" + def.name;
        std::vector<unsigned char> want, got;
        std::string err;
        bool ok;
        const bool inImg = app.img.extract(def.name + ".dff", want, err);
        std::string bytes = inImg ? slurp(dir + "/" + def.name + ".dff", ok) : std::string();
        ST_CHECK(t, !inImg || std::string(want.begin(), want.end()) == bytes, def.name + ".dff is a byte copy of the archive entry");
        if (def.txd != "generic" && app.img.extract(def.txd + ".txd", want, err)) {
            bytes = slurp(dir + "/" + def.txd + ".txd", ok);
            ST_CHECK(t, ok && std::string(want.begin(), want.end()) == bytes, def.name + ": " + def.txd + ".txd is a byte copy");
        }
        bytes = slurp(dir + "/" + def.name + ".col", ok);
        if (ok) {
            ++withCol;
            std::vector<ColModel> models;
            ST_CHECK(t, parseCol(std::vector<unsigned char>(bytes.begin(), bytes.end()), models, err) && models.size() == 1 && toLower(models[0].name) == def.model,
                     def.name + ".col is a valid one-model COL file of that model");
            ST_CHECK(t, !r.colSource.empty(), def.name + ": the COL source is named");
        } else {
            ST_CHECK(t, r.colSource.empty() && !r.warnings.empty(), def.name + ": a missing collision is reported, not hidden");
        }
        const std::string info = slurp(dir + "/" + def.name + ".txt", ok);
        ST_CHECK(t, ok && info.find("IDE line: " + def.ideLine) != std::string::npos, def.name + ".txt holds the IDE line");
        ST_CHECK(t, extractTargetsThatExist(out, extractRequestFor(ids[i])).size() == r.written.size(), def.name + ": every file written is found again (overwrite question)");
    }
    ST_CHECK(t, withCol * 10 >= int(ids.size()) * 9, "nearly every model has its collision copied (" + std::to_string(withCol) + " of " + std::to_string(ids.size()) + ")");
    for (size_t i = 0; i < gameFiles.size(); ++i) ST_CHECK(t, fingerprint(gameFiles[i]) == before[i], "extracting does not touch " + gameFiles[i]);
    ST_CHECK(t, mapState() == mapBefore, "extracting does not change the map, the undo list or the unsaved files");

    // a selection: each model once, in order; bad indexes ignored
    int a = -1, b = -1;
    for (int i = 0; i < int(app.instances.size()) && b < 0; ++i) {
        if (a < 0) a = i;
        else if (app.instances[i].id != app.instances[a].id) b = i;
    }
    int a2 = -1;
    for (int i = 0; i < int(app.instances.size()); ++i)
        if (i != a && app.instances[i].id == app.instances[a].id) a2 = i;
    const std::vector<int> m = modelIdsOf({a, b, a2 >= 0 ? a2 : a, -5, int(app.instances.size()) + 3});
    ST_CHECK(t, m.size() == 2 && m[0] == app.instances[a].id && m[1] == app.instances[b].id, "a selection gives each model once");
    ST_CHECK(t, modelIdsOf({}).empty() && extractModelsTo(out, {-12345}).empty(), "nothing selected or an unknown id extracts nothing");
    fs::remove_all(out, ec);
}

// Replace Object: a building and its LOD stand-in are replaced as a pair; everything else stays exactly as it was, and
// one undo brings the whole map back.
std::string stMapState() {
    std::string s = std::to_string(app.instances.size()) + "|";
    char b[256];
    for (size_t i = 0; i < app.instances.size(); ++i) {
        const IplInstance& o = app.instances[i];
        std::snprintf(b, sizeof b, "%d,%s,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%d;", o.id, o.model.c_str(), o.x, o.y, o.z, o.rotX, o.rotY, o.rotZ, o.rotW, app.instFile[i]);
        s += b;
    }
    return s;
}

int stModelId(const char* name) {
    for (const auto& kv : app.defs)
        if (kv.second.model == toLower(name)) return kv.first;
    return -1;
}

int stPlacementOf(const char* name) {
    for (size_t i = 0; i < app.instances.size(); ++i)
        if (toLower(app.instances[i].model) == toLower(name)) return int(i);
    return -1;
}

void runReplaceSelfTest(SelfTest& t) {
    insp.quiet = true;
    const int supaSave = stModelId("SupaSave"), lodSupaSave = stModelId("LODaSave"), petfoodId = stModelId("petfoodfactory");
    ST_CHECK(t, supaSave >= 0 && lodSupaSave >= 0 && petfoodId >= 0, "the test models are defined");
    if (supaSave < 0 || lodSupaSave < 0 || petfoodId < 0) return;
    // the earlier sections edit the map at random, so pick the placements from what is there now: the hospital when it
    // is still a pair, else any building with exactly one stand-in; and any building with no stand-in and no LOD model
    const std::vector<bool> origAtStart = originalFlags();
    auto isPairBuilding = [&](int i) {
        if (!origAtStart[size_t(i)]) return false;
        const IplInstance& o = app.instances[size_t(i)];
        if (app.defs[o.id].farModel || o.id == supaSave || o.id == petfoodId) return false;
        const std::vector<size_t> p = findLodPartners(app.instances, size_t(i));
        return p.size() == 1 && origAtStart[p[0]] && app.defs[app.instances[p[0]].id].farModel && app.instances[p[0]].id != lodSupaSave && app.instances[p[0]].z != kSunkZ && findLodPartners(app.instances, p[0]).size() == 1;
    };
    int hospital = stPlacementOf("hospitaland"), petfood = -1;
    if (hospital >= 0 && !isPairBuilding(hospital)) hospital = -1;
    for (int i = 0; i < int(app.instances.size()) && hospital < 0; ++i) if (isPairBuilding(i)) hospital = i;
    for (int i = 0; i < int(app.instances.size()) && petfood < 0; ++i) {
        const IplInstance& o = app.instances[size_t(i)];
        if (!app.defs[o.id].farModel && o.id != supaSave && lodModelIdFor(o.id) < 0 && findLodPartners(app.instances, size_t(i)).empty()) petfood = i;
    }
    ST_CHECK(t, hospital >= 0 && petfood >= 0, "a building with a stand-in and one without are on the map");
    if (hospital < 0 || petfood < 0) return;
    ST_CHECK(t, lodModelIdFor(supaSave) == lodSupaSave && lodModelIdFor(petfoodId) == -1 && lodModelIdFor(lodSupaSave) == -1, "the LOD model of a building is found by name");
    const std::vector<size_t> hospLods = findLodPartners(app.instances, size_t(hospital));
    ST_CHECK(t, hospLods.size() == 1, "the building has its LOD stand-in");
    if (hospLods.size() != 1) return;
    const int hospLod = int(hospLods[0]);
    const std::string before = stMapState();
    const size_t undoBefore = app.history.undoCount();
    ReplaceReport rep;

    // refused or nothing to do: no change, no undo step
    app.selected.clear();
    ST_CHECK(t, !setModelId(app.hwnd, supaSave, &rep), "nothing selected: nothing to replace");
    app.selected = {hospital};
    ST_CHECK(t, !setModelId(app.hwnd, 999999, &rep), "an unknown model ID is refused");
    ST_CHECK(t, setModelId(app.hwnd, app.instances[hospital].id, &rep) && rep.replaced == 0, "the same model again changes nothing");
    ST_CHECK(t, stMapState() == before && app.history.undoCount() == undoBefore, "refusals and no-ops leave the map and the undo list alone");

    const IplInstance hospWas = app.instances[hospital], lodWas = app.instances[hospLod];
    const size_t countWas = app.instances.size();

    // 1. the new building has a LOD model: the stand-in gets it
    app.selected = {hospital};
    ST_CHECK(t, setModelId(app.hwnd, supaSave, &rep), "hospital -> SupaSave");
    ST_CHECK(t, rep.replaced == 1 && rep.lodSwitched == 1 && rep.lodSunk == 0 && rep.lodAdded == 0, "one replaced, its stand-in switched");
    {
        const IplInstance &h = app.instances[hospital], &l = app.instances[hospLod];
        ST_CHECK(t, h.id == supaSave && h.model == app.defs[supaSave].name && h.x == hospWas.x && h.y == hospWas.y && h.z == hospWas.z && h.rotW == hospWas.rotW && h.rotZ == hospWas.rotZ,
                 "the building has the new model at the same place and turn");
        ST_CHECK(t, l.id == lodSupaSave && l.x == lodWas.x && l.y == lodWas.y && l.z == lodWas.z && l.rotW == lodWas.rotW, "the stand-in has the new LOD model at the same place");
        const std::vector<size_t> p = findLodPartners(app.instances, size_t(hospital));
        ST_CHECK(t, p.size() == 1 && int(p[0]) == hospLod, "the new pair is a pair");
        ST_CHECK(t, app.instances.size() == countWas && app.history.undoCount() == std::min(undoBefore + 1, kUndoLimit), "nothing added, one undo step");
        ST_CHECK(t, app.selected == std::vector<int>{hospital}, "the selection stays on the replaced object");
    }
    stInvariants(t, "replace with LOD");
    ST_CHECK(t, undoAction() && stMapState() == before, "undo brings the whole map back");
    ST_CHECK(t, redoAction() && app.instances[hospital].id == supaSave && app.instances[hospLod].id == lodSupaSave, "redo replaces again");
    ST_CHECK(t, undoAction() && stMapState() == before, "and undo again");

    // 2. the new building has no LOD model: the old stand-in goes below the map
    app.selected = {hospital};
    ST_CHECK(t, setModelId(app.hwnd, petfoodId, &rep) && rep.replaced == 1 && rep.lodSunk == 1 && rep.lodSwitched == 0 && rep.lodAdded == 0, "hospital -> petfoodfactory");
    ST_CHECK(t, app.instances[hospLod].z == kSunkZ && app.instances[hospLod].id == lodWas.id && app.instances[hospLod].x == lodWas.x, "the old stand-in is below the map, otherwise unchanged");
    stInvariants(t, "replace without LOD");
    ST_CHECK(t, undoAction() && stMapState() == before, "undo");

    // 3. the old building had no stand-in, the new one has a LOD model: one is added
    const IplInstance petWas = app.instances[petfood];
    app.selected = {petfood};
    ST_CHECK(t, setModelId(app.hwnd, supaSave, &rep) && rep.replaced == 1 && rep.lodAdded == 1, "petfoodfactory -> SupaSave adds a stand-in");
    {
        const IplInstance& n = app.instances.back();
        ST_CHECK(t, app.instances.size() == countWas + 1 && n.id == lodSupaSave && n.x == petWas.x && n.y == petWas.y && n.z == petWas.z && n.rotW == petWas.rotW &&
                        app.instFile.back() == app.instFile[petfood],
                 "the new stand-in sits at the building's place and turn, in its file");
        const std::vector<size_t> p = findLodPartners(app.instances, size_t(petfood));
        ST_CHECK(t, p.size() == 1 && p[0] == app.instances.size() - 1, "and pairs with it");
    }
    stInvariants(t, "replace adds LOD");
    ST_CHECK(t, undoAction() && stMapState() == before, "undo removes the added stand-in");

    // 4. building and stand-in both selected: the stand-in is not turned into a building
    app.selected = {hospital, hospLod};
    ST_CHECK(t, setModelId(app.hwnd, supaSave, &rep) && rep.replaced == 1 && rep.lodSwitched == 1, "building + its stand-in selected");
    ST_CHECK(t, app.instances[hospital].id == supaSave && app.instances[hospLod].id == lodSupaSave, "the stand-in gets the LOD model, not the building");
    ST_CHECK(t, undoAction() && stMapState() == before, "undo");

    // 5. only the stand-in selected: just it changes (the user asked for exactly that one)
    app.selected = {hospLod};
    ST_CHECK(t, setModelId(app.hwnd, lodSupaSave, &rep) && rep.replaced == 1 && rep.lodSwitched + rep.lodSunk + rep.lodAdded == 0, "a stand-in alone");
    ST_CHECK(t, app.instances[hospLod].id == lodSupaSave && app.instances[hospital].id == hospWas.id, "the building is untouched");
    ST_CHECK(t, undoAction() && stMapState() == before, "undo");

    // 6. a building turned into a far model: the old stand-in would show the old building, so it goes below the map
    app.selected = {hospital};
    ST_CHECK(t, setModelId(app.hwnd, lodSupaSave, &rep) && rep.replaced == 1 && rep.lodSunk == 1, "building -> a LOD model");
    ST_CHECK(t, undoAction() && stMapState() == before, "undo");

    // 7. the user's case: a DUPLICATED building replaced by one without a LOD model: the duplicate's stand-in (added, not in
    //    the original game) is removed from the file; the original pair is untouched
    {
        ST_CHECK(t, originalFlags()[size_t(hospital)] && originalFlags()[size_t(hospLod)], "the original pair counts as original");
        app.selected = {hospital};
        const size_t n0 = app.instances.size();
        ST_CHECK(t, duplicateSelected() == 2 && app.instances.size() == n0 + 2, "duplicate the building and its stand-in");
        std::vector<bool> orig = originalFlags();
        ST_CHECK(t, !orig[n0] && !orig[n0 + 1] && orig[size_t(hospital)] && orig[size_t(hospLod)], "the copies are added, the originals stay original");
        int copy = -1, copyLod = -1;
        for (int s2 : {int(n0), int(n0 + 1)}) (app.defs[app.instances[s2].id].farModel ? copyLod : copy) = s2;
        ST_CHECK(t, copy >= 0 && copyLod >= 0, "a building copy and a stand-in copy");
        app.selected = {copy};
        const std::string dup = stMapState();
        ST_CHECK(t, setModelId(app.hwnd, petfoodId, &rep) && rep.replaced == 1 && rep.lodRemoved == 1 && rep.lodSunk == 0, "the copy -> petfoodfactory removes the copied stand-in");
        ST_CHECK(t, app.instances.size() == n0 + 1 && app.instances[size_t(copy)].id == petfoodId && app.selected == std::vector<int>{copy}, "one placement fewer, the copy replaced and still selected");
        ST_CHECK(t, app.instances[size_t(hospital)].id == hospWas.id && app.instances[size_t(hospLod)].id == lodWas.id && app.instances[size_t(hospLod)].z == lodWas.z,
                 "the original building and stand-in are untouched");
        stInvariants(t, "replace a duplicate");
        ST_CHECK(t, undoAction() && stMapState() == dup, "undo brings the copied stand-in back");
        // the ORIGINAL replaced while the copy exists: its (original) stand-in is only moved below the map
        app.selected = {hospital};
        ST_CHECK(t, setModelId(app.hwnd, petfoodId, &rep) && rep.lodSunk == 1 && rep.lodRemoved == 0 && app.instances.size() == n0 + 2, "the original -> petfoodfactory sinks the original stand-in");
        ST_CHECK(t, undoAction() && stMapState() == dup, "undo");
        // the copy replaced by a building WITH a LOD model: the copied stand-in is switched, nothing removed
        app.selected = {copy};
        ST_CHECK(t, setModelId(app.hwnd, supaSave, &rep) && rep.lodSwitched == 1 && rep.lodRemoved == 0 && app.instances[size_t(copyLod)].id == lodSupaSave, "the copy -> SupaSave switches its stand-in");
        ST_CHECK(t, undoAction() && stMapState() == dup, "undo");
        ST_CHECK(t, undoAction() && stMapState() == before, "undo the duplicate too");
    }

    // 8. many random replaces: only the intended placements change, the count grows by what was added, undo restores
    unsigned seed = 4242u;
    auto rnd = [&](int n) { seed = seed * 1103515245u + 12345u; return n <= 0 ? 0 : int((seed >> 8) % unsigned(n)); };
    std::vector<int> ids;
    for (const auto& kv : app.defs) ids.push_back(kv.first);
    for (int round = 0; round < 300; ++round) {
        const std::string was = stMapState();
        const std::vector<IplInstance> old = app.instances;
        app.selected.clear();
        const int k = 1 + rnd(3);
        for (int j = 0; j < k; ++j) {
            const int idx = rnd(int(app.instances.size()));
            if (std::find(app.selected.begin(), app.selected.end(), idx) == app.selected.end()) app.selected.push_back(idx);
        }
        const int to = ids[size_t(rnd(int(ids.size())))];
        const std::vector<int> sel = app.selected;
        const std::vector<int> oldFile = app.instFile;
        const std::vector<bool> wasOriginal = originalFlags();
        const size_t undoWas = app.history.undoCount();
        if (!setModelId(app.hwnd, to, &rep)) { ST_CHECK(t, false, "a known model is accepted"); continue; }
        ST_CHECK(t, app.instances.size() == old.size() + size_t(rep.lodAdded) - size_t(rep.lodRemoved), "the count changes exactly by the added and removed stand-ins");
        // line the old placements up with the new ones: the removed ones (reported by Replace) are skipped, the rest keep
        // their order
        int changedId = 0, sunk = 0, removed = 0;
        size_t j = 0;
        for (size_t i = 0; i < old.size(); ++i) {
            const IplInstance& a = old[i];
            if (std::binary_search(rep.removedIdx.begin(), rep.removedIdx.end(), int(i))) {  // must be a stand-in the user had added
                ++removed;
                ST_CHECK(t, app.defs[a.id].farModel && !wasOriginal[i], "only an added LOD stand-in is removed from the file");
                continue;
            }
            const IplInstance& b = app.instances[j++];
            ST_CHECK(t, app.instFile[j - 1] == oldFile[i] && a.x == b.x && a.y == b.y && a.rotX == b.rotX && a.rotY == b.rotY && a.rotZ == b.rotZ && a.rotW == b.rotW,
                     "no kept placement moves sideways, turns or changes file");
            if (a.z != b.z) { ++sunk; ST_CHECK(t, b.z == kSunkZ && app.defs[b.id].farModel && wasOriginal[i], "only an original LOD stand-in changes height, to below the map"); }
            if (a.id != b.id) {
                ++changedId;
                const bool selected = std::find(sel.begin(), sel.end(), int(i)) != sel.end();
                ST_CHECK(t, (selected && (b.id == to || b.id == lodModelIdFor(to))) || (!selected && b.id == lodModelIdFor(to)), "a model only changes to the new model or its LOD model");
            }
        }
        ST_CHECK(t, removed == rep.lodRemoved, "the removed count matches");
        ST_CHECK(t, changedId == rep.replaced + rep.lodSwitched && sunk == rep.lodSunk, "the report matches what changed");
        for (size_t i = j; i < app.instances.size(); ++i) ST_CHECK(t, app.instances[i].id == lodModelIdFor(to), "an added placement is the LOD model");
        for (int i : app.selected) ST_CHECK(t, i >= 0 && i < int(app.instances.size()), "the selection still points at placements");
        const bool anyChange = rep.replaced > 0;
        ST_CHECK(t, app.history.undoCount() == (anyChange ? std::min(undoWas + 1, kUndoLimit) : undoWas), "one undo step, or none when nothing changed");
        if (round % 25 == 0) stInvariants(t, "random replace");
        if (anyChange) ST_CHECK(t, undoAction() && stMapState() == was, "undo restores the map exactly");
    }
    ST_CHECK(t, stMapState() == before, "after all rounds the map is as it was");
    app.selected.clear();
    stInvariants(t, "replace done");
}

// Road builder: nodes, strip, preview, attach. Every placed piece must meet the end it was attached to, nothing else in
// the map may change, previews change nothing, and undo brings the map back exactly.
void runRoadSelfTest(SelfTest& t) {
    insp.quiet = true;
    closeRoadStrip();
    // a road piece in open space (far from the map) so both ends are free
    int straightId = -1;
    for (const auto& kv : app.defs) if (kv.second.model == "rd_road2a50") straightId = kv.first;
    ST_CHECK(t, straightId >= 0 && roadEndsOf(straightId).size() == 2, "rd_Road2A50 is a road piece with two ends");
    if (straightId < 0) return;
    const int file = app.instFile[0];
    if (!app.regionOn.empty()) app.regionOn[size_t(file)] = 1;  // earlier random steps may have hidden it (hidden pieces join nothing)
    const std::string before = stMapState();
    const size_t undo0 = app.history.undoCount();
    const int piece = addNewObject(straightId, file, 3000, 3000, 20);
    ST_CHECK(t, piece >= 0 && isRoadPiece(piece), "a straight placed in open space");
    const std::string withPiece = stMapState();
    for (const RoadEnd& e : roadEndsOf(straightId)) ST_CHECK(t, !roadEndJoined(piece, endInWorld(app.instances[piece], e)), "both ends of a lone piece are open");

    // the joined check agrees with a brute-force search on Rockstar's own roads (all placements)
    {
        int checked = 0;
        for (size_t i = 0; i < app.instances.size() && checked < 60; ++i) {
            if (!isRoadPiece(int(i)) || app.instances[i].z <= kDeletedDepth) continue;
            for (const RoadEnd& e : roadEndsOf(app.instances[i].id)) {
                const RoadEnd w = endInWorld(app.instances[i], e);
                bool brute = false;
                for (size_t j = 0; j < app.instances.size() && !brute; ++j) {
                    if (j == i || (!app.regionOn.empty() && !app.regionOn[app.instFile[j]])) continue;
                    for (const RoadEnd& f : roadEndsOf(app.instances[j].id)) brute = brute || endsMeet(w, endInWorld(app.instances[j], f));
                }
                ST_CHECK(t, brute == roadEndJoined(int(i), w), "joined / open agrees with a search of the whole map");
            }
            ++checked;
        }
    }

    // every distinct variant of every width attaches so that its joining end meets the target end, turned about z only
    for (double width : {20.0, 30.0, 40.0}) {
        const std::vector<RoadVariant> vs = roadVariantsFor(width);
        ST_CHECK(t, !vs.empty(), "pieces fit a road " + num(width, 0) + " wide");
        for (const RoadVariant& v : vs) {
            const std::vector<RoadEnd>& ve = roadEndsOf(v.id);
            ST_CHECK(t, v.end >= 0 && v.end < int(ve.size()) && std::fabs(ve[size_t(v.end)].width - width) <= 0.6 && ve.size() >= 2, v.name + ": the joining end fits");
        }
    }
    app.selected.assign(1, piece);
    openRoadStrip(piece, 1);
    ST_CHECK(t, roadStripOpen() && rb.target == piece && rb.targetEnd == 1 && !rb.variants.empty(), "the strip opens on the chosen end");
    const RoadEnd target = endInWorld(app.instances[piece], roadEndsOf(straightId)[1]);
    for (size_t i = 0; i < rb.variants.size(); ++i) {
        IplInstance p;
        ST_CHECK(t, roadVariantPlacement(rb.variants[i], p), rb.variants[i].name + " can be placed");
        ST_CHECK(t, endsMeet(target, endInWorld(p, roadEndsOf(p.id)[size_t(rb.variants[i].end)]), 1e-6), rb.variants[i].name + " " + rb.variants[i].label + ": its end meets the target end");
        ST_CHECK(t, p.rotX == 0 && p.rotY == 0, rb.variants[i].name + ": turned about the vertical only");
    }

    // hovering previews and changes nothing
    for (int i : {0, 3, int(rb.variants.size()) - 1, -1}) {
        SendMessageA(rb.strip, WM_USER + 1, WPARAM(i), 0);
        ST_CHECK(t, rb.hover == (i >= 0 ? i : -1), "hover follows the strip");
        render();
        ST_CHECK(t, stMapState() == withPiece && app.history.undoCount() == std::min(undo0 + 1, kUndoLimit), "a preview changes nothing");
    }

    // clicking a thumbnail places it: one placement (plus a LOD stand-in if the model has one), selected, one undo step,
    // and the strip moves on to the new piece's far end
    {
        const int n0 = int(app.instances.size());
        const size_t u0 = app.history.undoCount();
        const RoadVariant v = rb.variants[0];
        const int x = 6 + 0 * kStripItemW + 20, y = 26 + 20;
        SendMessageA(rb.strip, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
        const int added = int(app.instances.size()) - n0;
        ST_CHECK(t, added == (lodModelIdFor(v.id) >= 0 ? 2 : 1), "a click places the piece (and its LOD stand-in)");
        const int fresh = n0;
        ST_CHECK(t, app.selected == std::vector<int>{fresh} && app.instances[size_t(fresh)].id == v.id, "the new piece is selected");
        ST_CHECK(t, app.history.undoCount() == std::min(u0 + 1, kUndoLimit), "one undo step");
        ST_CHECK(t, roadEndJoined(piece, target), "the target end is joined now");
        ST_CHECK(t, roadStripOpen() && rb.target == fresh && rb.targetEnd != v.end, "the strip moved on to the new piece's other end");
        ST_CHECK(t, !roadEndJoined(fresh, endInWorld(app.instances[size_t(fresh)], roadEndsOf(v.id)[size_t(rb.targetEnd)])), "and that end is open");
        stInvariants(t, "road piece placed");
    }

    // build a random road of 15 pieces (the undo list keeps 20 steps); every joint meets; then undo all of it exactly
    {
        unsigned seed = 909u;
        auto rnd = [&](int n) { seed = seed * 1103515245u + 12345u; return n <= 0 ? 0 : int((seed >> 8) % unsigned(n)); };
        const std::string start = stMapState();
        int placed = 0;
        for (int k = 0; k < 15 && roadStripOpen(); ++k) {
            const int tgt = rb.target, te = rb.targetEnd;
            const RoadEnd tw = endInWorld(app.instances[size_t(tgt)], roadEndsOf(app.instances[size_t(tgt)].id)[size_t(te)]);
            const int vi = rnd(int(rb.variants.size()));
            const RoadVariant v = rb.variants[size_t(vi)];
            const int idx = placeRoadVariant(vi);
            if (idx < 0) { ST_CHECK(t, false, "a level end always takes a piece"); break; }
            ++placed;
            ST_CHECK(t, endsMeet(tw, endInWorld(app.instances[size_t(idx)], roadEndsOf(v.id)[size_t(v.end)]), 1e-4), "random road: the joint meets (" + v.name + " " + v.label + ")");
            if (!roadStripOpen()) openRoadStrip(idx, 0);  // a dead end: carry on from another end so the road keeps growing
            if (!isRoadPiece(rb.target)) break;
            if (roadEndJoined(rb.target, endInWorld(app.instances[size_t(rb.target)], roadEndsOf(app.instances[size_t(rb.target)].id)[size_t(rb.targetEnd)]))) break;
        }
        ST_CHECK(t, placed >= 5, "a road of several pieces was built (" + std::to_string(placed) + ")");
        stInvariants(t, "random road");
        int undone = 0;
        while (stMapState() != start && undone < placed && undoAction()) ++undone;
        ST_CHECK(t, stMapState() == start, "undo takes the whole road back exactly");
        roadSync();
        ST_CHECK(t, !roadStripOpen() || isRoadPiece(rb.target), "after undo the strip is closed or points at a piece that exists");
    }

    // clicking a node opens the strip on that end; a joined node does not
    {
        closeRoadStrip();
        app.selected.assign(1, piece);
        centreOnObject(piece);
        glViewport(app.viewX, 0, app.width, app.height);
        setViewMatrices(-1, -1);
        GLdouble mv[16], pr[16];
        GLint vp[4];
        glGetDoublev(GL_MODELVIEW_MATRIX, mv);
        glGetDoublev(GL_PROJECTION_MATRIX, pr);
        glGetIntegerv(GL_VIEWPORT, vp);
        for (int k = 0; k < 2; ++k) {
            const RoadEnd w = endInWorld(app.instances[piece], roadEndsOf(straightId)[size_t(k)]);
            GLdouble sx, sy, sz;
            if (!gluProject(w.x, w.y, w.z + 1.5, mv, pr, vp, &sx, &sy, &sz) || sz < 0 || sz > 1) continue;
            closeRoadStrip();
            const bool hit = roadClick(int(sx), int(vp[3] - sy));
            ST_CHECK(t, hit, "a click on the node is recognised");
            const bool joined = roadEndJoined(piece, w);
            ST_CHECK(t, joined ? !roadStripOpen() : (roadStripOpen() && rb.targetEnd == k), joined ? "a joined node does not open the strip" : "an open node opens the strip on that end");
        }
        closeRoadStrip();
        ST_CHECK(t, !roadClick(5, 5), "a click away from the nodes is not a node");
    }

    // Ctrl+R: the strip on the first open end; with no road piece selected it only explains
    {
        closeRoadStrip();
        app.selected.assign(1, piece);
        doCommand(app.hwnd, ID_TB_ROADATTACH);
        ST_CHECK(t, roadStripOpen() && rb.target == piece, "Ctrl+R opens the strip on the selected road piece");
        closeRoadStrip();
        int other = -1;
        for (size_t i = 0; i < app.instances.size() && other < 0; ++i) if (!isRoadPiece(int(i))) other = int(i);
        app.selected.assign(1, other);
        rb.lastMessage.clear();
        doCommand(app.hwnd, ID_TB_ROADATTACH);
        ST_CHECK(t, !roadStripOpen() && !rb.lastMessage.empty(), "Ctrl+R on something that is not a road piece explains instead");
    }

    // a tilted piece takes no level piece; nothing changes
    {
        app.selected.assign(1, piece);
        const std::string s0 = stMapState();
        IplInstance keep = app.instances[piece];
        eulerDegToQuat(12, 0, 0, true, app.instances[piece].rotX, app.instances[piece].rotY, app.instances[piece].rotZ, app.instances[piece].rotW);
        openRoadStrip(piece, 0);
        const std::string tilted = stMapState();
        ST_CHECK(t, placeRoadVariant(0) < 0 && stMapState() == tilted, "a tilted end refuses a piece and changes nothing");
        app.instances[piece] = keep;
        ST_CHECK(t, stMapState() == s0, "restored");
    }

    // Esc closes the strip without losing the selection; selecting something else closes it too
    openRoadStrip(piece, 0);
    SendMessageA(app.hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
    ST_CHECK(t, !roadStripOpen() && app.selected == std::vector<int>{piece}, "Esc closes the strip and keeps the selection");
    openRoadStrip(piece, 0);
    app.selected.clear();
    roadSync();
    ST_CHECK(t, !roadStripOpen(), "a new selection closes the strip");

    // back to how it was
    for (int k = 0; k < 2 && stMapState() != before; ++k) undoAction();  // its own two steps: the lone straight and the clicked piece
    ST_CHECK(t, stMapState() == before, "the road test leaves the map as it found it");
    closeRoadStrip();
    app.selected.clear();
    stInvariants(t, "road test done");
}

// DFF Properties: the information table against an independent count, every control (buttons, check boxes, mouse,
// wheel, auto-rotate and its wrap), reopening, closing, and the map left alone.
void runDffViewerSelfTest(SelfTest& t) {
    insp.quiet = true;
    const std::string before = stMapState();
    ST_CHECK(t, openDffViewer(app.hwnd, -12345) == nullptr && viewers.empty(), "an unknown model opens nothing");
    int n = 0, opened = 0;
    for (const auto& kv : app.defs) {
        if (n++ % 50 != 0) continue;
        ModelViewer* v = openDffViewer(app.hwnd, kv.first);
        std::vector<unsigned char> data;
        std::string err;
        DffModel m;
        const bool readable = app.img.extract(kv.second.model + ".dff", data, err) && parseDff(data, m, err);
        if (!readable) {
            ST_CHECK(t, v == nullptr, kv.second.name + ": an unreadable model opens no viewer");
            continue;
        }
        ++opened;
        ST_CHECK(t, v && IsWindow(v->wnd) && v->rc, kv.second.name + ": DFF Properties opens with its own GL context");
        if (!v) continue;
        size_t verts = 0, faces = 0;
        for (const DffGeometry& g : m.geometries) { verts += g.vertexCount(); faces += g.triangles.size() / 3; }
        char text[8192];
        GetWindowTextA(v->info, text, sizeof text);
        const std::string info = text;
        ST_CHECK(t, info.find("Number of Vertices: " + std::to_string(verts) + "\r\n") != std::string::npos &&
                        info.find("Number of Faces: " + std::to_string(faces) + "\r\n") != std::string::npos &&
                        info.find("Number of Objects: " + std::to_string(m.atomics.size()) + "\r\n") != std::string::npos,
                 kv.second.name + ": the table counts vertices, faces and objects");
        ST_CHECK(t, SendMessageA(v->objectBox, CB_GETCOUNT, 0, 0) == LRESULT(m.atomics.size() + 1), kv.second.name + ": one Show Object Number entry per object, plus All");
        ST_CHECK(t, openDffViewer(app.hwnd, kv.first) == v && viewers.size() == 1, kv.second.name + ": opening it again brings the same window to the front");
        // buttons
        const GlViewState home = v->view;
        SendMessageA(v->wnd, WM_COMMAND, ID_MV_ZOOMIN, 0);
        ST_CHECK(t, v->view.tz == home.tz + 3, "Zoom In moves 3 closer");
        SendMessageA(v->wnd, WM_COMMAND, ID_MV_ZOOMOUT, 0);
        SendMessageA(v->wnd, WM_COMMAND, ID_MV_ZOOMOUT, 0);
        ST_CHECK(t, v->view.tz == home.tz - 3, "Zoom Out moves 3 away");
        // mouse: right button turns, left button moves, the wheel zooms
        SendMessageA(v->gl, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(100, 100));
        SendMessageA(v->gl, WM_MOUSEMOVE, MK_RBUTTON, MAKELPARAM(110, 105));
        SendMessageA(v->gl, WM_RBUTTONUP, 0, MAKELPARAM(110, 105));
        ST_CHECK(t, v->view.rotZ == home.rotZ + 10 && v->view.rotX == home.rotX + 5, "right drag turns a degree per pixel");
        SendMessageA(v->gl, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(50, 50));
        SendMessageA(v->gl, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(58, 46));
        SendMessageA(v->gl, WM_LBUTTONUP, 0, MAKELPARAM(58, 46));
        ST_CHECK(t, v->view.tx == home.tx + 2 && v->view.ty == home.ty + 1, "left drag moves 0.25 per pixel");
        const float z0 = v->view.tz;
        SendMessageA(v->gl, WM_MOUSEWHEEL, MAKEWPARAM(0, 120), 0);
        SendMessageA(v->gl, WM_MOUSEWHEEL, MAKEWPARAM(MK_SHIFT, WORD(-120)), 0);
        ST_CHECK(t, v->view.tz == z0 + 1 - 10, "the wheel moves 1, with Shift 10");
        SendMessageA(v->wnd, WM_COMMAND, ID_MV_RESET, 0);
        ST_CHECK(t, std::memcmp(&v->view, &home, sizeof home) == 0, "Reset View goes back to the start view");
        // check boxes
        SendMessageA(GetDlgItem(v->wnd, ID_MV_WIRE), BM_CLICK, 0, 0);
        ST_CHECK(t, v->wireframe, "Show Wireframe on");
        SendMessageA(GetDlgItem(v->wnd, ID_MV_WIRE), BM_CLICK, 0, 0);
        ST_CHECK(t, !v->wireframe, "and off");
        SendMessageA(GetDlgItem(v->wnd, ID_MV_ROTATE), BM_CLICK, 0, 0);
        ST_CHECK(t, v->autoRotate, "Automatically Rotate on");
        v->lastTick = GetTickCount() - 400;  // ten 40 ms ticks (one more if the clock moves on before the update)
        updateViewers();
        const float turnedZ = v->view.rotZ - home.rotZ;
        const bool tenOrEleven = std::fabs(turnedZ - 10) < 1e-3 || std::fabs(turnedZ - 11) < 1e-3;
        const float ticks = std::round(turnedZ);
        ST_CHECK(t, tenOrEleven && std::fabs(v->view.rotX - (home.rotX + 0.5f * ticks)) < 1e-3 && std::fabs(v->view.rotY - (home.rotY + 0.4f * ticks)) < 1e-3,
                 "each 40 ms tick turns Z 1, X 0.5, Y 0.4 degrees");
        v->view.rotZ = 359.5f;
        v->lastTick = GetTickCount() - 40;
        updateViewers();
        ST_CHECK(t, v->view.rotZ == 1 || v->view.rotZ == 2, "an angle past 360 starts again at 1 (MEd)");
        SendMessageA(GetDlgItem(v->wnd, ID_MV_ROTATE), BM_CLICK, 0, 0);
        ST_CHECK(t, !v->autoRotate, "Automatically Rotate off");
        // objects and the GTA III check boxes
        SendMessageA(v->objectBox, CB_SETCURSEL, 1, 0);
        SendMessageA(v->wnd, WM_COMMAND, MAKEWPARAM(ID_MV_OBJECT, CBN_SELCHANGE), LPARAM(v->objectBox));
        ST_CHECK(t, v->object == 1, "Show Object Number picks one object");
        bool night = false, fx = false;
        for (const DffGeometry& g : m.geometries) { night = night || g.nightColors; fx = fx || g.has2dfx; }
        ST_CHECK(t, (IsWindowEnabled(GetDlgItem(v->wnd, ID_MV_NIGHT)) != 0) == night && (IsWindowEnabled(GetDlgItem(v->wnd, ID_MV_2DFX)) != 0) == fx,
                 "the night colour and 2DFX boxes are only enabled when the model has them");
        drawViewer(*v);  // draws without trouble
        render();
        // close
        SendMessageA(v->wnd, WM_COMMAND, ID_MV_CLOSE, 0);
        updateViewers();
        ST_CHECK(t, viewers.empty(), "Close removes the viewer");
    }
    ST_CHECK(t, opened > 40, "DFF Properties opened for " + std::to_string(opened) + " models");
    // several at once, then Esc on each
    std::vector<int> ids;
    for (const auto& kv : app.defs) if (ids.size() < 4) ids.push_back(kv.first);
    for (int id : ids) openDffViewer(app.hwnd, id);
    ST_CHECK(t, viewers.size() <= ids.size() && !viewers.empty(), "several viewers can be open");
    updateViewers();
    std::vector<HWND> wnds;
    for (auto& v : viewers) wnds.push_back(v->wnd);
    for (HWND w : wnds) SendMessageA(w, WM_KEYDOWN, VK_ESCAPE, 0);
    updateViewers();
    ST_CHECK(t, viewers.empty(), "Esc closes each");
    ST_CHECK(t, stMapState() == before, "viewing models changes nothing in the map");
}

// Collision Object Properties and TXD Properties: their tables against independent reads, their controls, and the map
// left alone.
void runColTxdViewerSelfTest(SelfTest& t) {
    insp.quiet = true;
    const std::string before = stMapState();
    // collision: every 40th definition
    int n = 0, opened = 0, missing = 0;
    for (const auto& kv : app.defs) {
        if (n++ % 40 != 0) continue;
        std::vector<unsigned char> record;
        std::string source, err;
        std::vector<ColModel> models;
        const bool has = findCollisionRecord(app.colFiles, kv.second.model, record, source) && parseCol(record, models, err) && !models.empty();
        ModelViewer* v = openColViewer(app.hwnd, kv.first);
        if (!has) {
            ++missing;
            ST_CHECK(t, v == nullptr, kv.second.name + ": no collision, no viewer (a message instead)");
            continue;
        }
        ++opened;
        ST_CHECK(t, v && v->kind == 1 && IsWindow(v->wnd), kv.second.name + ": Collision Object Properties opens");
        if (!v) continue;
        const ColModel& c = models[0];
        char text[8192];
        GetWindowTextA(v->info, text, sizeof text);
        const std::string info = text;
        ST_CHECK(t, info.find("Number of Boxes: " + std::to_string(c.boxes.size()) + "\r\n") != std::string::npos &&
                        info.find("Number of Spheres: " + std::to_string(c.spheres.size()) + "\r\n") != std::string::npos &&
                        info.find("Collision Mesh Face Count: " + std::to_string(c.faces.size()) + "\r\n") != std::string::npos &&
                        info.find("Collision Mesh Vertices: " + std::to_string(c.vertices.size() / 3) + "\r\n") != std::string::npos,
                 kv.second.name + ": the table counts boxes, spheres and the mesh");
        ST_CHECK(t, openColViewer(app.hwnd, kv.first) == v, kv.second.name + ": opening it again brings the same window to the front");
        ModelViewer* d = openDffViewer(app.hwnd, kv.first);
        ST_CHECK(t, d && d != v && d->kind == 0, kv.second.name + ": its DFF Properties is a separate window");
        for (int id : {ID_MV_BOXES, ID_MV_SPHERES, ID_MV_MESH}) {
            SendMessageA(GetDlgItem(v->wnd, id), BM_CLICK, 0, 0);
            const bool off = id == ID_MV_BOXES ? !v->showBoxes : id == ID_MV_SPHERES ? !v->showSpheres : !v->showMesh;
            ST_CHECK(t, off, "a Show check box switches its shapes off");
            drawViewer(*v);
            SendMessageA(GetDlgItem(v->wnd, id), BM_CLICK, 0, 0);
        }
        ST_CHECK(t, v->showBoxes && v->showSpheres && v->showMesh && !IsWindowEnabled(GetDlgItem(v->wnd, ID_MV_SHADOW)), "all on again; the shadow mesh box is for COL3 only");
        SendMessageA(v->wnd, WM_COMMAND, ID_MV_ZOOMIN, 0);
        SendMessageA(v->wnd, WM_COMMAND, ID_MV_RESET, 0);
        ST_CHECK(t, std::memcmp(&v->view, &v->home, sizeof v->home) == 0, "Reset View");
        drawViewer(*v);
        SendMessageA(v->wnd, WM_COMMAND, ID_MV_CLOSE, 0);
        if (d) SendMessageA(d->wnd, WM_COMMAND, ID_MV_CLOSE, 0);
        updateViewers();
        ST_CHECK(t, viewers.empty(), "both closed");
    }
    ST_CHECK(t, opened > 40, "Collision Object Properties opened for " + std::to_string(opened) + " models (" + std::to_string(missing) + " without collision)");
    // every material colour is MEd's: a few fixed points of its table
    unsigned char rgb[3];
    colMaterialColour(2, rgb);
    ST_CHECK(t, rgb[0] == 0 && rgb[1] == 255 && rgb[2] == 0, "grass is green");
    colMaterialColour(0x13, rgb);
    ST_CHECK(t, rgb[0] == 0 && rgb[1] == 0 && rgb[2] == 255, "water is blue");
    colMaterialColour(0x12, rgb);
    ST_CHECK(t, rgb[0] == 255 && rgb[1] == 0xce && rgb[2] == 0x9d, "sand is sandy");
    colMaterialColour(200, rgb);
    ST_CHECK(t, rgb[0] == 100 && rgb[1] == 100 && rgb[2] == 100, "an unknown material is grey");

    // textures: every 15th dictionary in use, every texture shown once
    std::vector<std::string> txds;
    {
        std::set<std::string> seen;
        for (const auto& kv : app.defs) seen.insert(kv.second.txd);
        int k = 0;
        for (const std::string& x : seen) if (k++ % 15 == 0) txds.push_back(x);
        txds.push_back("generic");
    }
    int shownDicts = 0;
    for (const std::string& name : txds) {
        TxdFile f;
        std::string src, err;
        const bool readable = readTxd(name, f, src, err);
        TxdViewer* v = openTxdViewer(app.hwnd, name);
        if (!readable) {
            ST_CHECK(t, v == nullptr, name + ": an unreadable dictionary opens no viewer");
            continue;
        }
        ++shownDicts;
        ST_CHECK(t, v && IsWindow(v->wnd), name + ".txd: TXD Properties opens");
        if (!v) continue;
        ST_CHECK(t, ListView_GetItemCount(v->list) == int(f.textures.size()), name + ".txd: one row per texture");
        ST_CHECK(t, f.textures.empty() || v->sel == 0, name + ".txd: the first texture is shown");
        ST_CHECK(t, openTxdViewer(app.hwnd, name) == v, name + ".txd: opening it again brings the same window to the front");
        for (int i = 0; i < int(f.textures.size()); ++i) {
            ListView_SetItemState(v->list, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            const TxdTexture& x = f.textures[size_t(i)];
            char text[4096];
            GetWindowTextA(v->info, text, sizeof text);
            const std::string info = text;
            ST_CHECK(t, v->sel == i && info.find("Texture Name: " + x.name + "\r\n") != std::string::npos &&
                            info.find("Size: " + std::to_string(x.width) + " x " + std::to_string(x.height) + "\r\n") != std::string::npos &&
                            v->bgra.size() == size_t(x.width) * x.height * 4,
                     name + ".txd: picking " + x.name + " shows it and its details");
            UpdateWindow(v->preview);
        }
        SendMessageA(GetDlgItem(v->wnd, ID_TV_FIT), BM_CLICK, 0, 0);
        SendMessageA(GetDlgItem(v->wnd, ID_TV_ALPHA), BM_CLICK, 0, 0);
        ST_CHECK(t, !v->fit && !v->alpha, "Fit to window and Show alpha switch off");
        UpdateWindow(v->preview);
        SendMessageA(v->wnd, WM_KEYDOWN, VK_ESCAPE, 0);
        updateTxdViewers();
        ST_CHECK(t, txdViewers.empty(), name + ".txd: Esc closes it");
    }
    ST_CHECK(t, shownDicts > 20, "TXD Properties opened for " + std::to_string(shownDicts) + " dictionaries");
    ST_CHECK(t, openTxdViewer(app.hwnd, "no_such_dictionary") == nullptr && txdViewers.empty(), "an unknown dictionary opens nothing");
    ST_CHECK(t, stMapState() == before, "viewing collision and textures changes nothing in the map");
}

// Loose files in the viewers: extracted copies of real models open from disk and show the same as the game's own;
// a .col with many models picks one; reopening reuses the window; missing, damaged and wrong files are refused.
void runLooseFileSelfTest(SelfTest& t) {
    namespace fs = std::filesystem;
    insp.quiet = true;
    const std::string before = stMapState();
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    const std::string out = std::string(tmp) + "med_selftest_loose_" + std::to_string(GetCurrentProcessId());
    std::error_code ec;
    fs::remove_all(out, ec);
    std::vector<int> ids;
    int n = 0;
    for (const auto& kv : app.defs) if (n++ % 120 == 0) ids.push_back(kv.first);
    for (const ExtractResult& r : extractModelsTo(out, ids)) (void)r;
    int checked = 0;
    for (int id : ids) {
        const ModelDef& def = app.defs[id];
        const std::string dir = out + "/" + def.name;
        // the model, with its dictionary found beside it
        if (fs::exists(dir + "/" + def.name + ".dff", ec)) {
            ST_CHECK(t, openLooseFile(app.hwnd, dir + "/" + def.name + ".dff"), def.name + ".dff opens from disk");
            ModelViewer* v = viewers.empty() ? nullptr : viewers.back().get();
            if (v) {
                ++checked;
                ModelViewer* g = openDffViewer(app.hwnd, id);
                ST_CHECK(t, g && g != v, def.name + ": the game's model is a separate window from the loose file's");
                if (g) {
                    char a[8192], b[8192];
                    GetWindowTextA(v->info, a, sizeof a);
                    GetWindowTextA(g->info, b, sizeof b);
                    const std::string sa = a, sb = b;
                    ST_CHECK(t, sa.substr(sa.find("Number of Objects")) == sb.substr(sb.find("Number of Objects")), def.name + ": the loose copy reads the same as the game's model");
                }
                ST_CHECK(t, fs::exists(dir + "/" + def.txd + ".txd", ec) == (v->txd == def.txd + ".txd"), def.name + ": its dictionary is found beside it");
                const size_t open = viewers.size();
                bool same = openLooseFile(app.hwnd, dir + "/" + def.name + ".dff") && viewers.size() == open;
                same = same && std::any_of(viewers.begin(), viewers.end(), [&](const std::unique_ptr<ModelViewer>& x) { return x.get() == v; });
                ST_CHECK(t, same, "opening the file again reuses its window");
                drawViewer(*v);
            }
        }
        // its collision and textures
        if (fs::exists(dir + "/" + def.name + ".col", ec)) {
            ST_CHECK(t, openLooseFile(app.hwnd, dir + "/" + def.name + ".col"), def.name + ".col opens from disk");
            ModelViewer* c = viewers.empty() ? nullptr : viewers.back().get();
            ST_CHECK(t, c && c->kind == 1 && toLower(c->col.name) == def.model && c->colModels.size() == 1 && !c->objectBox, def.name + ".col: one model, no picker");
            if (c) drawViewer(*c);
        }
        if (fs::exists(dir + "/" + def.txd + ".txd", ec)) {
            ST_CHECK(t, openLooseFile(app.hwnd, dir + "/" + def.txd + ".txd"), def.txd + ".txd opens from disk");
            TxdFile f;
            std::string src, err;
            ST_CHECK(t, !txdViewers.empty() && readTxd(def.txd, f, src, err) && ListView_GetItemCount(txdViewers.back()->list) == int(f.textures.size()),
                     def.txd + ".txd: the loose copy lists the same textures as the game's");
        }
        for (auto& v : viewers) SendMessageA(v->wnd, WM_COMMAND, ID_MV_CLOSE, 0);
        for (auto& v : txdViewers) SendMessageA(v->wnd, WM_COMMAND, ID_TV_CLOSE, 0);
        updateViewers();
        updateTxdViewers();
        ST_CHECK(t, viewers.empty() && txdViewers.empty(), "all closed");
    }
    ST_CHECK(t, checked >= 10, "loose models checked: " + std::to_string(checked));
    // a COL file with many models: pick through them
    if (!app.colFiles.empty()) {
        std::string many;
        for (const std::string& f : app.colFiles) if (toLower(f).find("generic") != std::string::npos) many = f;
        if (many.empty()) many = app.colFiles[0];
        ST_CHECK(t, openLooseFile(app.hwnd, many), "a whole COL file opens");
        ModelViewer* c = viewers.empty() ? nullptr : viewers.back().get();
        ST_CHECK(t, c && c->colModels.size() > 1 && c->objectBox && SendMessageA(c->objectBox, CB_GETCOUNT, 0, 0) == LRESULT(c->colModels.size()), "Model in file lists every model");
        if (c)
            for (int k : {1, int(c->colModels.size()) / 2, int(c->colModels.size()) - 1}) {
                SendMessageA(c->objectBox, CB_SETCURSEL, WPARAM(k), 0);
                SendMessageA(c->wnd, WM_COMMAND, MAKEWPARAM(ID_MV_OBJECT, CBN_SELCHANGE), LPARAM(c->objectBox));
                char text[8192];
                GetWindowTextA(c->info, text, sizeof text);
                ST_CHECK(t, c->col.name == c->colModels[size_t(k)].name && std::string(text).find("Model: " + c->col.name + "\r\n") == 0 &&
                                std::memcmp(&c->view, &c->home, sizeof c->home) == 0,
                         "picking model " + std::to_string(k) + " shows it, its table and a fresh view");
                drawViewer(*c);
            }
        if (c) SendMessageA(c->wnd, WM_COMMAND, ID_MV_CLOSE, 0);
        updateViewers();
    }
    // refused: missing, wrong extension, damaged files of each kind; nothing opens
    std::ofstream(out + "/broken.dff", std::ios::binary) << "this is not a model";
    std::ofstream(out + "/broken.col", std::ios::binary) << "COLL" << std::string(3, '\0');
    std::ofstream(out + "/broken.txd", std::ios::binary) << "nope";
    std::ofstream(out + "/notes.txt", std::ios::binary) << "hello";
    for (const char* bad : {"/missing.dff", "/broken.dff", "/broken.col", "/broken.txd", "/notes.txt", "/missing.col", "/missing.txd"}) {
        ST_CHECK(t, !openLooseFile(app.hwnd, out + bad), std::string(bad + 1) + " is refused with a message");
        ST_CHECK(t, viewers.empty() && txdViewers.empty(), std::string(bad + 1) + ": no viewer is left open");
    }
    fs::remove_all(out, ec);
    ST_CHECK(t, stMapState() == before, "viewing loose files changes nothing in the map");
}

// ---- self-test of Import Model (runs inside --selftest) ---------------------------------------------------------------
// Drives the Import Model window with the messages typing and clicking produce, imports for real into the game copy the
// self-test runs on (after giving that copy its own gta3.img when it shares one through a hard link), checks every file
// and the editor's state, then puts every file back as it was.
namespace stimp {
void setText(HWND h, const std::string& s) { SetWindowTextA(h, s.c_str()); }  // an edit control sends EN_CHANGE
void radio(HWND pick) {
    for (HWND h : {imp.startEmpty, imp.startBox, imp.startMesh}) SendMessageA(h, BM_SETCHECK, h == pick ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(imp.wnd, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(pick), BN_CLICKED), (LPARAM)pick);
}
void region(int row) {
    SendMessageA(imp.region, CB_SETCURSEL, WPARAM(row), 0);
    SendMessageA(imp.wnd, WM_COMMAND, MAKEWPARAM(ID_IM_REGION, CBN_SELCHANGE), (LPARAM)imp.region);
}
void button(int id) { SendMessageA(imp.wnd, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)GetDlgItem(imp.wnd, id)); }
std::string checks() {
    char b[8192] = "";
    GetWindowTextA(imp.checks, b, sizeof b);
    return b;
}
int regionRow(const std::string& name) {
    const int n = int(SendMessageA(imp.region, CB_GETCOUNT, 0, 0));
    for (int i = 0; i < n; ++i) {
        char b[256] = "";
        SendMessageA(imp.region, CB_GETLBTEXT, WPARAM(i), (LPARAM)b);
        if (toLower(b) == toLower(name)) return i;
    }
    return -1;
}
void open(const std::string& dff) {
    imp.quiet = true;
    importModelUi(app.hwnd, dff);
}
void close() {
    if (imp.wnd) DestroyWindow(imp.wnd);
    imp = ImportUi();
}
std::vector<unsigned char> bytesOf(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
}  // namespace stimp

// What the window shows must agree with the fields: the Import button and the "Ready" text go together, the collision
// choice is only open without a collision file, and a few simple rules checked here independently are never let through.
void stImportInvariants(SelfTest& t, const std::string& what) {
    const bool enabled = IsWindowEnabled(imp.importBtn) != 0;
    const std::string text = stimp::checks();
    ST_CHECK(t, enabled == (text.rfind("Ready to import", 0) == 0), what + ": the Import button is on exactly when the window says Ready");
    ST_CHECK(t, enabled != (text.rfind("Not ready yet:", 0) == 0), what + ": otherwise it lists why");
    const bool noCol = imText(imp.col).empty();
    for (HWND h : {imp.startEmpty, imp.startBox, imp.startMesh}) ST_CHECK(t, (IsWindowEnabled(h) != 0) == noCol, what + ": the collision choice is open only without a .col");
    if (!enabled) return;
    const std::string name = imText(imp.name), idText = imText(imp.id), drawText = imText(imp.draw);
    bool nameOk = !name.empty() && name.size() <= 19 && std::all_of(name.begin(), name.end(), [](char c) { return std::isalnum((unsigned char)c) || c == '_' || c == '-'; });
    for (const auto& kv : app.defs) nameOk = nameOk && kv.second.model != toLower(name);
    ST_CHECK(t, nameOk, what + ": Import is never on with a bad or taken name (" + name + ")");
    const bool idDigits = !idText.empty() && std::all_of(idText.begin(), idText.end(), [](char c) { return c >= '0' && c <= '9'; });
    const int id = idDigits && idText.size() < 9 ? std::atoi(idText.c_str()) : -1;
    ST_CHECK(t, id >= 0 && id < 5500 && !app.defs.count(id), what + ": Import is never on with a bad or taken id (" + idText + ")");
    char* end = nullptr;
    const double dd = std::strtod(drawText.c_str(), &end);
    ST_CHECK(t, !drawText.empty() && end && *end == 0 && dd > 0 && dd <= 3000, what + ": Import is never on with a bad draw distance (" + drawText + ")");
    ST_CHECK(t, SendMessageA(imp.region, CB_GETCURSEL, 0, 0) >= 0, what + ": Import is never on without a region");
    ST_CHECK(t, !imText(imp.txd).empty(), what + ": Import is never on without textures");
}

void runImportSelfTest(SelfTest& t) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const std::string mapBefore = stMapState();
    const std::string pid = std::to_string(GetCurrentProcessId() % 100000);
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    const std::string out = std::string(tmp) + "med_selftest_import_" + pid;
    fs::remove_all(out, ec);
    // source files: two of the game's own models, written out and renamed
    int bin = -1, cone = -1;
    for (const auto& kv : app.defs) {
        if (kv.second.model == "dumpster01") bin = kv.first;
        if (kv.second.model == "trafficcone") cone = kv.first;
    }
    ST_CHECK(t, bin >= 0 && cone >= 0, "the test models are in the game");
    if (bin < 0 || cone < 0) return;
    extractModelsTo(out + "/x", {bin, cone});
    const ModelDef binDef = app.defs[bin], coneDef = app.defs[cone];
    const std::string a = "sta" + pid, b = "stb" + pid, c = "stc" + pid;  // with a .col / without / sharing a's textures
    fs::create_directories(out + "/a", ec);
    fs::create_directories(out + "/b", ec);
    fs::create_directories(out + "/c", ec);
    fs::copy_file(out + "/x/" + binDef.name + "/" + binDef.name + ".dff", out + "/a/" + a + ".dff", ec);
    fs::copy_file(out + "/x/" + binDef.name + "/" + binDef.txd + ".txd", out + "/a/" + a + ".txd", ec);
    fs::copy_file(out + "/x/" + binDef.name + "/" + binDef.name + ".col", out + "/a/" + a + ".col", ec);
    fs::copy_file(out + "/x/" + coneDef.name + "/" + coneDef.name + ".dff", out + "/b/" + b + ".dff", ec);
    fs::copy_file(out + "/x/" + coneDef.name + "/" + coneDef.txd + ".txd", out + "/b/" + b + ".txd", ec);
    fs::copy_file(out + "/x/" + coneDef.name + "/" + coneDef.name + ".dff", out + "/c/" + c + ".dff", ec);
    ST_CHECK(t, fs::exists(out + "/a/" + a + ".col") && fs::exists(out + "/b/" + b + ".txd") && fs::exists(out + "/c/" + c + ".dff"), "the source files are written");
    std::ofstream(out + "/broken.dff", std::ios::binary) << "not a model";
    std::ofstream(out + "/broken.txd", std::ios::binary) << "nope";
    std::ofstream(out + "/broken.col", std::ios::binary) << "COLL" << std::string(3, '\0');
    {  // a texture dictionary named like one in the game but with other contents
        std::vector<unsigned char> d = stimp::bytesOf(out + "/b/" + b + ".txd");
        std::ofstream(out + "/" + binDef.txd + ".txd", std::ios::binary).write((const char*)d.data(), std::streamsize(d.size()));
    }

    // what is on disk before, to put back at the end
    const std::string imgPath = app.img.path(), dirPath = imgPath.substr(0, imgPath.size() - 4) + ".dir", datPath = app.root + "/data/gta3.dat";
    // 1. a gta3.img shared through a hard link with another folder is refused: the import would change that folder too
    if (fs::hard_link_count(imgPath, ec) > 1 || fs::hard_link_count(dirPath, ec) > 1) {
        std::string rel, err;
        ST_CHECK(t, createRegionNamed("stlink" + pid, rel, err), "a region for the test");
        stimp::open(out + "/a/" + a + ".dff");
        stimp::region(stimp::regionRow("stlink" + pid));
        const uintmax_t size = fs::file_size(imgPath, ec);
        ST_CHECK(t, !IsWindowEnabled(imp.importBtn) && stimp::checks().find("hard link") != std::string::npos, "a hard-linked gta3.img is refused, and the window says why");
        stimp::button(ID_IM_IMPORT);
        ST_CHECK(t, !imp.imported && fs::file_size(imgPath, ec) == size, "pressing Import anyway writes nothing");
        stimp::close();
        // give this game copy its own archive (the linked original is not touched: only this folder's name is removed)
        for (const std::string& f : {imgPath, dirPath}) {
            fs::copy_file(f, f + ".own", ec);
            fs::remove(f, ec);
            fs::rename(f + ".own", f, ec);
        }
        ST_CHECK(t, fs::hard_link_count(imgPath, ec) == 1 && fs::hard_link_count(dirPath, ec) == 1, "the test's game copy has its own gta3.img now");
        std::string err2;
        ImgArchive::open(imgPath, app.img, err2);
    }
    const uintmax_t imgSize = fs::file_size(imgPath, ec);
    const std::vector<unsigned char> dirBytes = stimp::bytesOf(dirPath), datBytes = stimp::bytesOf(datPath);
    const size_t entries = app.img.entries().size(), defs = app.defs.size(), cols = app.cols.size(), colFiles = app.colFiles.size();
    const bool hadDatBackup = fs::exists(datPath + ".med-backup", ec);
    const bool hadNote = fs::exists(fs::path(imgPath).parent_path() / "med-imports.txt", ec);
    std::string rel, err;
    const std::string region = "stimp" + pid;
    ST_CHECK(t, createRegionNamed(region, rel, err), "the destination region is made");
    const std::vector<unsigned char> datWithRegion = stimp::bytesOf(datPath);
    const std::string ideFile = app.root + "/data/maps/" + region + "/" + region + ".ide", colFile = app.root + "/data/maps/" + region + "/" + region + ".col";

    // 2. the window fills itself in from the .dff
    stimp::open(out + "/a/" + a + ".dff");
    ST_CHECK(t, imp.wnd && fs::equivalent(imText(imp.txd), out + "/a/" + a + ".txd", ec) && fs::equivalent(imText(imp.col), out + "/a/" + a + ".col", ec),
             "picking the .dff fills in the .txd and .col beside it");
    ST_CHECK(t, imText(imp.name) == a, "the name is the file's name");
    std::set<int> ids;
    std::set<std::string> names;
    gameIdsAndNames(app.root, ids, names);
    ST_CHECK(t, imText(imp.id) == std::to_string(proposeModelId(ids)), "the suggested id is the first free one");
    ST_CHECK(t, stimp::regionRow(region) >= 0 && stimp::regionRow("industse") < 0 && stimp::regionRow("props") < 0, "the region list has the user's regions and not Rockstar's");
    stImportInvariants(t, "opened");
    if (SendMessageA(imp.region, CB_GETCOUNT, 0, 0) > 1) {
        stimp::region(-1);
        ST_CHECK(t, !IsWindowEnabled(imp.importBtn) && stimp::checks().find("Pick the region") != std::string::npos, "no region: not ready, and it says so");
    }
    stimp::region(stimp::regionRow(region));
    ST_CHECK(t, IsWindowEnabled(imp.importBtn) && stimp::checks().find("renamed " + a) != std::string::npos, "ready, and it says the collision record is renamed");
    stImportInvariants(t, "region picked");

    // 3. every refusal through the fields, each one put right again
    const std::string goodId = imText(imp.id);
    int takenId = -1;
    for (const auto& kv : app.defs) if (takenId < 0) takenId = kv.first;
    struct Bad { HWND* field; std::string value, says; };
    const std::vector<Bad> bads = {
        {&imp.name, "", "Model name"}, {&imp.name, "two words", "Model name"}, {&imp.name, "trafficcone", "already exists"},
        {&imp.name, "TRAFFICCONE", "already exists"}, {&imp.name, std::string(20, 'q'), "Model name"}, {&imp.name, "a<b", "Model name"},
        {&imp.id, "abc", "whole number"}, {&imp.id, "", "between 0 and"}, {&imp.id, "-5", "between 0 and"}, {&imp.id, "5500", "between 0 and"},
        {&imp.id, std::to_string(takenId), "already used"}, {&imp.id, "12x", "whole number"},
        {&imp.draw, "0", "draw distance"}, {&imp.draw, "3001", "draw distance"}, {&imp.draw, "x", "draw distance"}, {&imp.draw, "", "draw distance"},
        {&imp.flags, "q", "flags"}, {&imp.dff, out + "/broken.dff", "not a model"}, {&imp.dff, out + "/missing.dff", "Cannot read"},
        {&imp.txd, out + "/broken.txd", "not a texture dictionary"}, {&imp.txd, "", "is required"}, {&imp.txd, out + "/" + binDef.txd + ".txd", "different"},
        {&imp.col, out + "/broken.col", "not a collision file"}, {&imp.col, out + "/missing.col", "Cannot read"},
    };
    for (const Bad& x : bads) {
        const std::string keep = imText(*x.field);
        stimp::setText(*x.field, x.value);
        const std::string text = stimp::checks();
        ST_CHECK(t, !IsWindowEnabled(imp.importBtn) && text.find(x.says) != std::string::npos, "'" + x.value + "' is refused and the window says '" + x.says + "'");
        stImportInvariants(t, "refused " + x.value);
        const uintmax_t size = fs::file_size(imgPath, ec);
        stimp::button(ID_IM_IMPORT);
        ST_CHECK(t, !imp.imported && fs::file_size(imgPath, ec) == size && !fs::exists(ideFile, ec), "Import pressed while refused writes nothing");
        stimp::setText(*x.field, keep);
        ST_CHECK(t, IsWindowEnabled(imp.importBtn), "put right again: ready");
    }
    // the three ways to start without a collision file
    stimp::button(ID_IM_COL_CLEAR);
    ST_CHECK(t, imText(imp.col).empty() && IsWindowEnabled(imp.startBox), "No collision file opens the choice");
    stimp::radio(imp.startEmpty);
    ST_CHECK(t, stimp::checks().find("empty (a clean slate)") != std::string::npos, "Empty is a clean slate");
    stimp::radio(imp.startBox);
    ST_CHECK(t, stimp::checks().find("one box around the model") != std::string::npos, "Box is one box");
    stimp::radio(imp.startMesh);
    ST_CHECK(t, stimp::checks().find("the model's own triangles") != std::string::npos, "Mesh is the model's triangles");
    stImportInvariants(t, "starting choices");
    stimp::setText(imp.col, out + "/a/" + a + ".col");
    stimp::button(ID_IM_SUGGEST);
    ST_CHECK(t, imText(imp.id) == goodId, "Suggest gives the first free id");

    // 4. the import: with its own collision file
    const int idA = std::atoi(goodId.c_str());
    stimp::button(ID_IM_IMPORT);
    ST_CHECK(t, imp.imported && imp.importedId == idA && imp.done, "Import imports and closes the window");
    stimp::close();
    auto checkImported = [&](const std::string& name, int id, const std::string& txd, bool txdAdded, size_t boxes, size_t faces, const std::string& w) {
        auto d = app.defs.find(id);
        ST_CHECK(t, d != app.defs.end() && d->second.model == name && d->second.txd == txd && d->second.ide == region + ".ide" && d->second.pool == Pool::Building,
                 w + ": the editor knows the model at once");
        ST_CHECK(t, app.img.find(name + ".dff") >= 0 && (app.img.find(txd + ".txd") >= 0), w + ": model and textures are in gta3.img");
        ImgArchive again;
        std::string e;
        ST_CHECK(t, ImgArchive::open(imgPath, again, e) && again.entries().size() == app.img.entries().size() && again.find(name + ".dff") >= 0, w + ": gta3.img reads back from disk");
        std::vector<unsigned char> dff;
        DffModel m;
        ST_CHECK(t, again.extract(name + ".dff", dff, e) && parseDff(dff, m, e) && !m.geometries.empty(), w + ": the model in the archive reads back");
        bool ok = false;
        const std::string ide = slurp(ideFile, ok);
        ST_CHECK(t, ok && ide.find(std::to_string(id) + ", " + name + ", " + txd + ", 1, 200, 0") != std::string::npos, w + ": the IDE line is written");
        std::vector<ColModel> cm;
        const std::vector<unsigned char> colBytes = stimp::bytesOf(colFile);
        const ColModel* rec = nullptr;
        if (parseCol(colBytes, cm, e))
            for (const ColModel& x : cm) if (x.name == name) rec = &x;
        ST_CHECK(t, rec && rec->modelId == id && rec->boxes.size() == boxes && rec->faces.size() == faces, w + ": the collision record is in the region's COL file");
        auto col = app.cols.find(name);
        ST_CHECK(t, col != app.cols.end() && col->second.boxes.size() == boxes && col->second.faces.size() == faces, w + ": the editor has its collision");
        const std::string dat = slurp(datPath, ok);
        ST_CHECK(t, toLower(dat).find(toLower("IDE DATA\\MAPS\\" + region + "\\" + region + ".IDE")) != std::string::npos &&
                        toLower(dat).find(toLower("COLFILE 0 DATA\\MAPS\\" + region + "\\" + region + ".COL")) != std::string::npos,
                 w + ": gta3.dat lists the region's IDE and COL");
        ST_CHECK(t, fs::exists(dirPath + ".before-" + name + ".med-backup", ec), w + ": gta3.dir was backed up");
        SendMessageA(it.show, CB_SETCURSEL, 0, 0);  // Show: all models (an earlier section may have left placed / not placed)
        SetWindowTextA(it.search, name.c_str());
        ST_CHECK(t, it.rows.size() == 1 && it.rows[0] == id, w + ": the Item tab lists it");
        (void)txdAdded;
    };
    std::vector<ColModel> source;
    std::string e;
    parseCol(stimp::bytesOf(out + "/a/" + a + ".col"), source, e);
    ST_CHECK(t, source.size() == 1, "the source .col holds one model");
    if (source.size() == 1) checkImported(a, idA, a, true, source[0].boxes.size(), source[0].faces.size(), a);
    ST_CHECK(t, app.img.entries().size() == entries + 2, "two entries were added to gta3.img (model and textures)");
    ST_CHECK(t, std::count(app.colFiles.begin(), app.colFiles.end(), colFile) == 1, "the region's COL file is known once");
    {  // it can be placed and drawn
        const size_t n = app.instances.size();
        ST_CHECK(t, addNewObject(idA, regions.back().file, app.cam.x, app.cam.y + 30, app.cam.z - app.cam.h) >= 0 && app.instances.size() == n + 1, "the imported model can be placed");
        render();
        ST_CHECK(t, getModel(a, a).triangles > 0, "its model loads from gta3.img with its triangles");
        TxdFile tf;
        std::string src, terr;
        ST_CHECK(t, readTxd(a, tf, src, terr) && !tf.textures.empty(), "its textures load from gta3.img");
        undoAction();
        ST_CHECK(t, app.instances.size() == n, "undo takes the placement away again");
    }
    // the same name or id again is refused now
    stimp::open(out + "/a/" + a + ".dff");
    stimp::region(stimp::regionRow(region));
    ST_CHECK(t, !IsWindowEnabled(imp.importBtn) && stimp::checks().find("already exists") != std::string::npos, "the same name cannot be imported twice");
    ST_CHECK(t, imText(imp.id) == std::to_string(idA + 1), "the suggested id moved on");
    stimp::close();

    // 5. without a collision file, starting as one box; 6. sharing a's textures, starting as the mesh
    stimp::open(out + "/b/" + b + ".dff");
    ST_CHECK(t, imText(imp.col).empty() && IsWindowEnabled(imp.startBox), "no .col beside it: the choice is open");
    stimp::region(stimp::regionRow(region));
    stimp::radio(imp.startBox);
    const int idB = std::atoi(imText(imp.id).c_str());
    stimp::button(ID_IM_IMPORT);
    ST_CHECK(t, imp.imported, b + " is imported");
    stimp::close();
    checkImported(b, idB, b, true, 1, 0, b);
    stimp::open(out + "/c/" + c + ".dff");
    ST_CHECK(t, imText(imp.txd).empty() && !IsWindowEnabled(imp.importBtn), "no .txd beside it: not ready");
    stimp::setText(imp.txd, out + "/a/" + a + ".txd");
    stimp::region(stimp::regionRow(region));
    stimp::radio(imp.startMesh);
    ST_CHECK(t, stimp::checks().find("already in the game (the same file)") != std::string::npos, "the same dictionary is shared, and it says so");
    const int idC = std::atoi(imText(imp.id).c_str());
    const size_t before = app.img.entries().size();
    stimp::button(ID_IM_IMPORT);
    ST_CHECK(t, imp.imported && app.img.entries().size() == before + 1, c + " adds only its model to gta3.img");
    stimp::close();
    {
        std::vector<float> v;
        std::vector<uint32_t> tri;
        DffModel m;
        parseDff(stimp::bytesOf(out + "/c/" + c + ".dff"), m, e);
        dffMesh(m, v, tri);
        checkImported(c, idC, a, false, 0, tri.size() / 3, c);
    }

    // 7. random field changes: the window must never let a bad import through, and a refused Import writes nothing
    unsigned seed = std::getenv("MED_SELFTEST_SEED") ? unsigned(std::atoi(std::getenv("MED_SELFTEST_SEED"))) : 12345u;
    auto rnd = [&](int n) { seed = seed * 1103515245u + 12345u; return n <= 0 ? 0 : int((seed >> 8) % unsigned(n)); };
    stimp::open(out + "/a/" + a + ".dff");
    const std::vector<std::string> nameVals = {"stz" + pid, "", "x y", a, b, "TRAFFICCONE", std::string(19, 'k'), std::string(20, 'k'), "ok-name_1", "$"};
    const std::vector<std::string> idVals = {std::to_string(idC + 1), "", "abc", "-1", "5499", "5500", std::to_string(idA), std::to_string(takenId), " 7", "99999999999"};
    const std::vector<std::string> drawVals = {"200", "0", "3000", "3001", "x", "1e3", "-4", "0.5", ""};
    const std::vector<std::string> fileVals = {out + "/a/" + a + ".dff", out + "/b/" + b + ".dff", out + "/broken.dff", out + "/missing.dff"};
    const std::vector<std::string> txdVals = {out + "/a/" + a + ".txd", out + "/b/" + b + ".txd", out + "/broken.txd", "", out + "/" + binDef.txd + ".txd"};
    const std::vector<std::string> colVals = {"", out + "/a/" + a + ".col", out + "/broken.col", out + "/x/" + binDef.name + "/" + binDef.name + ".col"};
    const int steps = std::max(200, (std::getenv("MED_SELFTEST_STEPS") ? std::atoi(std::getenv("MED_SELFTEST_STEPS")) : 1500) / 3);
    for (int step = 0; step < steps; ++step) {
        const int op = rnd(9);
        std::string what;
        switch (op) {
            case 0: what = "name"; stimp::setText(imp.name, nameVals[size_t(rnd(int(nameVals.size())))]); break;
            case 1: what = "id"; stimp::setText(imp.id, idVals[size_t(rnd(int(idVals.size())))]); break;
            case 2: what = "draw"; stimp::setText(imp.draw, drawVals[size_t(rnd(int(drawVals.size())))]); break;
            case 3: what = "flags"; stimp::setText(imp.flags, rnd(3) == 0 ? "q" : std::to_string(rnd(4))); break;
            case 4: what = "dff"; stimp::setText(imp.dff, fileVals[size_t(rnd(int(fileVals.size())))]); break;
            case 5: what = "txd"; stimp::setText(imp.txd, txdVals[size_t(rnd(int(txdVals.size())))]); break;
            case 6: what = "col"; stimp::setText(imp.col, colVals[size_t(rnd(int(colVals.size())))]); break;
            case 7: what = "start"; if (imText(imp.col).empty()) stimp::radio(rnd(3) == 0 ? imp.startEmpty : rnd(2) ? imp.startBox : imp.startMesh); break;
            case 8: what = "region"; stimp::region(rnd(3) == 0 ? -1 : stimp::regionRow(region)); break;
        }
        stImportInvariants(t, "random step " + std::to_string(step) + " (" + what + ")");
        if (!IsWindowEnabled(imp.importBtn) && rnd(4) == 0) {
            const uintmax_t size = fs::file_size(imgPath, ec);
            stimp::button(ID_IM_IMPORT);
            ST_CHECK(t, !imp.imported && fs::file_size(imgPath, ec) == size, "random step " + std::to_string(step) + ": a refused Import writes nothing");
        }
    }
    stimp::close();

    // 8. put everything back: the archive, gta3.dat, the region's files; the editor forgets the imported models
    fs::resize_file(imgPath, imgSize, ec);
    std::ofstream(dirPath, std::ios::binary | std::ios::trunc).write((const char*)dirBytes.data(), std::streamsize(dirBytes.size()));
    std::ofstream(datPath, std::ios::binary | std::ios::trunc).write((const char*)datWithRegion.data(), std::streamsize(datWithRegion.size()));
    for (const std::string& n : {a, b, c}) fs::remove(dirPath + ".before-" + n + ".med-backup", ec);
    fs::remove(ideFile, ec);
    fs::remove(colFile, ec);
    if (!hadNote) fs::remove(fs::path(imgPath).parent_path() / "med-imports.txt", ec);
    for (int id : {idA, idB, idC}) app.defs.erase(id);
    for (const std::string& n : {a, b, c}) { app.cols.erase(n); app.models.erase(n); }
    app.colFiles.erase(std::remove(app.colFiles.begin(), app.colFiles.end(), colFile), app.colFiles.end());
    ImgArchive::open(imgPath, app.img, err);
    SetWindowTextA(it.search, "");
    ST_CHECK(t, fs::file_size(imgPath, ec) == imgSize && stimp::bytesOf(dirPath) == dirBytes && app.img.entries().size() == entries, "gta3.img and gta3.dir are back as they were");
    ST_CHECK(t, app.defs.size() == defs && app.cols.size() == cols && app.colFiles.size() == colFiles, "the editor is back to the game's models");
    ST_CHECK(t, hadDatBackup || fs::exists(datPath + ".med-backup", ec), "gta3.dat keeps its backup");
    (void)datBytes;
    fs::remove_all(out, ec);
    ST_CHECK(t, stMapState() == mapBefore, "importing changes no placement (the test placement was undone)");
}

// ---- self-test of the collision editor (runs inside --selftest; saves only into a region it makes, put back after) ------
namespace stce {
void cmd(ModelViewer& v, int id, int code = BN_CLICKED) { SendMessageA(v.wnd, WM_COMMAND, MAKEWPARAM(id, code), (LPARAM)v.ce[id]); }
void text(ModelViewer& v, int id, const std::string& s) { SetWindowTextA(v.ce[id], s.c_str()); }
std::string read(HWND h) {
    char b[8192] = "";
    GetWindowTextA(h, b, sizeof b);
    return b;
}
void mouse(ModelViewer& v, UINT msg, int x, int y, WPARAM keys) { SendMessageA(v.gl, msg, keys, MAKELPARAM(x, y)); }
void click(ModelViewer& v, int x, int y, WPARAM keys = 0) {
    mouse(v, WM_LBUTTONDOWN, x, y, MK_LBUTTON | keys);
    mouse(v, WM_LBUTTONUP, x, y, keys);
}
void drag(ModelViewer& v, int x0, int y0, int x1, int y1, WPARAM keys = 0, int steps = 6) {
    mouse(v, WM_LBUTTONDOWN, x0, y0, MK_LBUTTON | keys);
    for (int k = 1; k <= steps; ++k) mouse(v, WM_MOUSEMOVE, x0 + (x1 - x0) * k / steps, y0 + (y1 - y0) * k / steps, MK_LBUTTON | keys);
    mouse(v, WM_LBUTTONUP, x1, y1, keys);
}
// Where a point of the model shows in the 3D view (as last drawn); false when behind the camera or outside.
bool screenOf(const ModelViewer& v, const float p[3], int& x, int& y) {
    double sx, sy, sz;
    if (gluProject(p[0], p[1], p[2], v.mv, v.proj, v.vp, &sx, &sy, &sz) != GL_TRUE || sz <= 0 || sz >= 1) return false;
    x = int(std::lround(sx));
    y = int(std::lround(v.vp[3] - sy));
    return x >= 2 && y >= 2 && x < v.vp[2] - 2 && y < v.vp[3] - 2;
}
std::vector<unsigned char> bytes(const ColModel& m) { return writeColl(m); }
}  // namespace stce

// Everything the Edit tab must agree with after every step.
void stColInvariants(SelfTest& t, ModelViewer& v, const std::string& what) {
    const ColModel& c = v.col;
    const size_t nv = c.vertices.size() / 3;
    bool ok = c.vertices.size() % 3 == 0;
    for (const ColFace& f : c.faces) ok = ok && f.a < nv && f.b < nv && f.c < nv;
    ST_CHECK(t, ok, what + ": every face uses existing vertices");
    bool boxesOk = true;
    for (const ColBox& b : c.boxes)
        for (int k = 0; k < 3; ++k) boxesOk = boxesOk && b.max[k] - b.min[k] >= kMinBoxSize - 1e-4f;
    ST_CHECK(t, boxesOk, what + ": no box is thinner than the minimum");
    bool spheresOk = true;
    for (const ColSphere& s : c.spheres) spheresOk = spheresOk && s.radius >= kMinSphereRadius - 1e-4f;
    ST_CHECK(t, spheresOk, what + ": no sphere is smaller than the minimum");
    bool selOk = true;
    for (int i : v.selBoxes) selOk = selOk && i >= 0 && i < int(c.boxes.size());
    for (int i : v.selSpheres) selOk = selOk && i >= 0 && i < int(c.spheres.size());
    for (size_t i : v.selFaces) selOk = selOk && i < c.faces.size();
    ST_CHECK(t, selOk, what + ": the selection names only existing shapes");
    ColModel rb = c;
    computeColBounds(rb);
    bool boundsOk = true;
    if (v.dirty && (!c.boxes.empty() || !c.spheres.empty() || !c.faces.empty()))  // an untouched record keeps its own bounds
        for (int k = 0; k < 3; ++k) boundsOk = boundsOk && rb.boundMin[k] == c.boundMin[k] && rb.boundMax[k] == c.boundMax[k];
    ST_CHECK(t, boundsOk, what + ": the bounds hold every shape");
    std::vector<ColModel> back;
    std::string err;
    ST_CHECK(t, parseCol(writeColl(c), back, err) && back.size() == 1 && writeColl(back[0]) == writeColl(c), what + ": the record writes and reads back unchanged");
    ST_CHECK(t, v.dirty == (writeColl(c) != writeColl(v.loaded)), what + ": unsaved exactly when it differs from the saved record");
    ST_CHECK(t, v.undo.size() <= 50, what + ": the undo list keeps 50 steps at most");
    ST_CHECK(t, (IsWindowEnabled(v.ce[ID_CE_DELETE]) != 0) == ceHasSelection(v), what + ": Delete works exactly when something is selected");
    ST_CHECK(t, (IsWindowEnabled(v.ce[ID_CE_UNDO]) != 0) == !v.undo.empty() && (IsWindowEnabled(v.ce[ID_CE_REDO]) != 0) == !v.redo.empty(), what + ": Undo and Redo buttons");
    const std::string status = stce::read(v.ce[ID_CE_STATUS]);
    ST_CHECK(t, status.find(std::to_string(c.faces.size()) + (c.faces.size() == 1 ? " mesh face" : " mesh faces")) != std::string::npos &&
                    status.find(std::to_string(c.boxes.size()) + (c.boxes.size() == 1 ? " box" : " boxes")) != std::string::npos,
             what + ": the status counts the shapes");
    ST_CHECK(t, (status.find("Unsaved changes") != std::string::npos) == v.dirty, what + ": the status says whether something is unsaved");
    char title[512] = "";
    GetWindowTextA(v.wnd, title, sizeof title);
    const std::string tt = title;
    ST_CHECK(t, (tt.size() > 2 && tt.substr(tt.size() - 2) == " *") == v.dirty, what + ": the title has * exactly when something is unsaved");
}

// An independent ray / triangle test (for checking what a click selected).
bool stRayTri(const float o[3], const float d[3], const float* a, const float* b, const float* c, double& t) {
    const double ab[3] = {b[0] - double(a[0]), b[1] - double(a[1]), b[2] - double(a[2])}, ac[3] = {c[0] - double(a[0]), c[1] - double(a[1]), c[2] - double(a[2])};
    const double n[3] = {ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2], ab[0] * ac[1] - ab[1] * ac[0]};
    const double dn = n[0] * d[0] + n[1] * d[1] + n[2] * d[2];
    if (std::fabs(dn) < 1e-12) return false;
    t = (n[0] * (a[0] - double(o[0])) + n[1] * (a[1] - double(o[1])) + n[2] * (a[2] - double(o[2]))) / dn;
    if (t <= 1e-6) return false;
    const double p[3] = {o[0] + d[0] * t, o[1] + d[1] * t, o[2] + d[2] * t};
    auto side = [&](const float* u, const float* w) {  // p on the inner side of edge u -> w
        const double e[3] = {w[0] - double(u[0]), w[1] - double(u[1]), w[2] - double(u[2])}, q[3] = {p[0] - u[0], p[1] - u[1], p[2] - u[2]};
        const double x[3] = {e[1] * q[2] - e[2] * q[1], e[2] * q[0] - e[0] * q[2], e[0] * q[1] - e[1] * q[0]};
        return x[0] * n[0] + x[1] * n[1] + x[2] * n[2] >= -1e-9 * (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    };
    return side(a, b) && side(b, c) && side(c, a);
}

void runColEditSelfTest(SelfTest& t) {
    namespace fs = std::filesystem;
    std::error_code ec;
    ceQuiet = true;
    const std::string mapBefore = stMapState();
    const std::string datPath = app.root + "/data/gta3.dat";
    bool ok = false;
    const std::string datBefore = slurp(datPath, ok);
    const auto colsBefore = app.cols.size();
    const std::vector<std::string> colFilesBefore = app.colFiles;
    int hosp = -1;
    for (const auto& kv : app.defs) if (kv.second.model == "hospitaland") hosp = kv.first;
    ST_CHECK(t, hosp >= 0, "the test model is in the game");
    if (hosp < 0) return;
    const ColModel hospOriginal = app.cols["hospitaland"];

    // 1. the window and its two tabs
    ModelViewer* pv = openColViewer(app.hwnd, hosp);
    ST_CHECK(t, pv && pv->tabs && !pv->editTab, "the collision window opens on its View tab");
    if (!pv) return;
    ModelViewer& v = *pv;
    ST_CHECK(t, IsWindowVisible(v.info) && !IsWindowVisible(v.ce[ID_CE_ADDBOX]), "View shows the table, not the editing buttons");
    ceShowTab(v, true);
    viewerLayout(v);
    ST_CHECK(t, !IsWindowVisible(v.info) && IsWindowVisible(v.ce[ID_CE_ADDBOX]) && IsWindowVisible(v.ce[ID_CE_SAVE]), "Edit shows the editing buttons");
    drawViewer(v);
    stColInvariants(t, v, "opened");
    const std::vector<unsigned char> start = stce::bytes(v.col);
    ST_CHECK(t, !v.dirty && v.undo.empty(), "nothing is unsaved at first");

    // 2. a box: add, pull, push, refused push, typed size
    const size_t boxes0 = v.col.boxes.size();
    stce::cmd(v, ID_CE_ADDBOX);
    ST_CHECK(t, v.col.boxes.size() == boxes0 + 1 && v.selBoxes == std::set<int>{int(boxes0)} && v.selBoxFace == 5, "Add Box adds one box, selected, top side ready");
    const ColBox added = v.col.boxes.back();
    {
        const ColBox expect = newBoxFor(app.cols["hospitaland"]);
        bool same = true;
        for (int k = 0; k < 3; ++k) same = same && added.min[k] == expect.min[k] && added.max[k] == expect.max[k];
        ST_CHECK(t, same, "the new box fits the model's bounds");
    }
    stColInvariants(t, v, "add box");
    stce::text(v, ID_CE_DIST, "1.25");
    stce::cmd(v, ID_CE_OUT);
    ST_CHECK(t, std::fabs(v.col.boxes.back().max[2] - (added.max[2] + 1.25f)) < 1e-4f && v.col.boxes.back().min[2] == added.min[2], "Pull out 1.25 raises the top by exactly 1.25");
    stce::cmd(v, ID_CE_IN);
    ST_CHECK(t, std::fabs(v.col.boxes.back().max[2] - added.max[2]) < 1e-4f, "Push in 1.25 lowers it back");
    const size_t undoBefore = v.undo.size();
    stce::text(v, ID_CE_DIST, "100000");
    v.lastMessage.clear();
    stce::cmd(v, ID_CE_IN);
    ST_CHECK(t, !v.lastMessage.empty() && v.undo.size() == undoBefore && std::fabs(v.col.boxes.back().max[2] - added.max[2]) < 1e-4f, "pushing through the box is refused, no change, no undo step");
    for (const char* bad : {"", "0", "-1", "abc", "1e400", "nan"}) {
        v.lastMessage.clear();
        stce::text(v, ID_CE_DIST, bad);
        stce::cmd(v, ID_CE_OUT);
        ST_CHECK(t, !v.lastMessage.empty() && v.undo.size() == undoBefore, std::string("the distance '") + bad + "' is refused");
    }
    stce::text(v, ID_CE_DIST, "0.5");
    stColInvariants(t, v, "box push / pull");
    // typed size
    const std::string fieldsShown = stce::read(v.ce[ID_CE_F5]);
    ST_CHECK(t, fieldsShown == num(v.col.boxes.back().max[2], 3), "the selected box's numbers are shown");
    const char* typed[6] = {"-1", "-2", "-3", "4", "5", "6.5"};
    for (int k = 0; k < 6; ++k) stce::text(v, ID_CE_F0 + k, typed[k]);
    stce::cmd(v, ID_CE_APPLY);
    ST_CHECK(t, v.col.boxes.back().min[0] == -1 && v.col.boxes.back().min[2] == -3 && v.col.boxes.back().max[2] == 6.5f, "Apply sets the typed size exactly");
    for (const auto& bad : std::vector<std::pair<int, std::string>>{{0, "abc"}, {1, ""}, {2, "1e400"}, {3, "-1"}, {5, "-2.99"}}) {
        const auto before = stce::bytes(v.col);
        v.lastMessage.clear();
        stce::text(v, ID_CE_F0 + bad.first, bad.second);
        stce::cmd(v, ID_CE_APPLY);
        ST_CHECK(t, !v.lastMessage.empty() && stce::bytes(v.col) == before, "typed '" + bad.second + "' is refused, no change");
        v.shownShapeKey.clear();
        ceRefresh(v);
    }
    stColInvariants(t, v, "typed size");

    // 3. a sphere: add, grow, refused shrink, typed size
    stce::cmd(v, ID_CE_ADDSPHERE);
    ST_CHECK(t, v.selSpheres.size() == 1 && v.selBoxes.empty(), "Add Sphere selects the new sphere");
    const float r0 = v.col.spheres.back().radius;
    stce::text(v, ID_CE_DIST, "0.5");
    stce::cmd(v, ID_CE_OUT);
    ST_CHECK(t, std::fabs(v.col.spheres.back().radius - (r0 + 0.5f)) < 1e-4f, "Pull out grows the sphere");
    stce::text(v, ID_CE_DIST, "100000");
    v.lastMessage.clear();
    stce::cmd(v, ID_CE_IN);
    ST_CHECK(t, !v.lastMessage.empty() && std::fabs(v.col.spheres.back().radius - (r0 + 0.5f)) < 1e-4f, "shrinking it to nothing is refused");
    for (int k = 0; k < 4; ++k) stce::text(v, ID_CE_F0 + k, k == 3 ? "2.5" : std::to_string(k + 1));
    stce::cmd(v, ID_CE_APPLY);
    ST_CHECK(t, v.col.spheres.back().center[1] == 2 && v.col.spheres.back().radius == 2.5f, "Apply sets the sphere's centre and radius");
    stColInvariants(t, v, "sphere");

    // 4. move, material
    v.selBoxes = {int(v.col.boxes.size()) - 1};
    v.selBoxFace = -1;
    ceRefresh(v);
    const ColBox mb = v.col.boxes.back();
    const ColSphere ms = v.col.spheres.back();
    stce::text(v, ID_CE_MX, "1.5");
    stce::text(v, ID_CE_MY, "");
    stce::text(v, ID_CE_MZ, "-2");
    stce::cmd(v, ID_CE_MOVE);
    ST_CHECK(t, v.col.boxes.back().min[0] == mb.min[0] + 1.5f && v.col.boxes.back().max[2] == mb.max[2] - 2 && v.col.spheres.back().center[0] == ms.center[0] + 1.5f &&
                    v.col.spheres.back().center[1] == ms.center[1],
             "Move shifts the box and the sphere by exactly X 1.5, Y 0 (empty), Z -2");
    v.lastMessage.clear();
    stce::text(v, ID_CE_MX, "x");
    const auto beforeMove = stce::bytes(v.col);
    stce::cmd(v, ID_CE_MOVE);
    ST_CHECK(t, !v.lastMessage.empty() && stce::bytes(v.col) == beforeMove, "a move that is not a number is refused");
    stce::text(v, ID_CE_MX, "0");
    SendMessageA(v.ce[ID_CE_MAT], CB_SETCURSEL, 17, 0);
    stce::cmd(v, ID_CE_MAT, CBN_SELCHANGE);
    ST_CHECK(t, v.col.boxes.back().surface.material == 17 && v.col.spheres.back().surface.material == 17, "the material list sets the selection's material");
    ceClearSelection(v);
    ceRefresh(v);
    SendMessageA(v.ce[ID_CE_MAT], CB_SETCURSEL, 4, 0);
    const auto beforeMat = stce::bytes(v.col);
    stce::cmd(v, ID_CE_MAT, CBN_SELCHANGE);
    ST_CHECK(t, v.newMaterial == 4 && stce::bytes(v.col) == beforeMat, "with nothing selected it sets the material of new shapes");
    stce::cmd(v, ID_CE_ADDBOX);
    ST_CHECK(t, v.col.boxes.back().surface.material == 4, "the next box gets it");
    stColInvariants(t, v, "move and material");

    // 5. undo everything, redo everything
    std::vector<std::vector<unsigned char>> states;
    while (!v.undo.empty()) {
        states.push_back(stce::bytes(v.col));
        stce::cmd(v, ID_CE_UNDO);
    }
    ST_CHECK(t, stce::bytes(v.col) == start && !v.dirty, "undoing every step gives back the record as opened, nothing unsaved");
    for (auto it = states.rbegin(); it != states.rend(); ++it) {
        stce::cmd(v, ID_CE_REDO);
        ST_CHECK(t, stce::bytes(v.col) == *it, "redo gives back each step exactly");
    }
    while (!v.undo.empty()) stce::cmd(v, ID_CE_UNDO);
    stColInvariants(t, v, "undo and redo");

    // 6. the mesh: clicks select the face under the mouse, double-click the flat side, Shift adds, Ctrl+drag a rectangle
    drawViewer(v);
    int clicks = 0;
    for (size_t i = 0; i < v.col.faces.size() && clicks < 25; i += 3) {
        const ColFace& f = v.col.faces[i];
        float p[3];
        for (int k = 0; k < 3; ++k) p[k] = (v.col.vertices[f.a * 3 + k] + v.col.vertices[f.b * 3 + k] + v.col.vertices[f.c * 3 + k]) / 3;
        int x, y;
        if (!stce::screenOf(v, p, x, y)) continue;
        stce::click(v, x, y);
        float o[3], d[3];
        ceRay(v, x, y, o, d);
        // the nearest shape along the ray, worked out independently: the selection must be a face the ray hits, and nothing nearer
        double best = 1e30;
        for (size_t j = 0; j < v.col.faces.size(); ++j) {
            const ColFace& g = v.col.faces[j];
            double tt;
            if (stRayTri(o, d, &v.col.vertices[g.a * 3], &v.col.vertices[g.b * 3], &v.col.vertices[g.c * 3], tt)) best = std::min(best, tt);
        }
        if (v.selFaces.size() == 1) {
            const ColFace& g = v.col.faces[*v.selFaces.begin()];
            double tt = 0;
            const bool hit = stRayTri(o, d, &v.col.vertices[g.a * 3], &v.col.vertices[g.b * 3], &v.col.vertices[g.c * 3], tt);
            ST_CHECK(t, hit && tt <= best + 1e-3 * std::max(1.0, best), "a click selects the nearest face under the mouse (face " + std::to_string(i) + ")");
        } else if (best < 1e30) {  // the ray through the rounded pixel hits a face (a sliver's middle can fall beside it)
            ST_CHECK(t, !v.selBoxes.empty() || !v.selSpheres.empty(), "a click on a face selects something (face " + std::to_string(i) + ")");
        }
        ++clicks;
    }
    ST_CHECK(t, clicks >= 10, "enough faces were clicked: " + std::to_string(clicks));
    stColInvariants(t, v, "clicks");
    stce::click(v, 1, 1);
    ST_CHECK(t, !ceHasSelection(v), "a click on empty space clears the selection");
    // Shift adds and removes
    if (v.col.faces.size() > 10) {
        size_t a = SIZE_MAX, b = SIZE_MAX;
        int ax = 0, ay = 0, bx = 0, by = 0;
        for (size_t i = 0; i < v.col.faces.size() && b == SIZE_MAX; ++i) {
            const ColFace& f = v.col.faces[i];
            float p[3];
            for (int k = 0; k < 3; ++k) p[k] = (v.col.vertices[f.a * 3 + k] + v.col.vertices[f.b * 3 + k] + v.col.vertices[f.c * 3 + k]) / 3;
            int x, y;
            if (!stce::screenOf(v, p, x, y)) continue;
            stce::click(v, x, y);
            if (v.selFaces.size() != 1) continue;
            if (a == SIZE_MAX) { a = *v.selFaces.begin(); ax = x; ay = y; }
            else if (*v.selFaces.begin() != a) { b = *v.selFaces.begin(); bx = x; by = y; }
        }
        if (b != SIZE_MAX) {
            stce::click(v, ax, ay);
            stce::click(v, bx, by, MK_SHIFT);
            ST_CHECK(t, v.selFaces == std::set<size_t>({a, b}), "Shift+click adds a face");
            stce::click(v, bx, by, MK_SHIFT);
            ST_CHECK(t, v.selFaces == std::set<size_t>({a}), "Shift+click on a selected face removes it");
            stce::mouse(v, WM_LBUTTONDBLCLK, ax, ay, MK_LBUTTON);
            stce::mouse(v, WM_LBUTTONUP, ax, ay, 0);
            ST_CHECK(t, v.selFaces == coplanarNeighbours(v.col, a), "double-click selects the whole flat side");
        }
    }
    // Ctrl+drag over the whole view: every face, box and sphere whose points are all in view
    stce::drag(v, 1, 1, v.vp[2] - 2, v.vp[3] - 2, MK_CONTROL);
    {
        size_t expect = 0;
        for (const ColFace& f : v.col.faces) {
            bool in = true;
            for (uint32_t k : {f.a, f.b, f.c}) {
                int x, y;
                in = in && stce::screenOf(v, &v.col.vertices[k * 3], x, y);
            }
            expect += in;
        }
        ST_CHECK(t, v.selFaces.size() >= expect && v.selFaces.size() > 0, "Ctrl+drag over the view selects the faces in it (" + std::to_string(v.selFaces.size()) + ")");
    }
    stce::drag(v, 2, 2, 6, 6, MK_CONTROL);
    ST_CHECK(t, !ceHasSelection(v), "a rectangle over empty space selects nothing");
    stce::cmd(v, ID_CE_SELALL);
    ST_CHECK(t, v.selFaces.size() == v.col.faces.size(), "Select all faces");
    stColInvariants(t, v, "selection");

    // 7. delete faces, open edges, undo
    {
        std::set<size_t> some;
        for (size_t i = 0; i < v.col.faces.size(); i += 4) some.insert(i);
        v.selFaces = some;
        v.selBoxes.clear();
        ceRefresh(v);
        const auto before = stce::bytes(v.col);
        const size_t n = v.col.faces.size();
        SendMessageA(v.gl, WM_KEYDOWN, VK_DELETE, 0);
        ST_CHECK(t, v.col.faces.size() == n - some.size() && !ceHasSelection(v), "Del removes the selected faces");
        ST_CHECK(t, ceOpenEdges(v) == openEdges(v.col), "the red open edges follow the change");
        stce::cmd(v, ID_CE_UNDO);
        ST_CHECK(t, stce::bytes(v.col) == before, "undo brings them back exactly");
    }
    stColInvariants(t, v, "delete faces");

    // 8. dragging a selected box's side and a selected mesh face pushes / pulls them; letting go where it started changes nothing
    stce::cmd(v, ID_CE_ADDBOX);
    drawViewer(v);
    {
        ColBox& b = v.col.boxes.back();
        bool dragged = false;
        for (int face : {5, 1, 0, 3, 2}) {
            float p[3];
            for (int k = 0; k < 3; ++k) p[k] = (b.min[k] + b.max[k]) / 2;
            p[face / 2] = face % 2 ? b.max[face / 2] : b.min[face / 2];
            int x, y;
            if (!stce::screenOf(v, p, x, y)) continue;
            float o[3], d[3];
            ceRay(v, x, y, o, d);
            const ColHit h = pickCol(v.col, o, d);
            if (h.kind != ColHit::Box || h.index != int(v.col.boxes.size()) - 1 || h.boxFace != face) continue;
            v.selBoxes = {h.index};
            v.selBoxFace = face;
            ceRefresh(v);
            const ColBox before = v.col.boxes.back();
            const size_t undos = v.undo.size();
            stce::drag(v, x, y, x + 25, y - 35);
            const ColBox after = v.col.boxes.back();
            bool onlyThatSide = true;
            for (int k = 0; k < 3; ++k) {
                if (k == face / 2) continue;
                onlyThatSide = onlyThatSide && after.min[k] == before.min[k] && after.max[k] == before.max[k];
            }
            const float moved = face % 2 ? after.max[face / 2] - before.max[face / 2] : before.min[face / 2] - after.min[face / 2];
            ST_CHECK(t, onlyThatSide && (face % 2 ? after.min[face / 2] == before.min[face / 2] : after.max[face / 2] == before.max[face / 2]), "a drag moves only the side that was grabbed");
            ST_CHECK(t, moved != 0 && v.undo.size() == undos + 1, "a drag moves it and is one undo step");
            ST_CHECK(t, std::fabs(moved * 100 - std::round(moved * 100)) < 1e-2, "a drag moves in hundredths");
            const size_t undos2 = v.undo.size();
            const auto before2 = stce::bytes(v.col);
            drawViewer(v);
            int x2, y2;
            float q[3];
            for (int k = 0; k < 3; ++k) q[k] = (after.min[k] + after.max[k]) / 2;
            q[face / 2] = face % 2 ? after.max[face / 2] : after.min[face / 2];
            if (stce::screenOf(v, q, x2, y2)) {
                stce::mouse(v, WM_LBUTTONDOWN, x2, y2, MK_LBUTTON);
                stce::mouse(v, WM_MOUSEMOVE, x2 + 20, y2 + 20, MK_LBUTTON);
                stce::mouse(v, WM_MOUSEMOVE, x2, y2, MK_LBUTTON);
                stce::mouse(v, WM_LBUTTONUP, x2, y2, 0);
                ST_CHECK(t, v.undo.size() == undos2 && stce::bytes(v.col) == before2, "a drag that ends where it started changes nothing and adds no undo step");
            }
            dragged = true;
            break;
        }
        ST_CHECK(t, dragged, "a side of the new box could be grabbed");
    }
    stColInvariants(t, v, "drag");

    // 9. Save: a copy into the user's own region file of the same level, gta3.dat lists it once, Rockstar's file untouched
    const std::string region = "stce" + std::to_string(GetCurrentProcessId() % 100000);
    std::string rel, err;
    ST_CHECK(t, createRegionNamed(region, rel, err), "a region for the test");
    const std::string datWithRegion = slurp(datPath, ok);  // put back at the end (the region itself stays, as other sections' do)
    ceFillRegions(v, region);
    ceRefresh(v);
    const std::string rockstarFile = app.root + "/" + v.colSource;
    const std::string rockstarBytes = slurp(rockstarFile, ok);
    bool dat2ok = false;
    const int level = colFileLevel(slurp(datPath, dat2ok), v.colSource);
    const ColSaveTarget target = colSaveTarget(region, level);
    ST_CHECK(t, level >= 1 && stce::read(v.ce[ID_CE_STATUS]).find(target.relPath) != std::string::npos, "the status names the file Save will write");
    ST_CHECK(t, IsWindowEnabled(v.ce[ID_CE_SAVE]) && v.dirty, "Save is on when something is unsaved");
    stce::cmd(v, ID_CE_SAVE);
    const std::string own = app.root + "/" + target.relPath;
    std::vector<ColModel> saved;
    {
        const std::string sb = slurp(own, ok);
        ST_CHECK(t, ok && parseCol(std::vector<unsigned char>(sb.begin(), sb.end()), saved, err) && saved.size() == 1 && writeColl(saved[0]) == writeColl(v.col),
                 "Save writes the record into " + target.relPath);
    }
    ST_CHECK(t, !v.dirty && !IsWindowEnabled(v.ce[ID_CE_SAVE]) && v.colSource == target.relPath, "after Save nothing is unsaved and the window shows the own file");
    ST_CHECK(t, slurp(rockstarFile, ok) == rockstarBytes, "Rockstar's file is not changed");
    {
        const std::string dat = slurp(datPath, ok);
        size_t n = 0;
        for (size_t p = dat.find(target.datLine); p != std::string::npos; p = dat.find(target.datLine, p + 1)) ++n;
        ST_CHECK(t, n == 1 && dat.find(target.datLine) > dat.rfind("COLFILE " + std::to_string(level) + " DATA" + std::string(1, char(92)) + "MAPS" + std::string(1, char(92)) + "INDUSTSE"),
                 "gta3.dat lists the file once, after Rockstar's");
        ST_CHECK(t, colFileLevel(dat, target.relPath) == level, "with Rockstar's level");
    }
    ST_CHECK(t, writeColl(app.cols["hospitaland"]) == writeColl(v.col), "the editor uses the saved collision at once");
    // a second change saved again: the same file, still one gta3.dat line, one record
    stce::cmd(v, ID_CE_ADDSPHERE);
    stce::cmd(v, ID_CE_SAVE);
    {
        const std::string sb = slurp(own, ok);
        saved.clear();
        ST_CHECK(t, parseCol(std::vector<unsigned char>(sb.begin(), sb.end()), saved, err) && saved.size() == 1 && saved[0].spheres.size() == v.col.spheres.size(),
                 "saving again replaces the record in the own file");
        const std::string dat = slurp(datPath, ok);
        ST_CHECK(t, dat.find(target.datLine) == dat.rfind(target.datLine), "still one gta3.dat line");
    }
    // closed and opened again: the saved copy is what is shown (the last file wins, as in the game)
    const auto savedBytes = stce::bytes(v.col);
    SendMessageA(v.wnd, WM_CLOSE, 0, 0);
    updateViewers();
    ModelViewer* again = openColViewer(app.hwnd, hosp);
    ST_CHECK(t, again && stce::bytes(again->col) == savedBytes && ceLowerSlashes(again->colSource).find(ceLowerSlashes(target.relPath)) != std::string::npos,
             "opened again it shows the saved copy from the own file");
    if (again) {
        ceShowTab(*again, true);
        stce::cmd(*again, ID_CE_ADDBOX);
        ST_CHECK(t, again->dirty, "an edit");
        SendMessageA(again->wnd, WM_CLOSE, 0, 0);  // quiet: unsaved edits are dropped
        updateViewers();
        ST_CHECK(t, std::none_of(viewers.begin(), viewers.end(), [&](const std::unique_ptr<ModelViewer>& x) { return x.get() == again; }), "closing with unsaved edits closes (the test drops them)");
        ST_CHECK(t, stce::bytes(app.cols["hospitaland"]) == savedBytes, "unsaved edits never reach the map");
    }

    // 10. a loose .col with two models: edit the second, the first stays byte for byte, a backup is kept, picking is guarded
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    const std::string loose = std::string(tmp) + "med_selftest_col_" + std::to_string(GetCurrentProcessId()) + ".col";
    {
        ColModel a = hospOriginal, b = hospOriginal;
        a.name = "first";
        a.nameField.clear();
        b.name = "second";
        b.nameField.clear();
        std::vector<unsigned char> f = writeColl(a), g = writeColl(b);
        f.insert(f.end(), g.begin(), g.end());
        std::ofstream(loose, std::ios::binary).write((const char*)f.data(), std::streamsize(f.size()));
    }
    const std::string looseBefore = slurp(loose, ok);
    ST_CHECK(t, openLooseFile(app.hwnd, loose), "the loose .col opens");
    ModelViewer* lv = viewers.empty() ? nullptr : viewers.back().get();
    if (lv && lv->kind == 1 && lv->objectBox) {
        ceShowTab(*lv, true);
        SendMessageA(lv->objectBox, CB_SETCURSEL, 1, 0);
        SendMessageA(lv->wnd, WM_COMMAND, MAKEWPARAM(ID_MV_OBJECT, CBN_SELCHANGE), (LPARAM)lv->objectBox);
        ST_CHECK(t, lv->col.name == "second", "Model in file picks the second model");
        stce::cmd(*lv, ID_CE_ADDBOX);
        lv->lastMessage.clear();
        SendMessageA(lv->objectBox, CB_SETCURSEL, 0, 0);
        SendMessageA(lv->wnd, WM_COMMAND, MAKEWPARAM(ID_MV_OBJECT, CBN_SELCHANGE), (LPARAM)lv->objectBox);
        ST_CHECK(t, lv->col.name == "second" && !lv->lastMessage.empty() && SendMessageA(lv->objectBox, CB_GETCURSEL, 0, 0) == 1, "picking another model with unsaved edits is refused");
        ST_CHECK(t, stce::read(lv->ce[ID_CE_STATUS]).find("back into") != std::string::npos, "the status says Save writes back into the file");
        stce::cmd(*lv, ID_CE_SAVE);
        std::vector<ColModel> back;
        const std::string after = slurp(loose, ok);
        ST_CHECK(t, parseCol(std::vector<unsigned char>(after.begin(), after.end()), back, err) && back.size() == 2 && back[1].boxes.size() == lv->col.boxes.size(),
                 "Save writes the second model back into the file");
        ST_CHECK(t, after.compare(0, size_t(back[0].fileSize), looseBefore, 0, size_t(back[0].fileSize)) == 0, "the first model is unchanged byte for byte");
        ST_CHECK(t, slurp(loose + ".med-backup", ok) == looseBefore, "the file's original is kept as .med-backup");
        ST_CHECK(t, !IsWindowEnabled(lv->ce[ID_CE_REGION]), "no region choice for a loose file");
        SendMessageA(lv->wnd, WM_CLOSE, 0, 0);
        updateViewers();
    } else {
        ST_CHECK(t, false, "the loose .col window has a Model in file list");
    }
    fs::remove(loose, ec);
    fs::remove(loose + ".med-backup", ec);

    // 11. random steps with every action, checking everything after each
    ModelViewer* rv = openColViewer(app.hwnd, hosp);
    ST_CHECK(t, rv != nullptr, "the collision window opens again for the random steps (" + insp.lastMessage + ")");
    if (rv) {
        ModelViewer& r = *rv;
        ceShowTab(r, true);
        viewerLayout(r);
        drawViewer(r);
        unsigned seed = std::getenv("MED_SELFTEST_SEED") ? unsigned(std::atoi(std::getenv("MED_SELFTEST_SEED"))) : 12345u;
        auto rnd = [&](int n) { seed = seed * 1103515245u + 12345u; return n <= 0 ? 0 : int((seed >> 8) % unsigned(n)); };
        const int steps = std::max(300, (std::getenv("MED_SELFTEST_STEPS") ? std::atoi(std::getenv("MED_SELFTEST_STEPS")) : 1500) / 2);
        const char* dists[] = {"0.5", "0.01", "2", "abc", "", "-1", "0.25", "7"};
        const char* nums[] = {"0", "1", "-1.5", "0.25", "x", "", "3"};
        int changed[17] = {};
        for (int step = 0; step < steps; ++step) {
            const int op = rnd(17);
            std::string what;
            const auto beforeBytes = stce::bytes(r.col);
            const size_t undoN = r.undo.size();
            switch (op) {
                case 0: what = "add box"; if (r.col.boxes.size() < 12) stce::cmd(r, ID_CE_ADDBOX); break;
                case 1: what = "add sphere"; if (r.col.spheres.size() < 12) stce::cmd(r, ID_CE_ADDSPHERE); break;
                case 2: what = "click"; stce::click(r, rnd(r.vp[2]), rnd(r.vp[3]), rnd(3) == 0 ? MK_SHIFT : 0); break;
                case 3: what = "double-click"; { const int x = rnd(r.vp[2]), y = rnd(r.vp[3]); stce::mouse(r, WM_LBUTTONDBLCLK, x, y, MK_LBUTTON); stce::mouse(r, WM_LBUTTONUP, x, y, 0); } break;
                case 4: what = "rectangle"; stce::drag(r, rnd(r.vp[2]), rnd(r.vp[3]), rnd(r.vp[2]), rnd(r.vp[3]), MK_CONTROL | (rnd(2) ? MK_SHIFT : 0)); break;
                case 5: what = "drag"; {
                    int x = rnd(r.vp[2]), y = rnd(r.vp[3]);
                    float p[3];
                    bool grab = false;  // mostly grab the selection where it is on screen, as a user would
                    if (rnd(4) != 0) {
                        if (!r.selFaces.empty()) {
                            auto it = r.selFaces.begin();
                            std::advance(it, rnd(int(r.selFaces.size())));
                            const ColFace& f = r.col.faces[*it];
                            for (int k = 0; k < 3; ++k) p[k] = (r.col.vertices[f.a * 3 + k] + r.col.vertices[f.b * 3 + k] + r.col.vertices[f.c * 3 + k]) / 3;
                            grab = true;
                        } else if (r.selBoxes.size() == 1 && r.selBoxFace >= 0) {
                            const ColBox& b = r.col.boxes[size_t(*r.selBoxes.begin())];
                            for (int k = 0; k < 3; ++k) p[k] = (b.min[k] + b.max[k]) / 2;
                            p[r.selBoxFace / 2] = r.selBoxFace % 2 ? b.max[r.selBoxFace / 2] : b.min[r.selBoxFace / 2];
                            grab = true;
                        } else if (r.selSpheres.size() == 1) {
                            const ColSphere& sp = r.col.spheres[size_t(*r.selSpheres.begin())];
                            for (int k = 0; k < 3; ++k) p[k] = sp.center[k];
                            p[2] += sp.radius * 0.7f;
                            grab = true;
                        }
                    }
                    if (grab) stce::screenOf(r, p, x, y);
                    stce::drag(r, x, y, x + rnd(81) - 40, y + rnd(81) - 40, rnd(2) ? MK_SHIFT : 0);
                } break;
                case 6: what = "pull out"; stce::text(r, ID_CE_DIST, dists[rnd(8)]); stce::cmd(r, ID_CE_OUT); break;
                case 7: what = "push in"; stce::text(r, ID_CE_DIST, dists[rnd(8)]); stce::cmd(r, ID_CE_IN); break;
                case 8: what = "move";
                    stce::text(r, ID_CE_MX, nums[rnd(7)]); stce::text(r, ID_CE_MY, nums[rnd(7)]); stce::text(r, ID_CE_MZ, nums[rnd(7)]);
                    stce::cmd(r, ID_CE_MOVE); break;
                case 9: what = "material"; SendMessageA(r.ce[ID_CE_MAT], CB_SETCURSEL, WPARAM(rnd(kColMaterialCount)), 0); stce::cmd(r, ID_CE_MAT, CBN_SELCHANGE); break;
                case 10: what = "delete"; if (rnd(3) == 0) SendMessageA(r.gl, WM_KEYDOWN, VK_DELETE, 0); break;
                case 11: what = "undo"; stce::cmd(r, ID_CE_UNDO); break;
                case 12: what = "redo"; stce::cmd(r, ID_CE_REDO); break;
                case 13: what = "select side"; stce::cmd(r, ID_CE_SELSIDE); break;
                case 14: what = "select all"; if (rnd(3) == 0) stce::cmd(r, ID_CE_SELALL); break;
                case 15: what = "typed size"; for (int k = 0; k < 6; ++k) if (rnd(2)) stce::text(r, ID_CE_F0 + k, nums[rnd(7)]); stce::cmd(r, ID_CE_APPLY); break;
                case 16: what = "clear selection"; stce::cmd(r, ID_CE_SELNONE); break;
            }
            drawViewer(r);
            stColInvariants(t, r, "random step " + std::to_string(step) + " (" + what + ")");
            // every change is one undo step that gives back exactly what was there
            if (stce::bytes(r.col) != beforeBytes) ++changed[op];
            if (stce::bytes(r.col) != beforeBytes && op != 11 && op != 12) {
                ST_CHECK(t, r.undo.size() == std::min<size_t>(undoN + 1, 50) && (r.undo.size() < 50 ? stce::bytes(r.undo.back()) == beforeBytes : true),
                         "random step " + std::to_string(step) + " (" + what + "): one undo step holding the shape before");
            }
        }
        std::printf("collision editor: %d random steps; changes by add box %d, add sphere %d, drag %d, pull %d, push %d, move %d, material %d, delete %d, undo %d, "
                    "redo %d, typed size %d\n",
                    steps, changed[0], changed[1], changed[5], changed[6], changed[7], changed[8], changed[9], changed[10], changed[11], changed[12], changed[15]);
        while (!r.undo.empty()) stce::cmd(r, ID_CE_UNDO);
        SendMessageA(r.wnd, WM_CLOSE, 0, 0);
        updateViewers();
    }

    // put everything back: gta3.dat, the region's files, the editor's collision table
    {
        std::ofstream(datPath, std::ios::binary | std::ios::trunc).write(datWithRegion.data(), std::streamsize(datWithRegion.size()));
    }
    fs::remove(app.root + "/" + target.relPath, ec);
    (void)datBefore;
    app.cols["hospitaland"] = hospOriginal;
    app.colFiles = colFilesBefore;
    ST_CHECK(t, app.cols.size() == colsBefore, "the editor's collision table is back");
    ST_CHECK(t, stMapState() == mapBefore, "editing collision changes no placement");
    ceQuiet = false;
}

void runBookmarksSelfTest(SelfTest& t) {
    const std::string realFile = app.bookmarkFile;
    const std::vector<Bookmark> realList = app.bookmarks;
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    app.bookmarkFile = std::string(tmp) + "med_selftest_bookmarks_" + std::to_string(GetCurrentProcessId()) + ".ini";  // own file per run
    const std::string medText =
        "<bookmark id=0>GTA III - INDUSTSE - Portland Hospital</bookmark><location id=0>1145.68:-576.017:13.528:15.6316:204.8:358.4:0</location>\r\n"
        "<bookmark id=1>GTA VC - OCEANDRV - Ocean Hotel</bookmark><location id=1>229.921:-1262.81:19.484:25.0519:102:359.2:0</location>\r\n"
        "<bookmark id=2>San Andreas - LAe2 - Grove Street</bookmark><location id=2>2494.15:-1669.84:51.0703:55.3825:5.8:358:0</location>\r\n\r\n";
    app.bookmarks = parseBookmarks(medText);
    bm.quiet = true;
    bmSave();
    showBookmarksWindow(app.hwnd);
    ST_CHECK(t, bm.wnd && ListView_GetItemCount(bm.list) == 3, "the window opens with MEd's three bookmarks");
    stBookmarkInvariants(t, "bookmarks start");

    // add the camera
    app.cam.x = 12.5f; app.cam.y = -34.25f; app.cam.z = 56.0f; app.cam.h = 7.0f; app.cam.yaw = 90.0f; app.cam.pitch = 30.0f;
    ST_CHECK(t, bmAdd("Test spot"), "a bookmark can be added");
    ST_CHECK(t, app.bookmarks.size() == 4 && app.bookmarks[3].x == 12.5 && app.bookmarks[3].yaw == 90 && bmSelected() == 3, "it holds the camera and is selected");
    stBookmarkInvariants(t, "after add");
    for (const char* bad : {"", "   ", "a<b", "a>b", "two\nlines"}) {
        bm.lastMessage.clear();
        ST_CHECK(t, !bmAdd(bad) && !bm.lastMessage.empty() && app.bookmarks.size() == 4, std::string("the name '") + bad + "' is refused with a message");
    }
    ST_CHECK(t, bmAdd("  spaces around  ") && app.bookmarks.back().name == "spaces around", "spaces around a name are trimmed");
    stBookmarkInvariants(t, "after bad names");

    // rename
    ST_CHECK(t, bmRename(0, "Hospital (renamed)") && app.bookmarks[0].name == "Hospital (renamed)", "rename works");
    ST_CHECK(t, !bmRename(0, "<x>") && app.bookmarks[0].name == "Hospital (renamed)", "a bad new name is refused");
    ST_CHECK(t, !bmRename(99, "x") && !bmRename(-1, "x"), "renaming a row that does not exist does nothing");
    stBookmarkInvariants(t, "after rename");

    // move up / down, with the ends
    ST_CHECK(t, !bmMove(0, -1), "the first one cannot move up");
    ST_CHECK(t, !bmMove(int(app.bookmarks.size()) - 1, 1), "the last one cannot move down");
    const std::string first = app.bookmarks[0].name, second = app.bookmarks[1].name;
    ST_CHECK(t, bmMove(0, 1) && app.bookmarks[0].name == second && app.bookmarks[1].name == first && bmSelected() == 1, "move down swaps and keeps it selected");
    ST_CHECK(t, bmMove(1, -1) && app.bookmarks[0].name == first, "move up swaps back");
    stBookmarkInvariants(t, "after moves");

    // go to: the camera takes the values
    bmGoTo(2);
    ST_CHECK(t, app.cam.x == float(app.bookmarks[2].x) && app.cam.pitch == float(app.bookmarks[2].pitch) && app.cam.h == float(app.bookmarks[2].h), "Move to location sets the camera");
    bmGoTo(-1);
    bmGoTo(1000);
    // keys: Enter flies, Delete removes
    ListView_SetItemState(bm.list, 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    MSG m = {};
    m.message = WM_KEYDOWN;
    m.hwnd = bm.list;
    m.wParam = VK_RETURN;
    bmHandleKey(m);
    ST_CHECK(t, app.cam.x == float(app.bookmarks[1].x), "Enter flies to the selected bookmark");
    const size_t before = app.bookmarks.size();
    m.wParam = VK_DELETE;
    bmHandleKey(m);
    ST_CHECK(t, app.bookmarks.size() == before - 1, "Delete removes the selected bookmark");
    stBookmarkInvariants(t, "after keys");

    // the buttons, through the window's own messages
    SendMessageA(bm.wnd, WM_COMMAND, MAKEWPARAM(ID_BM_GO, BN_CLICKED), (LPARAM)GetDlgItem(bm.wnd, ID_BM_GO));
    SendMessageA(bm.wnd, WM_COMMAND, MAKEWPARAM(ID_BM_DOWN, BN_CLICKED), (LPARAM)GetDlgItem(bm.wnd, ID_BM_DOWN));
    SendMessageA(bm.wnd, WM_COMMAND, MAKEWPARAM(ID_BM_UP, BN_CLICKED), (LPARAM)GetDlgItem(bm.wnd, ID_BM_UP));
    SendMessageA(bm.wnd, WM_COMMAND, MAKEWPARAM(ID_BM_REMOVE, BN_CLICKED), (LPARAM)GetDlgItem(bm.wnd, ID_BM_REMOVE));
    stBookmarkInvariants(t, "after buttons");

    // empty list: every action is harmless, the window says what to do
    while (!app.bookmarks.empty()) bmRemove(0);
    char st[300] = "";
    GetWindowTextA(bm.status, st, sizeof st);
    ST_CHECK(t, std::string(st).find("No bookmarks yet") == 0, "an empty list explains how to add one");
    for (int id : {ID_BM_RENAME, ID_BM_REMOVE, ID_BM_UP, ID_BM_DOWN, ID_BM_GO}) SendMessageA(bm.wnd, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)GetDlgItem(bm.wnd, id));
    stBookmarkInvariants(t, "empty list");

    // a file that cannot be written: a message, the list is kept
    const std::string goodFile = app.bookmarkFile;
    app.bookmarkFile = std::string(tmp);  // a folder, not a file
    bm.lastMessage.clear();
    ST_CHECK(t, bmAdd("kept in memory") && !bm.lastMessage.empty() && app.bookmarks.size() == 1, "a failed save is reported and the bookmark is kept");
    app.bookmarkFile = goodFile;
    bmSave();

    // closing and opening the window again
    DestroyWindow(bm.wnd);
    ST_CHECK(t, bm.wnd == nullptr && bm.list == nullptr, "closing the window clears it");
    bmFill(0);  // no window: harmless
    showBookmarksWindow(app.hwnd);
    ST_CHECK(t, bm.wnd && ListView_GetItemCount(bm.list) == 1, "opening again shows the list");
    stBookmarkInvariants(t, "reopened");

    // random use
    unsigned seed = 99u + (std::getenv("MED_SELFTEST_SEED") ? unsigned(std::atoi(std::getenv("MED_SELFTEST_SEED"))) : 0u);
    auto rnd = [&](int n) { seed = seed * 1103515245u + 54321u; return n <= 0 ? 0 : int((seed >> 8) % unsigned(n)); };
    const char* names[] = {"Docks", "Hospital", "", "<bad>", "Bridge view", "x", "  padded  ", "Name with - dashes", "a>b", "Liberty"};
    for (int step = 0; step < 800; ++step) {
        const int n = int(app.bookmarks.size());
        const int op = rnd(8);
        std::string what;
        switch (op) {
            case 0: what = "add"; app.cam.x = float(rnd(4000) - 2000); app.cam.yaw = float(rnd(360)); bmAdd(names[rnd(10)]); break;
            case 1: what = "rename"; bmRename(n ? rnd(n) : 0, names[rnd(10)]); break;
            case 2: what = "remove"; bmRemove(n ? rnd(n) : -1); break;
            case 3: what = "move"; bmMove(n ? rnd(n) : 0, rnd(2) ? 1 : -1); break;
            case 4: what = "go"; bmGoTo(n ? rnd(n) : 0); break;
            case 5: what = "select"; if (n) ListView_SetItemState(bm.list, rnd(n), LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); break;
            case 6: what = "button"; { const int id = ID_BM_RENAME + 1 + rnd(4); SendMessageA(bm.wnd, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)GetDlgItem(bm.wnd, id)); } break;
            case 7: what = "reload from disk"; { bool ok; app.bookmarks = parseBookmarks(slurp(app.bookmarkFile, ok)); bmFill(0); } break;
        }
        const int failuresBefore = t.failures;
        stBookmarkInvariants(t, "bookmarks random step " + std::to_string(step) + " (" + what + ")");
        if (t.failures > failuresBefore + 10) break;
    }

    DestroyWindow(bm.wnd);
    bm.quiet = false;
    DeleteFileA(app.bookmarkFile.c_str());
    app.bookmarkFile = realFile;  // the user's file was never written
    app.bookmarks = realList;
}

int runPlacementsSelfTest() {
    SelfTest t;
    std::printf("Placements tab self-test on %s (%zu placements, %zu regions)\n", app.root.c_str(), app.instances.size(), regions.size());
    if (const char* only = std::getenv("MED_SELFTEST_ONLY"); only && (std::string(only) == "import" || std::string(only) == "coledit")) {  // one section, for quick runs
        if (std::string(only) == "import") runImportSelfTest(t);
        else runColEditSelfTest(t);
        std::printf("self-test: %d checks, %d failures\n", t.checks, t.failures);
        std::fflush(stdout);
        return t.failures == 0 ? 0 : 1;
    }
    showLeftTab(1);
    ST_CHECK(t, !app.showEmptyRegions, "View > Show hidden regions is off at start");
    setShowEmptyRegions(true);  // the sections below use the empty CULL.IPL
    auto fileOf = [](const std::string& name) {
        for (const Region& r : regions) if (toLower(r.name) == toLower(name)) return r.file;
        return -1;
    };
    const int props = fileOf("props.IPL"), cull = fileOf("CULL.IPL"), comsw = fileOf("COMSW.IPL");
    ST_CHECK(t, props >= 0 && cull >= 0 && comsw >= 0, "the test needs props.IPL, CULL.IPL and COMSW.IPL");

    // 1. nothing picked: empty list and a hint
    stInvariants(t, "start");
    ST_CHECK(t, pl.rows.empty() || pl.region >= 0, "nothing should be listed before a region is picked");
    ST_CHECK(t, stui::text(pl.count).find("Pick a region") == 0, "the hint to pick a region is missing");

    // 2. a region, then searches
    stui::region(props);
    ST_CHECK(t, pl.region == props && pl.rows.size() == 456, "props.IPL lists its 456 objects (" + std::to_string(pl.rows.size()) + ")");
    stInvariants(t, "props");
    stui::search("cone");
    ST_CHECK(t, pl.rows.size() == 17, "props + cone = 17 (" + std::to_string(pl.rows.size()) + ")");
    stInvariants(t, "props cone");
    stui::search("  CONE ");
    ST_CHECK(t, pl.rows.size() == 17, "upper case and spaces around the search are ignored");
    stui::search("1360");
    ST_CHECK(t, pl.rows.size() == 17, "the model id 1360 finds the 17 cones of props");
    stui::search("zzzz_nothing");
    ST_CHECK(t, pl.rows.empty() && stui::text(pl.count).find("Nothing matches") == 0, "no match: empty list and a hint");
    stInvariants(t, "no match");
    stui::search("99999");
    ST_CHECK(t, pl.rows.empty(), "an id that does not exist finds nothing");
    stui::search(std::string(300, 'x'));
    ST_CHECK(t, pl.rows.empty(), "a very long search does not crash and finds nothing");
    stui::search("%s%d\\'\"[]*?");
    stInvariants(t, "odd characters");
    stui::search("");
    ST_CHECK(t, pl.rows.size() == 456, "clearing the search lists the whole region again");

    // 3. an empty region
    stui::region(cull);
    ST_CHECK(t, pl.rows.empty() && stui::text(pl.count).find("This region has no objects") == 0, "an empty region says so");
    stInvariants(t, "empty region");
    ST_CHECK(t, stui::key(pl.search, VK_F3), "F3 in the search box is handled");
    ST_CHECK(t, stui::key(pl.list, VK_RETURN), "Enter on an empty list does nothing harmful");
    stInvariants(t, "keys on an empty list");

    // 4. all regions
    stui::scope(true);
    stui::search("hospital");
    ST_CHECK(t, pl.rows.size() == 5, "all regions + hospital = 5 (" + std::to_string(pl.rows.size()) + ")");
    stInvariants(t, "all hospital");
    stui::search("1360");
    const size_t cones = pl.rows.size();
    ST_CHECK(t, cones >= 17, "all regions + 1360 lists every cone");
    stInvariants(t, "all 1360");

    // 5. sorting, every column both ways
    for (int col = 0; col < 6; ++col)
        for (int twice = 0; twice < 2; ++twice) {
            stui::header(col);
            ST_CHECK(t, pl.sortCol == col && pl.sortDesc == (twice == 1), "a header click sorts, a second click reverses");
            stInvariants(t, "sort column " + std::to_string(col) + (twice ? " descending" : " ascending"));
        }
    pl.sortCol = -1;  // back to file order for the rest
    plRefill();

    // 6. stepping: Next from nothing starts at the first match, wraps around, Previous goes back
    app.selected.clear();
    plMirrorSelection();
    stui::key(pl.search, VK_F3);
    ST_CHECK(t, app.selected.size() == 1 && app.selected[0] == pl.rows[0], "F3 from nothing selects the first match");
    const float camX = app.cam.x, camY = app.cam.y;
    ST_CHECK(t, std::fabs(camX - app.instances[pl.rows[0]].x) < 0.01 && std::fabs(camY - (app.instances[pl.rows[0]].y - 20)) < 0.01, "the camera flew to it");
    for (size_t k = 1; k < pl.rows.size(); ++k) stui::key(pl.search, VK_F3);
    ST_CHECK(t, app.selected.size() == 1 && app.selected[0] == pl.rows.back(), "F3 reaches the last match");
    stui::key(pl.search, VK_F3);
    ST_CHECK(t, app.selected.size() == 1 && app.selected[0] == pl.rows[0], "F3 after the last wraps to the first");
    stui::key(pl.search, VK_F3, true);
    ST_CHECK(t, app.selected.size() == 1 && app.selected[0] == pl.rows.back(), "Shift+F3 from the first wraps to the last");
    stui::key(pl.search, VK_RETURN);
    ST_CHECK(t, app.selected.size() == 1 && app.selected[0] == pl.rows[0], "Enter in the search box goes to the next match");
    SendMessageA(pl.panel, WM_COMMAND, MAKEWPARAM(ID_PL_NEXT, BN_CLICKED), (LPARAM)GetDlgItem(pl.panel, ID_PL_NEXT));
    ST_CHECK(t, app.selected.size() == 1 && app.selected[0] == pl.rows[1], "the > button goes to the next match");
    SendMessageA(pl.panel, WM_COMMAND, MAKEWPARAM(ID_PL_PREV, BN_CLICKED), (LPARAM)GetDlgItem(pl.panel, ID_PL_PREV));
    ST_CHECK(t, app.selected.size() == 1 && app.selected[0] == pl.rows[0], "the < button goes back");
    stInvariants(t, "stepping");

    // 7. clicking rows selects in 3D; several; double-click flies
    stui::clickRows({2});
    ST_CHECK(t, app.selected == std::vector<int>{pl.rows[2]}, "a click on a row selects that object");
    stui::clickRows({1, 3, 5});
    ST_CHECK(t, app.selected.size() == 3, "Ctrl+click selects several");
    stInvariants(t, "several rows");
    stui::doubleClick(4);
    ST_CHECK(t, app.selected == std::vector<int>{pl.rows[4]} && std::fabs(app.cam.x - app.instances[pl.rows[4]].x) < 0.01, "a double-click selects and flies there");
    stui::key(pl.list, VK_RETURN);
    stInvariants(t, "double-click");
    stui::doubleClick(-1);
    stui::doubleClick(int(pl.rows.size()) + 5);
    stInvariants(t, "double-click outside the rows");

    // 8. a hidden region: picking its object in the list shows the region again
    stui::scope(false);
    stui::search("");
    stui::region(props);
    setRegion(props, false);
    fillRegionList();
    stui::clickRows({0});
    ST_CHECK(t, app.regionOn[props] && app.selected == std::vector<int>{pl.rows[0]}, "selecting an object of a hidden region ticks the region on");
    setRegion(props, false);
    fillRegionList();
    stui::doubleClick(1);
    ST_CHECK(t, app.regionOn[props], "flying to an object of a hidden region ticks the region on");
    stInvariants(t, "hidden region");

    // 9. selecting in 3D switches the region and highlights the row
    {
        int other = -1;
        for (size_t i = 0; i < app.instances.size(); ++i) if (app.instFile[i] == comsw) { other = int(i); break; }
        app.selected.assign(1, other);
        plSyncFromSelection();
        ST_CHECK(t, pl.region == comsw, "selecting an object of another region in 3D switches the listed region");
        ST_CHECK(t, stui::listSelection().size() == 1 && pl.rows[stui::listSelection()[0]] == other, "and highlights its row");
        stInvariants(t, "3D selection");
        stui::search("cone");  // a search that hides the selected object: no highlight, selection kept
        ST_CHECK(t, app.selected == std::vector<int>{other} || app.instances[other].model.find("cone") != std::string::npos, "filtering does not change the selection");
        stInvariants(t, "selection hidden by the search");
        stui::search("");
    }

    // 10. edits keep the list right
    stui::region(props);
    stui::clickRows({0});
    const size_t before = app.instances.size();
    duplicateSelected();
    ST_CHECK(t, app.instances.size() > before, "duplicate adds placements");
    stInvariants(t, "after duplicate");
    undoAction();
    ST_CHECK(t, app.instances.size() == before, "undo removes them");
    stInvariants(t, "after undo");
    redoAction();
    stInvariants(t, "after redo");
    undoAction();
    stui::clickRows({0, 1});
    copySelected();
    pasteClipboard(props, 0, 0);
    stInvariants(t, "after paste");
    stui::clickRows({3});
    deleteSelected();
    stInvariants(t, "after a permanent delete (indices shift)");
    stui::clickRows({3});
    sinkSelected();
    stInvariants(t, "after delete below the map");
    stui::clickRows({4});
    moveSelectedToRegion(comsw);
    stInvariants(t, "after move to another region");
    {
        int id = -1;
        for (const auto& kv : app.defs) if (kv.second.model == "trafficcone") id = kv.first;
        addNewObject(id, props, 1.0, 2.0, 3.0);
        stInvariants(t, "after new object");
    }
    for (int k = 0; k < 6; ++k) undoAction();
    stInvariants(t, "after six undos");
    for (int k = 0; k < 6; ++k) redoAction();
    stInvariants(t, "after six redos");

    // 11. a new region appears in the picker and can be listed
    {
        std::string rel, err;
        const size_t regionsBefore = regions.size();
        const bool made = createRegionNamed("selftest", rel, err);
        ST_CHECK(t, made, "creating a region works (" + err + ")");
        ST_CHECK(t, regions.size() == regionsBefore + 1, "the new region is in the region list");
        stInvariants(t, "after New region");
        stui::region(regions.back().file);
        ST_CHECK(t, pl.region == regions.back().file && pl.rows.empty(), "the new region can be picked and is empty");
        stInvariants(t, "new region picked");
        ST_CHECK(t, placementsRegion() == regions.back().file, "the picked region is the destination of New Object and Paste");
    }

    // 11b. View > Show hidden regions: off leaves out the regions that were empty at load, never a new one
    {
        auto inPicker = [](int file) {
            const int n = int(SendMessageA(pl.regionBox, CB_GETCOUNT, 0, 0));
            for (int i = 0; i < n; ++i) if (int(SendMessageA(pl.regionBox, CB_GETITEMDATA, i, 0)) == file) return true;
            return false;
        };
        auto inScene = [](int file) {
            for (int r : sceneRows) if (regions[r].file == file) return true;
            return false;
        };
        int zon = -1;
        for (const Region& r : regions) if (toLower(r.name) == "gta3.zon") zon = r.file;
        const int made = regions.back().file;  // the region made in section 11, empty
        stui::region(cull);
        ST_CHECK(t, inPicker(cull) && inScene(cull) && inPicker(zon), "with the option on, the empty regions are listed");
        doCommand(app.hwnd, ID_TB_VIEWEMPTY);  // the menu entry: toggles it off
        ST_CHECK(t, !app.showEmptyRegions, "the menu entry toggles the option off");
        ST_CHECK(t, !inPicker(cull) && !inScene(cull) && !inPicker(zon) && !inScene(zon), "off: GTA3.ZON and CULL.IPL are left out of both lists");
        ST_CHECK(t, inPicker(made) && inScene(made), "off: a region made with New region stays listed although it is empty");
        ST_CHECK(t, pl.region == -1, "the picked region (CULL.IPL) was unpicked when it was hidden");
        stInvariants(t, "hidden regions off");
        for (int row = 0; row < int(sceneRows.size()); ++row) {  // the Scene rows still lead to the right regions
            LVITEMA li = {};
            li.mask = LVIF_PARAM;
            li.iItem = row;
            SendMessageA(scene.list, LVM_GETITEMA, 0, (LPARAM)&li);
            if (int(li.lParam) != regions[sceneRowRegion(row)].file) { ST_CHECK(t, false, "a Scene row leads to the wrong region"); break; }
        }
        const int file = regions[sceneRowRegion(0)].file;  // ticking a row switches its own region
        const bool was = app.regionOn[file] != 0;
        ListView_SetCheckState(scene.list, 0, was ? FALSE : TRUE);
        ST_CHECK(t, (app.regionOn[file] != 0) == !was, "the check box of a row switches its own region");
        ListView_SetCheckState(scene.list, 0, was ? TRUE : FALSE);
        doCommand(app.hwnd, ID_TB_VIEWEMPTY);
        ST_CHECK(t, app.showEmptyRegions && inPicker(cull) && inScene(cull), "clicked again: the empty regions are back");
        stInvariants(t, "hidden regions on again");
    }

    // 12. Esc clears the search; Ctrl+F is not tested here (it needs the real keyboard state)
    stui::search("cone");
    stui::key(pl.search, VK_ESCAPE);
    ST_CHECK(t, stui::text(pl.search).empty(), "Esc clears the search");
    stInvariants(t, "Esc");

    // the properties panel (its own scripted sections and random sequence)
    runInspectorSelfTest(t);
    // the Item tab
    runItemSelfTest(t);
    showLeftTab(1);
    // the Bookmarks window (on a temporary file)
    runBookmarksSelfTest(t);
    // Extract Object (copies into a temporary folder)
    runExtractSelfTest(t);
    // Replace Object (building and LOD stand-in as a pair)
    runReplaceSelfTest(t);
    // the road builder (nodes, strip, preview, attach)
    runRoadSelfTest(t);
    // DFF Properties (MEd's model viewer)
    runDffViewerSelfTest(t);
    // Collision Object Properties and TXD Properties
    runColTxdViewerSelfTest(t);
    // loose .dff / .txd / .col files from disk
    runLooseFileSelfTest(t);
    // Import Model (real imports into the test's game copy, put back afterwards)
    runImportSelfTest(t);
    // the collision editor (saves into a region it makes, put back afterwards)
    runColEditSelfTest(t);

    // 13. a long random sequence of everything
    unsigned seed = std::getenv("MED_SELFTEST_SEED") ? unsigned(std::atoi(std::getenv("MED_SELFTEST_SEED"))) : 12345u;  // other random sequences
    auto rnd = [&](int n) { seed = seed * 1103515245u + 12345u; return n <= 0 ? 0 : int((seed >> 8) % unsigned(n)); };
    const char* terms[] = {"", "cone", "lod", "1360", "a", "x", "bar", "_", "hospital", "zzz", "LOD", "938"};
    int id = -1;
    for (const auto& kv : app.defs) if (kv.second.model == "trafficcone") id = kv.first;
    const int steps = std::getenv("MED_SELFTEST_STEPS") ? std::atoi(std::getenv("MED_SELFTEST_STEPS")) : 1500;
    std::printf("random sequence: seed %u, %d steps\n", seed, steps);
    for (int step = 0; step < steps; ++step) {
        const int op = rnd(19);
        const int rows = int(pl.rows.size());
        std::string what;
        switch (op) {
            case 0: what = "search"; stui::search(terms[rnd(12)]); break;
            case 1: what = "region"; stui::region(regions[rnd(int(regions.size()))].file); break;
            case 2: what = "scope"; stui::scope(rnd(2) == 1); break;
            case 3: what = "sort"; stui::header(rnd(6)); break;
            case 4: what = "click"; if (rows) stui::clickRows({rnd(rows)}); break;
            case 5: what = "click several"; if (rows) stui::clickRows({rnd(rows), rnd(rows), rnd(rows)}); break;
            case 6: what = "double-click"; stui::doubleClick(rows ? rnd(rows) : 0); break;
            case 7: what = "F3"; stui::key(pl.search, VK_F3, rnd(2) == 1); break;
            case 8: what = "undo"; undoAction(); break;
            case 9: what = "redo"; redoAction(); break;
            case 10: what = "duplicate"; duplicateSelected(); break;
            case 11: what = "delete below the map"; sinkSelected(); break;
            case 12: what = "delete permanently"; deleteSelected(); break;
            case 13: what = "copy and paste"; copySelected(); if (pl.region >= 0) pasteClipboard(pl.region, rnd(100), rnd(100)); break;
            case 14: what = "move to region"; if (!app.selected.empty()) moveSelectedToRegion(regions[rnd(int(regions.size()))].file); break;
            case 15: what = "new object"; if (pl.region >= 0 && id >= 0) addNewObject(id, pl.region, rnd(100), rnd(100), 10); break;
            case 16: what = "select in 3D"; if (!app.instances.empty()) app.selected.assign(1, rnd(int(app.instances.size()))); break;
            case 17: what = "show hidden regions"; setShowEmptyRegions(rnd(2) == 1); break;
            case 18: {  // replace with a random model (half of the time one that has a LOD model)
                what = "replace";
                std::vector<int> all;
                for (const auto& kv : app.defs) if (rnd(2) == 0 || lodModelIdFor(kv.first) >= 0) all.push_back(kv.first);
                if (!all.empty() && !app.selected.empty()) setModelId(app.hwnd, all[size_t(rnd(int(all.size())))]);
                break;
            }
        }
        const int failuresBefore = t.failures;
        stInvariants(t, "random step " + std::to_string(step) + " (" + what + ")");
        if (t.failures > failuresBefore + 20) break;  // one broken step should not flood the output
    }

    std::printf("self-test: %d checks, %d failures\n", t.checks, t.failures);
    std::fflush(stdout);
    return t.failures == 0 ? 0 : 1;
}

LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (std::getenv("MED_DEBUG") && (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_LBUTTONDBLCLK || msg == WM_RBUTTONDOWN)) {
        std::printf("mouse message 0x%04x at (%d, %d)\n", unsigned(msg), int(short(LOWORD(l))), int(short(HIWORD(l))));
        std::fflush(stdout);
    }
    switch (msg) {
        case WM_SIZE:
            if (app.toolbar && app.menubar) {  // two rows, one under the other
                MoveWindow(app.menubar, 0, 0, LOWORD(l), app.menubarH, TRUE);
                MoveWindow(app.toolbar, 0, app.menubarH, LOWORD(l), app.toolbarH - app.menubarH, TRUE);
            }
            app.viewX = app.leftW;
            app.width = std::max<int>(LOWORD(l) - app.inspectorW - app.leftW, 1);
            app.height = std::max<int>(HIWORD(l) - app.toolbarH, 1);  // the 3D view is the part below the toolbar, left of the panel
            if (app.leftTabs) MoveWindow(app.leftTabs, 0, app.toolbarH, app.leftW, app.leftTabH, TRUE);
            if (app.scenePanel) MoveWindow(app.scenePanel, 0, app.toolbarH + app.leftTabH, app.leftW, std::max<int>(HIWORD(l) - app.toolbarH - app.leftTabH, 1), TRUE);
            if (pl.panel) MoveWindow(pl.panel, 0, app.toolbarH + app.leftTabH, app.leftW, std::max<int>(HIWORD(l) - app.toolbarH - app.leftTabH, 1), TRUE);
            if (it.panel) MoveWindow(it.panel, 0, app.toolbarH + app.leftTabH, app.leftW, std::max<int>(HIWORD(l) - app.toolbarH - app.leftTabH, 1), TRUE);
            if (app.inspector) MoveWindow(app.inspector, LOWORD(l) - app.inspectorW, app.toolbarH, app.inspectorW, std::max<int>(HIWORD(l) - app.toolbarH, 1), TRUE);
            if (roadStripOpen()) closeRoadStrip();  // reopened by clicking a node again, at the new size
            // repaint the panels completely: without this, maximising or resizing left old pixels where controls had been
            RedrawWindow(h, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            return 0;
        case WM_NOTIFY: {
            const NMHDR* nh = reinterpret_cast<const NMHDR*>(l);
            if (nh->hwndFrom == app.leftTabs && nh->code == TCN_SELCHANGE) {
                showLeftTab(TabCtrl_GetCurSel(app.leftTabs));
                return 0;
            }
            if (nh->hwndFrom == app.menubar && nh->code == TBN_DROPDOWN) {
                showToolbarMenu(h, reinterpret_cast<const NMTOOLBARA*>(l)->iItem);
                return TBDDRET_DEFAULT;
            }
            return 0;
        }
        case WM_COMMAND:
            if (std::getenv("MED_DEBUG")) { std::printf("WM_COMMAND id %d code %d from %p\n", int(LOWORD(w)), int(HIWORD(w)), (void*)l); std::fflush(stdout); }
            if (LOWORD(w) >= ID_TB_SAVE && LOWORD(w) <= ID_TB_VIEWPATHS) doCommand(h, LOWORD(w));
            return 0;
        case WM_KEYDOWN:
            if (w < 256) app.keys[w] = true;
            if (w == VK_DELETE) {
                if (GetKeyState(VK_SHIFT) & 0x8000) deleteSelected();  // really remove from the file
                else sinkSelected();                                    // move below the map
                return 0;
            }
            if (w == VK_SPACE && !app.selected.empty()) {  // MEd: Space = Centre on Selected Object
                centreOnObject(app.selected[0]);
                return 0;
            }
            if (w == VK_ESCAPE && roadStripOpen()) { closeRoadStrip(); return 0; }  // Esc first closes the road strip
            if (w == VK_ESCAPE) { app.selected.clear(); app.history.endSession(); }  // MEd: Esc cancels the selection
            if (w == 'S' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                saveFromUi(h);
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && w == 'Z') {
                if (GetKeyState(VK_SHIFT) & 0x8000) redoAction(); else undoAction();
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && w == 'R') {  // road builder: attach at the first open end
                doCommand(h, ID_TB_ROADATTACH);
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && w == 'L') {  // LOD view on or off
                app.lodView = !app.lodView;
                SendMessageA(app.toolbar, TB_CHECKBUTTON, ID_TB_LODVIEW, MAKELONG(app.lodView ? TRUE : FALSE, 0));
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && w == 'B') {  // the Bookmarks window
                showBookmarksWindow(h);
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && w == 'F' && pl.search) {  // find: the Placements tab's search box
                showLeftTab(1);
                SetFocus(pl.search);
                SendMessageA(pl.search, EM_SETSEL, 0, -1);
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && w == 'C') {
                copySelected();
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && w == 'V') {
                pasteAt(h, app.viewX + app.width / 2, app.toolbarH + app.height / 2);
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && w == 'D') {
                duplicateSelected();
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && w == 'Y') {
                redoAction();
                return 0;
            }
            if (w == VK_F7) { showCheckWindow(h); return 0; }
            if (w == VK_F2) app.conjugate = !app.conjugate;
            if (w == VK_F3) { app.prelit = (app.prelit + 1) % 3; for (auto& m : app.models) if (m.second.list) glDeleteLists(m.second.list, 1); app.models.clear(); }
            return 0;
        case WM_SYSKEYDOWN:  // Alt combinations belong to the editor, not the system menu
            if (w == VK_F4) break;
            if (w < 256) app.keys[w] = true;
            return 0;
        case WM_SYSKEYUP:
            if (w < 256) app.keys[w] = false;
            return 0;
        case WM_MOUSEWHEEL:
            cameraWheel(app.cam, GET_WHEEL_DELTA_WPARAM(w), (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            return 0;
        case WM_DROPFILES: {  // .dff / .txd / .col files dropped on the window open in the viewers
            HDROP drop = reinterpret_cast<HDROP>(w);
            const UINT n = DragQueryFileA(drop, 0xFFFFFFFF, nullptr, 0);
            std::vector<std::string> files;
            for (UINT i = 0; i < n; ++i) {
                char path[MAX_PATH];
                if (DragQueryFileA(drop, i, path, MAX_PATH)) files.push_back(path);
            }
            DragFinish(drop);
            for (const std::string& f : files) openLooseFile(h, f);
            return 0;
        }
        case WM_RBUTTONDOWN:
            SetFocus(h);
            showObjectMenu(h, int(short(LOWORD(l))), int(short(HIWORD(l))));  // client coordinates, converted inside
            return 0;
        case WM_LBUTTONDBLCLK:
            doubleClickSelect(int(short(LOWORD(l))), int(short(HIWORD(l))) - app.toolbarH, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            return 0;
        case WM_KEYUP:
            if (w < 256) app.keys[w] = false;
            return 0;
        case WM_LBUTTONDOWN:
            SetFocus(h);
            app.dragging = true;
            GetCursorPos(&app.lastMouse);
            app.downMouse = app.lastMouse;
            SetCapture(h);
            // Alt + left button with a selection drags the selected objects over the plane z - h.
            app.movingObjects = (GetKeyState(VK_MENU) & 0x8000) && !app.selected.empty() &&
                                unprojectToPlane(int(short(LOWORD(l))), int(short(HIWORD(l))) - app.toolbarH, app.lastPick);
            return 0;
        case WM_LBUTTONUP: {
            app.dragging = false;
            POINT up;
            GetCursorPos(&up);
            const bool click = std::abs(up.x - app.downMouse.x) <= 3 && std::abs(up.y - app.downMouse.y) <= 3;
            if (click && !app.movingObjects) roadClick(int(short(LOWORD(l))), int(short(HIWORD(l))) - app.toolbarH);  // a road node opens the piece strip
            if (app.movingObjects) plRefreshAfterEdit();
            app.movingObjects = false;
            ReleaseCapture();
            return 0;
        }
        case WM_MOUSEMOVE:
            if (app.dragging) {
                POINT p;
                GetCursorPos(&p);
                double dx = p.x - app.lastMouse.x, dy = p.y - app.lastMouse.y;
                app.lastMouse = p;
                double cur[3];
                if (app.movingObjects && (GetKeyState(VK_MENU) & 0x8000)) {
                    POINT cp = p;
                    ScreenToClient(h, &cp);
                    if (unprojectToPlane(cp.x, cp.y - app.toolbarH, cur)) {
                        moveSelected(cur[0] - app.lastPick[0], cur[1] - app.lastPick[1], 0);
                        std::copy(cur, cur + 3, app.lastPick);
                    }
                } else if (GetKeyState(VK_CONTROL) & 0x8000) cameraMouseMove(app.cam, dx, dy);  // Ctrl + left button: move
                else cameraMouseRotate(app.cam, dx, dy);                                 // left button: rotate
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(h, msg, w, l);
}

bool createWindow(bool visible) {
    WNDCLASS wc = {};
    wc.style = CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "MEdViewer";
    RegisterClass(&wc);
    RECT r = {0, 0, app.width, app.height};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    app.hwnd = CreateWindow("MEdViewer", "MEd 3D viewer", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | (visible ? WS_VISIBLE : 0), 80, 60,
                            r.right - r.left, r.bottom - r.top, nullptr, nullptr, wc.hInstance, nullptr);
    if (!app.hwnd) return false;
    if (visible) {
        createToolbar(app.hwnd);
        DragAcceptFiles(app.hwnd, TRUE);  // drop .dff / .txd / .col files to view them
        createInspector(app.hwnd);
        createScenePanel(app.hwnd);
        createPlacementsPanel(app.hwnd);
        RECT cr;
        GetClientRect(app.hwnd, &cr);  // lay out the toolbar and the 3D view below it
        SendMessage(app.hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(cr.right, cr.bottom));
    }
    app.dc = GetDC(app.hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 24;
    int fmt = ChoosePixelFormat(app.dc, &pfd);
    if (!fmt || !SetPixelFormat(app.dc, fmt, &pfd)) return false;
    app.rc = wglCreateContext(app.dc);
    return app.rc && wglMakeCurrent(app.dc, app.rc);
}

bool down(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

// MEd polls the keyboard from a 1 ms QTimer (CMapView::initializeGL, QTimer::start(1)), so the
// camera gets one pollKeyboard() step per elapsed millisecond.
void updateCamera() {
    DWORD now = GetTickCount();
    app.tickRemainder += double(now - app.lastTick);
    app.lastTick = now;
    int ticks = int(app.tickRemainder);
    app.tickRemainder -= ticks;
    ticks = std::min(ticks, 100);  // do not fly away after a stall
    if (GetForegroundWindow() != app.hwnd) return;
    { HWND f = GetFocus(); if (f && f != app.hwnd) return; }  // typing in the panel is not camera input
    CameraKeys k;
    k.forward = down('W') || down(VK_UP);
    k.back = down('S') || down(VK_DOWN);
    k.left = down('A') || down(VK_LEFT);
    k.right = down('D') || down(VK_RIGHT);
    k.pageUp = down(VK_PRIOR) || down('R');
    k.pageDown = down(VK_NEXT) || down('F');
    k.shift = down(VK_SHIFT);
    k.ctrl = down(VK_CONTROL);
    k.alt = down(VK_MENU);
    for (int i = 0; i < ticks; ++i) {
        cameraKeyboardTick(app.cam, k);
        if (!app.selected.empty()) {
            ObjectMove m = objectKeyboardTick(app.cam, k);
            if (m.any()) moveSelected(m.dx, m.dy, m.dz);
        }
    }
}

}  // namespace

// Which game folder to open when none is given on the command line: the one used last time (kept in med_viewer.ini next
// to the program), else ask with a folder picker. Returns an empty string when the user cancels.
std::string chooseGameFolder() {
    auto isGame = [](const std::string& dir) { return !dir.empty() && fs::exists(dir + "/data/gta3.dat"); };
    char last[MAX_PATH] = "";
    GetPrivateProfileStringA("Editor", "game", "", last, sizeof last, modIniPath().c_str());
    if (isGame(last)) return last;
    OleInitialize(nullptr);
    while (true) {
        BROWSEINFOA bi = {};
        bi.lpszTitle = "Open the GTA III folder to edit (the one with data\\gta3.dat):";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST id = SHBrowseForFolderA(&bi);
        if (!id) return "";
        char path[MAX_PATH];
        const bool got = SHGetPathFromIDListA(id, path) != FALSE;
        CoTaskMemFree(id);
        if (got && isGame(path)) return path;
        MessageBoxA(nullptr, "That folder has no data\\gta3.dat. Choose the GTA III folder.", "MEd 3D viewer", MB_ICONWARNING);
    }
}

int main(int argc, char** argv) {
    std::vector<std::string> argStore;
    std::vector<char*> argPtrs;
    const bool noFolder = argc < 2 || std::string(argv[1]).compare(0, 2, "--") == 0;
    if (noFolder) {  // double-clicked, or only options given: reopen the last folder or ask for one
        const std::string folder = chooseGameFolder();
        if (folder.empty()) {
            std::printf("usage: med_viewer [GTA III folder] [--shot out.bmp] [--pos x y z] [--yaw d] [--pitch d] [--size w h] [--conj 0|1]\n");
            return 2;
        }
        argStore.push_back(argv[0]);
        argStore.push_back(folder);
        for (int i = 1; i < argc; ++i) argStore.push_back(argv[i]);
        for (std::string& s : argStore) argPtrs.push_back(s.data());
        argc = int(argPtrs.size());
        argv = argPtrs.data();
    }
    app.root = argv[1];
    bool rememberFolder = true;
    for (int i = 2; i < argc; ++i)
        if (std::string(argv[i]) == "--shot" || std::string(argv[i]) == "--selftest") rememberFolder = false;  // test runs do not change what the editor reopens
    if (rememberFolder) WritePrivateProfileStringA("Editor", "game", app.root.c_str(), modIniPath().c_str());
    std::string shot;
    int bookmark = 0;
    bool explicitPos = false;
    int selectX = -1, selectY = -1;
    bool saveAfter = false;
    int undoCount = 0, redoCount = 0;
    double rotateBy = 0;
    std::string replaceWith;  // --replace model: Replace Object on the selection
    struct AttachStep { int targetEnd; std::string model; int modelEnd; bool preview; };
    std::vector<AttachStep> attaches;  // --attach / --preview-attach targetEnd model modelEnd (road builder, on the selection)
    bool listRoadEnds = false;          // --road-ends: print the selected road piece's ends
    bool selfTest = false;
    std::string newRegionName, moveToRegion, newObjectModel, pasteRegion;
    std::string importDff, importRegion, importName, importId, importStart;  // --import dff region [--import-name n] [--import-id n] [--import-start empty|box|mesh]
    std::vector<std::pair<std::string, std::string>> extracts;  // --extract model folder (repeatable)
    double pastePos[3] = {0, 0, 0};
    double newObjectPos[3] = {0, 0, 0};
    bool deleteAfter = false, duplicateAfter = false, sinkAfter = false, centreAfter = false, checkAfter = false;
    std::string onlyRegions;
    int benchmark = 0;
    std::string selectModel, pickTestModel, openDffAtStart, openColAtStart, openTxdAtStart;
    bool colEditAtStart = false;  // --col-edit: the collision window opened by --open-col starts on its Edit tab
    std::vector<std::string> openFilesAtStart;  // --open-file path (repeatable): loose files in the viewers
    bool addedOnly = false;
    bool selectAdded = false;  // --select-added: select every placement that is not part of the original game (and list them)
    double moveBy[3] = {0, 0, 0};
    // MEd's bookmarks live in MEdv032/ of this project; look next to the game folder first, then up from the program
    // (build/dev/med_viewer.exe is two levels below the project), so a game folder kept anywhere still finds them.
    bool bookmarksGiven = false;
    std::string bookmarkFile = app.root + "/../MEdv032/bookmarks.ini";
    {
        char exe[MAX_PATH];
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        std::string dir = exe;
        for (int up = 0; up < 4; ++up) {
            dir = dir.substr(0, dir.find_last_of("\\/"));
            const std::string candidate = dir + "/MEdv032/bookmarks.ini";
            if (!fs::exists(app.root + "/../MEdv032/bookmarks.ini") && fs::exists(candidate)) { bookmarkFile = candidate; break; }
        }
    }
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        auto num = [&](int k) { return i + k < argc ? std::atof(argv[i + k]) : 0.0; };
        if (a == "--shot" && i + 1 < argc) shot = argv[++i];
        else if (a == "--bookmark") bookmark = int(num(1)), ++i;
        else if (a == "--bookmarks" && i + 1 < argc) { bookmarkFile = argv[++i]; bookmarksGiven = true; }
        else if (a == "--h") app.cam.h = float(num(1)), ++i;
        else if (a == "--select") { selectX = int(num(1)); selectY = int(num(2)); i += 2; }
        else if (a == "--delete") deleteAfter = true;
        else if (a == "--select-model" && i + 1 < argc) selectModel = argv[++i];
        else if (a == "--added-only") addedOnly = true;
        else if (a == "--select-added") selectAdded = true;
        else if (a == "--open-dff" && i + 1 < argc) openDffAtStart = argv[++i];
        else if (a == "--open-col" && i + 1 < argc) openColAtStart = argv[++i];
        else if (a == "--col-edit") colEditAtStart = true;
        else if (a == "--open-txd" && i + 1 < argc) openTxdAtStart = argv[++i];
        else if (a == "--open-file" && i + 1 < argc) openFilesAtStart.push_back(argv[++i]);
        else if (a == "--import-window" && i + 1 < argc) importAtStart = argv[++i];
        else if (a == "--pick-test" && i + 1 < argc) pickTestModel = argv[++i];
        else if (a == "--only" && i + 1 < argc) onlyRegions = argv[++i];
        else if (a == "--lod-near") app.lodNear = float(num(1)), ++i;
        else if (a == "--no-lod") app.showLod = false;
        else if (a == "--lod-view") app.lodView = true;
        else if (a == "--paths") app.showPaths = true;
        else if (a == "--selftest") selfTest = true;
        else if (a == "--vanilla") app.forceVanilla = true;
        else if (a == "--benchmark") benchmark = int(num(1)), ++i;
        else if (a == "--check") checkAfter = true;
        else if (a == "--sink") sinkAfter = true;
        else if (a == "--duplicate") duplicateAfter = true;
        else if (a == "--new-region" && i + 1 < argc) newRegionName = argv[++i];
        else if (a == "--import" && i + 2 < argc) { importDff = argv[++i]; importRegion = argv[++i]; }
        else if (a == "--import-name" && i + 1 < argc) importName = argv[++i];
        else if (a == "--import-id" && i + 1 < argc) importId = argv[++i];
        else if (a == "--import-start" && i + 1 < argc) importStart = argv[++i];
        else if (a == "--extract" && i + 2 < argc) { extracts.emplace_back(argv[i + 1], argv[i + 2]); i += 2; }
        else if (a == "--copy-paste" && i + 4 < argc) { pasteRegion = argv[i + 1]; pastePos[0] = std::atof(argv[i + 2]); pastePos[1] = std::atof(argv[i + 3]); pastePos[2] = std::atof(argv[i + 4]); i += 4; }
        else if (a == "--new-object" && i + 4 < argc) { newObjectModel = argv[i + 1]; newObjectPos[0] = std::atof(argv[i + 2]); newObjectPos[1] = std::atof(argv[i + 3]); newObjectPos[2] = std::atof(argv[i + 4]); i += 4; }
        else if (a == "--move-to-region" && i + 1 < argc) moveToRegion = argv[++i];
        else if (a == "--rotate") rotateBy = num(1), ++i;
        else if ((a == "--attach" || a == "--preview-attach") && i + 3 < argc) {
            attaches.push_back({std::atoi(argv[i + 1]), argv[i + 2], std::atoi(argv[i + 3]), a == "--preview-attach"});
            i += 3;
        } else if (a == "--road-ends") listRoadEnds = true;
        else if (a == "--roads-only") app.roadsOnly = true;
        else if (a == "--far" && i + 1 < argc) app.farExtra = float(std::atof(argv[++i]));
        else if (a == "--replace" && i + 1 < argc) {
            replaceWith = argv[++i];
        }
        else if (a == "--centre") centreAfter = true;
        else if (a == "--undo") undoCount = int(num(1)), ++i;
        else if (a == "--redo") redoCount = int(num(1)), ++i;
        else if (a == "--save") saveAfter = true;
        else if (a == "--in-place") app.saveInPlace = true;
        else if (a == "--save-to" && i + 1 < argc) app.saveRoot = argv[++i];
        else if (a == "--move") { moveBy[0] = num(1); moveBy[1] = num(2); moveBy[2] = num(3); i += 3; }
        else if (a == "--pos") { app.cam.x = float(num(1)); app.cam.y = float(num(2)); app.cam.z = float(num(3)); i += 3; explicitPos = true; }
        else if (a == "--yaw") app.cam.yaw = float(num(1)), ++i, explicitPos = true;
        else if (a == "--pitch") app.cam.pitch = float(num(1)), ++i, explicitPos = true;
        else if (a == "--size") { app.width = int(num(1)); app.height = int(num(2)); i += 2; }
        else if (a == "--conj") app.conjugate = num(1) != 0, ++i;
        else if (a == "--prelit") app.prelit = int(num(1)), ++i;
    }
    // Interactive editing saves straight into the folder that was opened (as MEd does). Test runs (--shot) and --save-to keep
    // writing to a separate folder so the game files stay untouched.
    if (shot.empty() && app.saveRoot.empty()) app.saveInPlace = true;
    if (app.saveRoot.empty()) app.saveRoot = app.root + "/../MEd_saved";
    {  // the bookmarks: bookmarks.ini next to the editor, started from MEd's own file the first time (that one is never written)
        char exe[MAX_PATH];
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        std::string dir = exe;
        dir = dir.substr(0, dir.find_last_of("\\/"));
        app.bookmarkFile = bookmarksGiven ? bookmarkFile : dir + "/bookmarks.ini";
        bool ok;
        std::string text = slurp(app.bookmarkFile, ok);
        if (!ok && !bookmarksGiven) {
            text = slurp(bookmarkFile, ok);  // MEd's
            if (ok) {
                std::ofstream seed(app.bookmarkFile, std::ios::binary);
                seed << text;
            }
        }
        app.bookmarks = parseBookmarks(text);
    }
    if (!explicitPos) {  // start at a bookmark (the first of MEd's is Portland Hospital)
        if (bookmark >= 0 && bookmark < int(app.bookmarks.size())) {
            const Bookmark& b = app.bookmarks[bookmark];
            app.cam.x = float(b.x); app.cam.y = float(b.y); app.cam.z = float(b.z);
            app.cam.h = float(b.h); app.cam.yaw = float(b.yaw); app.cam.pitch = float(b.pitch);
            std::printf("camera at bookmark %d: %s\n", bookmark, b.name.c_str());
        } else {
            std::printf("no bookmark %d in %s, camera at the origin\n", bookmark, app.bookmarkFile.c_str());
        }
    }
    if (!createWindow(shot.empty())) {
        std::fprintf(stderr, "could not create an OpenGL window\n");
        return 1;
    }
    std::printf("OpenGL %s on %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    if (!loadGameData()) return 1;
    initRegions();
    buildLodPartnerRanges();
    if (!onlyRegions.empty()) {  // --only a,b: start with just the regions whose file name contains one of these words
        std::vector<std::string> words;
        for (size_t pos = 0; pos <= onlyRegions.size();) {
            size_t c = onlyRegions.find(',', pos);
            words.push_back(toLower(onlyRegions.substr(pos, c == std::string::npos ? std::string::npos : c - pos)));
            if (c == std::string::npos) break;
            pos = c + 1;
        }
        for (const Region& r : regions) {
            bool on = false;
            for (const std::string& w : words) on = on || (!w.empty() && toLower(r.name).find(w) != std::string::npos);
            app.regionOn[r.file] = on ? 1 : 0;
        }
    }
    for (const Region& r : regions) app.regionHideable[r.file] = r.count == 0 ? 1 : 0;  // empty at load: GTA3.ZON, CULL.IPL
    if (scene.list) fillRegionList();
    plRefreshAfterEdit();  // the Placements tab was made before the map was loaded
    if (selfTest) return pl.panel ? runPlacementsSelfTest() : 2;  // --selftest (needs the window, so not with --shot)

    if (!shot.empty()) {
        render();  // first pass loads models and textures
        if (benchmark > 0) {
            render();
            glFinish();
            LARGE_INTEGER f0, a0, b0;
            QueryPerformanceFrequency(&f0);
            QueryPerformanceCounter(&a0);
            if (std::getenv("MED_NOVSYNC")) {  // measure the graphics card without waiting for the 60 Hz display
                typedef BOOL(WINAPI * SwapIntervalFn)(int);
                SwapIntervalFn swapInterval = (SwapIntervalFn)wglGetProcAddress("wglSwapIntervalEXT");
                if (swapInterval) swapInterval(0);
            }
            app.msCollect = app.msSubmit = 0;
            for (int i = 0; i < benchmark; ++i) { render(); SwapBuffers(app.dc); }
            { long tris = 0, mats = 0; for (int idx : app.visible) { const ModelDef& d = app.defs.find(app.instances[idx].id)->second; const GlModel& g = app.models[d.model]; tris += g.triangles; mats += g.materials; } std::printf("benchmark: %ld triangles and %ld materials in view per frame\n", tris, mats); }
            std::printf("benchmark: choosing objects %.2f ms, submitting them %.2f ms per frame (CPU time)\n", app.msCollect / benchmark, app.msSubmit / benchmark);
            glFinish();
            QueryPerformanceCounter(&b0);
            const double ms = 1000.0 * double(b0.QuadPart - a0.QuadPart) / double(f0.QuadPart) / benchmark;
            int shown = 0;
            for (size_t i = 0; i < app.instances.size(); ++i) shown += app.regionOn[app.instFile[i]];
            std::printf("benchmark: %.2f ms per frame (%.0f frames per second), %zu objects drawn, %d of %zu placements in shown regions\n", ms, 1000.0 / ms,
                        app.visible.size(), shown, app.instances.size());
        }
        if (!selectModel.empty()) {  // test helper: select the placement of this model nearest to the camera
            int best = -1;
            double bestDist = 1e30;
            const std::vector<bool> orig = originalFlags();
            for (size_t i = 0; i < app.instances.size(); ++i) {
                if (toLower(app.instances[i].model) != toLower(selectModel)) continue;
                if (addedOnly && orig[i]) continue;  // --added-only: only placements the user added
                double d = std::hypot(app.instances[i].x - app.cam.x, app.instances[i].y - app.cam.y);
                if (d < bestDist) { bestDist = d; best = int(i); }
            }
            if (best >= 0) app.selected.assign(1, best);
            std::printf("%s\n", best >= 0 ? ("selected " + app.instances[best].model + " by name").c_str() : "no such model");
        }
        if (selectAdded) {
            const std::vector<bool> orig = originalFlags();
            app.selected.clear();
            for (size_t i = 0; i < app.instances.size(); ++i)
                if (!orig[i]) {
                    app.selected.push_back(int(i));
                    const IplInstance& o = app.instances[i];
                    std::printf("  added: %s (id %d) in %s at %.4f %.4f %.4f\n", o.model.c_str(), o.id, app.iplPaths[size_t(app.instFile[i])].c_str(), o.x, o.y, o.z);
                }
            std::printf("selected %zu added placement(s)\n", app.selected.size());
        }
        if (!pickTestModel.empty()) {  // test helper: click the centre of every drawn placement of this model and report what is hit
            glViewport(app.viewX, 0, app.width, app.height);
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            setViewMatrices(-1, -1);
            GLdouble proj[16], model[16];
            GLint vp[4];
            glGetDoublev(GL_PROJECTION_MATRIX, proj);
            glGetDoublev(GL_MODELVIEW_MATRIX, model);
            glGetIntegerv(GL_VIEWPORT, vp);
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            int tried = 0, hitSelf = 0, hitOther = 0, miss = 0;
            for (int idx : std::vector<int>(app.visible)) {
                const IplInstance& o = app.instances[idx];
                if (toLower(o.model) != toLower(pickTestModel)) continue;
                double sx, sy, sz;
                if (gluProject(o.x, o.y, o.z + 0.3, model, proj, vp, &sx, &sy, &sz) != GL_TRUE) continue;
                const int mx = int(sx) - app.viewX + app.viewX, my = app.height - int(sy);
                const int hit = pickInstance(mx, my);
                ++tried;
                if (hit == idx) ++hitSelf;
                else if (hit >= 0) ++hitOther;
                else ++miss;
                if (tried <= 8)
                    std::printf("  %s at %.1f %.1f %.1f, pixel %d,%d: %s\n", o.model.c_str(), o.x, o.y, o.z, mx, my,
                                hit == idx ? "hit it" : hit >= 0 ? ("hit " + app.instances[hit].model).c_str() : "nothing");
            }
            std::printf("pick test: %d drawn, %d hit themselves, %d hit another object, %d missed\n", tried, hitSelf, hitOther, miss);
        }
        if (selectX >= 0) {
            doubleClickSelect(selectX, selectY, false);
            std::printf("selected %zu object(s)", app.selected.size());
            for (int idx : app.selected)
                std::printf("  [id %d %s at %.2f %.2f %.2f]", app.instances[idx].id, app.defs[app.instances[idx].id].model.c_str(),
                            app.instances[idx].x, app.instances[idx].y, app.instances[idx].z);
            std::printf("\n");
            moveSelected(moveBy[0], moveBy[1], moveBy[2]);
            if (moveBy[0] != 0 || moveBy[1] != 0 || moveBy[2] != 0)
                for (int idx : app.selected)
                    std::printf("moved to %.2f %.2f %.2f\n", app.instances[idx].x, app.instances[idx].y, app.instances[idx].z);
        }
        if (centreAfter && !app.selected.empty()) {
            centreOnObject(app.selected[0]);
            std::printf("camera centred on object: %.2f : %.2f : %.2f : %.2f : %.1f : %.1f\n", app.cam.x, app.cam.y, app.cam.z, app.cam.h,
                        app.cam.yaw, app.cam.pitch);
        }
        if (!newRegionName.empty()) {  // test helper: the New region... button without the dialog
            std::string rel, err;
            if (addIplRegion(app.root, newRegionName, rel, err)) { app.iplPaths.push_back(rel); initRegions(); std::printf("created region %s (%zu regions listed)\n", rel.c_str(), regions.size()); }
            else std::printf("new region failed: %s\n", err.c_str());
        }
        if (!importDff.empty()) {  // test helper: the Import Model window, filled in and Import pressed by the program
            imp.quiet = true;
            importModelUi(app.hwnd, importDff);
            imFillRegions(importRegion);
            if (!importName.empty()) SetWindowTextA(imp.name, importName.c_str());
            if (!importId.empty()) SetWindowTextA(imp.id, importId.c_str());
            if (!importStart.empty()) {
                for (HWND h : {imp.startEmpty, imp.startBox, imp.startMesh}) SendMessageA(h, BM_SETCHECK, BST_UNCHECKED, 0);
                SendMessageA(importStart == "box" ? imp.startBox : importStart == "mesh" ? imp.startMesh : imp.startEmpty, BM_SETCHECK, BST_CHECKED, 0);
            }
            imRecheck();
            char text[8192] = "";
            GetWindowTextA(imp.checks, text, sizeof text);
            std::printf("import check:\n%s\n", text);
            if (imDoImport(app.hwnd)) std::printf("imported id %d, Item tab rows %zu\n", imp.importedId, it.rows.size());
            else std::printf("import not done%s%s\n", imp.lastError.empty() ? "" : ": ", imp.lastError.c_str());
            DestroyWindow(imp.wnd);
            imp = ImportUi();
        }
        for (const auto& [extractModel, extractFolder] : extracts) {  // test helper: Extract Object without the dialogs
            int id = -1;
            for (const auto& kv : app.defs) if (kv.second.model == toLower(extractModel)) id = kv.first;
            for (const ExtractResult& r : extractModelsTo(extractFolder, {id})) {
                std::printf("extracted into %s:\n", r.folder.c_str());
                for (const std::string& f : r.written) std::printf("  wrote %s\n", f.c_str());
                for (const std::string& w : r.warnings) std::printf("  warning: %s\n", w.c_str());
                if (!r.colSource.empty()) std::printf("  collision from %s\n", r.colSource.c_str());
            }
            if (id < 0) std::printf("no model named %s\n", extractModel.c_str());
        }
        if (!pasteRegion.empty() && !app.selected.empty()) {  // test helper: Copy, then Paste into this region at x y z
            int file = -1;
            for (const Region& r : regions) if (toLower(r.name) == toLower(pasteRegion) + ".ipl") file = r.file;
            const size_t copied = copySelected();
            const size_t pasted = file >= 0 ? pasteClipboard(file, pastePos[0], pastePos[1]) : 0;
            std::printf("copied %zu placement(s), pasted %zu into %s at %.2f %.2f %.2f (placements %zu, undo %zu, files changed %zu, selection %zu)\n", copied, pasted,
                        pasteRegion.c_str(), pastePos[0], pastePos[1], pastePos[2], app.instances.size(), app.history.undoCount(), app.dirty.size(), app.selected.size());
        }
        if (!newObjectModel.empty()) {  // test helper: New Object... without the dialogs, into the region given with --move-to-region (or the selection's file)
            int id = -1, file = app.selected.empty() ? -1 : app.instFile[app.selected[0]];
            for (const auto& kv : app.defs) if (kv.second.model == toLower(newObjectModel)) id = kv.first;
            for (const Region& r : regions) if (!moveToRegion.empty() && toLower(r.name) == toLower(moveToRegion) + ".ipl") file = r.file;
            const int idx = (id >= 0 && file >= 0) ? addNewObject(id, file, newObjectPos[0], newObjectPos[1], newObjectPos[2]) : -1;
            if (idx >= 0) std::printf("new object: %s id %d added to %s at %.2f %.2f %.2f (placements %zu, undo %zu, files changed %zu)\n", app.instances[idx].model.c_str(), id, app.iplPaths[file].c_str(), newObjectPos[0], newObjectPos[1], newObjectPos[2], app.instances.size(), app.history.undoCount(), app.dirty.size());
            else std::printf("new object failed (model %s found: %d, region found: %d)\n", newObjectModel.c_str(), id >= 0, file >= 0);
            moveToRegion.clear();
        }
        if (!moveToRegion.empty()) {  // test helper: Move selected objects here
            int file = -1;
            for (const Region& r : regions) if (toLower(r.name) == toLower(moveToRegion) + ".ipl") file = r.file;
            const size_t n = file >= 0 ? moveSelectedToRegion(file) : 0;
            std::printf("moved %zu placement(s) to region %s (files changed %zu)\n", n, moveToRegion.c_str(), app.dirty.size());
            for (const Region& r : regions) if (r.count > 0 || toLower(r.name) == toLower(moveToRegion) + ".ipl") if (toLower(r.name).find(toLower(moveToRegion)) != std::string::npos) std::printf("  region %s now has %d objects\n", r.name.c_str(), r.count);
        }
        for (const AttachStep& st : attaches) {  // test helper: the road builder without the strip
            if (app.selected.size() != 1 || !isRoadPiece(app.selected[0])) { std::printf("attach: select one road piece first\n"); break; }
            int id = -1;
            for (const auto& kv : app.defs) if (kv.second.model == toLower(st.model)) id = kv.first;
            if (id < 0) { std::printf("attach: no model %s\n", st.model.c_str()); break; }
            rb.target = app.selected[0];
            rb.targetEnd = st.targetEnd;
            RoadVariant v;
            v.id = id;
            v.end = st.modelEnd;
            rb.variants.assign(1, v);
            if (st.preview) { rb.hover = 0; rb.forcePreview = true; std::printf("previewing %s end %d at end %d\n", st.model.c_str(), st.modelEnd, st.targetEnd); continue; }
            const int idx = placeRoadVariant(0);
            if (idx < 0) { std::printf("attach: refused (%s)\n", rb.lastMessage.c_str()); break; }
            const IplInstance& n = app.instances[idx];
            std::printf("attached %s by its end %d at end %d: now at %.4f %.4f %.4f rotation %.6g %.6g %.6g %.6g (placements %zu, undo %zu)\n", n.model.c_str(), st.modelEnd,
                        st.targetEnd, n.x, n.y, n.z, n.rotX, n.rotY, n.rotZ, n.rotW, app.instances.size(), app.history.undoCount());
        }
        if (listRoadEnds && app.selected.size() == 1 && isRoadPiece(app.selected[0])) {
            const IplInstance& o = app.instances[app.selected[0]];
            const std::vector<RoadEnd>& e = roadEndsOf(o.id);
            for (size_t k = 0; k < e.size(); ++k) {
                const RoadEnd w = endInWorld(o, e[k]);
                std::printf("  end %zu of %s: %.4f %.4f %.4f facing %.3f %.3f, width %.1f, %s\n", k, o.model.c_str(), w.x, w.y, w.z, w.dx, w.dy, w.width,
                            roadEndJoined(app.selected[0], w) ? "joined" : "open");
            }
            const std::vector<RoadVariant> vs = roadVariantsFor(e[0].width);
            std::printf("  %zu pieces fit end 0:\n", vs.size());
            for (const RoadVariant& v : vs) std::printf("    %-18s end %d  %s\n", v.name.c_str(), v.end, v.label.c_str());
        }
        if (!replaceWith.empty() && !app.selected.empty()) {
            int id = -1;
            for (const auto& kv : app.defs) if (kv.second.model == toLower(replaceWith)) id = kv.first;
            ReplaceReport rep;
            const bool ok = id >= 0 && setModelId(app.hwnd, id, &rep);
            const IplInstance& o = app.instances[app.selected[0]];
            std::printf("replace with %s: %s, now %d %s at %.4f %.4f %.4f rotation %.6g %.6g %.6g %.6g; replaced %d, LOD stand-ins switched %d, moved below the map %d, added %d, removed %d; placements %zu, undo %zu, files changed %zu\n",
                        replaceWith.c_str(), ok ? "done" : "FAILED", o.id, o.model.c_str(), o.x, o.y, o.z, o.rotX, o.rotY, o.rotZ, o.rotW, rep.replaced, rep.lodSwitched, rep.lodSunk,
                        rep.lodAdded, rep.lodRemoved, app.instances.size(), app.history.undoCount(), app.dirty.size());
            for (size_t p : lodPartnersOf(app.selected[0]))
                std::printf("  its LOD stand-in is now %s at %.4f %.4f %.4f\n", app.instances[p].model.c_str(), app.instances[p].x, app.instances[p].y, app.instances[p].z);
        }
        if (rotateBy != 0 && !app.selected.empty()) {
            rotateSelected(rotateBy);
            const IplInstance& o = app.instances[app.selected[0]];
            std::printf("rotated %.0f degrees: %s now has rotation %.6g %.6g %.6g %.6g (undo %zu, files changed %zu)\n", rotateBy, o.model.c_str(), o.rotX, o.rotY, o.rotZ, o.rotW,
                        app.history.undoCount(), app.dirty.size());
        }
        if (duplicateAfter) {
            const size_t was = app.instances.size();
            size_t n = duplicateSelected();
            std::printf("duplicated: %zu copy(ies) made (%zu placements before, %zu after), selection is now the copy at %.2f %.2f %.2f, undo %zu, files changed %zu\n", n,
                        was, app.instances.size(), app.selected.empty() ? 0.0 : app.instances[app.selected[0]].x,
                        app.selected.empty() ? 0.0 : app.instances[app.selected[0]].y, app.selected.empty() ? 0.0 : app.instances[app.selected[0]].z,
                        app.history.undoCount(), app.dirty.size());
        }
        if (sinkAfter) {
            std::vector<int> before = app.selected;
            size_t n = sinkSelected();
            std::printf("sunk %zu placement(s) to z = %.0f (%zu selected + %zu LOD partner(s)), undo %zu, files changed %zu\n", n, kSunkZ,
                        before.size(), n >= before.size() ? n - before.size() : 0, app.history.undoCount(), app.dirty.size());
            for (size_t i = 0; i < app.instances.size(); ++i)
                if (app.instances[i].z == kSunkZ) std::printf("  now at z -200: %s (%.2f, %.2f)\n", app.instances[i].model.c_str(), app.instances[i].x, app.instances[i].y);
        }
        const size_t countBefore = app.instances.size();
        if (deleteAfter) {
            bool did = deleteSelected();
            std::printf("%s: %zu placements before, %zu after (undo %zu, files changed %zu)\n", did ? "deleted" : "delete (nothing selected)",
                        countBefore, app.instances.size(), app.history.undoCount(), app.dirty.size());
        }
        auto show = [&](const char* what, int idx) {
            if (idx < int(app.instances.size()))
                std::printf("%s: object %d at %.2f %.2f %.2f  (placements %zu, undo %zu, redo %zu, files changed %zu)\n", what, idx,
                            app.instances[idx].x, app.instances[idx].y, app.instances[idx].z, app.instances.size(),
                            app.history.undoCount(), app.history.redoCount(), app.dirty.size());
            else
                std::printf("%s: placements %zu, undo %zu, redo %zu, files changed %zu\n", what, app.instances.size(),
                            app.history.undoCount(), app.history.redoCount(), app.dirty.size());
        };
        if (undoCount > 0 || redoCount > 0) {
            int tracked = app.selected.empty() ? 0 : app.selected[0];
            for (int i = 0; i < undoCount; ++i) {
                bool did = undoAction();
                show(did ? "undo" : "undo (nothing to undo)", tracked);
            }
            for (int i = 0; i < redoCount; ++i) {
                bool did = redoAction();
                show(did ? "redo" : "redo (nothing to redo)", tracked);
            }
        }
        if (checkAfter) {
            CheckResult cr = runCheck();
            std::printf("%s", checkReportText(cr).c_str());
        }
        if (saveAfter) {
            bool ok;
            std::printf("%s", saveAll(&ok).c_str());
            if (!ok) return 1;
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        render();
        SwapBuffers(app.dc);
        render();  // read back from a fully drawn back buffer
        bool ok = writeBmp(shot);
        std::printf("%s %s (%d placements drawn, %zu models, %zu texture dictionaries)\n", ok ? "wrote" : "FAILED to write", shot.c_str(),
                    app.drawn, app.models.size(), app.loadedTxds.size());
        return ok ? 0 : 1;
    }

    if (!selectModel.empty()) {  // also in the live window (tests): the placement of this model nearest to the camera
        int best = -1;
        double bestDist = 1e30;
        for (size_t i = 0; i < app.instances.size(); ++i) {
            if (toLower(app.instances[i].model) != toLower(selectModel)) continue;
            const double d = std::hypot(app.instances[i].x - app.cam.x, app.instances[i].y - app.cam.y);
            if (d < bestDist) { bestDist = d; best = int(i); }
        }
        if (best >= 0) app.selected.assign(1, best);
    }
    if (!openTxdAtStart.empty()) openTxdViewer(app.hwnd, openTxdAtStart);  // tests: open TXD Properties for this dictionary
    for (const std::string& f : openFilesAtStart) openLooseFile(app.hwnd, f);
    if (!importAtStart.empty()) PostMessageA(app.hwnd, WM_COMMAND, ID_TB_IMPORT, 0);  // opened from the message loop (the window is modal)
    if (!openColAtStart.empty())  // tests: open Collision Object Properties for this model
        for (const auto& kv : app.defs)
            if (kv.second.model == toLower(openColAtStart)) {
                ModelViewer* cv = openColViewer(app.hwnd, kv.first);
                if (cv && colEditAtStart) { ceShowTab(*cv, true); viewerLayout(*cv); }
                break;
            }
    if (!openDffAtStart.empty())  // tests: open DFF Properties for this model
        for (const auto& kv : app.defs)
            if (kv.second.model == toLower(openDffAtStart)) { openDffViewer(app.hwnd, kv.first); break; }
    MSG msg;
    app.lastTick = GetTickCount();
    bool running = true;
    while (running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            if (plHandleKey(msg)) continue;  // F3 / Enter / Esc of the Placements tab
            if (inspHandleKey(msg)) continue;  // Enter / Esc in the properties boxes
            if (itHandleKey(msg)) continue;    // Enter / Esc in the Item tab
            if (bmHandleKey(msg)) continue;    // Enter / Delete in the Bookmarks window
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        updateCamera();
        updateToolbarState();
        refreshInspector();
        roadSync();
        renderRoadThumbnails();
        render();
        SwapBuffers(app.dc);
        updateViewers();  // the open DFF Properties windows
        updateTxdViewers();
        // the project (the opened game folder's name, for example ProjectUpstate) leads the title
        static const std::string folderLabel = [] {
            std::string r = app.root;
            for (char& c : r) if (c == '/') c = char(92);
            while (!r.empty() && r.back() == char(92)) r.pop_back();
            const size_t last = r.find_last_of(char(92));
            return last == std::string::npos ? r : r.substr(last + 1);
        }();
        char title[400];
        std::snprintf(title, sizeof title, "%s  -  MEd 3D viewer  |  %.2f : %.2f : %.2f : %.2f : %.1f : %.1f  |  %d objects  |  undo %zu redo %zu%s%s%s", folderLabel.c_str(), app.cam.x,
                      app.cam.y, app.cam.z, app.cam.h, app.cam.yaw, app.cam.pitch, app.drawn,
                      app.history.undoCount(), app.history.redoCount(), app.dirty.empty() ? "" : "  |  UNSAVED (Ctrl+S)",
                      app.selected.empty() ? "" : ("  |  " + selectionTitle()).c_str(),
                      app.lodViewText.empty() ? "" : ("  |  " + app.lodViewText).c_str());
        // undo/redo depth is shown too: see the status text below
        static DWORD fpsStart = GetTickCount();
        static int fpsFrames = 0;
        static int fpsShown = 0;
        ++fpsFrames;
        if (GetTickCount() - fpsStart >= 1000) { fpsShown = fpsFrames; fpsFrames = 0; fpsStart = GetTickCount(); }
        std::string full = std::string(title) + "  |  " + std::to_string(fpsShown) + " fps";
        SetWindowTextA(app.hwnd, full.c_str());
        refreshScene();
        plSyncFromSelection();
        itSyncFromSelection();
        static DWORD plRedraw = GetTickCount();  // positions change while objects are moved: repaint the list now and then
        if (pl.panel && IsWindowVisible(pl.panel) && GetTickCount() - plRedraw > 300) { InvalidateRect(pl.list, nullptr, FALSE); plRedraw = GetTickCount(); }
    }
    return 0;
}
