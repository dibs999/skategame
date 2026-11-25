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
        Vec3 red{1.0f, 0.3f, 0.2f};
        Vec3 shad{0.02f, 0.02f, 0.03f};
        float z = 0;
        // drop-shadowed text: dark copy offset down-right, then the glyph
        auto text = [&](const std::string& s, float x, float y, float sc,
                        const Vec3& col, float a = 1.f) {
            font.draw(lb, s, x + 1.6f * u, y + 1.6f * u, sc, shad, a * 0.55f);
            font.draw(lb, s, x, y, sc, col, a);
        };
        auto textBold = [&](const std::string& s, float x, float y, float sc,
                            const Vec3& col, float a = 1.f) {
            font.draw(lb, s, x + 2.f * u, y + 2.f * u, sc, shad, a * 0.55f);
            font.drawBold(lb, s, x, y, sc, col, a);
        };

        // soft shade plate behind the bottom-left readout for readability
        auto plate = [&](float x, float y, float w, float h, float a) {
            int lines = 14;
            for (int i = 0; i < lines; i++) {
                float yy = y + h * i / (lines - 1);
                float fade = 1.f - std::abs(i / (lines - 1.f) - 0.5f) * 1.2f;
                lb.line({x, yy, z}, {x + w, yy, z}, shad, a * fade);
            }
        };
        // trick name bottom-left (Skate 3 style corner readout)
        float tx = 18 * u, ty = H - 90 * u;
        bool anyTrick = !s.trickName.empty() || (s.lineScore > 0);
        if (anyTrick) plate(tx - 8 * u, ty - 34 * u, 330 * u, 100 * u, 0.16f);
        if (!s.trickName.empty()) {
            textBold(s.trickName, tx, ty, 3.4f * u, white);
            // thin accent underline beneath the trick name
            float tw2 = font.textWidth(s.trickName, 3.4f * u);
            lb.line({tx, ty + 15 * u, z}, {tx + tw2, ty + 15 * u, z}, accent, 0.85f);
        }
        // running trick chain of the current line (Skate 3 bottom readout)
        if (!s.lineTricks.empty() && s.lineScore > 0) {
            text(s.lineTricks, tx, ty - 26 * u, 1.9f * u, dim);
        }
        // line score + multiplier under it
        char buf[64];
        if (s.lineScore > 0) {
            snprintf(buf, sizeof buf, "%d", s.lineScore);
            text(buf, tx, ty + 30 * u, 3.0f * u, white);
            float sw2 = font.textWidth(buf, 3.0f * u);
            snprintf(buf, sizeof buf, "%dX", s.mult);
            text(buf, tx + sw2 + 14 * u, ty + 30 * u, 3.0f * u, accent);
        }
        // total score top-left
        snprintf(buf, sizeof buf, "TOTAL %d", s.totalScore + s.lineScore);
        text(buf, 18 * u, 20 * u, 2.6f * u, dim);
        // score-attack run timer + running points, top-center
        if (s.runTimer > 0.f) {
            int sec = (int)s.runTimer;
            snprintf(buf, sizeof buf, "RUN %d   %d:%02d", s.totalScore + s.lineScore - s.runBase, sec / 60, sec % 60);
            textBold(buf, W / 2 - font.textWidth(buf, 2.8f * u) / 2, 20 * u, 2.8f * u, sec < 10 ? red : accent);
        } else if (s.lastRun > 0) {
            snprintf(buf, sizeof buf, "LAST RUN %d", s.lastRun);
            text(buf, 18 * u, 44 * u, 2.2f * u, dim);
        }

        // speedometer bottom-right
        float kph = s.speed * 3.6f;
        snprintf(buf, sizeof buf, "%d", (int)(kph + 0.5f));
        float sw = font.textWidth(buf, 3.4f * u);
        text(buf, W - 20 * u - sw, H - 40 * u, 3.4f * u, white);
        text("KM/H", W - 20 * u - sw - font.textWidth("KM/H", 1.6f * u) - 6 * u, H - 40 * u + 4 * u, 1.6f * u, dim);
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
            text(buf, W - 20 * u - aw, H - 80 * u, 2.4f * u, white);
        }
        // balance meter while grinding / manualing
        if (s.state == S_GRIND || s.state == S_MANUAL) {
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
            textBold(t, W / 2 - tw / 2, H * 0.30f, 6.4f * u, {1, 0.3f, 0.2f});
            float bw2 = font.textWidth(s.bailText, 3.2f * u);
            text(s.bailText, W / 2 - bw2 / 2, H * 0.30f + 52 * u, 3.2f * u, white);
            snprintf(buf, sizeof buf, "+%d", s.meatGain);
            text(buf, W / 2 - font.textWidth(buf, 2.8f * u) / 2, H * 0.30f + 82 * u, 2.8f * u, accent);
        }
        if (s.meatPts > 0 && s.state != S_BAIL) {
            snprintf(buf, sizeof buf, "MEAT %d", s.meatPts);
            text(buf, W - 18 * u - font.textWidth(buf, 2.2f * u), 44 * u, 2.2f * u, dim);
        }
        if (s.bestScore > 0) {
            snprintf(buf, sizeof buf, "BEST %d", s.bestScore);
            text(buf, W - 18 * u - font.textWidth(buf, 2.0f * u), 62 * u, 2.0f * u, dim);
        }
        // controls hint first seconds (always small bottom-center)
        const char* hint = "W PUSH  A/D CARVE  CTRL OLLIE  ARROWS TRICKS  SPC BONELESS  Q/E/F/G GRAB  R/V MANUAL";
        const char* hint2 = "M/T MARKER  1-5 SPOTS  O RUN  N DUSK  BKSP RESET";
        // hints fade out once the player has had time to learn them
        float hintA = 0.55f * clampf((30.f - g.now) / 8.f, 0.f, 1.f);
        if (hintA > 0.01f) {
            text(hint, W / 2 - font.textWidth(hint, 2.0f * u) / 2, H - 40 * u, 2.0f * u, dim, hintA);
            text(hint2, W / 2 - font.textWidth(hint2, 2.0f * u) / 2, H - 20 * u, 2.0f * u, dim, hintA);
        }
    }
};
