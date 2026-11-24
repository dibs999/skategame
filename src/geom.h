// Mesh builders: all park/skater/board geometry is generated procedurally.
#pragma once
#include "glcore.h"
#include "math3d.h"
#include <vector>
#include <cmath>
#include <functional>

struct MeshBuilder {
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    Vec3 color = {1, 1, 1};
    float uvScale = 1.f;
    float aoMul = 1.f;

    void setColor(const Vec3& c, float ao = 1.f) { color = c; aoMul = ao; }

    unsigned vert(const Vec3& p, const Vec3& n, float u, float vv) {
        Vertex x{p.x, p.y, p.z, n.x, n.y, n.z, u, vv, color.x, color.y, color.z, aoMul};
        v.push_back(x);
        return (unsigned)v.size() - 1;
    }
    void tri(unsigned a, unsigned b, unsigned c) { idx.push_back(a); idx.push_back(b); idx.push_back(c); }

    void quad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& n,
              float us = 1.f, float vs = 1.f) {
        unsigned i0 = vert(a, n, 0, 0), i1 = vert(b, n, us, 0),
                 i2 = vert(c, n, us, vs), i3 = vert(d, n, 0, vs);
        tri(i0, i1, i2); tri(i0, i2, i3);
    }

    // Axis-aligned box centered at c with half extents h.
    void box(const Vec3& c, const Vec3& h) {
        static const Vec3 N[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        for (int f = 0; f < 6; f++) {
            Vec3 n = N[f];
            Vec3 t = (std::abs(n.y) < 0.9f) ? n.cross({0,1,0}) : Vec3(1,0,0);
            t = t.normalized();
            Vec3 b = n.cross(t);
            Vec3 cc = c + n * Vec3(std::abs(n.x)*h.x + std::abs(n.y)*h.y + std::abs(n.z)*h.z,
                                   std::abs(n.y)*h.y + std::abs(n.x)*h.x, std::abs(n.z)*h.z);
            // simpler: face center = c + n*h componentwise
            cc = c + Vec3(n.x*h.x, n.y*h.y, n.z*h.z);
            Vec3 p0 = cc - t*h - b*h, p1 = cc + t*h - b*h,
                 p2 = cc + t*h + b*h, p3 = cc - t*h + b*h;
            quad(p0, p1, p2, p3, n, h.len(), h.len());
        }
    }

    void cylinder(const Vec3& a, const Vec3& b, float r, int seg = 18, bool caps = true) {
        Vec3 ax = (b - a).normalized();
        Vec3 t = (std::abs(ax.y) < 0.9f) ? ax.cross({0,1,0}).normalized()
                                       : ax.cross({1,0,0}).normalized();
        Vec3 bb = ax.cross(t).normalized();
        unsigned base0 = (unsigned)v.size();
        for (int i = 0; i <= seg; i++) {
            float a_ = i / (float)seg * 6.2831853f;
            Vec3 n = t * std::cos(a_) + bb * std::sin(a_);
            vert(a + n * r, n, i / (float)seg * 4, 0);
            vert(b + n * r, n, i / (float)seg * 4, 1);
        }
        for (int i = 0; i < seg; i++) {
            unsigned i0 = base0 + i * 2, i1 = i0 + 1, i2 = i0 + 3, i3 = i0 + 2;
            tri(i0, i1, i2); tri(i0, i2, i3);
        }
        if (caps) {
            unsigned ca = vert(a, -ax, 0.5f, 0.5f), cb = vert(b, ax, 0.5f, 0.5f);
            unsigned sa = (unsigned)v.size(), sb = sa + seg + 1;
            for (int i = 0; i <= seg; i++) {
                float a_ = i / (float)seg * 6.2831853f;
                Vec3 n = t * std::cos(a_) + bb * std::sin(a_);
                vert(a + n * r, -ax, std::cos(a_)*0.5f+0.5f, std::sin(a_)*0.5f+0.5f);
                vert(b + n * r, ax, std::cos(a_)*0.5f+0.5f, std::sin(a_)*0.5f+0.5f);
            }
            for (int i = 0; i < seg; i++) {
                tri(ca, sa + i + 1, sa + i);
                tri(cb, sb + i, sb + i + 1);
            }
        }
    }

    void sphere(const Vec3& c, float r, int seg = 20, int rings = 14) {
        unsigned b0 = (unsigned)v.size();
        for (int j = 0; j <= rings; j++) {
            float ph = j / (float)rings * 3.14159265f;
            for (int i = 0; i <= seg; i++) {
                float th = i / (float)seg * 6.2831853f;
                Vec3 n(std::sin(ph)*std::cos(th), std::cos(ph), std::sin(ph)*std::sin(th));
                vert(c + n * r, n, i/(float)seg, j/(float)rings);
            }
        }
        for (int j = 0; j < rings; j++)
            for (int i = 0; i < seg; i++) {
                unsigned a0 = b0 + j*(seg+1)+i, a1 = a0+1, a2 = a0+seg+2, a3 = a0+seg+1;
                tri(a0, a3, a2); tri(a0, a2, a1);
            }
    }

    void capsule(const Vec3& a, const Vec3& b, float r, int seg = 18) {
        cylinder(a, b, r, seg, false);
        sphere(a, r, seg, 6);
        sphere(b, r, seg, 6);
    }

    // Quarter-pipe transition: arc of radius R starting at (ox, oy) rising along +dir,
    // spanning width w along the other axis. 'segs' subdivisions.
    void quarterPipe(const Vec3& org, const Vec3& dir, float width, float R,
                     int segs = 14, bool vertWall = true, float arc = -1.f) {
        Vec3 side = dir.cross({0,1,0}).normalized() * -1.f; // perpendicular, horizontal
        Vec3 prev[2];
        float arcEnd = arc > 0 ? arc : (vertWall ? 1.5707f : 1.35f);
        for (int i = 0; i <= segs; i++) {
            float t = i / (float)segs * arcEnd;
            Vec3 p = org + dir * (R * std::sin(t)) + Vec3(0, R - R * std::cos(t), 0);
            Vec3 n = (dir * (-std::sin(t)) + Vec3(0, std::cos(t), 0)).normalized();
            Vec3 cur[2] = {p - side * (width * 0.5f), p + side * (width * 0.5f)};
            if (i == 0) { prev[0] = cur[0]; prev[1] = cur[1]; continue; }
            unsigned i0 = vert(prev[0], n, 0, i/(float)segs*4);
            unsigned i1 = vert(prev[1], n, 1, i/(float)segs*4);
            unsigned i2 = vert(cur[1], n, 1, (i+1)/(float)segs*4);
            unsigned i3 = vert(cur[0], n, 0, (i+1)/(float)segs*4);
            tri(i0, i1, i2); tri(i0, i2, i3);
            // back wall
            if (i == segs) {
                float h = p.y - org.y;
                Vec3 bp0 = cur[0] + dir*0.3f, bp1 = cur[1] + dir*0.3f;
                quad(cur[0], cur[1], bp1, bp0, -n, width, h);
                Vec3 top0 = cur[0], top1 = cur[1];
                quad(top0, top1, top1 + Vec3(0,0.4f,0) + dir*0.3f,
                     top0 + Vec3(0,0.4f,0) + dir*0.3f, {0,1,0}, width, 0.4f);
            }
            prev[0] = cur[0]; prev[1] = cur[1];
        }
        (void)side;
    }

    // Flat banked ramp: wedge from edge line, rising 'rise' over 'run' along dir.
    void bank(const Vec3& org, const Vec3& dir, float width, float run, float rise) {
        Vec3 side = Vec3(0,1,0).cross(dir).normalized();
        Vec3 a = org - side*width*0.5f, b = org + side*width*0.5f;
        Vec3 c = b + dir*run + Vec3(0,rise,0), d = a + dir*run + Vec3(0,rise,0);
        Vec3 n = Vec3(0,rise,0).cross(dir).cross(dir).normalized(); // compute slope normal
        n = dir.cross(side).normalized() * -1.f;
        if (n.y < 0) n = -n;
        quad(a, b, c, d, n, width, run);
        // side triangles + back
        Vec3 e = d + Vec3(0,-rise,0), f = c + Vec3(0,-rise,0);
        tri(vert(a,-side,0,0), vert(d,-side,1,1), vert(e,-side,1,0));
        tri(vert(b, side,0,0), vert(f, side,1,0), vert(c, side,1,1));
        quad(e, f, c, d, -dir, width, 1.f);
    }

    // Skate bowl: radial transition. Center c, radius R, depth D.
    void bowl(const Vec3& c, float R, float D, int segA = 48, int segR = 10) {
        // rings from flat rim into bottom
        for (int j = 0; j <= segR; j++) {
            float t = j / (float)segR;
            float r = R * (1.f - t * 0.55f);               // bottom radius ~0.45R
            float y = -D * (t * t * (3 - 2 * t));           // smoothstep depth
            for (int i = 0; i <= segA; i++) {
                float a = i / (float)segA * 6.2831853f;
                Vec3 p = c + Vec3(std::cos(a)*r, y, std::sin(a)*r);
                // normal: tilt outward
                float dydr = D * 6.f * t * (1-t) / (R*0.55f);
                Vec3 n(std::cos(a)*dydr, 1.f, std::sin(a)*dydr);
                n = n.normalized();
                vert(p, n, i/(float)segA*8.f, t*4.f);
            }
        }
        for (int j = 0; j < segR; j++)
            for (int i = 0; i < segA; i++) {
                unsigned a0 = j*(segA+1)+i, a1 = a0+1, a2 = a0+segA+2, a3 = a0+segA+1;
                tri(a0, a2, a1); tri(a0, a3, a2);
            }
        // flat bottom
        unsigned cb = vert(c + Vec3(0,-D,0), {0,1,0}, 0.5f, 0.5f);
        unsigned rb = (unsigned)v.size();
        float br = R * 0.45f;
        for (int i = 0; i <= segA; i++) {
            float a = i / (float)segA * 6.2831853f;
            vert(c + Vec3(std::cos(a)*br, -D, std::sin(a)*br), {0,1,0},
                 std::cos(a)*0.5f+0.5f, std::sin(a)*0.5f+0.5f);
        }
        for (int i = 0; i < segA; i++) tri(cb, rb+i, rb+i+1);
    }

    // Skateboard deck: rounded rectangle with kicked nose/tail.
    void deck(float len, float wid, float th, float kick) {
        int seg = 20;
        // top + bottom surfaces via section profile along length
        // cross-sections at stations, plus kick bend at both ends
        auto section = [&](float t, float& x, float& ybend, float& w) {
            x = (t - 0.5f) * len;
            w = wid * (0.82f + 0.18f * std::sin(t * 3.14159f)); // taper at ends
            float bend = 0.f;
            float edge = std::fabs(t - 0.5f) * 2.f;
            if (edge > 0.72f) bend = std::pow((edge - 0.72f) / 0.28f, 1.8f) * kick;
            ybend = bend;
        };
        std::vector<unsigned> rings;
        int sides = 12;
        for (int s = 0; s <= seg; s++) {
            float t = s / (float)seg, x, yb, w;
            section(t, x, yb, w);
            for (int i = 0; i < sides; i++) {
                float a = i / (float)sides * 6.2831853f;
                // rounded rect cross-section: width w, thickness th
                float yy = std::sin(a), zz = std::cos(a);
                Vec3 p(x, yb + yy * th * 0.5f, zz * w * 0.5f * (0.75f + 0.25f*std::abs(zz)));
                Vec3 n = Vec3(0, yy * 4.f, zz * 2.f).normalized();
                rings.push_back(vert(p, n, t * 2.f, i/(float)sides));
            }
        }
        for (int s = 0; s < seg; s++)
            for (int i = 0; i < sides; i++) {
                unsigned a0 = rings[s*sides+i], a1 = rings[s*sides+(i+1)%sides];
                unsigned a2 = rings[(s+1)*sides+(i+1)%sides], a3 = rings[(s+1)*sides+i];
                tri(a0, a1, a2); tri(a0, a2, a3);
            }
        // end caps
        unsigned c0 = vert({-len*0.5f, kick, 0}, {-1,0.4f,0}, 0.5f, 0.5f);
        unsigned c1 = vert({len*0.5f, kick, 0}, {1,0.4f,0}, 0.5f, 0.5f);
        for (int i = 0; i < sides; i++) {
            tri(c0, rings[(i+1)%sides], rings[i]);
            unsigned base_ = seg*sides;
            tri(c1, rings[base_+i], rings[base_+(i+1)%sides]);
        }
    }

    // Extrude a 2D polygon (xz) vertically: top face + sides. Used for ledges/blocks.
    void prism(const std::vector<Vec2>& poly, float y0, float y1) {
        // fan top
        unsigned ctop = vert({0,y1,0}, {0,1,0}, 0.5f,0.5f);
        // compute centroid for uv
        (void)ctop;
        unsigned b0 = (unsigned)v.size();
        for (auto& p : poly) vert({p.x,y1,p.y}, {0,1,0}, p.x*0.2f, p.y*0.2f);
        for (size_t i = 1; i + 1 < poly.size(); i++) tri(b0, b0+i+1, b0+i);
        unsigned b1 = (unsigned)v.size();
        for (auto& p : poly) vert({p.x,y0,p.y}, {0,-1,0}, p.x*0.2f, p.y*0.2f);
        for (size_t i = 1; i + 1 < poly.size(); i++) tri(b1, b1+i, b1+i+1);
        for (size_t i = 0; i < poly.size(); i++) {
            Vec2 a = poly[i], b = poly[(i+1)%poly.size()];
            Vec3 n({-(b.y-a.y), 0, (b.x-a.x)});
            n = n.normalized();
            Vec3 p0(a.x,y0,a.y), p1(b.x,y0,b.y), p2(b.x,y1,b.y), p3(a.x,y1,a.y);
            quad(p0, p1, p2, p3, n, (b-a).x*0+ (b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y), y1-y0);
        }
    }

    Mesh build() const {
        Mesh m;
        m.upload(const_cast<std::vector<Vertex>&>(v),
                 const_cast<std::vector<unsigned>&>(idx));
        return m;
    }
};
