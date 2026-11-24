// Wavefront OBJ loader for Blender-exported body parts (skater.obj).
// Parses o/v/vn/f groups into GPU meshes keyed by object name.
#pragma once
#include "glcore.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

inline bool loadObjParts(const char* path, std::map<std::string, Mesh>& out) {
    FILE* f = fopen(path, "r");
    if (!f) return false;
    std::vector<Vec3> pos, nrm;
    struct Acc { std::vector<Vertex> v; std::vector<unsigned> idx; };
    std::map<std::string, Acc> parts;
    Acc* cur = nullptr;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == 'o' && line[1] == ' ') {
            std::string name(line + 2);
            while (!name.empty() && (name.back() == '\n' || name.back() == '\r' || name.back() == ' ')) name.pop_back();
            cur = &parts[name];
        } else if (line[0] == 'v' && line[1] == ' ') {
            float x, y, z;
            if (sscanf(line + 2, "%f %f %f", &x, &y, &z) == 3) pos.push_back({x, y, z});
        } else if (line[0] == 'v' && line[1] == 'n') {
            float x, y, z;
            if (sscanf(line + 2, "%f %f %f", &x, &y, &z) == 3) nrm.push_back({x, y, z});
        } else if (line[0] == 'f' && line[1] == ' ' && cur) {
            // f v//vn v//vn v//vn  (indices 1-based into global arrays)
            unsigned vi[3], ni[3];
            int got = sscanf(line + 2, "%u//%u %u//%u %u//%u",
                             &vi[0], &ni[0], &vi[1], &ni[1], &vi[2], &ni[2]);
            if (got != 6) continue;
            for (int k = 0; k < 3; k++) {
                Vec3 p = pos[vi[k] - 1], n = nrm.empty() ? Vec3(0, 1, 0) : nrm[ni[k] - 1];
                Vertex vx{p.x, p.y, p.z, n.x, n.y, n.z, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
                cur->v.push_back(vx);
                cur->idx.push_back((unsigned)cur->v.size() - 1);
            }
        }
    }
    fclose(f);
    if (parts.empty()) return false;
    for (auto& kv : parts) {
        Mesh m;
        m.upload(kv.second.v, kv.second.idx);
        out[kv.first] = m;
    }
    return true;
}
