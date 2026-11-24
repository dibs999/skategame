// Park construction: terrain height field, solid boxes, rideable height fields,
// grind edges, and all static geometry with materials.
#include "world.h"
#include "geom.h"
#include <cmath>
#include <vector>
#include <deque>
#include <algorithm>

// ---- height fields (rideable analytic surfaces) ----
struct HeightField {
    float x0, z0, x1, z1;
    int type;
    float p0, p1;
    float eval(float x, float z) const {
        (void)z;
        switch (type) {
        case 0: { // quarter pipe rising with +x: foot at p0, radius p1
            float dx = x - p0;
            if (dx < 0 || dx > p1) return -1e9f;
            float v = p1 * p1 - dx * dx;
            return p1 - std::sqrt(v < 0 ? 0 : v);
        }
        case 1: { // linear slope: h=p0 at x0, h=p1 at x1
            float t = (x - x0) / (x1 - x0);
            return p0 + t * (p1 - p0);
        }
        case 2: { // quarter pipe rising with -x: foot at p0, radius p1
            float dx = p0 - x;
            if (dx < 0 || dx > p1) return -1e9f;
            float v = p1 * p1 - dx * dx;
            return p1 - std::sqrt(v < 0 ? 0 : v);
        }
        }
        return -1e9f;
    }
};

static std::vector<HeightField> g_fields;
static std::deque<Mesh> g_meshes;   // stable addresses for DrawItems

float World::terrainHeight(float x, float z) const {
    float h = 0.f;
    float d = std::hypot(x - BOWL_X, z - BOWL_Z);
    if (d < BOWL_R) {
        if (d < BOWL_R * 0.45f) h = -BOWL_D;
        else {
            float t = (BOWL_R - d) / (BOWL_R * 0.55f);
            h = -BOWL_D * (t * t * (3 - 2 * t));
        }
    }
    return h;
}

float World::supportHeight(float x, float z, float refY) const {
    float h = terrainHeight(x, z);
    for (auto& b : solids)
        if (x >= b.mn.x && x <= b.mx.x && z >= b.mn.z && z <= b.mx.z &&
            b.mx.y <= refY + STEP && b.mx.y > h)
            h = b.mx.y;
    for (auto& f : g_fields)
        if (x >= f.x0 && x <= f.x1 && z >= f.z0 && z <= f.z1) {
            float hh = f.eval(x, z);
            if (hh > -1e8f && hh <= refY + STEP && hh > h) h = hh;
        }
    return h;
}


Vec3 World::supportNormal(float x, float z, float refY) const {
    float h = terrainHeight(x, z);
    int win = -2; // -2 terrain, -1 box, >=0 field index
    for (auto& b : solids)
        if (x >= b.mn.x && x <= b.mx.x && z >= b.mn.z && z <= b.mx.z &&
            b.mx.y <= refY + STEP && b.mx.y > h) { h = b.mx.y; win = -1; }
    for (size_t i = 0; i < g_fields.size(); i++) {
        auto& f = g_fields[i];
        if (x >= f.x0 && x <= f.x1 && z >= f.z0 && z <= f.z1) {
            float hh = f.eval(x, z);
            if (hh > -1e8f && hh <= refY + STEP && hh > h) { h = hh; win = (int)i; }
        }
    }
    if (win == -1) return {0, 1, 0};
    float e = 0.06f;
    auto surf = [&](float X, float Z) {
        if (win >= 0) {
            float hh = g_fields[win].eval(X, Z);
            return hh > -1e8f ? hh : h;
        }
        return terrainHeight(X, Z);
    };
    float dhdx = (surf(x + e, z) - surf(x - e, z)) / (2 * e);
    float dhdz = (surf(x, z + e) - surf(x, z - e)) / (2 * e);
    return Vec3(-dhdx, 1, -dhdz).normalized();
}

bool World::collideWalls(Vec3& pos, Vec3& vel) const {
    bool hit = false;
    const float r = 0.26f;
    for (auto& b : solids) {
        if (pos.x > b.mn.x - r && pos.x < b.mx.x + r &&
            pos.z > b.mn.z - r && pos.z < b.mx.z + r &&
            pos.y + 0.05f > b.mn.y && pos.y < b.mx.y - STEP) {
            float px0 = pos.x - (b.mn.x - r), px1 = (b.mx.x + r) - pos.x;
            float pz0 = pos.z - (b.mn.z - r), pz1 = (b.mx.z + r) - pos.z;
            float m = std::min(std::min(px0, px1), std::min(pz0, pz1));
            if (m == px0) { pos.x = b.mn.x - r; if (vel.x > 0) vel.x = 0; }
            else if (m == px1) { pos.x = b.mx.x + r; if (vel.x < 0) vel.x = 0; }
            else if (m == pz0) { pos.z = b.mn.z - r; if (vel.z > 0) vel.z = 0; }
            else { pos.z = b.mx.z + r; if (vel.z < 0) vel.z = 0; }
            hit = true;
        }
    }
    // height fields whose surface is well above the skater are walls
    // (all fields ramp along x, so push out along x)
    for (auto& f : g_fields) {
        if (pos.x < f.x0 - r || pos.x > f.x1 + r ||
            pos.z < f.z0 - r || pos.z > f.z1 + r) continue;
        float hh = f.eval(pos.x, pos.z);
        if (hh > -1e8f && hh > pos.y + STEP + 0.05f) {
            if (pos.x - f.x0 < f.x1 - pos.x) { pos.x = f.x0 - r; if (vel.x > 0) vel.x = 0; }
            else { pos.x = f.x1 + r; if (vel.x < 0) vel.x = 0; }
            hit = true;
        }
    }
    if (pos.x < -72) { pos.x = -72; if (vel.x < 0) vel.x = 0; hit = true; }
    if (pos.x > 104) { pos.x = 104; if (vel.x > 0) vel.x = 0; hit = true; }
    if (pos.z < -47) { pos.z = -47; if (vel.z < 0) vel.z = 0; hit = true; }
    if (pos.z > 62) { pos.z = 62; if (vel.z > 0) vel.z = 0; hit = true; }
    return hit;
}

int World::findGrindEdge(const Vec3& p) const {
    int best = -1;
    float bestD = 1e9f;
    for (size_t i = 0; i < edges.size(); i++) {
        auto& e = edges[i];
        Vec3 ab = e.b - e.a;
        float t = clampf((p - e.a).dot(ab) / std::max(ab.dot(ab), 1e-5f), 0.f, 1.f);
        Vec3 c = e.a + ab * t;
        float dxz = std::hypot(p.x - c.x, p.z - c.z);
        float dy = std::abs(p.y - c.y);
        float tol = e.rail ? 0.42f : 0.5f;
        if (dxz < tol && dy < 0.5f && p.y < c.y + 0.4f && dxz + dy < bestD) {
            bestD = dxz + dy;
            best = (int)i;
        }
    }
    return best;
}

// ---- construction ----
namespace {

struct Ctx {
    const TexSet& tex;
    World& w;
    Ctx(const TexSet& t, World& ww) : tex(t), w(ww) {}
    std::vector<MeshBuilder> builders;
    MeshBuilder& mb() { builders.emplace_back(); return builders.back(); }
    void add(const MeshBuilder& m, const Material& mat, bool shadow = true) {
        g_meshes.push_back(const_cast<MeshBuilder&>(m).build());
        w.items.push_back({&g_meshes.back(), Mat4(), mat, shadow});
    }
    void ledgeEdges(float x0, float z0, float x1, float z1, float y) {
        w.edges.push_back({{x0, y, z0}, {x1, y, z0}, false});
        w.edges.push_back({{x0, y, z1}, {x1, y, z1}, false});
        w.edges.push_back({{x0, y, z0}, {x0, y, z1}, false});
        w.edges.push_back({{x1, y, z0}, {x1, y, z1}, false});
    }
};

void addHF(float x0, float z0, float x1, float z1, int type, float p0, float p1) {
    g_fields.push_back({x0, z0, x1, z1, type, p0, p1});
}

} // namespace

void World::build(const TexSet& tex) {
    g_fields.clear();
    g_meshes.clear();
    solids.clear();
    edges.clear();
    items.clear();
    Ctx cx(tex, *this);
    const float zc = (MEGA_Z0 + MEGA_Z1) / 2, mw = MEGA_Z1 - MEGA_Z0;

    // ---- height fields ----
    addHF(-6, 12, -4, 16, 1, 0.0f, 0.9f);        // funbox west bank
    addHF(0, 12, 2, 16, 1, 0.9f, 0.0f);          // funbox east bank
    addHF(56, MEGA_Z0, 64, MEGA_Z1, 2, 64, 8);   // mega roll-in (rises toward -x)
    addHF(69, MEGA_Z0, 73, MEGA_Z1, 0, 69, 5.5f); // kicker (46 deg lip)
    addHF(79, MEGA_Z0, 91, MEGA_Z1, 1, 4.0f, 0.0f); // landing
    addHF(91, MEGA_Z0, 98, MEGA_Z1, 0, 91, 7);   // big quarter (rises toward +x)
    addHF(42, -38, 44.5f, -30, 2, 44.5f, 2.5f);  // mini ramp west transition
    addHF(49.5f, -38, 52, -30, 0, 49.5f, 2.5f);  // mini ramp east transition

    // ---- solids ----
    solids.push_back({{PLAT_X0, 0, PLAT_Z0}, {PLAT_X1, PLAT_H, PLAT_Z1}}); // plateau
    for (int i = 0; i < 5; i++) {                                          // stairs
        float zA = STAIR_Z0 + i * STAIR_D;
        solids.push_back({{STAIR_X0, 0, zA}, {STAIR_X1, PLAT_H - (i + 1) * 0.25f, zA + STAIR_D}});
    }
    for (int i = 0; i < 6; i++) {                                          // hubbas
        float zA = STAIR_Z0 + i * STAIR_D, top = PLAT_H + 0.22f - i * 0.25f;
        solids.push_back({{STAIR_X0 - 1.3f, 0, zA}, {STAIR_X0 - 0.05f, top, zA + STAIR_D}});
        solids.push_back({{STAIR_X1 + 0.05f, 0, zA}, {STAIR_X1 + 1.3f, top, zA + STAIR_D}});
    }
    solids.push_back({{-4.5f, 0, -4}, {0, 0.45f, -2.2f}});                 // ledge 1
    solids.push_back({{31, 0, 6}, {36.5f, 0.5f, 7.7f}});                   // ledge 2
    solids.push_back({{-4, 0, 12}, {0, 0.9f, 16}});                        // funbox
    solids.push_back({{-21.2f, 0, 19.6f}, {-19, 0.42f, 20.4f}});           // bench
    solids.push_back({{7, 0, 29.6f}, {9.2f, 0.42f, 30.4f}});               // bench
    solids.push_back({{7.2f, 0, -22.6f}, {8.8f, 0.72f, -21.4f}});          // picnic tabletop
    solids.push_back({{7.2f, 0, -23.3f}, {8.8f, 0.42f, -22.8f}});          // picnic seat S
    solids.push_back({{7.2f, 0, -21.2f}, {8.8f, 0.42f, -20.7f}});          // picnic seat N
    solids.push_back({{-41.1f, 0, 8.9f}, {-38.9f, 0.55f, 11.1f}});         // planter
    solids.push_back({{38.9f, 0, -21.1f}, {41.1f, 0.55f, -18.9f}});        // planter
    solids.push_back({{-9.1f, 0, 34.9f}, {-6.9f, 0.55f, 37.1f}});          // planter
    solids.push_back({{-49, 0, -9}, {-47, 0.8f, -7.6f}});                  // barrier
    solids.push_back({{19, 0, 35.4f}, {21, 0.8f, 36.8f}});                 // barrier
    solids.push_back({{-21, 0, 37.4f}, {-19, 0.8f, 38.8f}});               // barrier
    solids.push_back({{50, 0, MEGA_Z0}, {56, 8, MEGA_Z1}});                // mega tower
    solids.push_back({{98, 0, MEGA_Z0}, {106, 7, MEGA_Z1}});               // mega deck
    solids.push_back({{39, 0, -38}, {42, 2.5f, -30}});                     // mini ramp west deck
    solids.push_back({{52, 0, -38}, {55, 2.5f, -30}});                     // mini ramp east deck
    solids.push_back({{-30, 0, 18.4f}, {30, 0.14f, 19.0f}});               // curb
    solids.push_back({{-30, 0, -8.6f}, {10, 0.14f, -8.0f}});               // curb
    solids.push_back({{-73.5f, 0, -48}, {-72.5f, 1, 64}});                 // west wall
    solids.push_back({{-74, 0, -48.5f}, {106, 1, -47.5f}});                // north wall
    solids.push_back({{-74, 0, 62.5f}, {106, 1, 63.5f}});                  // south wall

    // ---- grind edges ----
    edges.push_back({{18, 2.15f, -6.4f}, {18, 0.85f, 4.2f}, true});        // handrail mid
    edges.push_back({{16.6f, 2.15f, -6.4f}, {16.6f, 0.85f, 4.2f}, true});  // handrail left
    edges.push_back({{STAIR_X0 - 0.05f, PLAT_H + 0.22f, STAIR_Z0},
                     {STAIR_X0 - 0.05f, PLAT_H + 0.22f - 6 * 0.25f, STAIR_Z0 + 6 * STAIR_D}, false});
    edges.push_back({{STAIR_X1 + 0.05f, PLAT_H + 0.22f, STAIR_Z0},
                     {STAIR_X1 + 0.05f, PLAT_H + 0.22f - 6 * 0.25f, STAIR_Z0 + 6 * STAIR_D}, false});
    cx.ledgeEdges(-4.5f, -4, 0, -2.2f, 0.45f);
    cx.ledgeEdges(31, 6, 36.5f, 7.7f, 0.5f);
    cx.ledgeEdges(-4, 12, 0, 16, 0.9f);
    cx.ledgeEdges(-21.2f, 19.6f, -19, 20.4f, 0.42f);
    cx.ledgeEdges(7, 29.6f, 9.2f, 30.4f, 0.42f);
    cx.ledgeEdges(7.2f, -22.6f, 8.8f, -21.4f, 0.72f);
    cx.ledgeEdges(-49, -9, -47, -7.6f, 0.8f);
    cx.ledgeEdges(19, 35.4f, 21, 36.8f, 0.8f);
    edges.push_back({{-19, 0.55f, 6}, {-13, 0.55f, 6}, true});             // flatbar
    edges.push_back({{-46, 0.55f, 28}, {-40, 0.55f, 28}, true});           // lot rail
    addHF(-26, -14, -22, -10, 1, 0.f, 0.8f);                               // kicker ramp
    // mini ramp coping (grindable lips)
    edges.push_back({{42, 2.52f, -38}, {42, 2.52f, -30}, false});
    edges.push_back({{52, 2.52f, -38}, {52, 2.52f, -30}, false});
    {   // bowl coping ring
        for (int i = 0; i < 24; i++) {
            float a0 = i / 24.f * 6.2831853f, a1 = (i + 1) / 24.f * 6.2831853f;
            float r = BOWL_R + 0.02f;
            edges.push_back({{BOWL_X + std::cos(a0) * r, 0.06f, BOWL_Z + std::sin(a0) * r},
                             {BOWL_X + std::cos(a1) * r, 0.06f, BOWL_Z + std::sin(a1) * r}, true});
        }
    }

    // ---- ground + asphalt lot ----
    {
        auto& m = cx.mb();
        m.setColor({1, 1, 1});
        const float x0 = -74, x1 = 106, z0 = -48, z1 = 64, st = 2.f;
        const float holeR = BOWL_R + 1.6f;
        for (float x = x0; x < x1; x += st)
            for (float z = z0; z < z1; z += st) {
                float mx = x + st / 2, mz = z + st / 2;
                if (std::hypot(mx - BOWL_X, mz - BOWL_Z) < holeR) continue;
                unsigned a = m.vert({x, 0, z}, {0, 1, 0}, x * 0.22f, z * 0.22f);
                unsigned b = m.vert({x + st, 0, z}, {0, 1, 0}, (x + st) * 0.22f, z * 0.22f);
                unsigned c = m.vert({x + st, 0, z + st}, {0, 1, 0}, (x + st) * 0.22f, (z + st) * 0.22f);
                unsigned d = m.vert({x, 0, z + st}, {0, 1, 0}, x * 0.22f, (z + st) * 0.22f);
                m.tri(a, c, b); m.tri(a, d, c);
            }
        cx.add(m, {tex.concrete, 1.f, {1,1,1}, 0.18f, 24.f, 1.f, 0.f, 0.f}, false);
    }
    {
        auto& m = cx.mb();
        m.setColor({1, 1, 1});
        const float st = 2.f;
        for (float x = -58; x < -20; x += st)
            for (float z = 22; z < 48; z += st) {
                unsigned a = m.vert({x, 0.015f, z}, {0, 1, 0}, x * 0.08f, z * 0.08f);
                unsigned b = m.vert({x + st, 0.015f, z}, {0, 1, 0}, (x + st) * 0.08f, z * 0.08f);
                unsigned c = m.vert({x + st, 0.015f, z + st}, {0, 1, 0}, (x + st) * 0.08f, (z + st) * 0.08f);
                unsigned d = m.vert({x, 0.015f, z + st}, {0, 1, 0}, x * 0.08f, (z + st) * 0.08f);
                m.tri(a, c, b); m.tri(a, d, c);
            }
        cx.add(m, {tex.asphalt, 1.f, {1,1,1}, 0.15f, 20.f, 1.f, 0.f, 0.f}, false);
    }
    // bowl + rim flange
    {
        auto& m = cx.mb();
        m.setColor({1, 1, 1});
        m.bowl({BOWL_X, 0, BOWL_Z}, BOWL_R, BOWL_D, 56, 10);
        int n = 56;
        for (int i = 0; i < n; i++) {
            float a0 = i / (float)n * 6.2831853f, a1 = (i + 1) / (float)n * 6.2831853f;
            Vec3 p00(BOWL_X + std::cos(a0) * BOWL_R, -0.001f, BOWL_Z + std::sin(a0) * BOWL_R);
            Vec3 p01(BOWL_X + std::cos(a1) * BOWL_R, -0.001f, BOWL_Z + std::sin(a1) * BOWL_R);
            Vec3 p10(BOWL_X + std::cos(a0) * (BOWL_R + 1.8f), 0.02f, BOWL_Z + std::sin(a0) * (BOWL_R + 1.8f));
            Vec3 p11(BOWL_X + std::cos(a1) * (BOWL_R + 1.8f), 0.02f, BOWL_Z + std::sin(a1) * (BOWL_R + 1.8f));
            m.quad(p00, p01, p11, p10, {0, 1, 0}, 1.f, 1.f);
        }
        cx.add(m, {tex.concreteBowl, 1.6f, {1,1,1}, 0.22f, 28.f, 1.f, 0.f, 0.f});
        // coping
        auto& c = cx.mb();
        c.setColor({1, 1, 1});
        for (int i = 0; i < 48; i++) {
            float a0 = i / 48.f * 6.2831853f, a1 = (i + 1) / 48.f * 6.2831853f;
            float r = BOWL_R + 0.02f;
            c.cylinder({BOWL_X + std::cos(a0) * r, 0.055f, BOWL_Z + std::sin(a0) * r},
                       {BOWL_X + std::cos(a1) * r, 0.055f, BOWL_Z + std::sin(a1) * r}, 0.05f, 6);
        }
        cx.add(c, {tex.metal, 1.f, {0.9f,0.92f,0.95f}, 0.9f, 90.f, 1.f, 0.8f, 0.f});
    }
    // plateau + stairs (concrete)
    {
        auto& m = cx.mb();
        m.setColor({1, 1, 1});
        m.box({(PLAT_X0 + PLAT_X1) / 2, PLAT_H / 2, (PLAT_Z0 + PLAT_Z1) / 2},
              {(PLAT_X1 - PLAT_X0) / 2, PLAT_H / 2, (PLAT_Z1 - PLAT_Z0) / 2});
        for (int i = 0; i < 5; i++) {
            float zA = STAIR_Z0 + i * STAIR_D, top = PLAT_H - (i + 1) * 0.25f;
            m.box({(STAIR_X0 + STAIR_X1) / 2, top / 2, zA + STAIR_D / 2},
                  {(STAIR_X1 - STAIR_X0) / 2, top / 2, STAIR_D / 2});
        }
        cx.add(m, {tex.concrete, 0.4f, {1,1,1}, 0.18f, 24.f, 1.f, 0.f, 0.f});
    }
    // hubbas (painted)
    {
        auto& hb = cx.mb();
        hb.setColor({1, 1, 1});
        for (int i = 0; i < 6; i++) {
            float zA = STAIR_Z0 + i * STAIR_D, top = PLAT_H + 0.22f - i * 0.25f;
            hb.box({STAIR_X0 - 0.675f, top / 2, zA + STAIR_D / 2}, {0.625f, top / 2, STAIR_D / 2});
            hb.box({STAIR_X1 + 0.675f, top / 2, zA + STAIR_D / 2}, {0.625f, top / 2, STAIR_D / 2});
        }
        cx.add(hb, {tex.curbPaint, 0.6f, {1,1,1}, 0.3f, 40.f, 1.f, 0.05f, 0.f});
    }
    // rails (metal)
    {
        auto& m = cx.mb();
        m.setColor({1, 1, 1});
        auto rail = [&](float x) {
            m.cylinder({x, 2.15f, -6.4f}, {x, 0.85f, 4.2f}, 0.04f, 12);
            m.cylinder({x, 0, -6.2f}, {x, 1.9f, -6.2f}, 0.03f, 8);
            m.cylinder({x, 0, 4.0f}, {x, 0.7f, 4.0f}, 0.03f, 8);
            m.cylinder({x, 0, -1.2f}, {x, 1.5f, -1.2f}, 0.03f, 8);
        };
        rail(18); rail(16.6f);
        m.cylinder({-19, 0.55f, 6}, {-13, 0.55f, 6}, 0.04f, 12);
        m.cylinder({-19, 0, 6}, {-19, 0.55f, 6}, 0.03f, 8);
        m.cylinder({-13, 0, 6}, {-13, 0.55f, 6}, 0.03f, 8);
        m.cylinder({-16, 0, 6}, {-16, 0.55f, 6}, 0.03f, 8);
        m.cylinder({-46, 0.55f, 28}, {-40, 0.55f, 28}, 0.04f, 12);
        m.cylinder({-46, 0, 28}, {-46, 0.55f, 28}, 0.03f, 8);
        m.cylinder({-40, 0, 28}, {-40, 0.55f, 28}, 0.03f, 8);
        m.cylinder({-43, 0, 28}, {-43, 0.55f, 28}, 0.03f, 8);
        cx.add(m, {tex.metal, 1.f, {0.92f,0.93f,0.96f}, 1.0f, 128.f, 1.f, 0.9f, 0.f});
    }
    // ledges, benches, funbox top, planters (concrete)
    {
        auto& m = cx.mb();
        m.setColor({1, 1, 1});
        m.box({-2.25f, 0.225f, -3.1f}, {2.25f, 0.225f, 0.9f});
        m.box({33.75f, 0.25f, 6.85f}, {2.75f, 0.25f, 0.85f});
        m.box({-2, 0.45f, 14}, {2, 0.45f, 2});
        m.box({-20.1f, 0.21f, 20}, {1.1f, 0.21f, 0.4f});
        m.box({8.1f, 0.21f, 30}, {1.1f, 0.21f, 0.4f});
        for (auto& p : {Vec2(-40, 10), Vec2(40, -20), Vec2(-8, 36)})
            m.cylinder({p.x, 0, p.y}, {p.x, 0.55f, p.y}, 1.1f, 20);
        cx.add(m, {tex.concrete, 0.5f, {1,1,1}, 0.2f, 30.f, 1.f, 0.f, 0.f});
    }
    // barriers + curbs (painted)
    {
        auto& pb = cx.mb();
        pb.setColor({1, 1, 1});
        pb.box({-48, 0.4f, -8.3f}, {1.0f, 0.4f, 0.7f});
        pb.box({20, 0.4f, 36.1f}, {1.0f, 0.4f, 0.7f});
        pb.box({-20, 0.4f, 38.1f}, {1.0f, 0.4f, 0.7f});
        pb.box({0, 0.07f, 18.7f}, {30, 0.07f, 0.3f});
        pb.box({-10, 0.07f, -8.3f}, {20, 0.07f, 0.3f});
        cx.add(pb, {tex.curbPaint, 0.6f, {1,1,1}, 0.3f, 40.f, 1.f, 0.05f, 0.f});
    }
    // trees
    {
        auto& tr = cx.mb();
        tr.setColor({1, 1, 1});
        for (auto& p : {Vec2(-40, 10), Vec2(40, -20), Vec2(-8, 36)})
            tr.cylinder({p.x, 0.55f, p.y}, {p.x, 2.6f, p.y}, 0.14f, 8);
        cx.add(tr, {tex.bark, 1.f, {1,1,1}, 0.1f, 16.f, 1.f, 0.f, 0.f});
        auto& lf = cx.mb();
        lf.setColor({1, 1, 1});
        unsigned sd = 9;
        for (auto& p : {Vec2(-40, 10), Vec2(40, -20), Vec2(-8, 36)}) {
            lf.sphere({p.x, 3.6f, p.y}, 1.5f, 12, 8);
            lf.sphere({p.x + 0.8f * (randf(sd) - 0.3f), 3.0f, p.y + 0.6f * randf(sd)}, 1.0f, 10, 7);
            lf.sphere({p.x - 0.7f * randf(sd), 4.2f, p.y - 0.5f * randf(sd)}, 0.9f, 10, 7);
        }
        cx.add(lf, {tex.leaf, 1.f, {1,1,1}, 0.15f, 20.f, 1.f, 0.f, 0.f});
    }
    // funbox banks
    {
        auto& m = cx.mb();
        m.setColor({1, 1, 1});
        m.bank({-6, 0, 14}, {1, 0, 0}, 4, 2, 0.9f);
        m.bank({2, 0, 14}, {-1, 0, 0}, 4, 2, 0.9f);
        m.bank({-26, 0, -12}, {1, 0, 0}, 4, 4, 0.8f);   // kicker
        cx.add(m, {tex.concrete, 0.5f, {1,1,1}, 0.2f, 30.f, 1.f, 0.f, 0.f});
    }
    // picnic table (wooden top + seats, metal legs)
    {
        auto& m = cx.mb();
        m.setColor({1, 1, 1});
        m.box({8, 0.72f, -22}, {0.85f, 0.035f, 0.65f});
        m.box({8, 0.42f, -23.05f}, {0.85f, 0.03f, 0.14f});
        m.box({8, 0.42f, -20.95f}, {0.85f, 0.03f, 0.14f});
        cx.add(m, {tex.wood, 0.6f, {1,1,1}, 0.25f, 40.f, 1.f, 0.f, 0.f});
        auto& lg = cx.mb();
        lg.setColor({1, 1, 1});
        for (float sx : {-1.f, 1.f}) {
            lg.cylinder({8 + 0.55f * sx, 0, -22.45f}, {8 + 0.75f * sx, 0.7f, -21.7f}, 0.035f, 8);
            lg.cylinder({8 + 0.55f * sx, 0, -21.55f}, {8 + 0.75f * sx, 0.7f, -22.3f}, 0.035f, 8);
        }
        lg.cylinder({7.3f, 0.5f, -22}, {8.7f, 0.5f, -22}, 0.03f, 8);        // brace
        cx.add(lg, {tex.metal, 0.4f, {0.7f,0.72f,0.75f}, 0.5f, 90.f, 1.f, 0.4f, 0.f});
    }
    // mini ramp (plywood transitions + decks + coping)
    {
        auto& m = cx.mb();
        m.setColor({1, 1, 1});
        m.quarterPipe({44.5f, 0, -34}, {-1, 0, 0}, 8, 2.5f, 12);  // west pipe
        m.quarterPipe({49.5f, 0, -34}, {1, 0, 0}, 8, 2.5f, 12);   // east pipe
        m.box({40.5f, 1.25f, -34}, {1.5f, 1.25f, 4});             // west deck
        m.box({53.5f, 1.25f, -34}, {1.5f, 1.25f, 4});             // east deck
        m.box({47, 0.06f, -34}, {2.6f, 0.06f, 4});                // flat bottom
        cx.add(m, {tex.wood, 0.8f, {1,1,1}, 0.22f, 30.f, 1.f, 0.f, 0.f});
        auto& cp = cx.mb();
        cp.setColor({1, 1, 1});
        cp.cylinder({42, 2.52f, -38}, {42, 2.52f, -30}, 0.045f, 10);
        cp.cylinder({52, 2.52f, -38}, {52, 2.52f, -30}, 0.045f, 10);
        cx.add(cp, {tex.metal, 1.f, {0.9f,0.92f,0.95f}, 0.9f, 90.f, 1.f, 0.8f, 0.f});
    }
    // mega ramp (wooden surfaces)
    {
        auto& m = cx.mb();
        m.setColor({1, 1, 1});
        m.box({53, 4, zc}, {3, 4, mw / 2});                    // tower
        m.quarterPipe({64, 0, zc}, {-1, 0, 0}, mw, 8, 18);     // roll-in
        m.quarterPipe({69, 0, zc}, {1, 0, 0}, mw, 5.5f, 10, true, 0.81f); // kicker
        m.bank({91, 0, zc}, {-1, 0, 0}, mw, 12, 4.0f);         // landing
        m.quarterPipe({91, 0, zc}, {1, 0, 0}, mw, 7, 18);      // big quarter
        m.box({102, 3.5f, zc}, {4, 3.5f, mw / 2});             // deck
        cx.add(m, {tex.wood, 0.5f, {1,1,1}, 0.25f, 32.f, 1.f, 0.f, 0.f});
        // safety railings
        auto& r = cx.mb();
        r.setColor({1, 1, 1});
        r.cylinder({50, 9.0f, MEGA_Z0}, {50, 9.0f, MEGA_Z1}, 0.05f, 8);
        r.cylinder({50, 8, MEGA_Z0}, {50, 9.0f, MEGA_Z0}, 0.04f, 8);
        r.cylinder({50, 8, MEGA_Z1}, {50, 9.0f, MEGA_Z1}, 0.04f, 8);
        r.cylinder({105.8f, 7, MEGA_Z0}, {105.8f, 8.2f, MEGA_Z0}, 0.04f, 8);
        r.cylinder({105.8f, 7, MEGA_Z1}, {105.8f, 8.2f, MEGA_Z1}, 0.04f, 8);
        r.cylinder({105.8f, 8.2f, MEGA_Z0}, {105.8f, 8.2f, MEGA_Z1}, 0.05f, 8);
        cx.add(r, {tex.metal, 1.f, {0.9f,0.9f,0.92f}, 0.9f, 90.f, 1.f, 0.8f, 0.f});
    }
    // backdrop: distant city blocks + ocean
    {
        // backdrop towers in three material groups for skyline variety
        unsigned sd = 42;
        MeshBuilder groups[3];
        for (int i = 0; i < 3; i++) groups[i].setColor({1, 1, 1});
        auto addTower = [&](float bx, float bz, float bh, float bs, int grp) {
            groups[grp].box({bx, bh / 2, bz}, {bs / 2, bh / 2, bs / 2});
        };
        for (int i = 0; i < 14; i++) {
            float bx = -80 + randf(sd) * 200, bz = -80 - randf(sd) * 40;
            float bh = 12 + randf(sd) * 30, bs = 8 + randf(sd) * 10;
            addTower(bx, bz, bh, bs, i % 3);
        }
        for (int i = 0; i < 10; i++) {
            float bx = -80 + randf(sd) * 200, bz = 78 + randf(sd) * 40;
            float bh = 10 + randf(sd) * 26, bs = 8 + randf(sd) * 10;
            addTower(bx, bz, bh, bs, (i + 1) % 3);
        }
        cx.add(groups[0], {tex.building, 3.5f, {0.75f,0.82f,0.95f}, 0.15f, 24.f, 1.f, 0.25f, 0.f}, false); // glass towers
        cx.add(groups[1], {tex.building, 2.6f, {1.0f,0.92f,0.80f}, 0.08f, 16.f, 1.f, 0.f, 0.f}, false);   // warm concrete
        cx.add(groups[2], {0, 1.f, {0.82f,0.84f,0.9f}, 0.05f, 8.f, 1.f, 0.f, 0.f}, false);                // plain massing
        auto& oc = cx.mb();
        oc.setColor({0.15f, 0.4f, 0.62f});
        oc.quad({-200, -0.5f, 120}, {200, -0.5f, 120}, {200, -0.5f, 300}, {-200, -0.5f, 300}, {0, 1, 0}, 10, 10);
        cx.add(oc, {tex.water, 14.f, {0.9f,0.95f,1.05f}, 0.55f, 80.f, 1.f, 0.35f, 0.f}, false);
        // boundary walls (visual)
        auto& bw = cx.mb();
        bw.setColor({1, 1, 1});
        bw.box({16, 0.5f, -47.9f}, {90, 0.5f, 0.4f});
        bw.box({16, 0.5f, 63.0f}, {90, 0.5f, 0.4f});
        bw.box({-73.4f, 0.5f, 8}, {0.4f, 0.5f, 56});
        cx.add(bw, {tex.concrete, 0.4f, {1,1,1}, 0.18f, 24.f, 1.f, 0.f, 0.f});
    }
}
