// Thin OpenGL helpers: shader program, mesh VBO/VAO, texture, FBO, dynamic batches.
#pragma once
#include <GL/glew.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include "math3d.h"

struct Vertex {
    float px, py, pz;   // position
    float nx, ny, nz;   // normal
    float u, v;         // uv
    float r, g, b, a;   // vertex color (a doubles as AO bake)
};

struct Shader {
    GLuint prog = 0;
    bool compile(const char* vs, const char* fs) {
        GLuint v = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(v, 1, &vs, nullptr);
        glCompileShader(v);
        if (!check(v, "vertex")) return false;
        GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(f, 1, &fs, nullptr);
        glCompileShader(f);
        if (!check(f, "fragment")) { glDeleteShader(v); return false; }
        prog = glCreateProgram();
        glAttachShader(prog, v);
        glAttachShader(prog, f);
        glLinkProgram(prog);
        glDeleteShader(v);
        glDeleteShader(f);
        GLint ok = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
            fprintf(stderr, "shader link: %s\n", log);
            return false;
        }
        return true;
    }
    static bool check(GLuint s, const char* what) {
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetShaderInfoLog(s, sizeof(log), nullptr, log);
            fprintf(stderr, "%s shader: %s\n", what, log);
            return false;
        }
        return true;
    }
    void use() const { glUseProgram(prog); }
    GLint loc(const char* n) const { return glGetUniformLocation(prog, n); }
    void setMat4(const char* n, const Mat4& m) const {
        glUniformMatrix4fv(loc(n), 1, GL_FALSE, m.m);
    }
    void setMat3(const char* n, const float m[9]) const {
        glUniformMatrix3fv(loc(n), 1, GL_FALSE, m);
    }
    void setVec3(const char* n, const Vec3& v) const { glUniform3f(loc(n), v.x, v.y, v.z); }
    void setVec2(const char* n, float x, float y) const { glUniform2f(loc(n), x, y); }
    void setF(const char* n, float v) const { glUniform1f(loc(n), v); }
    void setI(const char* n, int v) const { glUniform1i(loc(n), v); }
};

struct Mesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei count = 0;
    void upload(const std::vector<Vertex>& verts, const std::vector<unsigned>& idx) {
        if (!vao) {
            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
            glGenBuffers(1, &ebo);
        }
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)12);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)24);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)32);
        glBindVertexArray(0);
        count = (GLsizei)idx.size();
    }
    void draw() const {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }
};

// A draw item: mesh + material + transform. World builds a list, renderer walks it.
struct Material {
    GLuint tex = 0;
    float texScale = 1.f;         // world-space uv scale multiplier
    Vec3 color = {1, 1, 1};
    float specular = 0.25f;       // Blinn-Phong spec strength
    float shininess = 32.f;
    float ao = 1.f;               // ambient occlusion multiplier
    float metal = 0.f;            // >0: adds sky-tinted fresnel sheen
    float emissive = 0.f;
};

struct DrawItem {
    const Mesh* mesh = nullptr;
    Mat4 model;
    Material mat;
    bool castShadow = true;
};

inline GLuint makeTexture(int w, int h, const unsigned char* rgba, bool mipmaps = true) {
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    if (mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.4f);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

struct FBO {
    GLuint fbo = 0, tex = 0;
    int w = 0, h = 0;
    bool create(int w_, int h_, bool depthOnly, bool hdr) {
        w = w_; h = h_;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        if (depthOnly) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0,
                         GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            float border[4] = {1.f, 1.f, 1.f, 1.f};
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        } else {
            GLenum internal = hdr ? GL_RGBA16F : GL_RGBA8;
            glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
            GLuint rbo;
            glGenRenderbuffers(1, &rbo);
            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                      GL_RENDERBUFFER, rbo);
        }
        bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        if (!ok) fprintf(stderr, "FBO incomplete\n");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return ok;
    }
    void bind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, w, h);
    }
};

// Screen-space line batch for the vector-font HUD and debug lines.
struct LineBatch {
    GLuint vao = 0, vbo = 0;
    std::vector<float> data; // x,y,z,r,g,b,a per vertex, pairs = lines
    void init() {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
    }
    void line(const Vec3& a, const Vec3& b, const Vec3& c, float alpha = 1.f) {
        data.insert(data.end(), {a.x, a.y, a.z, c.x, c.y, c.z, alpha,
                                 b.x, b.y, b.z, c.x, c.y, c.z, alpha});
    }
    void flush() {
        if (data.empty()) return;
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)12);
        glDrawArrays(GL_LINES, 0, (GLsizei)(data.size() / 7));
        glBindVertexArray(0);
        data.clear();
    }
};

// Point-sprite particle batch.
struct SpriteBatch {
    GLuint vao = 0, vbo = 0;
    std::vector<float> data; // x,y,z,size,r,g,b,a
    void init() {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
    }
    void sprite(const Vec3& p, float size, const Vec3& c, float alpha) {
        data.insert(data.end(), {p.x, p.y, p.z, size, c.x, c.y, c.z, alpha});
    }
    void flush() {
        if (data.empty()) return;
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)12);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)28);
        glDrawArrays(GL_POINTS, 0, (GLsizei)(data.size() / 8));
        glBindVertexArray(0);
        data.clear();
    }
};
