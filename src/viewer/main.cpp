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

struct ModelDef {
    std::string model, txd;
    std::string name;  // the model name as written in the IDE (model is lower case)
    std::string ide;   // the IDE file that defines it (file name only)
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
    std::vector<Bookmark> bookmarks;  // the Bookmarks window's list (MEd's format)
    std::string bookmarkFile;         // where it is saved: bookmarks.ini next to the editor
    PoolLimits limits;                     // pool sizes the game can really have: vanilla, or from a limit adjuster file in the game folder
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
    DWORD lastTick = 0;
    double tickRemainder = 0;  // milliseconds not yet turned into 1 ms keyboard ticks
    HWND hwnd = nullptr;
    HDC dc = nullptr;
    HGLRC rc = nullptr;
} app;

// bookmarks.ini lines look like
//   <bookmark id=0>name</bookmark><location id=0>x:y:z:h:yaw:pitch:flag</location>
// ---- loading -----------------------------------------------------------------------------

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
        for (const IdeObject& o : f.objects) {
            ModelDef d;
            d.model = toLower(o.model);
            d.name = o.model;
            d.ide = p.substr(p.find_last_of('/') + 1);
            d.txd = toLower(o.txd);
            d.drawDistance = o.drawDistances.empty() ? 300 : *std::max_element(o.drawDistances.begin(), o.drawDistances.end());
            d.farModel = isFarModel(d.model, d.drawDistance);
            app.defs[o.id] = d;
        }
    }
    for (auto& kv : app.defs) kv.second.pool = poolFor(objectDat.count(kv.second.model) > 0, pathIds.count(kv.first) > 0);
    for (const std::string& p : ipls) {
        std::string text = slurp(app.root + "/" + p, ok);
        if (!ok) continue;
        IplFile f = parseTextIpl(text, GameVersion::GTA3);
        app.instances.insert(app.instances.end(), f.instances.begin(), f.instances.end());
        app.instFile.insert(app.instFile.end(), f.instances.size(), int(app.iplPaths.size()));
        app.iplPaths.push_back(p);
    }
    for (const std::string& cp : colPaths) {
        std::string bytes = slurp(app.root + "/" + cp, ok);
        if (!ok) continue;
        std::vector<ColModel> models;
        std::string cerr;
        if (!parseCol(std::vector<unsigned char>(bytes.begin(), bytes.end()), models, cerr)) continue;
        for (ColModel& m : models) app.cols.emplace(toLower(m.name), std::move(m));
    }
    std::printf("loaded %zu collision models\n", app.cols.size());
    std::printf("loaded %zu object definitions, %zu placements, %zu archive entries\n", app.defs.size(),
                app.instances.size(), app.img.entries().size());
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

GlModel& getModel(const std::string& name, const std::string& txdName) {
    auto it = app.models.find(name);
    if (it != app.models.end()) return it->second;
    GlModel& gm = app.models[name];
    std::vector<unsigned char> data;
    std::string err;
    DffModel dff;
    if (!app.img.extract(name + ".dff", data, err) || !parseDff(data, dff, err)) return gm;
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
        const bool swapped = swap && group.count(int(i)) > 0;
        if (swapped && !def.farModel) continue;  // LOD view: the normal model gives way to its stand-in
        double dx = inst.x - v.eyeX, dy = inst.y - v.eyeY, dz = inst.z - v.eyeZ;
        double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!swapped && dist > def.drawDistance + 20) continue;
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

// The selection plus the LOD partner of each selected placement: a building and its LOD stand-in must
// always be moved or removed together, otherwise the game crashes (findLodPartners, ipl.h).
std::vector<int> withLodPartners(const std::vector<int>& selection) {
    std::set<int> all(selection.begin(), selection.end());
    for (int idx : selection)
        for (size_t partner : findLodPartners(app.instances, size_t(idx))) all.insert(int(partner));
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

// Delete, as in CMapView::deleteSelected (0x0045d0a0): do nothing without a selection; always take an
// undo snapshot first (not only once per selection); remove every selected placement from its IPL file,
// repairing the LOD links of that file (removeIplInstance); clear the selection. The file is rewritten
// on the next save.
bool deleteSelected() {
    if (app.selected.empty()) return false;
    app.history.snapshot(captureState());
    std::vector<int> idxs = withLodPartners(app.selected);  // MEd leaves the LOD behind, which crashes the game
    std::sort(idxs.begin(), idxs.end(), std::greater<int>());  // highest first so lower indices stay valid
    idxs.erase(std::unique(idxs.begin(), idxs.end()), idxs.end());
    for (int idx : idxs) {
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
    app.selected.clear();
    app.history.endSession();
    buildLodPartnerRanges();
    refreshRegions();
    return true;
}

bool newObjectTargetFile(int& file, std::string& problem);
bool chooseObject(HWND owner, int& id);

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

enum { ID_TB_SAVE = 1001, ID_TB_UNDO, ID_TB_REDO, ID_TB_CENTRE, ID_TB_DELETE, ID_TB_CHECK, ID_TB_POOLS, ID_TB_SAVEMOD, ID_TB_DUPLICATE, ID_TB_OPEN, ID_TB_LODVIEW, ID_TB_NEWOBJ, ID_TB_FILEMENU, ID_TB_REFMENU, ID_TB_COPY, ID_TB_PASTE, ID_TB_BOOKMARKS, ID_TB_VIEWMENU, ID_TB_VIEWEMPTY };

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

void doCommand(HWND h, int id) {
    switch (id) {
        case ID_TB_OPEN: openGameFolder(h); return;
        case ID_TB_BOOKMARKS: showBookmarksWindow(h); return;  // the window keeps the focus
        case ID_TB_VIEWEMPTY: setShowEmptyRegions(!app.showEmptyRegions); break;
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
    } else if (buttonId == ID_TB_VIEWMENU) {
        AppendMenuA(menu, MF_STRING | (app.showEmptyRegions ? MF_CHECKED : MF_UNCHECKED), ID_TB_VIEWEMPTY, "Show hidden regions");
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
    const std::vector<size_t> partners = findLodPartners(app.instances, size_t(idx));
    if (partners.empty()) t += "LOD partner: none\r\n";
    else {
        t += "LOD partner (moved, turned and removed with it):";
        for (size_t pi : partners) t += " " + app.instances[pi].model;
        t += "\r\n";
    }
    if (o.z <= kSunkZ + 0.5) t += "This object is below the map (moved there instead of being deleted).\r\n";
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

// Give the selected placements another model (MEd: Replace Object / the ID box of its properties). Only the selected
// ones: a LOD stand-in keeps its own model. Returns false (with a warning) for an ID that no IDE defines.
bool setModelId(HWND owner, int id) {
    if (app.selected.empty()) return false;
    auto d = app.defs.find(id);
    if (d == app.defs.end()) {
        inspWarn(owner, "No object has the model ID " + std::to_string(id) + ". Press Choose... to pick one from the list.", "Model ID");
        return false;
    }
    app.history.snapshot(captureState());
    for (int idx : app.selected) {
        app.instances[idx].id = id;
        app.instances[idx].model = d->second.name;
        app.dirty.insert(app.instFile[idx]);
    }
    app.history.endSession();
    buildLodPartnerRanges();
    plRefreshAfterEdit();
    return true;
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
                int chosen;
                if (chooseObject(GetParent(w), chosen)) setModelId(w, chosen);
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
    if (!c.ids.empty()) ListView_SetItemState(c.list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    SendMessageA(c.list, WM_SETREDRAW, TRUE, 0);
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
bool chooseObject(HWND owner, int& id) {
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
    RECT r;
    GetWindowRect(owner, &r);
    HWND w = CreateWindowExA(WS_EX_DLGMODALFRAME, "MEdChooser", "New Object - choose an object (type to search)", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, r.left + 80,
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
    make("BUTTON", "Place", BS_DEFPUSHBUTTON | WS_TABSTOP, 0, IDOK);
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

// ---- the Item tab (after MEd's "Item" tab: CIDETree with CIDEFinder, "Find Item Definition", search by name, ID or
// texture) ------------------------------------------------------------------------------------------------------------------
// Every object definition of the IDE files: name, ID, how often it is placed, the game pool its placements take, texture
// dictionary, draw distance, IDE file. Search while typing (name, texture dictionary, or ID when a number is typed), show
// all / placed / not placed, sort by any column. "Add to map" places the item in the region picked in the Placements tab
// (as New Object, without the chooser); "Show where it is placed" (also double-click / Enter) opens the Placements tab
// filtered to that model in all regions. Selecting an object in 3D highlights its definition here.

enum { ID_IT_SEARCH = 6001, ID_IT_SHOW, ID_IT_LIST, ID_IT_COUNT, ID_IT_ADD, ID_IT_WHERE, ID_IT_LBL_SEARCH, ID_IT_LBL_SHOW };

struct ItemUi {
    HWND panel = nullptr, search = nullptr, show = nullptr, list = nullptr, count = nullptr, tips = nullptr;
    std::vector<int> rows;                  // model ids shown, in list order
    std::unordered_map<int, int> placed;    // model id -> number of placements
    int sortCol = 0;                        // by name
    bool sortDesc = false;
    bool syncing = false;
    int lastSelectedId = -2;                // the model id last mirrored from the 3D selection
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
    if (id != it.lastSelectedId) itMirrorSelection();
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
            HWND where = make("BUTTON", "Show where it is placed", BS_PUSHBUTTON, 0, ID_IT_WHERE);
            itAddTip(it.search, "Type part of a name (tree), a texture dictionary (dyncones) or a model ID (1360). Enter: to the list. Esc: clear.");
            itAddTip(it.show, "All definitions, only those placed somewhere on the map, or only the ones not used yet");
            itAddTip(it.list, "Click a column header to sort. Placed: how many placements use it. Pool: the game pool its placements take "
                              "(Treadable is nearly full on the original map). Double-click or Enter: show where it is placed.");
            itAddTip(add, "Place the selected item in the region picked in the Placements tab, in the middle of the view");
            itAddTip(where, "Open the Placements tab with every placement of this item (all regions)");
            return 0;
        }
        case WM_SIZE: {
            const int cw = LOWORD(lp), ch = HIWORD(lp), m = 6, iw = cw - 2 * m;
            int y = m;
            MoveWindow(GetDlgItem(w, ID_IT_LBL_SEARCH), m, y, iw, 16, TRUE); y += 17;
            MoveWindow(it.search, m, y, iw, 22, TRUE); y += 28;
            MoveWindow(GetDlgItem(w, ID_IT_LBL_SHOW), m, y + 4, 36, 16, TRUE);
            MoveWindow(it.show, m + 38, y, iw - 38, 200, TRUE); y += 30;
            const int bottom = ch - m - 26 - 6 - 30;
            MoveWindow(it.list, m, y, iw, std::max(60, bottom - y), TRUE);
            y = std::max(y + 60, bottom) + 4;
            MoveWindow(it.count, m, y, iw, 30, TRUE); y += 32;
            const int half = (iw - 4) / 2;
            MoveWindow(GetDlgItem(w, ID_IT_ADD), m, y, half, 26, TRUE);
            MoveWindow(GetDlgItem(w, ID_IT_WHERE), m + half + 4, y, half, 26, TRUE);
            return 0;
        }
        case WM_COMMAND: {
            const int id = LOWORD(wp), code = HIWORD(wp);
            if (id == ID_IT_SEARCH && code == EN_CHANGE) itRefill();
            else if (id == ID_IT_SHOW && code == CBN_SELCHANGE) itRefill();
            else if (id == ID_IT_ADD && code == BN_CLICKED) itAddToMap(GetParent(w));
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
        ST_CHECK(t, steps >= 6 && std::fabs(steps - std::round(steps)) < 1e-6, "holding > repeats whole steps (" + num(steps, 2) + " steps)");
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

void runBookmarksSelfTest(SelfTest& t) {
    const std::string realFile = app.bookmarkFile;
    const std::vector<Bookmark> realList = app.bookmarks;
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    app.bookmarkFile = std::string(tmp) + "med_selftest_bookmarks.ini";
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

    // 13. a long random sequence of everything
    unsigned seed = std::getenv("MED_SELFTEST_SEED") ? unsigned(std::atoi(std::getenv("MED_SELFTEST_SEED"))) : 12345u;  // other random sequences
    auto rnd = [&](int n) { seed = seed * 1103515245u + 12345u; return n <= 0 ? 0 : int((seed >> 8) % unsigned(n)); };
    const char* terms[] = {"", "cone", "lod", "1360", "a", "x", "bar", "_", "hospital", "zzz", "LOD", "938"};
    int id = -1;
    for (const auto& kv : app.defs) if (kv.second.model == "trafficcone") id = kv.first;
    const int steps = std::getenv("MED_SELFTEST_STEPS") ? std::atoi(std::getenv("MED_SELFTEST_STEPS")) : 1500;
    std::printf("random sequence: seed %u, %d steps\n", seed, steps);
    for (int step = 0; step < steps; ++step) {
        const int op = rnd(18);
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
            if (LOWORD(w) >= ID_TB_SAVE && LOWORD(w) <= ID_TB_VIEWEMPTY) doCommand(h, LOWORD(w));
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
            if (w == VK_ESCAPE) { app.selected.clear(); app.history.endSession(); }  // MEd: Esc cancels the selection
            if (w == 'S' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                saveFromUi(h);
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && w == 'Z') {
                if (GetKeyState(VK_SHIFT) & 0x8000) redoAction(); else undoAction();
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
            SetCapture(h);
            // Alt + left button with a selection drags the selected objects over the plane z - h.
            app.movingObjects = (GetKeyState(VK_MENU) & 0x8000) && !app.selected.empty() &&
                                unprojectToPlane(int(short(LOWORD(l))), int(short(HIWORD(l))) - app.toolbarH, app.lastPick);
            return 0;
        case WM_LBUTTONUP:
            app.dragging = false;
            if (app.movingObjects) plRefreshAfterEdit();
            app.movingObjects = false;
            ReleaseCapture();
            return 0;
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
    bool selfTest = false;
    std::string newRegionName, moveToRegion, newObjectModel, pasteRegion;
    double pastePos[3] = {0, 0, 0};
    double newObjectPos[3] = {0, 0, 0};
    bool deleteAfter = false, duplicateAfter = false, sinkAfter = false, centreAfter = false, checkAfter = false;
    std::string onlyRegions;
    int benchmark = 0;
    std::string selectModel, pickTestModel;
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
        else if (a == "--pick-test" && i + 1 < argc) pickTestModel = argv[++i];
        else if (a == "--only" && i + 1 < argc) onlyRegions = argv[++i];
        else if (a == "--lod-near") app.lodNear = float(num(1)), ++i;
        else if (a == "--no-lod") app.showLod = false;
        else if (a == "--lod-view") app.lodView = true;
        else if (a == "--selftest") selfTest = true;
        else if (a == "--vanilla") app.forceVanilla = true;
        else if (a == "--benchmark") benchmark = int(num(1)), ++i;
        else if (a == "--check") checkAfter = true;
        else if (a == "--sink") sinkAfter = true;
        else if (a == "--duplicate") duplicateAfter = true;
        else if (a == "--new-region" && i + 1 < argc) newRegionName = argv[++i];
        else if (a == "--copy-paste" && i + 4 < argc) { pasteRegion = argv[i + 1]; pastePos[0] = std::atof(argv[i + 2]); pastePos[1] = std::atof(argv[i + 3]); pastePos[2] = std::atof(argv[i + 4]); i += 4; }
        else if (a == "--new-object" && i + 4 < argc) { newObjectModel = argv[i + 1]; newObjectPos[0] = std::atof(argv[i + 2]); newObjectPos[1] = std::atof(argv[i + 3]); newObjectPos[2] = std::atof(argv[i + 4]); i += 4; }
        else if (a == "--move-to-region" && i + 1 < argc) moveToRegion = argv[++i];
        else if (a == "--rotate") rotateBy = num(1), ++i;
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
            for (size_t i = 0; i < app.instances.size(); ++i) {
                if (toLower(app.instances[i].model) != toLower(selectModel)) continue;
                double d = std::hypot(app.instances[i].x - app.cam.x, app.instances[i].y - app.cam.y);
                if (d < bestDist) { bestDist = d; best = int(i); }
            }
            if (best >= 0) app.selected.assign(1, best);
            std::printf("%s\n", best >= 0 ? ("selected " + app.instances[best].model + " by name").c_str() : "no such model");
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
        render();
        SwapBuffers(app.dc);
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
