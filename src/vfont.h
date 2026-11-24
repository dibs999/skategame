// Vector stroke font for the HUD: every glyph is line segments on a 4x6 grid,
// drawn through the line shader. Seven-segment style digits for the techy HUD look.
#pragma once
#include "glcore.h"
#include "math3d.h"
#include <string>
#include <unordered_map>
#include <cctype>

struct VFont {
    std::unordered_map<char, std::string> g;
    VFont() {
        g['A'] = "0620|2046|1434";
        g['B'] = "0006|0030|3041|4233|3344|4536|3606|3303";
        g['C'] = "4000|0006|0646";
        g['D'] = "0006|0030|3041|4145|4536|3606";
        g['E'] = "4000|0006|0646|0333";
        g['F'] = "4000|0006|0333";
        g['G'] = "4000|0006|0646|4644|4424";
        g['H'] = "0006|4006|0343";
        g['I'] = "0040|2026|0646";
        g['J'] = "4046|4626|2605|0504";
        g['K'] = "0006|4003|0346";
        g['L'] = "0006|0646";
        g['M'] = "0600|0024|2440|4046";
        g['N'] = "0600|0046|4640";
        g['O'] = "1030|3041|4145|4536|3516|1505|0501|0110";
        g['P'] = "0600|0030|3041|4123|2303";
        g['Q'] = "1030|3041|4145|4536|3516|1505|0501|0110|3446";
        g['R'] = "0600|0030|3041|4123|2303|2346";
        g['S'] = "4010|1001|0102|0213|1333|3344|4445|4536|3606";
        g['T'] = "0040|2026";
        g['U'] = "0005|0516|1636|3645|4540";
        g['V'] = "0026|2640";
        g['W'] = "0016|1623|2336|3640";
        g['X'] = "0046|4606";
        g['Y'] = "0023|4023|2326";
        g['Z'] = "0040|4006|0646";
        g['0'] = "1030|4003|4346|1636|0306|0003";
        g['1'] = "4006|2636";
        g['2'] = "1030|4003|1333|0306|1636";
        g['3'] = "1030|4003|1333|4346|1636";
        g['4'] = "0003|1333|4006";
        g['5'] = "1030|0003|1333|4346|1636";
        g['6'] = "1030|0003|1333|0306|4346|1636";
        g['7'] = "1030|4006";
        g['8'] = "1030|4003|4346|1636|0306|0003|1333";
        g['9'] = "1030|4003|4346|1636|0003|1333";
        g['-'] = "1333";
        g['+'] = "2224|1333";
        g[':'] = "2122|2425";
        g['.'] = "2526";
        g['!'] = "2024|2526";
        g['/'] = "0640";
        g['('] = "3011|1115|1536";
        g[')'] = "1031|3135|3516";
        g['<'] = "3023|2336";
        g['>'] = "1023|1336";
        g['%'] = "0046|1111|5455";
        g['*'] = "2020|2424|2224|2024|2420";
        g['_'] = "0636";
        g['='] = "1323|1443";
        g['|'] = "2006";
        g['#'] = "1115|3115|0214|0243";
    }
    float width(char c) const { return c == ' ' ? 3.f : (g.count(std::toupper(c)) ? 5.f : 5.f); }
    float textWidth(const std::string& s, float scale) const {
        float w = 0;
        for (char c : s) w += width(c);
        return w * scale;
    }
    // Emit strokes into a line batch. x,y = top-left in screen px, scale = px per glyph unit.
    void draw(LineBatch& lb, const std::string& s, float x, float y, float scale,
              const Vec3& col, float alpha = 1.f, float z = 0.f) const {
        float cx = x;
        for (char cc : s) {
            char c = std::toupper(cc);
            auto it = g.find(c);
            if (it == g.end()) { cx += width(c) * scale; continue; }
            const std::string& d = it->second;
            for (size_t i = 0; i + 3 < d.size(); i++) {
                if (d[i] == '|') continue;
                float x1 = d[i] - '0', y1 = d[i+1] - '0', x2 = d[i+2] - '0', y2 = d[i+3] - '0';
                lb.line({cx + x1 * scale, y + y1 * scale, z},
                        {cx + x2 * scale, y + y2 * scale, z}, col, alpha);
                i += 3;
            }
            cx += width(c) * scale;
        }
    }
    // Simple thickness effect: draw twice with slight offset.
    void drawBold(LineBatch& lb, const std::string& s, float x, float y, float scale,
                  const Vec3& col, float alpha = 1.f) const {
        draw(lb, s, x, y, scale, col, alpha);
        draw(lb, s, x + scale * 0.13f, y + scale * 0.06f, scale, col, alpha * 0.9f);
    }
};
