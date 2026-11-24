// The park: Port Carverton-inspired concrete plaza. Terrain is an analytic
// height field plus solid boxes; grind edges are explicit segments.
#pragma once
#include "math3d.h"
#include "glcore.h"
#include "texgen.h"
#include <vector>

struct GrindEdge {
    Vec3 a, b;      // lip endpoints in world space
    bool rail;      // true = metal rail/handrail, false = ledge/hubba/coping
};

struct SolidBox {
    Vec3 mn, mx;
};

struct World {
    std::vector<DrawItem> items;        // static scenery
    std::vector<GrindEdge> edges;       // grindable lips
    std::vector<SolidBox> solids;       // boxes for side collision + tops

    // Park parameters (shared between collision + meshes)
    static constexpr float BOWL_X = -34, BOWL_Z = -26, BOWL_R = 10, BOWL_D = 2.6f;
    static constexpr float PLAT_X0 = 12, PLAT_X1 = 28, PLAT_Z0 = -24, PLAT_Z1 = -6, PLAT_H = 1.5f;
    static constexpr float STAIR_X0 = 15.5f, STAIR_X1 = 24.5f, STAIR_Z0 = -6, STAIR_D = 2.0f;
    static constexpr float MEGA_Z0 = 30, MEGA_Z1 = 50;
    static constexpr float STEP = 0.35f;  // max step-up while riding

    float terrainHeight(float x, float z) const;
    // Highest support surface at (x,z) not above refY+STEP.
    float supportHeight(float x, float z, float refY) const;
    Vec3 supportNormal(float x, float z, float refY) const;
    // Push a position out of solid box walls horizontally; returns true if collided.
    bool collideWalls(Vec3& pos, Vec3& vel) const;
    // Find grind edge near point; returns edge index or -1.
    int findGrindEdge(const Vec3& p) const;

    void build(const TexSet& tex);
};
