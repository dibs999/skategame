// HUD: Skate 3-style minimal readout drawn with the vector stroke font.
#pragma once
#include "vfont.h"
#include "game.h"
#include <cstdio>
#include <string>

struct HUD {
    VFont font;

    void draw(LineBatch& lb, const Game& g, int W, int H) {
        const Skater& s = g.s;
        float u = H / 540.f; // ui scale unit
        Vec3 white{0.95f, 0.95f, 0.92f};
        Vec3 dim{0.6f, 0.62f, 0.65f};
        Vec3 accent{1.0f, 0.55f, 0.15f};
        float z = 0;

        // trick name bottom-left (Skate 3 style corner readout)
        float tx = 18 * u, ty = H - 90 * u;
        if (!s.trickName.empty()) {
            font.drawBold(lb, s.trickName, tx, ty, 3.4f * u, white);
            ty += 0;
        }
        // line score + multiplier under it
        char buf[64];
        if (s.lineScore > 0) {
            snprintf(buf, sizeof buf, "%d", s.lineScore);
            font.draw(lb, buf, tx, ty + 30 * u, 3.0f * u, accent);
            snprintf(buf, sizeof buf, "%dX", s.mult);
            font.draw(lb, buf, tx + font.textWidth("LINE", 0) * 0 + 150 * u,
                      ty + 30 * u, 3.0f * u, accent);
        }
        // total score top-left
        snprintf(buf, sizeof buf, "TOTAL %d", s.totalScore + s.lineScore);
        font.draw(lb, buf, 18 * u, 20 * u, 2.6f * u, dim);

        // speedometer bottom-right
        float kph = s.speed * 3.6f;
        snprintf(buf, sizeof buf, "%d KM/H", (int)(kph + 0.5f));
        float sw = font.textWidth(buf, 2.6f * u);
        font.draw(lb, buf, W - 20 * u - sw, H - 40 * u, 2.6f * u, white);
        // speed bar
        float bw = 150 * u, bx = W - 20 * u - bw, by = H - 60 * u;
        lb.line({bx, by, z}, {bx + bw, by, z}, dim, 0.8f);
        lb.line({bx, by + 8 * u, z}, {bx + bw, by + 8 * u, z}, dim, 0.8f);
        float fill = clampf(s.speed / 25.f, 0, 1) * bw;
        lb.line({bx, by, z}, {bx + fill, by, z}, accent, 0.9f);
        lb.line({bx, by + 8 * u, z}, {bx + fill, by + 8 * u, z}, accent, 0.9f);
        lb.line({bx + fill, by, z}, {bx + fill, by + 8 * u, z}, accent, 0.9f);

        // air-time indicator
        if (s.state == S_AIR && s.airTime > 0.45f) {
            snprintf(buf, sizeof buf, "AIR %.1f", s.airTime);
            float aw = font.textWidth(buf, 2.4f * u);
            font.draw(lb, buf, W - 20 * u - aw, H - 80 * u, 2.4f * u, white);
        }
        // balance meter while grinding
        if (s.state == S_GRIND) {
            float mw = 140 * u, mx = W / 2 - mw / 2, my = H - 70 * u;
            lb.line({mx, my, z}, {mx + mw, my, z}, dim, 0.9f);
            lb.line({mx, my + 10 * u, z}, {mx + mw, my + 10 * u, z}, dim, 0.9f);
            lb.line({mx, my, z}, {mx, my + 10 * u, z}, dim, 0.9f);
            lb.line({mx + mw, my, z}, {mx + mw, my + 10 * u, z}, dim, 0.9f);
            float c = mx + mw / 2 + s.balance * mw / 2;
            Vec3 mc = std::abs(s.balance) > 0.6f ? Vec3{1, 0.25f, 0.2f} : accent;
            lb.line({c - 4 * u, my - 2 * u, z}, {c - 4 * u, my + 12 * u, z}, mc, 1.f);
            lb.line({c + 4 * u, my - 2 * u, z}, {c + 4 * u, my + 12 * u, z}, mc, 1.f);
            lb.line({c - 4 * u, my - 2 * u, z}, {c + 4 * u, my - 2 * u, z}, mc, 1.f);
            lb.line({c - 4 * u, my + 12 * u, z}, {c + 4 * u, my + 12 * u, z}, mc, 1.f);
        }
        // bail readout — Hall of Meat
        if (s.state == S_BAIL) {
            const char* t = "HALL OF MEAT";
            float tw = font.textWidth(t, 6.4f * u);
            font.drawBold(lb, t, W / 2 - tw / 2, H * 0.30f, 6.4f * u, {1, 0.3f, 0.2f});
            float bw2 = font.textWidth(s.bailText, 3.2f * u);
            font.draw(lb, s.bailText, W / 2 - bw2 / 2, H * 0.30f + 52 * u, 3.2f * u, white);
        }
        // controls hint first seconds (always small bottom-center)
        const char* hint = "W PUSH  A/D CARVE  CTRL OLLIE  ARROWS TRICKS  Q/E/F/G GRAB  SPACE POP  M MARKER  T TELEPORT";
        font.draw(lb, hint, W / 2 - font.textWidth(hint, 1.6f * u) / 2, H - 22 * u, 1.6f * u, dim, 0.75f);
    }
};
