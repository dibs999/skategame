// Procedural textures: tileable value noise + per-surface synthesis.
#include "texgen.h"
#include "glcore.h"
#include "math3d.h"
#include <vector>
#include <cmath>
#include <unordered_map>

static unsigned g_seed = 1;
static float hash2(int x, int y) {
    unsigned h = hashu((unsigned)(x * 73856093 ^ y * 19349663) + g_seed * 97U);
    return (h & 0xFFFFFF) / float(0x1000000);
}
static float vnoise(float x, float y, float period) {
    // tileable value noise with integer period in lattice cells
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float xf = x - xi, yf = y - yi;
    auto at = [&](int ix, int iy) {
        int px = ((ix % (int)period) + (int)period) % (int)period;
        int py = ((iy % (int)period) + (int)period) % (int)period;
        return hash2(px * 4 + py * 1024, px + py * 7);
    };
    float sx = xf * xf * (3 - 2 * xf), sy = yf * yf * (3 - 2 * yf);
    return lerpf(lerpf(at(xi, yi), at(xi + 1, yi), sx),
                 lerpf(at(xi, yi + 1), at(xi + 1, yi + 1), sx), sy);
}
static GLuint genGraffiti(int w);
static float fbm2(float x, float y, float period, int oct) {
    float v = 0, a = 0.5f, p = period;
    for (int i = 0; i < oct; i++) {
        v += a * vnoise(x * p, y * p, p);
        p *= 2; a *= 0.5f;
    }
    return v;
}
static void speckle(std::vector<unsigned char>& img, int w, int h, int n,
                    int r, int g, int b, int alpha, unsigned& seed, int szmin=1, int szmax=1) {
    for (int i = 0; i < n; i++) {
        int x = (int)(randf(seed) * w), y = (int)(randf(seed) * h);
        int sz = szmin + (int)(randf(seed) * (szmax - szmin + 1));
        float a = alpha * (0.4f + 0.6f * randf(seed));
        for (int dy = 0; dy < sz; dy++)
            for (int dx = 0; dx < sz; dx++) {
                int px = (x + dx) % w, py = (y + dy) % h;
                unsigned char* p = &img[(py * w + px) * 4];
                for (int c = 0; c < 3; c++)
                    p[c] = (unsigned char)lerpf(p[c], (float)(c == 0 ? r : c == 1 ? g : b), a / 255.f);
            }
    }
}
static void strokes(std::vector<unsigned char>& img, int w, int h, int n,
                    int r, int g, int b, int alpha, unsigned& seed, int maxlen, bool horiz) {
    // random scratches / wax smears
    for (int i = 0; i < n; i++) {
        int x = (int)(randf(seed) * w), y = (int)(randf(seed) * h);
        int len = 4 + (int)(randf(seed) * maxlen);
        float a = alpha * (0.5f + 0.5f * randf(seed)) / 255.f;
        for (int s = 0; s < len; s++) {
            int px = horiz ? (x + s) % w : x, py = horiz ? y : (y + s) % h;
            for (int th = -1; th <= 1; th++) {
                int qx = horiz ? px : (px + th + w) % w;
                int qy = horiz ? (py + th + h) % h : py;
                unsigned char* p = &img[(qy * w + qx) * 4];
                float k = a * (th == 0 ? 1.f : 0.4f);
                for (int c = 0; c < 3; c++)
                    p[c] = (unsigned char)lerpf(p[c], (float)(c == 0 ? r : c == 1 ? g : b), k);
            }
        }
    }
}
static GLuint finish(std::vector<unsigned char>& img, int w, int h) {
    return makeTexture(w, h, img.data());
}
static std::vector<unsigned char> base(int w, int h, float r, float g, float b) {
    std::vector<unsigned char> img(w * h * 4);
    for (int i = 0; i < w * h; i++) {
        img[i * 4 + 0] = (unsigned char)r;
        img[i * 4 + 1] = (unsigned char)g;
        img[i * 4 + 2] = (unsigned char)b;
        img[i * 4 + 3] = 255;
    }
    return img;
}

static GLuint genConcrete(int w, float basev, bool joints, bool wax) {
    auto img = base(w, w, basev, basev, basev - 4);
    int h = w;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float u = x / (float)w, v = y / (float)h;
            float n = fbm2(u * 8, v * 8, 8, 5) - 0.5f;         // big blotches
            float n2 = fbm2(u * 32, v * 32, 32, 4) - 0.5f;     // fine grain
            float t = 1.f + n * 0.22f + n2 * 0.10f;
            unsigned char* p = &img[(y * w + x) * 4];
            for (int c = 0; c < 3; c++) p[c] = (unsigned char)clampf(p[c] * t, 0, 255);
        }
    unsigned seed = 7;
    if (joints) {
        // expansion joints: dark seams every quarter, with slight wobble
        int per = w / 4;
        for (int j = 0; j < 4; j++) {
            int cx = j * per;
            for (int y = 0; y < h; y++) {
                int off = (int)(fbm2(0.3f, y * 0.03f, 8, 3) * 6 - 3);
                for (int d = -1; d <= 1; d++) {
                    int px = (cx + off + d + w) % w;
                    unsigned char* p = &img[(y * w + px) * 4];
                    float k = d == 0 ? 0.55f : 0.25f;
                    for (int c = 0; c < 3; c++) p[c] = (unsigned char)lerpf(p[c], 40.f, k);
                }
            }
            int cy = j * per;
            for (int x = 0; x < w; x++) {
                int off = (int)(fbm2(x * 0.03f, 0.7f, 8, 3) * 6 - 3);
                for (int d = -1; d <= 1; d++) {
                    int py = (cy + off + d + h) % h;
                    unsigned char* p = &img[(py * w + x) * 4];
                    float k = d == 0 ? 0.55f : 0.25f;
                    for (int c = 0; c < 3; c++) p[c] = (unsigned char)lerpf(p[c], 40.f, k);
                }
            }
        }
    }
    // oil/rust stains: irregular dark blotches that pool near joints
    for (int st = 0; st < 7; st++) {
        float cx = randf(seed), cy = randf(seed), r = 0.03f + randf(seed) * 0.08f;
        float tone = 0.55f + randf(seed) * 0.2f;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                float u = x / (float)w - cx, v = y / (float)h - cy;
                float d = std::sqrt(u * u + v * v) / r + (fbm2(u * 22 + 5, v * 22, 22, 3) - 0.5f) * 0.8f;
                if (d < 1.f) {
                    unsigned char* p = &img[(y * w + x) * 4];
                    float k = (1.f - d) * 0.35f * tone;
                    for (int cc = 0; cc < 3; cc++) p[cc] = (unsigned char)lerpf(p[cc], 88.f, k);
                }
            }
    }
    // hairline cracks: thin wandering dark polylines
    for (int crk = 0; crk < 5; crk++) {
        float x = randf(seed) * w, y = randf(seed) * h;
        float ang = randf(seed) * 6.28f;
        int segs = 20 + (int)(randf(seed) * 40);
        for (int sgm = 0; sgm < segs; sgm++) {
            ang += (fbm2(x * 0.05f, y * 0.05f, 8, 2) - 0.5f) * 1.4f;
            x += std::cos(ang) * 2.2f; y += std::sin(ang) * 2.2f;
            int px = ((int)x % w + w) % w, py = ((int)y % h + h) % h;
            unsigned char* p = &img[(py * w + px) * 4];
            for (int cc = 0; cc < 3; cc++) p[cc] = (unsigned char)lerpf(p[cc], 55.f, 0.4f);
        }
    }
    // scuffs and tire marks
    strokes(img, w, h, 60, 60, 60, 62, 90, seed, 30, false);
    strokes(img, w, h, 60, 60, 60, 62, 90, seed, 30, true);
    speckle(img, w, h, 900, 200, 200, 195, 60, seed);
    speckle(img, w, h, 900, 90, 90, 88, 70, seed);
    speckle(img, w, h, 160, 140, 125, 100, 50, seed, 2, 4); // small stains
    if (wax) {
        // pale wax smears (ledge wax rubbed on)
        strokes(img, w, h, 24, 225, 220, 205, 110, seed, 24, true);
    }
    return finish(img, w, h);
}

static GLuint genWood(int w) {
    auto img = base(w, w, 158, 118, 74);
    int plank = w / 8;
    for (int y = 0; y < w; y++) {
        int row = y / plank;
        float rowTint = hash2(row, 5) * 0.25f + 0.85f;
        for (int x = 0; x < w; x++) {
            float u = x / (float)w;
            float grain = fbm2(u * 6 + hash2(row, 9) * 8, y * 0.02f, 8, 4);
            float ring = std::sin((u * 14 + grain * 3.f) * 6.28f) * 0.5f + 0.5f;
            float t = rowTint * (0.82f + ring * 0.22f + (grain - 0.5f) * 0.18f);
            unsigned char* p = &img[(y * w + x) * 4];
            p[0] = (unsigned char)clampf(168 * t, 0, 255);
            p[1] = (unsigned char)clampf(128 * t, 0, 255);
            p[2] = (unsigned char)clampf(82 * t, 0, 255);
        }
    }
    // plank seams
    for (int y = 0; y < w; y++)
        if (y % plank == 0)
            for (int x = 0; x < w; x++) {
                unsigned char* p = &img[(y * w + x) * 4];
                p[0] /= 2; p[1] /= 2; p[2] /= 2;
            }
    unsigned seed = 31;
    speckle(img, w, w, 200, 60, 40, 20, 50, seed);
    return finish(img, w, w);
}

static GLuint genMetal(int w) {
    auto img = base(w, w, 165, 170, 178);
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            float streak = fbm2(x * 0.015f, y * 0.9f, 16, 4);
            float t = 0.78f + streak * 0.42f;
            unsigned char* p = &img[(y * w + x) * 4];
            for (int c = 0; c < 3; c++) p[c] = (unsigned char)clampf(p[c] * t, 0, 255);
        }
    unsigned seed = 3;
    speckle(img, w, w, 80, 120, 125, 135, 40, seed);
    return finish(img, w, w);
}

static GLuint genGrip(int w) {
    auto img = base(w, w, 22, 22, 24);
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            float n = fbm2(x * 0.9f, y * 0.9f, 64, 3);
            float t = 0.7f + n * 0.7f;
            unsigned char* p = &img[(y * w + x) * 4];
            for (int c = 0; c < 3; c++) p[c] = (unsigned char)clampf(p[c] * t, 0, 40);
        }
    unsigned seed = 13;
    speckle(img, w, w, 2600, 190, 190, 185, 90, seed); // grit sparkle
    return finish(img, w, w);
}

static GLuint genDeckArt(int w) {
    // bold deck graphic: diagonal stripes + circle logo + halftone
    auto img = base(w, w, 30, 34, 120);
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            float u = x / (float)w, v = y / (float)w;
            float band = std::fmod(u * 2.4f + v * 1.1f, 1.f);
            Vec3 c;
            if (band < 0.33f) c = {230, 60, 40};
            else if (band < 0.66f) c = {30, 34, 120};
            else c = {245, 200, 40};
            // halftone dots
            float d = std::fmod(u * 24, 1.f) - 0.5f + std::fmod(v * 24, 1.f) - 0.5f;
            if (d > 0.5f) c = c * 0.8f;
            // central burst
            float r = std::hypot(u - 0.5f, v - 0.5f);
            if (r < 0.22f) c = c.lerp({255, 255, 255}, 1.f - r / 0.22f);
            unsigned char* p = &img[(y * w + x) * 4];
            p[0] = (unsigned char)c.x; p[1] = (unsigned char)c.y; p[2] = (unsigned char)c.z;
        }
    unsigned seed = 77;
    speckle(img, w, w, 400, 20, 20, 24, 80, seed); // wear scratches
    strokes(img, w, w, 30, 210, 210, 210, 90, seed, 40, true);
    return finish(img, w, w);
}

static GLuint genDenim(int w) {
    auto img = base(w, w, 52, 70, 105);
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            float u = x / (float)w, v = y / (float)w;
            float weave = std::fmod(u * 96 + v * 96, 2.f) < 1.f ? 0.92f : 1.05f;
            float n = fbm2(u * 24, v * 24, 24, 4);
            float t = weave * (0.85f + n * 0.3f);
            unsigned char* p = &img[(y * w + x) * 4];
            p[0] = (unsigned char)clampf(56 * t, 0, 255);
            p[1] = (unsigned char)clampf(74 * t, 0, 255);
            p[2] = (unsigned char)clampf(110 * t, 0, 255);
        }
    unsigned seed = 55;
    strokes(img, w, w, 40, 140, 150, 170, 40, seed, 60, true); // fade streaks
    return finish(img, w, w);
}

static GLuint genFabric(int w, float r, float g, float b) {
    auto img = base(w, w, r, g, b);
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            float u = x / (float)w, v = y / (float)w;
            float n = fbm2(u * 48, v * 48, 48, 3) - 0.5f;
            float t = 1.f + n * 0.16f;
            unsigned char* p = &img[(y * w + x) * 4];
            for (int c = 0; c < 3; c++) p[c] = (unsigned char)clampf(p[c] * t, 0, 255);
        }
    return finish(img, w, w);
}

static GLuint genBark(int w) {
    auto img = base(w, w, 90, 70, 50);
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            float u = x / (float)w;
            float n = fbm2(u * 12, y * 0.02f, 16, 4);
            float t = 0.65f + n * 0.7f;
            unsigned char* p = &img[(y * w + x) * 4];
            for (int c = 0; c < 3; c++) p[c] = (unsigned char)clampf(p[c] * t, 0, 255);
        }
    return finish(img, w, w);
}

static GLuint genLeaf(int w) {
    auto img = base(w, w, 55, 120, 45);
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            float u = x / (float)w, v = y / (float)w;
            float n = fbm2(u * 20, v * 20, 20, 5);
            float t = 0.6f + n * 0.85f;
            unsigned char* p = &img[(y * w + x) * 4];
            p[0] = (unsigned char)clampf(60 * t, 0, 255);
            p[1] = (unsigned char)clampf(130 * t, 0, 255);
            p[2] = (unsigned char)clampf(50 * t, 0, 255);
        }
    return finish(img, w, w);
}

static GLuint genAsphalt(int w) {
    auto img = base(w, w, 62, 62, 66);
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            float u = x / (float)w, v = y / (float)w;
            float n = fbm2(u * 40, v * 40, 40, 4) - 0.5f;
            float t = 1.f + n * 0.3f;
            unsigned char* p = &img[(y * w + x) * 4];
            for (int c = 0; c < 3; c++) p[c] = (unsigned char)clampf(p[c] * t, 0, 255);
        }
    // painted parking lines: white stripes every 5th cell band, worn
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            float u = x / (float)w, v = y / (float)w;
            float line = std::fabs(std::fmod(u * 4.f, 1.f) - 0.02f);
            bool mark = line < 0.012f && v > 0.1f && v < 0.9f;
            float wear = fbm2(u * 30, v * 30, 30, 3);
            if (mark && wear > 0.35f) {
                unsigned char* p = &img[(y * w + x) * 4];
                p[0] = 215; p[1] = 215; p[2] = 210;
            }
            // yellow curb line
            if (v > 0.03f && v < 0.05f && wear > 0.3f) {
                unsigned char* p = &img[(y * w + x) * 4];
                p[0] = 220; p[1] = 180; p[2] = 40;
            }
        }
    unsigned seed = 91;
    speckle(img, w, w, 700, 90, 90, 95, 60, seed);
    speckle(img, w, w, 500, 40, 40, 44, 60, seed);
    return finish(img, w, w);
}

static GLuint genCurbPaint(int w) {
    auto img = base(w, w, 235, 235, 230);
    unsigned seed = 17;
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            float u = x / (float)w, v = y / (float)w;
            float wear = fbm2(u * 12, v * 12, 12, 4);
            if (wear < 0.42f) { // chipped paint -> concrete underneath
                unsigned char* p = &img[(y * w + x) * 4];
                p[0] = 165; p[1] = 163; p[2] = 158;
            }
        }
    speckle(img, w, w, 300, 120, 118, 110, 70, seed);
    return finish(img, w, w);
}

static GLuint genBuilding(int w) {
    // concrete frame + window grid: mostly dark glass, a few lit
    auto img = base(w, w, 188, 184, 178);
    int cols = 6, rows = 8;
    unsigned seed = 9;
    for (int ry = 0; ry < rows; ry++)
        for (int cx = 0; cx < cols; cx++) {
            int x0 = cx * w / cols + w / cols / 6, x1 = (cx + 1) * w / cols - w / cols / 6;
            int y0 = ry * w / rows + w / rows / 5, y1 = (ry + 1) * w / rows - w / rows / 7;
            float lit = hash2(cx * 3 + ry, ry * 7 + cx);
            Vec3 glass = lit > 0.82f ? Vec3{252, 214, 150} : Vec3{52, 66, 84};
            float tint = 0.8f + hash2(cx, ry) * 0.45f;
            for (int y = y0; y < y1; y++)
                for (int x = x0; x < x1; x++) {
                    // sky reflection gradient: lighter toward top of each pane
                    float g = glass.z * tint + (y - y0) / (float)(y1 - y0) * 26;
                    unsigned char* p = &img[(y * w + x) * 4];
                    p[0] = (unsigned char)clampf(glass.x * tint + (y - y0) * 0.4f, 0, 255);
                    p[1] = (unsigned char)clampf(glass.y * tint + (y - y0) * 0.5f, 0, 255);
                    p[2] = (unsigned char)clampf(g, 0, 255);
                }
        }
    // grime streaks + slab lines between floors
    strokes(img, w, w, 50, 120, 118, 112, 60, seed, 30, false);
    for (int ry = 0; ry < rows; ry++)
        for (int x = 0; x < w; x++) {
            int y = ry * w / rows;
            unsigned char* p = &img[(y * w + x) * 4];
            for (int c = 0; c < 3; c++) p[c] = (unsigned char)(p[c] * 0.82f);
        }
    return finish(img, w, w);
}

static GLuint genWater(int w) {
    auto img = base(w, w, 30, 88, 140);
    // sun-glitter speckle + soft wave bands
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            float v = fbm2(x * 0.045f, y * 0.045f, 31, 4);
            float band = 0.5f + 0.5f * std::sin(y * 0.11f + v * 6.f);
            unsigned char* p = &img[(y * w + x) * 4];
            float l = 0.75f + v * 0.5f + band * 0.18f;
            p[0] = (unsigned char)clampf(p[0] * l + band * 14, 0, 255);
            p[1] = (unsigned char)clampf(p[1] * l + band * 18, 0, 255);
            p[2] = (unsigned char)clampf(p[2] * l + band * 20, 0, 255);
        }
    unsigned seed = 77;
    speckle(img, w, w, 700, 220, 235, 245, 255, seed);
    return finish(img, w, w);
}


// rasterize stroke-font tags into a texture: real letterforms instead of blobs
static void tagText(std::vector<unsigned char>& img, int w, float x0, float y0,
                    float sc, const char* word, int r, int g, int b) {
    static const std::unordered_map<char, std::string> G = {
        {'A', "0620|2046|1434"}, {'C', "4000|0006|0646"}, {'E', "4000|0006|0646|0333"},
        {'K', "0006|4003|0346"}, {'P', "0600|0030|3041|4123|2303"}, {'R', "0600|0030|3041|4123|2303|2346"},
        {'S', "4010|1001|0102|0213|1333|3344|4445|4536|3606"}, {'T', "0040|2026"},
        {'8', "1030|4003|4346|1636|0306|0003|1333"}, {'V', "0026|2640"}, {'O', "1030|3041|4145|4536|3516|1505|0501|0110"},
        {'N', "0600|0046|4640"}, {'I', "0040|2026|0646"}, {'L', "0006|0646"}
    };
    auto dot = [&](int px, int py, int cr, int cg, int cb, float k, int th) {
        for (int dy = -th; dy <= th; dy++)
            for (int dx = -th; dx <= th; dx++) {
                int qx = px + dx, qy = py + dy;
                if (qx < 0 || qy < 0 || qx >= w || qy >= w) continue;
                if (dx * dx + dy * dy > th * th + 1) continue;
                unsigned char* p = &img[(qy * w + qx) * 4];
                for (int c2 = 0; c2 < 3; c2++)
                    p[c2] = (unsigned char)lerpf(p[c2], (float)(c2 == 0 ? cr : c2 == 1 ? cg : cb), k);
            }
    };
    float cx = x0;
    for (const char* wp = word; *wp; wp++) {
        auto it = G.find(*wp);
        if (it == G.end()) { cx += sc * 5.f; continue; }
        // pass 1: thick dark outline; pass 2: thinner bright fill
        for (int pass = 0; pass < 2; pass++) {
            int th = pass == 0 ? 3 : 1;
            int cr = pass == 0 ? 20 : r, cg = pass == 0 ? 16 : g, cb = pass == 0 ? 20 : b;
            float k = pass == 0 ? 0.8f : 0.95f;
            size_t st = 0;
            std::string s = it->second;
            while (st < s.size()) {
                size_t bar = s.find('|', st);
                std::string seg = s.substr(st, bar == std::string::npos ? bar : bar - st);
                if (seg.size() >= 4) {
                    float xa = cx + (seg[0] - '0') * sc, ya = y0 + (seg[1] - '0') * sc;
                    float xb = cx + (seg[2] - '0') * sc, yb = y0 + (seg[3] - '0') * sc;
                    int steps = (int)(std::hypot(xb - xa, yb - ya) * 1.5f) + 1;
                    for (int i = 0; i <= steps; i++) {
                        float t = i / (float)steps;
                        dot((int)(xa + (xb - xa) * t), (int)(ya + (yb - ya) * t), cr, cg, cb, k, th);
                    }
                }
                if (bar == std::string::npos) break;
                st = bar + 1;
            }
        }
        cx += sc * 5.2f;
    }
}

static GLuint genGraffiti(int w) {
    // weathered concrete base + layered spray-paint bubbles, arcs and drips
    auto img = base(w, w, 152, 148, 142);
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            float u = x / (float)w, v = y / (float)w;
            float n = fbm2(u * 20, v * 20, 20, 4) - 0.5f;
            unsigned char* p = &img[(y * w + x) * 4];
            float tt = 1.f + n * 0.22f;
            for (int cc = 0; cc < 3; cc++) p[cc] = (unsigned char)clampf(p[cc] * tt, 0, 255);
        }
    unsigned seed = 77;
    static const unsigned char COLS[6][3] = {
        {232, 44, 120}, {40, 190, 232}, {250, 158, 30}, {120, 220, 60}, {168, 84, 240}, {250, 240, 60}};
    // big overlapping spray bubbles with dark outlines, like throw-up letters
    for (int b = 0; b < 9; b++) {
        // letters sit in loose rows on a shared baseline, overlapping like a throw-up
        float row = b < 5 ? 0.34f : 0.62f;
        float cx = 0.12f + (b % 5) * 0.16f + randf(seed) * 0.05f;
        float cy = row + (randf(seed) - 0.5f) * 0.05f;
        float rx = 0.07f + randf(seed) * 0.06f, ry = rx * (0.9f + randf(seed) * 0.5f);
        int col = (int)(randf(seed) * 6) % 6;
        for (int y = 0; y < w; y++)
            for (int x = 0; x < w; x++) {
                float u = x / (float)w - cx, v = (y / (float)w - cy) * 1.0f;
                float d = (u * u) / (rx * rx) + (v * v) / (ry * ry);
                if (d > 1.3f) continue;
                unsigned char* p = &img[(y * w + x) * 4];
                if (d > 1.0f) { // outline
                    for (int cc = 0; cc < 3; cc++) p[cc] = (unsigned char)(p[cc] * 0.35f);
                } else {
                    float edge = clampf((1.f - d) * 3.f, 0.f, 1.f);   // soft spray edge
                    float spk = 0.85f + 0.3f * fbm2(x * 0.2f, y * 0.2f, 16, 2);
                    for (int cc = 0; cc < 3; cc++)
                        p[cc] = (unsigned char)lerpf(p[cc], clampf(COLS[col][cc] * spk, 0, 255), edge * 0.9f);
                }
            }
        // paint drips below the blob
        int nd = 2 + (int)(randf(seed) * 3);
        for (int dr = 0; dr < nd; dr++) {
            int dx = (int)((cx - rx * 0.5f + randf(seed) * rx) * w);
            int sy = (int)((cy + ry * 0.7f) * w);
            int len = 6 + (int)(randf(seed) * 26);
            for (int s = 0; s < len && sy + s < w; s++) {
                unsigned char* p = &img[((sy + s) * w + dx) * 4];
                float k = 0.55f * (1.f - s / (float)len);
                for (int cc = 0; cc < 3; cc++)
                    p[cc] = (unsigned char)lerpf(p[cc], (float)COLS[col][cc], k);
            }
        }
    }
    // white highlight streaks on top of letters
    strokes(img, w, w, 22, 255, 255, 255, 90, seed, 30, true);
    // grime re-weathering so it sits into the wall
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            float g = fbm2(x / (float)w * 7, y / (float)w * 7, 7, 4);
            if (g > 0.62f) {
                unsigned char* p = &img[(y * w + x) * 4];
                float k = (g - 0.62f) * 0.9f;
                for (int cc = 0; cc < 3; cc++) p[cc] = (unsigned char)lerpf(p[cc], 105, k);
            }
        }
    // stylized throw-up tags in real letterforms over the bubbles
    tagText(img, w, w * 0.07f, w * 0.16f, w * 0.028f, "SK8", 250, 240, 60);
    tagText(img, w, w * 0.42f, w * 0.52f, w * 0.024f, "PORT", 40, 190, 232);
    tagText(img, w, w * 0.10f, w * 0.68f, w * 0.020f, "CARVERTON", 232, 44, 120);
    return finish(img, w, w);
}

void generateAllTextures(TexSet& t) {
    g_seed = 12345;
    t.concrete = genConcrete(512, 165, true, true);
    g_seed = 22222;
    t.concreteBowl = genConcrete(512, 150, false, false);
    t.asphalt = genAsphalt(512);
    t.wood = genWood(512);
    t.metal = genMetal(256);
    t.grip = genGrip(256);
    t.deckArt = genDeckArt(256);
    t.denim = genDenim(256);
    t.fabric = genFabric(128, 200, 60, 55);
    t.bark = genBark(128);
    t.leaf = genLeaf(256);
    t.curbPaint = genCurbPaint(256);
    t.building = genBuilding(256);

    t.water = genWater(256);
    t.graffiti = genGraffiti(256);
}
