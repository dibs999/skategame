// Minimal 3D math library: vectors, matrices, helpers. Column-major Mat4, OpenGL layout.
#pragma once
#include <cmath>
#include <cstddef>

struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    float len() const { return std::sqrt(dot(*this)); }
    float len2() const { return dot(*this); }
    Vec3 normalized() const { float l = len(); return l > 1e-6f ? *this / l : Vec3(0, 0, 0); }
    Vec3 lerp(const Vec3& o, float t) const { return *this + (o - *this) * t; }
};

struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vec4(const Vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
};

struct Mat4 {
    float m[16]; // column-major
    Mat4() { for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.f : 0.f; }
    float& operator()(int c, int r) { return m[c * 4 + r]; }
    float operator()(int c, int r) const { return m[c * 4 + r]; }

    static Mat4 identity() { return Mat4(); }
    static Mat4 mul(const Mat4& a, const Mat4& b) {
        Mat4 r;
        for (int c = 0; c < 4; c++)
            for (int rr = 0; rr < 4; rr++) {
                float s = 0;
                for (int k = 0; k < 4; k++) s += a(k, rr) * b(c, k);
                r(c, rr) = s;
            }
        return r;
    }
    Mat4 operator*(const Mat4& o) const { return mul(*this, o); }
    Vec4 operator*(const Vec4& v) const {
        return {m[0]*v.x + m[4]*v.y + m[8]*v.z  + m[12]*v.w,
                m[1]*v.x + m[5]*v.y + m[9]*v.z  + m[13]*v.w,
                m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]*v.w,
                m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15]*v.w};
    }
    Vec3 transformPoint(const Vec3& v) const {
        Vec4 r = (*this) * Vec4(v, 1.f);
        return {r.x / r.w, r.y / r.w, r.z / r.w};
    }
    Vec3 transformDir(const Vec3& v) const {
        Vec4 r = (*this) * Vec4(v, 0.f);
        return {r.x, r.y, r.z};
    }
    static Mat4 translation(const Vec3& t) {
        Mat4 r;
        r(3, 0) = t.x; r(3, 1) = t.y; r(3, 2) = t.z;
        return r;
    }
    static Mat4 scale(const Vec3& s) {
        Mat4 r;
        r(0, 0) = s.x; r(1, 1) = s.y; r(2, 2) = s.z;
        return r;
    }
    static Mat4 rotateAxis(const Vec3& axis, float angle) {
        Vec3 a = axis.normalized();
        float c = std::cos(angle), s = std::sin(angle), t = 1.f - c;
        Mat4 r;
        r(0,0) = t*a.x*a.x + c;     r(0,1) = t*a.x*a.y + s*a.z; r(0,2) = t*a.x*a.z - s*a.y; r(0,3)=0;
        r(1,0) = t*a.x*a.y - s*a.z; r(1,1) = t*a.y*a.y + c;     r(1,2) = t*a.y*a.z + s*a.x; r(1,3)=0;
        r(2,0) = t*a.x*a.z + s*a.y; r(2,1) = t*a.y*a.z - s*a.x; r(2,2) = t*a.z*a.z + c;     r(2,3)=0;
        r(3,0)=0; r(3,1)=0; r(3,2)=0; r(3,3)=1;
        return r;
    }
    static Mat4 rotateX(float a) { return rotateAxis({1,0,0}, a); }
    static Mat4 rotateY(float a) { return rotateAxis({0,1,0}, a); }
    static Mat4 rotateZ(float a) { return rotateAxis({0,0,1}, a); }
    static Mat4 perspective(float fovy, float aspect, float zn, float zf) {
        Mat4 r;
        for (int i = 0; i < 16; i++) r.m[i] = 0;
        float t = 1.f / std::tan(fovy * 0.5f);
        r(0,0) = t / aspect;
        r(1,1) = t;
        r(2,2) = (zf + zn) / (zn - zf);
        r(2,3) = -1.f;
        r(3,2) = 2.f * zf * zn / (zn - zf);
        r(3,3) = 0;
        return r;
    }
    static Mat4 ortho(float l, float rt, float b, float t, float zn, float zf) {
        Mat4 r;
        for (int i = 0; i < 16; i++) r.m[i] = 0;
        r(0,0) = 2.f/(rt-l); r(1,1) = 2.f/(t-b); r(2,2) = -2.f/(zf-zn);
        r(3,0) = -(rt+l)/(rt-l); r(3,1) = -(t+b)/(t-b); r(3,2) = -(zf+zn)/(zf-zn); r(3,3)=1;
        return r;
    }
    static Mat4 lookAt(const Vec3& eye, const Vec3& ctr, const Vec3& up) {
        Vec3 f = (ctr - eye).normalized();
        Vec3 s = f.cross(up).normalized();
        Vec3 u = s.cross(f);
        Mat4 r;
        r(0,0)=s.x;  r(1,0)=s.y;  r(2,0)=s.z;  r(3,0)=-s.dot(eye);
        r(0,1)=u.x;  r(1,1)=u.y;  r(2,1)=u.z;  r(3,1)=-u.dot(eye);
        r(0,2)=-f.x; r(1,2)=-f.y; r(2,2)=-f.z; r(3,2)=f.dot(eye);
        r(0,3)=0;    r(1,3)=0;    r(2,3)=0;    r(3,3)=1;
        return r;
    }
    // Extract the upper 3x3 (for normals when scale is uniform-ish).
    void normalMat3(float out[9]) const {
        out[0]=m[0]; out[1]=m[1]; out[2]=m[2];
        out[3]=m[4]; out[4]=m[5]; out[5]=m[6];
        out[6]=m[8]; out[7]=m[9]; out[8]=m[10];
    }
    static Mat4 fromBasis(const Vec3& right, const Vec3& up, const Vec3& fwd, const Vec3& pos) {
        Mat4 r;
        r(0,0)=right.x; r(0,1)=right.y; r(0,2)=right.z; r(0,3)=0;
        r(1,0)=up.x;    r(1,1)=up.y;    r(1,2)=up.z;    r(1,3)=0;
        r(2,0)=fwd.x;   r(2,1)=fwd.y;   r(2,2)=fwd.z;   r(2,3)=0;
        r(3,0)=pos.x;   r(3,1)=pos.y;   r(3,2)=pos.z;   r(3,3)=1;
        return r;
    }
};

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstepf(float a, float b, float x) {
    float t = clampf((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}
// Frame-rate independent exponential damping.
inline float dampf(float cur, float target, float rate, float dt) {
    return lerpf(target, cur, std::exp(-rate * dt));
}
inline Vec3 dampv(const Vec3& cur, const Vec3& target, float rate, float dt) {
    return target.lerp(cur, std::exp(-rate * dt));
}
inline float wrapAngle(float a) {
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    return a;
}
inline float dampAngle(float cur, float target, float rate, float dt) {
    float d = wrapAngle(target - cur);
    return cur + d * (1.f - std::exp(-rate * dt));
}
inline unsigned hashu(unsigned x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
inline float randf(unsigned& s) { s = hashu(s); return (s & 0xFFFFFF) / float(0x1000000); }
