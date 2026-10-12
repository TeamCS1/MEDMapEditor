// Tests for the collision editor's core (coledit.h): box push/pull, mesh push/pull, deleting faces, open edges, picking,
// and where an edited record is saved.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

#include "coledit.h"

using namespace med;

static int failures = 0;
#define CHECK(c)                                                         \
    do {                                                                 \
        if (!(c)) {                                                      \
            ++failures;                                                  \
            std::printf("FAIL line %d: %s\n", __LINE__, #c);             \
        }                                                                \
    } while (0)

static bool near(double a, double b, double eps = 1e-4) { return std::fabs(a - b) <= eps; }

// A closed box mesh from (x0,y0,z0) to (x1,y1,z1) cut into n x n squares per side (two triangles each), wound outward.
static ColModel boxMesh(float x0, float y0, float z0, float x1, float y1, float z1, int n = 1) {
    ColModel m;
    m.name = "test";
    std::map<std::array<int, 3>, uint32_t> index;  // grid point -> vertex
    auto vtx = [&](int i, int j, int k) {
        const std::array<int, 3> key = {i, j, k};
        auto f = index.find(key);
        if (f != index.end()) return f->second;
        const uint32_t v = uint32_t(m.vertices.size() / 3);
        m.vertices.push_back(x0 + (x1 - x0) * i / n);
        m.vertices.push_back(y0 + (y1 - y0) * j / n);
        m.vertices.push_back(z0 + (z1 - z0) * k / n);
        index[key] = v;
        return v;
    };
    auto quad = [&](uint32_t a, uint32_t b, uint32_t c, uint32_t d) {  // a b c d counter-clockwise seen from outside
        ColFace f;
        f.a = a; f.b = b; f.c = c;
        m.faces.push_back(f);
        f.a = a; f.b = c; f.c = d;
        m.faces.push_back(f);
    };
    for (int s = 0; s < n; ++s)
        for (int t = 0; t < n; ++t) {
            quad(vtx(s, t, n), vtx(s + 1, t, n), vtx(s + 1, t + 1, n), vtx(s, t + 1, n));  // top (+z)
            quad(vtx(s, t, 0), vtx(s, t + 1, 0), vtx(s + 1, t + 1, 0), vtx(s + 1, t, 0));  // bottom (-z)
            quad(vtx(n, s, t), vtx(n, s + 1, t), vtx(n, s + 1, t + 1), vtx(n, s, t + 1));  // +x
            quad(vtx(0, s, t), vtx(0, s, t + 1), vtx(0, s + 1, t + 1), vtx(0, s + 1, t));  // -x
            quad(vtx(s, n, t), vtx(s, n, t + 1), vtx(s + 1, n, t + 1), vtx(s + 1, n, t));  // +y
            quad(vtx(s, 0, t), vtx(s + 1, 0, t), vtx(s + 1, 0, t + 1), vtx(s, 0, t + 1));  // -y
        }
    return m;
}

// Enclosed volume of a closed, outward-wound mesh (divergence theorem).
static double volume(const ColModel& m) {
    double v = 0;
    for (const ColFace& f : m.faces) {
        const float *a = &m.vertices[f.a * 3], *b = &m.vertices[f.b * 3], *c = &m.vertices[f.c * 3];
        v += (double(a[0]) * (double(b[1]) * c[2] - double(b[2]) * c[1]) - double(a[1]) * (double(b[0]) * c[2] - double(b[2]) * c[0]) +
              double(a[2]) * (double(b[0]) * c[1] - double(b[1]) * c[0])) / 6;
    }
    return v;
}

static bool indicesValid(const ColModel& m) {
    const size_t n = m.vertices.size() / 3;
    for (const ColFace& f : m.faces)
        if (f.a >= n || f.b >= n || f.c >= n) return false;
    return true;
}

static double area(const ColModel& m, const std::set<size_t>& faces) {
    double s = 0;
    for (size_t i : faces) {
        const ColFace& f = m.faces[i];
        const float *a = &m.vertices[f.a * 3], *b = &m.vertices[f.b * 3], *c = &m.vertices[f.c * 3];
        const double u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, w[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const double x = u[1] * w[2] - u[2] * w[1], y = u[2] * w[0] - u[0] * w[2], z = u[0] * w[1] - u[1] * w[0];
        s += std::sqrt(x * x + y * y + z * z) / 2;
    }
    return s;
}

int main() {
    // ---- boxes ----
    {
        ColBox b;
        b.min[0] = 0; b.min[1] = 0; b.min[2] = 0;
        b.max[0] = 1; b.max[1] = 2; b.max[2] = 3;
        CHECK(pushPullBoxFace(b, 1, 0.5f) && near(b.max[0], 1.5));    // +x out
        CHECK(pushPullBoxFace(b, 0, 0.5f) && near(b.min[0], -0.5));   // -x out
        CHECK(pushPullBoxFace(b, 5, -1) && near(b.max[2], 2));        // +z in
        CHECK(pushPullBoxFace(b, 2, 1) && near(b.min[1], -1));        // -y out
        const ColBox keep = b;
        CHECK(!pushPullBoxFace(b, 3, -2.99f) && b.max[1] == keep.max[1]);  // thinner than kMinBoxSize: refused
        CHECK(pushPullBoxFace(b, 3, -2.95f) && near(b.max[1] - b.min[1], 0.05));  // exactly the minimum is fine
        CHECK(!pushPullBoxFace(b, 6, 1) && !pushPullBoxFace(b, -1, 1));
        CHECK(!pushPullBoxFace(b, 1, std::nanf("")));
        for (int f = 0; f < 6; ++f) {
            float n[3];
            boxFaceNormal(f, n);
            CHECK(near(n[0] * n[0] + n[1] * n[1] + n[2] * n[2], 1) && n[f / 2] == (f % 2 ? 1.0f : -1.0f));
        }
        ColModel empty;
        const ColBox nb = newBoxFor(empty);
        CHECK(nb.min[0] == -0.5f && nb.max[2] == 0.5f);
        ColModel withBounds = boxMesh(1, 2, 3, 4, 6, 8);
        computeColBounds(withBounds);
        const ColBox wb = newBoxFor(withBounds);
        CHECK(near(wb.min[0], 1) && near(wb.max[1], 6) && near(wb.max[2], 8));
        const ColSphere ws = newSphereFor(withBounds);
        CHECK(near(ws.center[2], 5.5) && near(ws.radius, 2.5));
        CHECK(near(newSphereFor(empty).radius, 0.5));
    }
    // ---- open edges and deleting faces ----
    {
        ColModel c = boxMesh(0, 0, 0, 1, 1, 1);
        CHECK(c.faces.size() == 12 && c.vertices.size() == 24 && openEdges(c).empty() && near(volume(c), 1));
        ColModel g = boxMesh(0, 0, 0, 1, 1, 1, 3);
        CHECK(g.faces.size() == 108 && openEdges(g).empty() && near(volume(g), 1));
        ColModel d = c;
        CHECK(deleteFaces(d, {0}) == 1 && d.faces.size() == 11 && openEdges(d).size() == 3);
        CHECK(deleteFaces(d, {0}) == 1 && openEdges(d).size() == 4);  // the whole top square is a hole
        CHECK(d.vertices.size() == 24 && indicesValid(d));             // its corners are still used by the sides
        ColModel e = c;
        std::set<size_t> all;
        for (size_t i = 0; i < e.faces.size(); ++i) all.insert(i);
        CHECK(deleteFaces(e, all) == 12 && e.faces.empty() && e.vertices.empty());  // unused vertices go too
        ColModel f = c;
        CHECK(deleteFaces(f, {99}) == 0 && f.faces.size() == 12);  // nothing that exists: unchanged
        // deleting faces of one corner removes a vertex no face uses any more, and the rest are renumbered correctly
        ColModel h = c;
        std::set<size_t> corner;
        for (size_t i = 0; i < h.faces.size(); ++i)
            if (h.faces[i].a == 0 || h.faces[i].b == 0 || h.faces[i].c == 0) corner.insert(i);
        const ColModel before = h;
        deleteFaces(h, corner);
        CHECK(h.vertices.size() == 21 && indicesValid(h));
        // every remaining face still has its old corner positions
        size_t k = 0;
        for (size_t i = 0; i < before.faces.size(); ++i) {
            if (corner.count(i)) continue;
            const ColFace &o = before.faces[i], &n = h.faces[k++];
            for (int q = 0; q < 3; ++q) {
                CHECK(before.vertices[o.a * 3 + q] == h.vertices[n.a * 3 + q]);
                CHECK(before.vertices[o.c * 3 + q] == h.vertices[n.c * 3 + q]);
            }
        }
    }
    // ---- push/pull of mesh faces ----
    {
        // a box's top pulled up: the sides stretch, nothing is added
        ColModel c = boxMesh(0, 0, 0, 2, 3, 1);
        const std::set<size_t> top = coplanarNeighbours(c, 0);
        CHECK(top.size() == 2);
        std::string why;
        CHECK(pushPullFaces(c, top, 1.5f, why));
        CHECK(c.faces.size() == 12 && c.vertices.size() == 24 && openEdges(c).empty() && near(volume(c), 6 * 2.5));
        for (size_t i : top) CHECK(near(c.vertices[c.faces[i].a * 3 + 2], 2.5));
        CHECK(pushPullFaces(c, top, -2.0f, why) && near(volume(c), 6 * 0.5) && openEdges(c).empty());  // pushed back in
        // a side of a finer box: in-plane neighbours hold the border, so the vertices are copied and walls added
        ColModel g = boxMesh(0, 0, 0, 3, 3, 3, 3);
        size_t pick = 0;
        float n[3];
        for (size_t i = 0; i < g.faces.size(); ++i) {  // a triangle of the middle square of the top
            const ColFace& f = g.faces[i];
            float cx = 0, cy = 0, cz = 0;
            for (uint32_t v : {f.a, f.b, f.c}) { cx += g.vertices[v * 3]; cy += g.vertices[v * 3 + 1]; cz += g.vertices[v * 3 + 2]; }
            if (near(cz / 3, 3) && cx / 3 > 1 && cx / 3 < 2 && cy / 3 > 1 && cy / 3 < 2) { pick = i; break; }
        }
        std::set<size_t> square = coplanarNeighbours(g, pick);
        CHECK(square.size() == 18);  // the whole flat top
        // only the middle square: the triangle and the one sharing its diagonal
        std::set<size_t> mid = {pick};
        for (size_t i = 0; i < g.faces.size(); ++i) {
            if (i == pick) continue;
            const ColFace &a = g.faces[pick], &b = g.faces[i];
            int shared = 0;
            for (uint32_t x : {a.a, a.b, a.c}) for (uint32_t y : {b.a, b.b, b.c}) shared += x == y;
            if (shared == 2 && faceNormal(g, i, n) && near(n[2], 1)) {
                float cx = 0, cy = 0;
                for (uint32_t v : {b.a, b.b, b.c}) { cx += g.vertices[v * 3]; cy += g.vertices[v * 3 + 1]; }
                if (cx / 3 > 1 && cx / 3 < 2 && cy / 3 > 1 && cy / 3 < 2) mid.insert(i);
            }
        }
        CHECK(mid.size() == 2);
        const double v0 = volume(g);
        const size_t faces0 = g.faces.size(), verts0 = g.vertices.size() / 3;
        CHECK(pushPullFaces(g, mid, 2.0f, why));
        CHECK(g.vertices.size() / 3 == verts0 + 4 && g.faces.size() == faces0 + 8);  // 4 copies, 4 walls of 2 triangles
        CHECK(openEdges(g).empty() && near(volume(g), v0 + 2.0) && indicesValid(g));
        for (size_t i = faces0; i < g.faces.size(); ++i) {  // the walls stand upright and face outward
            CHECK(faceNormal(g, i, n) && near(n[2], 0));
            const ColFace& f = g.faces[i];
            float cx = 0, cy = 0;
            for (uint32_t v : {f.a, f.b, f.c}) { cx += g.vertices[v * 3]; cy += g.vertices[v * 3 + 1]; }
            CHECK((cx / 3 - 1.5f) * n[0] + (cy / 3 - 1.5f) * n[1] > 0);
        }
        // pushed into the box instead: a pocket, still closed, volume smaller
        ColModel p = boxMesh(0, 0, 0, 3, 3, 3, 3);
        const double pv = volume(p);
        CHECK(pushPullFaces(p, mid, -1.0f, why) && openEdges(p).empty() && near(volume(p), pv - 1.0));
        // refusals leave the model alone
        ColModel r = boxMesh(0, 0, 0, 1, 1, 1);
        const ColModel keep = r;
        CHECK(!pushPullFaces(r, {}, 1, why) && !why.empty());
        CHECK(!pushPullFaces(r, {0}, 0, why) && !pushPullFaces(r, {0}, std::nanf(""), why));
        CHECK(!pushPullFaces(r, {99}, 1, why));
        std::set<size_t> opposite = coplanarNeighbours(r, 0);  // top and bottom together: no direction
        for (size_t i = 0; i < r.faces.size(); ++i)
            if (faceNormal(r, i, n) && near(n[2], -1)) opposite.insert(i);
        CHECK(opposite.size() == 4 && !pushPullFaces(r, opposite, 1, why) && why.find("opposite") != std::string::npos);
        CHECK(r.faces.size() == keep.faces.size() && r.vertices == keep.vertices);
        // a lone quad (nothing around it) just moves
        ColModel q;
        q.vertices = {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
        ColFace f;
        f.a = 0; f.b = 1; f.c = 2; q.faces.push_back(f);
        f.a = 0; f.b = 2; f.c = 3; q.faces.push_back(f);
        CHECK(pushPullFaces(q, {0, 1}, 1, why) && q.faces.size() == 2 && q.vertices.size() == 12 && near(q.vertices[2], 1) && openEdges(q).size() == 4);
        // the walls take the material of the face beside them
        ColModel mat = boxMesh(0, 0, 0, 3, 3, 3, 3);
        for (size_t i : mid) mat.faces[i].material = 17;
        const size_t mf = mat.faces.size();
        CHECK(pushPullFaces(mat, mid, 1, why));
        for (size_t i = mf; i < mat.faces.size(); ++i) CHECK(mat.faces[i].material == 17);
        // written and read back unchanged
        std::vector<ColModel> back;
        std::string err;
        CHECK(parseCol(writeColl(g), back, err) && back.size() == 1 && back[0].faces.size() == g.faces.size() && back[0].vertices == g.vertices);
    }
    // ---- random push/pulls keep a box closed and change its volume by exactly area x distance ----
    {
        unsigned seed = 12345;
        auto rnd = [&](int n) { seed = seed * 1103515245u + 12345u; return int((seed >> 8) % unsigned(n)); };
        int runs = 0;
        for (int trial = 0; trial < 300; ++trial) {
            const int div = 1 + rnd(3);
            ColModel m = boxMesh(0, 0, 0, float(2 + rnd(5)), float(2 + rnd(5)), float(2 + rnd(5)), div);
            for (int step = 0; step < 6; ++step) {
                const size_t face = size_t(rnd(int(m.faces.size())));
                // a whole flat side, or one square of it (the face and its diagonal partner)
                std::set<size_t> sel = coplanarNeighbours(m, face);
                if (sel.empty()) continue;
                if (rnd(2)) {
                    std::set<size_t> one = {face};
                    const ColFace& a = m.faces[face];
                    float na[3];
                    faceNormal(m, face, na);
                    for (size_t i : sel) {
                        if (i == face) continue;
                        const ColFace& b = m.faces[i];
                        int shared = 0;
                        for (uint32_t x : {a.a, a.b, a.c}) for (uint32_t y : {b.a, b.b, b.c}) shared += x == y;
                        if (shared == 2) { one.insert(i); break; }
                    }
                    sel = one;
                }
                const double before = volume(m), a = area(m, sel);
                const float d = float(rnd(2) ? 1 : -1) * (0.25f + 0.25f * float(rnd(3)));
                // pushing in is only safe while the shape stays thicker than the push; keep pulls outward then
                std::string why;
                const float dd = d < 0 ? -0.25f : d;
                CHECK(pushPullFaces(m, sel, dd, why));
                CHECK(openEdges(m).empty());
                CHECK(indicesValid(m));
                CHECK(near(volume(m), before + a * dd, 1e-3 * std::max(1.0, before)));
                ++runs;
            }
        }
        CHECK(runs > 1000);
    }
    // ---- picking ----
    {
        ColModel m = boxMesh(0, 0, 0, 2, 2, 2);
        ColBox b;
        b.min[0] = 5; b.min[1] = 0; b.min[2] = 0;
        b.max[0] = 6; b.max[1] = 1; b.max[2] = 1;
        m.boxes.push_back(b);
        ColSphere s;
        s.center[0] = -5; s.center[1] = 0.5f; s.center[2] = 0.5f;
        s.radius = 1;
        m.spheres.push_back(s);
        const float down[3] = {0, 0, -1};
        const float o1[3] = {1, 1, 10};
        ColHit h = pickCol(m, o1, down);
        CHECK(h.kind == ColHit::Face && near(h.t, 8));
        float n[3];
        CHECK(faceNormal(m, size_t(h.index), n) && near(n[2], 1));  // the top
        const float o2[3] = {5.5f, 0.5f, 10};
        h = pickCol(m, o2, down);
        CHECK(h.kind == ColHit::Box && h.index == 0 && h.boxFace == 5 && near(h.t, 9));  // the box's top
        const float side[3] = {-1, 0, 0};
        const float o3[3] = {20, 0.5f, 0.5f};
        h = pickCol(m, o3, side);
        CHECK(h.kind == ColHit::Box && h.boxFace == 1 && near(h.t, 14));  // the box's +x side, nearer than the mesh
        const float o4[3] = {5.5f, 0.5f, 0.5f};  // from inside the box: the side it leaves through
        h = pickCol(m, o4, side);
        CHECK(h.kind == ColHit::Box && h.boxFace == 0 && near(h.t, 0.5));
        const float o5[3] = {-5, 0.5f, 10};
        h = pickCol(m, o5, down);
        CHECK(h.kind == ColHit::Sphere && near(h.t, 8.5));
        CHECK(pickCol(m, o5, down, true, false, true).kind == ColHit::None);  // spheres hidden
        CHECK(pickCol(m, o1, down, true, true, false).kind == ColHit::None);  // mesh hidden
        const float up[3] = {0, 0, 1};
        CHECK(pickCol(m, o1, up).kind == ColHit::None);  // pointing away
        const float slow[3] = {0, 0, -2};  // t is in the ray's own units
        CHECK(near(pickCol(m, o1, slow).t, 4));
    }
    // ---- where an edited record goes ----
    {
        const std::string bs(1, char(92));
        ColSaveTarget t = colSaveTarget("upstate", 1);
        CHECK(t.relPath == "data/maps/upstate/upstate_lvl1.col" && t.datLine == "COLFILE 1 DATA" + bs + "MAPS" + bs + "upstate" + bs + "upstate_lvl1.COL" && t.level == 1);
        t = colSaveTarget("upstate", 0);
        CHECK(t.relPath == "data/maps/upstate/upstate.col" && t.datLine == "COLFILE 0 DATA" + bs + "MAPS" + bs + "upstate" + bs + "upstate.COL");
        const std::string dat = "# comment\r\nCOLFILE 0 MODELS" + bs + "COLL" + bs + "GENERIC.COL\r\nCOLFILE 1 DATA" + bs + "MAPS" + bs + "INDUSTNE" + bs +
                                "INDUSTNE.col\r\nCOLFILE 2 DATA" + bs + "MAPS" + bs + "COMSE" + bs + "COMSE.col\r\n#COLFILE 3 DATA" + bs + "X.COL\r\nIPL DATA" + bs + "X.IPL\r\n";
        CHECK(colFileLevel(dat, "data/maps/industne/INDUSTNE.COL") == 1);
        CHECK(colFileLevel(dat, "DATA" + bs + "MAPS" + bs + "COMSE" + bs + "COMSE.col") == 2);
        CHECK(colFileLevel(dat, "models/coll/generic.col") == 0);
        CHECK(colFileLevel(dat, "data/x.col") == -1);  // only in a comment
        CHECK(colFileLevel(dat, "data/maps/nothere.col") == -1);
    }
    if (failures == 0) std::printf("all collision editor tests passed\n");
    return failures ? 1 : 0;
}
