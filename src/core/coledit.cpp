#include "coledit.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace med {

namespace {

const float* vert(const ColModel& m, uint32_t i) { return &m.vertices[size_t(i) * 3]; }

bool faceOk(const ColModel& m, const ColFace& f) {
    const size_t n = m.vertices.size() / 3;
    return f.a < n && f.b < n && f.c < n;
}

// The face's cross product (twice its area, along its normal); false for a bad index.
bool faceCross(const ColModel& m, const ColFace& f, double c[3]) {
    if (!faceOk(m, f)) return false;
    const float *a = vert(m, f.a), *b = vert(m, f.b), *d = vert(m, f.c);
    const double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2], wx = d[0] - a[0], wy = d[1] - a[1], wz = d[2] - a[2];
    c[0] = uy * wz - uz * wy;
    c[1] = uz * wx - ux * wz;
    c[2] = ux * wy - uy * wx;
    return true;
}

uint64_t edgeKey(uint32_t a, uint32_t b) { return a < b ? (uint64_t(a) << 32) | b : (uint64_t(b) << 32) | a; }

std::string lowerSlashes(std::string s) {
    for (char& c : s) {
        if (c == char(92)) c = '/';
        c = char(std::tolower((unsigned char)c));
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
    size_t k = 0;
    while (k < s.size() && (s[k] == ' ' || s[k] == '\t')) ++k;
    return s.substr(k);
}

}  // namespace

// ---- boxes and spheres ----

void boxFaceNormal(int face, float n[3]) {
    n[0] = n[1] = n[2] = 0;
    if (face < 0 || face > 5) return;
    n[face / 2] = (face % 2) ? 1.0f : -1.0f;
}

bool pushPullBoxFace(ColBox& box, int face, float d) {
    if (face < 0 || face > 5 || !std::isfinite(d)) return false;
    const int axis = face / 2;
    float mn = box.min[axis], mx = box.max[axis];
    if (face % 2) mx += d;
    else mn -= d;
    if (mx - mn < kMinBoxSize - 1e-6f) return false;
    box.min[axis] = mn;
    box.max[axis] = mx;
    return true;
}

namespace {
bool hasBounds(const ColModel& m) {
    for (int k = 0; k < 3; ++k)
        if (!(m.boundMax[k] > m.boundMin[k])) return false;
    return true;
}
}  // namespace

ColBox newBoxFor(const ColModel& m) {
    ColBox b;
    for (int k = 0; k < 3; ++k) {
        b.min[k] = hasBounds(m) ? m.boundMin[k] : -0.5f;
        b.max[k] = hasBounds(m) ? m.boundMax[k] : 0.5f;
    }
    return b;
}

ColSphere newSphereFor(const ColModel& m) {
    ColSphere s;
    if (hasBounds(m)) {
        for (int k = 0; k < 3; ++k) s.center[k] = (m.boundMin[k] + m.boundMax[k]) / 2;
        float r = 0;
        for (int k = 0; k < 3; ++k) r = std::max(r, (m.boundMax[k] - m.boundMin[k]) / 2);
        s.radius = std::max(r, kMinSphereRadius);
    } else {
        s.radius = 0.5f;
    }
    return s;
}

// ---- the mesh ----

bool faceNormal(const ColModel& m, size_t face, float n[3]) {
    if (face >= m.faces.size()) return false;
    double c[3];
    if (!faceCross(m, m.faces[face], c)) return false;
    const double len = std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
    if (len < 1e-12) return false;
    for (int k = 0; k < 3; ++k) n[k] = float(c[k] / len);
    return true;
}

std::vector<std::array<uint32_t, 2>> openEdges(const ColModel& m) {
    std::map<uint64_t, int> count;
    for (const ColFace& f : m.faces) {
        if (!faceOk(m, f)) continue;
        ++count[edgeKey(f.a, f.b)];
        ++count[edgeKey(f.b, f.c)];
        ++count[edgeKey(f.c, f.a)];
    }
    std::vector<std::array<uint32_t, 2>> out;
    for (const auto& kv : count)
        if (kv.second == 1) out.push_back({uint32_t(kv.first >> 32), uint32_t(kv.first & 0xffffffffu)});
    return out;
}

void removeUnusedVertices(ColModel& m) {
    const size_t n = m.vertices.size() / 3;
    std::vector<int64_t> newIndex(n, -1);
    std::vector<float> kept;
    // keep the vertices in their old order (only the unused ones drop out)
    std::vector<bool> used(n, false);
    for (const ColFace& f : m.faces)
        for (uint32_t i : {f.a, f.b, f.c})
            if (i < n) used[i] = true;
    for (size_t i = 0; i < n; ++i)
        if (used[i]) {
            newIndex[i] = int64_t(kept.size() / 3);
            kept.insert(kept.end(), m.vertices.begin() + std::ptrdiff_t(i) * 3, m.vertices.begin() + std::ptrdiff_t(i) * 3 + 3);
        }
    for (ColFace& f : m.faces) {
        for (uint32_t* i : {&f.a, &f.b, &f.c})
            if (*i < n) *i = uint32_t(newIndex[*i]);
    }
    m.vertices.swap(kept);
}

size_t deleteFaces(ColModel& m, const std::set<size_t>& faces) {
    std::vector<ColFace> kept;
    size_t gone = 0;
    for (size_t i = 0; i < m.faces.size(); ++i) {
        if (faces.count(i)) ++gone;
        else kept.push_back(m.faces[i]);
    }
    if (!gone) return 0;
    m.faces.swap(kept);
    removeUnusedVertices(m);
    return gone;
}

bool pushPullFaces(ColModel& m, const std::set<size_t>& faces, float d, std::string& why) {
    if (faces.empty()) { why = "No faces are selected."; return false; }
    if (!(std::isfinite(d)) || d == 0) { why = "The distance must be a number other than 0."; return false; }
    double sum[3] = {0, 0, 0};
    for (size_t i : faces) {
        if (i >= m.faces.size() || !faceOk(m, m.faces[i])) { why = "A selected face does not exist."; return false; }
        double c[3];
        faceCross(m, m.faces[i], c);
        for (int k = 0; k < 3; ++k) sum[k] += c[k];
    }
    const double len = std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
    if (len < 1e-9) { why = "The selected faces point in opposite directions, so there is no direction to push them."; return false; }
    const double N[3] = {sum[0] / len, sum[1] / len, sum[2] / len};
    const float off[3] = {float(N[0] * d), float(N[1] * d), float(N[2] * d)};

    // which vertices the selection uses, and the unselected faces at each
    std::map<uint32_t, std::vector<size_t>> others;  // vertex -> unselected faces using it
    std::set<uint32_t> selVerts;
    for (size_t i : faces) for (uint32_t v : {m.faces[i].a, m.faces[i].b, m.faces[i].c}) selVerts.insert(v);
    for (size_t i = 0; i < m.faces.size(); ++i) {
        if (faces.count(i)) continue;
        const ColFace& f = m.faces[i];
        for (uint32_t v : {f.a, f.b, f.c})
            if (selVerts.count(v)) others[v].push_back(i);
    }
    // the border of the selection (directed as in its face), before anything changes
    std::map<uint64_t, int> selEdgeCount;
    for (size_t i : faces) {
        const ColFace& f = m.faces[i];
        ++selEdgeCount[edgeKey(f.a, f.b)];
        ++selEdgeCount[edgeKey(f.b, f.c)];
        ++selEdgeCount[edgeKey(f.c, f.a)];
    }
    struct Border { uint32_t u, v; uint8_t material, light; };
    std::vector<Border> border;
    for (size_t i : faces) {
        const ColFace& f = m.faces[i];
        const uint32_t e[3][2] = {{f.a, f.b}, {f.b, f.c}, {f.c, f.a}};
        for (const auto& uv : e)
            if (selEdgeCount[edgeKey(uv[0], uv[1])] == 1) border.push_back({uv[0], uv[1], f.material, f.light});
    }
    // move, stretch or copy each vertex
    std::map<uint32_t, uint32_t> moved;  // vertex -> the index the selection uses afterwards
    for (uint32_t v : selVerts) {
        bool copy = false;
        auto it = others.find(v);
        if (it != others.end())
            for (size_t fi : it->second) {
                double c[3];
                if (!faceCross(m, m.faces[fi], c)) continue;
                const double cl = std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
                if (cl < 1e-12) continue;  // a face without area does not hold the vertex back
                if (std::fabs((c[0] * N[0] + c[1] * N[1] + c[2] * N[2]) / cl) > 0.02) { copy = true; break; }
            }
        if (copy) {
            const uint32_t ni = uint32_t(m.vertices.size() / 3);
            const float* p = vert(m, v);
            const float q[3] = {p[0] + off[0], p[1] + off[1], p[2] + off[2]};
            m.vertices.insert(m.vertices.end(), q, q + 3);
            moved[v] = ni;
        } else {
            moved[v] = v;
        }
    }
    for (uint32_t v : selVerts)
        if (moved[v] == v)
            for (int k = 0; k < 3; ++k) m.vertices[size_t(v) * 3 + size_t(k)] += off[k];
    for (size_t i : faces) {
        ColFace& f = m.faces[i];
        f.a = moved[f.a];
        f.b = moved[f.b];
        f.c = moved[f.c];
    }
    // side walls where the selection came away from the rest
    for (const Border& b : border) {
        const uint32_t nu = moved[b.u], nv = moved[b.v];
        if (nu == b.u && nv == b.v) continue;
        const uint32_t tri[2][3] = {{b.u, b.v, nv}, {b.u, nv, nu}};
        for (const auto& t : tri) {
            if (t[0] == t[1] || t[1] == t[2] || t[0] == t[2]) continue;
            ColFace w;
            w.a = t[0];
            w.b = t[1];
            w.c = t[2];
            w.material = b.material;
            w.light = b.light;
            double c[3];
            faceCross(m, w, c);
            if (std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]) < 1e-9) continue;
            m.faces.push_back(w);
        }
    }
    return true;
}

std::set<size_t> coplanarNeighbours(const ColModel& m, size_t face) {
    std::set<size_t> out;
    float n0[3];
    if (!faceNormal(m, face, n0)) return out;
    const float* p0 = vert(m, m.faces[face].a);
    const double plane = double(n0[0]) * p0[0] + double(n0[1]) * p0[1] + double(n0[2]) * p0[2];
    std::map<uint64_t, std::vector<size_t>> byEdge;
    for (size_t i = 0; i < m.faces.size(); ++i) {
        const ColFace& f = m.faces[i];
        if (!faceOk(m, f)) continue;
        byEdge[edgeKey(f.a, f.b)].push_back(i);
        byEdge[edgeKey(f.b, f.c)].push_back(i);
        byEdge[edgeKey(f.c, f.a)].push_back(i);
    }
    std::vector<size_t> todo = {face};
    out.insert(face);
    while (!todo.empty()) {
        const size_t i = todo.back();
        todo.pop_back();
        const ColFace& f = m.faces[i];
        for (uint64_t e : {edgeKey(f.a, f.b), edgeKey(f.b, f.c), edgeKey(f.c, f.a)})
            for (size_t j : byEdge[e]) {
                if (out.count(j)) continue;
                float n[3];
                if (!faceNormal(m, j, n)) continue;
                if (n[0] * n0[0] + n[1] * n0[1] + n[2] * n0[2] < 0.99985f) continue;  // about 1 degree
                bool inPlane = true;
                for (uint32_t v : {m.faces[j].a, m.faces[j].b, m.faces[j].c}) {
                    const float* p = vert(m, v);
                    if (std::fabs(double(n0[0]) * p[0] + double(n0[1]) * p[1] + double(n0[2]) * p[2] - plane) > 0.01) inPlane = false;
                }
                if (!inPlane) continue;
                out.insert(j);
                todo.push_back(j);
            }
    }
    return out;
}

// ---- picking ----

ColHit pickCol(const ColModel& m, const float o[3], const float dir[3], bool boxes, bool spheres, bool mesh) {
    ColHit best;
    auto take = [&](ColHit::Kind kind, int index, int boxFace, double t) {
        if (!(t > 1e-6) || (best.kind != ColHit::None && t >= best.t)) return;
        best.kind = kind;
        best.index = index;
        best.boxFace = boxFace;
        best.t = float(t);
    };
    if (spheres)
        for (size_t i = 0; i < m.spheres.size(); ++i) {
            const ColSphere& s = m.spheres[i];
            const double ox = o[0] - s.center[0], oy = o[1] - s.center[1], oz = o[2] - s.center[2];
            const double a = double(dir[0]) * dir[0] + double(dir[1]) * dir[1] + double(dir[2]) * dir[2];
            const double b = 2 * (ox * dir[0] + oy * dir[1] + oz * dir[2]);
            const double c = ox * ox + oy * oy + oz * oz - double(s.radius) * s.radius;
            const double disc = b * b - 4 * a * c;
            if (a <= 0 || disc < 0) continue;
            const double r = std::sqrt(disc);
            const double t0 = (-b - r) / (2 * a), t1 = (-b + r) / (2 * a);
            take(ColHit::Sphere, int(i), -1, t0 > 1e-6 ? t0 : t1);
        }
    if (boxes)
        for (size_t i = 0; i < m.boxes.size(); ++i) {
            const ColBox& bx = m.boxes[i];
            double tmin = -1e30, tmax = 1e30;
            int enter = -1, leave = -1;
            bool miss = false;
            for (int k = 0; k < 3 && !miss; ++k) {
                if (std::fabs(dir[k]) < 1e-12) {
                    if (o[k] < bx.min[k] || o[k] > bx.max[k]) miss = true;
                    continue;
                }
                double t1 = (bx.min[k] - o[k]) / dir[k], t2 = (bx.max[k] - o[k]) / dir[k];
                int f1 = k * 2, f2 = k * 2 + 1;  // entering through min when moving +, through max when moving -
                if (t1 > t2) { std::swap(t1, t2); std::swap(f1, f2); }
                if (t1 > tmin) { tmin = t1; enter = f1; }
                if (t2 < tmax) { tmax = t2; leave = f2; }
                if (tmin > tmax) miss = true;
            }
            if (miss) continue;
            if (tmin > 1e-6) take(ColHit::Box, int(i), enter, tmin);
            else take(ColHit::Box, int(i), leave, tmax);  // the ray starts inside: the side it leaves through
        }
    if (mesh)
        for (size_t i = 0; i < m.faces.size(); ++i) {
            const ColFace& f = m.faces[i];
            if (!faceOk(m, f)) continue;
            const float *a = vert(m, f.a), *b = vert(m, f.b), *c = vert(m, f.c);
            const double e1[3] = {double(b[0]) - a[0], double(b[1]) - a[1], double(b[2]) - a[2]};
            const double e2[3] = {double(c[0]) - a[0], double(c[1]) - a[1], double(c[2]) - a[2]};
            const double p[3] = {dir[1] * e2[2] - dir[2] * e2[1], dir[2] * e2[0] - dir[0] * e2[2], dir[0] * e2[1] - dir[1] * e2[0]};
            const double det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
            if (std::fabs(det) < 1e-12) continue;
            const double inv = 1 / det;
            const double s[3] = {double(o[0]) - a[0], double(o[1]) - a[1], double(o[2]) - a[2]};
            const double u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv;
            if (u < 0 || u > 1) continue;
            const double q[3] = {s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0]};
            const double v = (dir[0] * q[0] + dir[1] * q[1] + dir[2] * q[2]) * inv;
            if (v < 0 || u + v > 1) continue;
            take(ColHit::Face, int(i), -1, (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv);
        }
    return best;
}

// ---- surface materials ----

const char* colMaterialName(int material) {
    static const char* names[kColMaterialCount] = {
        "Default",            "Tarmac",          "Grass",           "Gravel",          "Dry mud",        "Pavement",
        "Car",                "Glass",           "See-through cloth", "Garage door",   "Car panel",      "Thick metal plate",
        "Scaffold pole",      "Lamp post",       "Fire hydrant",    "Girder",          "Chain-link fence", "Pedestrian",
        "Sand",               "Water",           "Wooden crates",   "Wooden bench",    "Solid wood",     "Rubber",
        "Plastic",            "Hedge",           "Steep cliff",     "Container",       "News stand",     "Wheel base",
        "Cardboard box",      "See-through stone", "Metal gate",    "(not used in GTA III)", "(not used in GTA III)"};
    return material >= 0 && material < kColMaterialCount ? names[material] : "";
}

GripGroup colMaterialGrip(int material) {
    // the game's CSurfaceTable function at 0x004aba60, case by case
    switch (material) {
        case 2: case 3: case 18: case 25: case 26: case 30: return GripGroup::Loose;
        case 4: case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 16:
        case 24: case 27: case 28: case 31: case 32: return GripGroup::Hard;
        case 17: case 23: case 29: return GripGroup::Rubber;
        case 19: return GripGroup::Wet;
        default: return GripGroup::Road;  // 0, 1, 5, 20, 21, 22 and anything else
    }
}

const char* gripGroupName(GripGroup g) {
    switch (g) {
        case GripGroup::Rubber: return "rubber";
        case GripGroup::Hard: return "hard";
        case GripGroup::Road: return "road";
        case GripGroup::Loose: return "loose";
        case GripGroup::Wet: return "wet";
    }
    return "";
}

// ---- saving ----

ColSaveTarget colSaveTarget(const std::string& region, int level) {
    ColSaveTarget t;
    t.level = std::max(0, std::min(level, 9));
    const std::string file = region + (t.level ? "_lvl" + std::to_string(t.level) : std::string());
    const std::string bs(1, char(92));
    t.relPath = "data/maps/" + region + "/" + file + ".col";
    t.datLine = "COLFILE " + std::to_string(t.level) + " DATA" + bs + "MAPS" + bs + region + bs + file + ".COL";
    return t;
}

int colFileLevel(const std::string& gta3dat, const std::string& relPath) {
    const std::string want = lowerSlashes(relPath);
    size_t pos = 0;
    int level = -1;
    while (pos <= gta3dat.size()) {
        size_t e = gta3dat.find('\n', pos);
        const std::string line = gta3dat.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
        pos = e == std::string::npos ? gta3dat.size() + 1 : e + 1;
        // the game: "COLFILE" in column 0, the level in column 8, the path from column 10 (CFileLoader_LoadLevel)
        if (line.size() < 11 || lowerSlashes(line.substr(0, 7)) != "colfile") continue;
        if (line[8] < '0' || line[8] > '9') continue;
        if (lowerSlashes(line.substr(10)) == want) level = line[8] - '0';  // the last line naming it decides
    }
    return level;
}

}  // namespace med
