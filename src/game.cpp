// Skater physics, tricks, grinds, bail, scoring, and the articulated model.
#include "game.h"
#include "audio.h"
#include "geom.h"
#include <cmath>
#include <algorithm>

static const float GRAV = 12.5f;
static const float PUSH_ACCEL = 4.2f, PUSH_CAP = 9.0f;

void Particles::emit(const Vec3& p, const Vec3& v, float life, float size,
                     const Vec3& c, int type) {
    ps.push_back({p, v, life, life, size, c, type});
    if (ps.size() > 900) ps.pop_front();
}
void Particles::dust(const Vec3& p, int n, float spread, const Vec3& baseVel) {
    for (int i = 0; i < n; i++) {
        float a = randf(seed) * 6.28f, r = randf(seed) * spread;
        Vec3 v(std::cos(a) * r, randf(seed) * 1.2f + 0.3f, std::sin(a) * r);
        emit(p, v + baseVel * 0.4f, 0.5f + randf(seed) * 0.7f,
             0.10f + randf(seed) * 0.16f, {0.72f, 0.70f, 0.66f}, 0);
    }
}
void Particles::sparks(const Vec3& p, const Vec3& dir, int n) {
    for (int i = 0; i < n; i++) {
        Vec3 v = dir * (2.5f + randf(seed) * 4.f) +
                 Vec3(randf(seed) - 0.5f, randf(seed) * 1.5f, randf(seed) - 0.5f);
        emit(p, v, 0.25f + randf(seed) * 0.3f, 0.035f + randf(seed) * 0.04f,
             {1.0f, 0.75f + randf(seed) * 0.2f, 0.3f}, 1);
    }
}
void Particles::update(float dt) {
    for (auto& p : ps) {
        p.life -= dt;
        p.v.y -= (p.type == 1 ? 9.f : 2.5f) * dt;
        p.v *= (1.f - (p.type == 0 ? 1.4f : 0.2f) * dt);
        p.p += p.v * dt;
        if (p.type == 0) p.size += dt * 0.22f;
    }
    while (!ps.empty() && ps.front().life <= 0) ps.pop_front();
}
void Particles::fill(SpriteBatch& sb) const {
    for (auto& p : ps) {
        float a = clampf(p.life / p.maxLife, 0, 1) * (p.type == 1 ? 1.f : 0.55f);
        sb.sprite(p.p, p.size, p.col, a);
    }
}

static Vec3 twoBoneIK(const Vec3& a, const Vec3& c, float l1, float l2, const Vec3& bendDir) {
    Vec3 ac = c - a;
    float d = std::min(ac.len(), l1 + l2 - 0.001f);
    Vec3 dir = ac.normalized();
    float cosA = clampf((l1 * l1 + d * d - l2 * l2) / (2 * l1 * d), -1.f, 1.f);
    float sinA = std::sqrt(std::max(0.f, 1 - cosA * cosA));
    Vec3 perp = (bendDir - dir * dir.dot(bendDir)).normalized();
    return a + dir * (cosA * l1) + perp * (sinA * l1);
}

static Mat4 capsuleXform(const Vec3& a, const Vec3& b, float r) {
    Vec3 ax = (b - a);
    float len = std::max(ax.len(), 0.01f);
    ax = ax / len;
    Vec3 up = std::abs(ax.y) < 0.95f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    Vec3 rgt = ax.cross(up).normalized();
    Vec3 up2 = rgt.cross(ax);
    Mat4 basis = Mat4::fromBasis(rgt, ax, up2, (a + b) * 0.5f);
    return Mat4::mul(basis, Mat4::scale({r * 2.f, len * 0.55f, r * 2.f}));
}

void Game::init(const TexSet& t) {
    tex = t;
    world.build(t);
    // skater/board meshes
    { MeshBuilder m; m.setColor({1,1,1}); m.capsule({0,-0.5f,0},{0,0.5f,0},0.5f,10); mCapsule = m.build(); }
    { MeshBuilder m; m.setColor({1,1,1}); m.sphere({0,0,0},1.f,12,8); mSphere = m.build(); }
    { MeshBuilder m; m.setColor({1,1,1}); m.box({0,0,0},{0.5f,0.5f,0.5f}); mBox = m.build(); }
    { MeshBuilder m; m.setColor({1,1,1}); m.deck(0.82f, 0.215f, 0.02f, 0.035f); mDeck = m.build(); }
    { MeshBuilder m; m.setColor({1,1,1}); m.deck(0.82f, 0.205f, 0.006f, 0.037f); mDeckTop = m.build(); }
    { // truck: baseplate + hanger + axle along z
        MeshBuilder m; m.setColor({1,1,1});
        m.box({0, -0.015f, 0}, {0.05f, 0.012f, 0.055f});
        m.box({0, -0.045f, 0}, {0.035f, 0.02f, 0.045f});
        m.cylinder({0, -0.055f, -0.105f}, {0, -0.055f, 0.105f}, 0.012f, 8);
        mTruck = m.build();
    }
    { // wheel along z + spoke marker so spin is visible
        MeshBuilder m; m.setColor({1,1,1});
        m.cylinder({0, 0, -0.02f}, {0, 0, 0.02f}, 0.027f, 12);
        m.box({0.012f, 0, 0}, {0.012f, 0.005f, 0.024f});
        mWheel = m.build();
    }
    { // session marker: pole + flag
        MeshBuilder m; m.setColor({1,1,1});
        m.cylinder({0, 0, 0}, {0, 0.6f, 0}, 0.015f, 8);
        mMarker = m.build();
        MeshBuilder f; f.setColor({1,1,1});
        f.box({0.11f, 0.52f, 0}, {0.11f, 0.07f, 0.005f});
        mFlag = f.build();
    }
    s.pos = {-10, 0, 4}; s.yaw = -0.4f;
    // dev/screenshot override: SKATE_SPAWN="x,z,yaw[,y]" places the skater
    if (const char* sp = getenv("SKATE_SPAWN")) {
        int n = sscanf(sp, "%f,%f,%f,%f", &s.pos.x, &s.pos.z, &s.yaw, &s.pos.y);
        if (n < 4) s.pos.y = world.supportHeight(s.pos.x, s.pos.z, 1e9f) + 0.05f; // on top of whatever is there
    }
    s.state = S_GROUND;
}

Mat4 Game::boardMatrix() const {
    const Skater& s = this->s;
    if (s.state == S_BAIL) {
        Vec3 p = s.looseBoard.pos;
        Vec3 f(std::cos(s.yaw), 0, std::sin(s.yaw));
        Mat4 basis = Mat4::fromBasis(f, {0,1,0}, f.cross({0,1,0}), p);
        return Mat4::mul(basis, Mat4::rotateAxis({0,0,1}, s.looseBoard.spin));
    }
    Vec3 f(std::cos(s.yaw), 0, std::sin(s.yaw));
    Vec3 n{0, 1, 0};
    if (s.state == S_GROUND || s.state == S_MANUAL) n = world.supportNormal(s.pos.x, s.pos.z, s.pos.y);
    if (s.state == S_GRIND && s.grindEdge >= 0 && !s.grindBoardslide) {
        // pitch board to the edge slope for 50-50
        auto& e = world.edges[s.grindEdge];
        Vec3 d = (e.b - e.a).normalized();
        if (d.dot(f) < 0) d = -d;
        Vec3 upG = (Vec3(0, 1, 0) - d * d.y).normalized();
        Vec3 rightG = d.cross(upG).normalized();
        Vec3 up2 = rightG.cross(d);
        float boardY = s.pos.y + 0.075f;
        Mat4 basis = Mat4::fromBasis(d, up2, d.cross(up2), {s.pos.x, boardY, s.pos.z});
        Mat4 m = basis;
        // smith/crooked ride one truck: deck tips onto the rail
        if (s.grindType == 2) m = Mat4::mul(m, Mat4::rotateZ(-0.28f));
        if (s.grindType == 3) m = Mat4::mul(m, Mat4::rotateZ(0.30f));
        m = Mat4::mul(m, Mat4::rotateY(s.boardSpin));
        m = Mat4::mul(m, Mat4::rotateX(s.boardRoll));
        return m;
    }
    Vec3 fwd = (f - n * f.dot(n)).normalized();
    Vec3 right = fwd.cross(n).normalized();
    Vec3 up = right.cross(fwd);
    float boardY = s.pos.y + 0.075f; // wheels+trucks+deck height above surface
    // deck length runs along local X -> nose points along fwd
    Mat4 basis = Mat4::fromBasis(fwd, up, fwd.cross(up), {s.pos.x, boardY, s.pos.z});
    // flip anims in board space
    Mat4 m = basis;
    m = Mat4::mul(m, Mat4::rotateY(s.boardSpin));
    m = Mat4::mul(m, Mat4::rotateX(s.boardRoll));
    // manual: pitch the deck on its axle (local Z) around the lifted end
    if (s.state == S_MANUAL)
        m = Mat4::mul(m, Mat4::rotateZ(s.manualDir * (0.30f + 0.35f * s.balance * s.manualDir)));
    return m;
}

void Game::score(int pts, const char* name) {
    s.lineScore += pts * s.mult;
    s.mult = std::min(s.mult + 1, 9);
    s.comboT = 3.0f;
    s.trickName = name;
    s.trickT = 2.2f;
}

void Game::doOllie(float power) {
    g_audio.fire(AudioFx::POP);
    s.vel.y = 2.6f + power * 3.6f;
    s.state = S_AIR;
    s.airTime = 0;
    s.trickWindow = 0.45f;
    s.fallStart = s.pos.y;
    s.flip = F_NONE; s.flipPhase = 0; s.boardRoll = 0; s.boardSpin = 0;
    s.grab = G_NONE; s.spinAmt = 0;
    s.popTimer = 0.18f;
    fx.dust(s.pos, 4, 0.5f);
}

void Game::startFlip(Flip f) {
    if (s.flip != F_NONE || s.state != S_AIR) return;
    s.flip = f;
    s.flipPhase = 0;
    const char* n = "";
    switch (f) {
    case F_KICK: n = "KICKFLIP"; break;
    case F_HEEL: n = "HEELFLIP"; break;
    case F_SHOVE: n = "POP SHOVE-IT"; break;
    case F_TREFLIP: n = "360 FLIP"; break;
    default: break;
    }
    int pts = f == F_TREFLIP ? 500 : (f == F_SHOVE ? 250 : 300);
    s.lineScore += pts * s.mult;
    s.mult = std::min(s.mult + 1, 9);
    s.comboT = 3.0f;
    s.trickName = n; s.trickT = 2.2f;
}

static const char* GRIND_NAMES[] = {"50-50", "BOARDSLIDE", "SMITH GRIND", "CROOKED", "LIPSLIDE"};

void Game::tryGrind(float steer) {
    int e = world.findGrindEdge(s.pos);
    if (e < 0) return;
    auto& g = world.edges[e];
    Vec3 ab = (g.b - g.a);
    Vec3 dir = ab.normalized();
    Vec3 f(std::cos(s.yaw), 0, std::sin(s.yaw));
    float align = std::abs(f.dot(dir));
    s.grindBoardslide = align < 0.55f;
    // steer input at lock-on picks the grind: angled onto the rail = crooked,
    // back truck only = smith, board popped over = lipslide
    if (s.grindBoardslide) s.grindType = steer > 0.3f ? 4 : 1;
    else                   s.grindType = steer > 0.3f ? 2 : (steer < -0.3f ? 3 : 0);
    s.grindEdge = e;
    s.state = S_GRIND;
    s.balance = 0;
    s.grindDist = 0;
    // travel direction along the edge, signed with current velocity
    Vec3 dirV = s.vel.dot(dir) >= 0 ? dir : -dir;
    if (!s.grindBoardslide) s.yaw = std::atan2(dirV.z, dirV.x);
    float t = clampf((s.pos - g.a).dot(ab) / std::max(ab.dot(ab), 1e-5f), 0.f, 1.f);
    s.grindT = t;
    Vec3 c = g.a + ab * t;
    s.pos = {c.x, c.y + 0.02f, c.z};
    s.vel = dirV * std::max(s.vel.len() * 0.9f, 2.f);
    s.trickName = GRIND_NAMES[s.grindType];
    s.trickT = 2.2f;
    fx.sparks(s.pos, dir * -1.f, 6);
    shake = std::max(shake, 0.25f);
}

void Game::endGrind(bool popOut) {
    int base = s.grindType == 4 ? 150 : (s.grindType >= 2 ? 140 : (s.grindType == 1 ? 120 : 100));
    int pts = base + (int)(s.grindDist * 45.f);
    score(pts, GRIND_NAMES[s.grindType]);
    s.grindEdge = -1;
    if (popOut) {
        s.vel.y = 3.6f;
        s.state = S_AIR;
        s.airTime = 0;
        s.trickWindow = 0.35f;
        s.fallStart = s.pos.y;
        s.flip = F_NONE; s.flipPhase = 0; s.boardRoll = 0; s.boardSpin = 0;
        s.grab = G_NONE; s.spinAmt = 0;
    } else {
        s.state = S_AIR;
        s.airTime = 0.15f;
        s.trickWindow = 0.3f;
        s.fallStart = s.pos.y;
        s.flip = F_NONE; s.flipPhase = 0; s.boardRoll = 0; s.boardSpin = 0;
        s.spinAmt = 0;
    }
}

void Game::bail(const char* text) {
    if (s.state == S_BAIL) return;
    g_audio.fire(AudioFx::BAIL);
    s.state = S_BAIL;
    s.bailT = 0;
    s.bailText = text;
    s.flailSeed = (unsigned)(s.pos.x * 977 + s.pos.z * 131 + s.vel.x * 7) + 1;
    s.looseBoard.pos = s.pos + Vec3(0, 0.12f, 0);
    s.looseBoard.vel = s.vel * 0.7f + Vec3(randf(s.flailSeed) * 2 - 1, 2.5f, randf(s.flailSeed) * 2 - 1);
    s.looseBoard.spin = 0;
    s.lineScore = 0;
    s.mult = 1;
    s.grindEdge = -1;
    shake = std::max(shake, 0.8f);
    flash = 0.5f;
    fx.dust(s.pos, 14, 2.5f, s.vel * 0.3f);
}

void Game::endManual() {
    s.state = S_GROUND;
    s.manualDir = 0;
    s.balance = 0;
    fx.dust(s.pos, 3, 0.5f);
    // exiting the manual is the landing — bank the line
    if (s.lineScore > 0) { s.totalScore += s.lineScore; s.lineScore = 0; s.mult = 1; }
}

void Game::land(bool bankLine) {
    float impact = -s.vel.y;
    bool bad = (s.flip != F_NONE && s.flipPhase < 0.8f);
    bool upsideDown = std::fmod(std::abs(s.boardRoll), 6.28f) > 2.2f &&
                      std::fmod(std::abs(s.boardRoll), 6.28f) < 4.1f;
    if (bad) { bail("UNDER-ROTATED"); return; }
    if (upsideDown) { bail("LANDED PRIMO"); return; }
    if (impact > 17.f) { bail("SLAMMED HARD"); return; }
    // air spins: nearest 180 counts; badly off-axis landings clip
    float spinDeg = s.spinAmt * 57.2958f;
    int spins = (int)std::lround(spinDeg / 180.f);
    float resid = std::abs(spinDeg - spins * 180.f);
    if (resid > 70.f && s.speed > 4.f) { bail("UNDER-ROTATED"); return; }
    s.state = S_GROUND;
    s.pos.y = world.supportHeight(s.pos.x, s.pos.z, s.pos.y + 0.3f);
    // landing on a downslope converts fall speed into forward speed
    {
        Vec3 n = world.supportNormal(s.pos.x, s.pos.z, s.pos.y);
        Vec3 dh(n.x, 0, n.z);
        float sl = dh.len();
        Vec3 fwd(std::cos(s.yaw), 0, std::sin(s.yaw));
        if (sl > 0.15f && (fwd.x * n.x + fwd.z * n.z) / sl > 0.3f) {
            float tot = s.vel.len();
            float sp = std::max(std::hypot(s.vel.x, s.vel.z), tot * 0.8f);
            s.vel.x = fwd.x * sp; s.vel.z = fwd.z * sp;
        }
    }
    s.vel.y = 0;
    s.flip = F_NONE; s.grab = G_NONE;
    if (impact > 7.f) { shake = std::max(shake, 0.55f); fx.dust(s.pos, 12, 1.6f, s.vel * 0.2f); g_audio.fire(AudioFx::LANDHARD); }
    else if (impact > 2.5f) { shake = std::max(shake, 0.2f); fx.dust(s.pos, 5, 0.9f); g_audio.fire(AudioFx::LAND); }
    if (std::abs(spins) >= 1) {
        char buf[80];
        std::snprintf(buf, sizeof buf, "%s%s %d",
                      s.trickName.empty() ? "" : (s.trickName + " ").c_str(),
                      spins > 0 ? "FS" : "BS", std::abs(spins) * 180);
        score(120 * std::abs(spins), buf);
    }
    if (s.airTime > 0.5f && s.trickName.empty()) score(50, "OLLIE");
    // bank the line
    if (bankLine && s.lineScore > 0) { s.totalScore += s.lineScore; s.lineScore = 0; s.mult = 1; }
}

void Game::setMarkerHere() {
    hasMarker = true;
    markerPos = s.pos;
    markerYaw = s.yaw;
    s.trickName = "SESSION MARKER SET"; s.trickT = 1.4f;
}
void Game::teleportMarker() {
    if (!hasMarker) { s.trickName = "NO MARKER SET"; s.trickT = 1.2f; return; }
    s.pos = markerPos;
    s.yaw = markerYaw;
    s.vel = {0, 0, 0};
    s.state = S_GROUND;
    s.lineScore = 0; s.mult = 1;
    flash = 0.25f;
}

void Game::teleportSpot(int i) {
    static const struct { float x, z, yaw; const char* name; } SPOTS[] = {
        {-10.f,    4.f, -0.4f,   "PLAZA"},
        {-34.f,  -37.f,  1.5708f, "THE BOWL"},
        { 53.f,   40.f,  0.f,     "MEGA RAMP"},
        { 40.5f, -34.f,  0.f,     "MINI RAMP"},
        { 20.f,   -9.f,  1.5708f, "STAIR SET"},
    };
    if (i < 0 || i >= 5) return;
    auto& sp = SPOTS[i];
    s.pos = {sp.x, 0, sp.z};
    s.pos.y = world.supportHeight(sp.x, sp.z, 1e9f) + 0.05f;
    s.yaw = sp.yaw;
    s.vel = {0, 0, 0};
    s.state = S_GROUND;
    s.lineScore = 0; s.mult = 1;
    s.trickName = std::string("SPOT: ") + sp.name;
    s.trickT = 1.6f;
    flash = 0.25f;
}

void Game::update(const Input& in, float dt) {
    Skater& s = this->s;
    if (getenv("SKATE_DEBUGPOS")) {           // dev aid: dump state ~2x/sec
        static int dbg = 0;
        if (++dbg % 30 == 0)
            fprintf(stderr, "POS %.1f %.1f %.1f st%d spd %.1f yaw %.2f sup %.2f\n",
                    s.pos.x, s.pos.y, s.pos.z, (int)s.state, s.speed, s.yaw,
                    world.supportHeight(s.pos.x, s.pos.z, s.pos.y));
    }
    if (in.setMarker) setMarkerHere();
    if (in.goMarker) teleportMarker();
    if (in.spot >= 0) teleportSpot(in.spot);

    s.speed = std::hypot(s.vel.x, s.vel.z);
    s.wheelSpin += s.speed * dt / 0.027f;
    g_audio.roll = (s.state == S_GROUND || s.state == S_MANUAL) ? std::min(s.speed / 16.f, 0.7f) : 0.f;
    g_audio.wind = clampf((s.speed - 8.f) / 30.f, 0.f, 0.5f);
    g_audio.grind = s.state == S_GRIND ? 0.5f : 0.f;
    s.pushAnim = std::max(0.f, s.pushAnim - dt);
    s.trickT = std::max(0.f, s.trickT - dt);
    if (s.trickT <= 0) s.trickName.clear();
    s.popTimer = std::max(0.f, s.popTimer - dt);
    if (s.comboT > 0) { s.comboT -= dt; if (s.comboT <= 0 && s.lineScore > 0) { s.totalScore += s.lineScore; s.lineScore = 0; s.mult = 1; } }
    shake = dampf(shake, 0, 6, dt);
    flash = dampf(flash, 0, 7, dt);
    s.crouchVis = dampf(s.crouchVis, s.crouch, 12, dt);

    Vec3 f(std::cos(s.yaw), 0, std::sin(s.yaw));
    float steer = (in.left ? 1.f : 0.f) - (in.right ? 1.f : 0.f);

    switch (s.state) {
    case S_GROUND: {
        // crouch charge
        s.crouch = dampf(s.crouch, in.crouch ? 1.f : 0.f, in.crouch ? 8.f : 14.f, dt);
        static float prevCrouchKey = 0;
        bool released = prevCrouchKey > 0.5f && !in.crouch;
        prevCrouchKey = in.crouch ? 1 : 0;
        if ((released && s.crouch > 0.15f) || in.jump) {
            doOllie(in.jump ? 0.55f : s.crouch);
            s.crouch = 0;
            break;
        }
        // manual entry while rolling
        if ((in.manual || in.noseManual) && s.speed > 1.f) {
            s.manualDir = in.noseManual ? -1 : 1;
            s.state = S_MANUAL;
            s.balance = 0;
            s.trickName = s.manualDir > 0 ? "MANUAL" : "NOSE MANUAL";
            s.trickT = 1.2f;
            break;
        }
        // carving: heading turns, velocity follows the deck (rides fakie after
        // a landed 180 until you carve back around)
        float yawRate = steer * (1.6f + s.speed * 0.12f);
        s.yaw += yawRate * dt;
        bool fakie = false;
        if (s.speed > 0.4f) {
            float vh = std::atan2(s.vel.z, s.vel.x);
            float dv = std::remainder(s.yaw - vh, 6.2832f);
            fakie = std::abs(dv) > 1.5708f;
            if (!fakie) {
                float a = vh + clampf(dv, -3.5f * dt, 3.5f * dt);
                s.vel.x = std::cos(a) * s.speed;
                s.vel.z = std::sin(a) * s.speed;
            }
        }
        // pushing
        if (in.push && s.pushAnim <= 0 && s.speed < PUSH_CAP) s.pushAnim = 0.75f;
        if (s.pushAnim > 0) s.vel += f * (PUSH_ACCEL * dt * (fakie ? -1.f : 1.f));
        s.lean = dampf(s.lean, steer * 0.5f, 8, dt);
        float scrub = std::abs(yawRate) * 0.05f;
        // friction + slope
        Vec3 n = world.supportNormal(s.pos.x, s.pos.z, s.pos.y);
        Vec3 g(0, -GRAV, 0);
        Vec3 a = g - n * g.dot(n);           // gravity along surface
        s.vel += a * dt;
        float fr = 0.4f + scrub;
        if (in.brake) fr += 3.0f;
        s.vel.x -= s.vel.x * fr * dt;
        s.vel.z -= s.vel.z * fr * dt;
        if (s.speed > PUSH_CAP * 1.02f && s.pushAnim > 0) {
            Vec3 hv = s.vel * (1.f / std::max(s.speed, 0.01f));
            s.vel.x = hv.x * std::min(s.speed, 22.f); s.vel.z = hv.z * std::min(s.speed, 22.f);
        }
        s.vel.y = 0;
        // move
        Vec3 np = s.pos + Vec3(s.vel.x, 0, s.vel.z) * dt;
        Vec3 sn = world.supportNormal(s.pos.x, s.pos.z, s.pos.y);
        float slope = -(s.vel.x * sn.x + s.vel.z * sn.z) / std::max(sn.y, 0.3f);
        bool wall = world.collideWalls(np, s.vel);
        s.pos.x = np.x; s.pos.z = np.z;
        float ground = world.supportHeight(s.pos.x, s.pos.z, s.pos.y + 0.3f);
        // on a steep transition (slope > ~45 deg) a wall means a lip: launch
        // up it instead of slamming into whatever sits behind the coping
        if (wall && slope > 1.0f) {
            float keep = std::min(1.f / std::sqrt(1 + slope * slope) + 0.3f, 1.f);
            s.vel.y = std::min(s.speed * slope, 10.f);
            s.vel.x *= keep; s.vel.z *= keep;
            s.state = S_AIR;
            s.airTime = 0; s.fallStart = s.pos.y;
            s.trickWindow = 0.25f;
            s.flip = F_NONE; s.flipPhase = 0; s.boardRoll = 0; s.boardSpin = 0;
            s.grab = G_NONE; s.spinAmt = 0;
            break;
        }
        if (wall && s.speed > 7.f) { bail("SKATED INTO A WALL"); break; }
        if (ground < s.pos.y - 0.3f) {
            // rolled off an edge — keep the slope's vertical velocity component
            // (launch off kicker lips / air out of quarter pipes)
            Vec3 n = world.supportNormal(s.pos.x - s.vel.x * dt, s.pos.z - s.vel.z * dt, s.pos.y);
            float t = -(s.vel.x * n.x + s.vel.z * n.z) / std::max(n.y, 0.3f);
            float keep = std::min(1.f / std::sqrt(1 + t * t) + 0.3f, 1.f);
            s.vel.y = clampf(s.speed * t, -10.f, 10.f);
            s.vel.x *= keep; s.vel.z *= keep;
            s.state = S_AIR;
            s.airTime = 0; s.fallStart = s.pos.y;
            s.trickWindow = 0.25f;
            s.flip = F_NONE; s.flipPhase = 0; s.boardRoll = 0; s.boardSpin = 0;
            s.grab = G_NONE; s.spinAmt = 0;
        } else {
            s.pos.y = ground;
        }
        break;
    }
    case S_AIR: {
        s.airTime += dt;
        s.trickWindow -= dt;
        s.vel.y -= GRAV * dt;
        // trick input
        if (s.flip == F_NONE) {
            if (in.trick[0]) startFlip(F_KICK);
            else if (in.trick[1]) startFlip(F_HEEL);
            else if (in.trick[2]) startFlip(F_SHOVE);
            else if (in.trick[3]) startFlip(F_TREFLIP);
        }
        // flip animation
        if (s.flip != F_NONE && s.flipPhase < 1.f) {
            s.flipPhase = std::min(1.f, s.flipPhase + dt * 2.4f);
            float ph = s.flipPhase * (1.6f - 0.6f * s.flipPhase); // ease
            switch (s.flip) {
            case F_KICK:    s.boardRoll = -ph * 6.283f; break;
            case F_HEEL:    s.boardRoll = ph * 6.283f; break;
            case F_SHOVE:   s.boardSpin = ph * 3.1416f; break;
            case F_TREFLIP: s.boardRoll = -ph * 6.283f; s.boardSpin = ph * 6.283f; break;
            default: break;
            }
        }
        // grabs
        s.grab = G_NONE;
        if (in.grab[0]) s.grab = G_INDY;
        else if (in.grab[1]) s.grab = G_MELON;
        else if (in.grab[2]) s.grab = G_NOSE;
        else if (in.grab[3]) s.grab = G_TAIL;
        if (s.grab != G_NONE) {
            const char* gn = s.grab == G_INDY ? "INDY" : s.grab == G_MELON ? "MELON" :
                             s.grab == G_NOSE ? "NOSEGRAB" : "TAILGRAB";
            s.trickName = gn; s.trickT = 0.4f;
            s.lineScore += (int)(70 * dt * s.mult);
        }
        // aerial rotation (A/D): FS/BS 180s, 360s, ... scored at landing
        s.yaw += steer * 4.4f * dt;
        s.spinAmt += steer * 4.4f * dt;
        // move
        Vec3 np = s.pos + s.vel * dt;
        bool wall = world.collideWalls(np, s.vel);
        s.pos = np;
        // rising past a lip: walls only push, so vert airs clear the coping
        if (wall && s.vel.y <= 0.5f) { bail(s.speed > 6.f ? "SLAMMED INTO A WALL" : "CLIPPED A WALL"); break; }
        float ground = world.supportHeight(s.pos.x, s.pos.z, s.pos.y + 0.25f);
        if (s.vel.y <= 0) {
            // try grind first when crossing an edge lip — but only after a real
            // pop, a long fall, or when actually traveling along the edge, so a
            // plain roll-off/drop-in doesn't lock onto coping
            int ge = world.findGrindEdge(s.pos);
            if (ge >= 0 && s.pos.y < ground + 0.45f + 0.35f) {
                auto& e = world.edges[ge];
                Vec3 d = (e.b - e.a).normalized();
                bool locked = s.popTimer > 0 || s.airTime > 0.4f ||
                              std::abs(s.vel.x * d.x + s.vel.z * d.z) > 1.0f;
                if (locked) { tryGrind(steer); break; }
            }
            if (s.pos.y <= ground + 0.02f) {
                // landing into a held manual keeps the line alive
                if (in.manual || in.noseManual) {
                    land(false);
                    if (s.state == S_GROUND) {
                        s.manualDir = in.noseManual ? -1 : 1;
                        s.state = S_MANUAL;
                        s.balance = 0;
                        s.trickName = s.manualDir > 0 ? "MANUAL" : "NOSE MANUAL";
                        s.trickT = 1.2f;
                    }
                } else {
                    land();
                }
            }
        }
        break;
    }
    case S_MANUAL: {
        // released the key or ran out of speed: wheels drop, line banks
        if (!(in.manual || in.noseManual) || s.speed < 0.3f) { endManual(); break; }
        int dir = s.manualDir;
        // balance creeps toward the lifted axle + noise; W lifts, S drops
        s.balance += dir * 0.22f * dt + (randf(s.flailSeed) - 0.5f) * 1.0f * dt;
        if (in.push)  s.balance += 1.9f * dt;
        if (in.brake) s.balance -= 1.9f * dt;
        if (s.balance * dir > 1.f) { bail(dir > 0 ? "LOOPED OUT" : "OVER THE NOSE"); break; }
        if (s.balance * dir < -0.35f) { endManual(); break; } // wheels touched back down
        // hop out onto the line
        if (in.jump) { doOllie(0.4f); break; }
        // steering + rolling (no pushing while up on the trucks)
        float yawRate = steer * (0.9f + s.speed * 0.05f);
        s.yaw += yawRate * dt;
        s.lean = dampf(s.lean, steer * 0.3f + s.balance * -0.15f * dir, 6, dt);
        Vec3 n = world.supportNormal(s.pos.x, s.pos.z, s.pos.y);
        Vec3 a = Vec3(0, -GRAV, 0) - n * Vec3(0, -GRAV, 0).dot(n);
        s.vel += a * dt;
        float fr = 0.30f;
        s.vel.x -= s.vel.x * fr * dt;
        s.vel.z -= s.vel.z * fr * dt;
        s.vel.y = 0;
        // move (same wall/ground logic as rolling)
        Vec3 np = s.pos + Vec3(s.vel.x, 0, s.vel.z) * dt;
        Vec3 sn = world.supportNormal(s.pos.x, s.pos.z, s.pos.y);
        float slope = -(s.vel.x * sn.x + s.vel.z * sn.z) / std::max(sn.y, 0.3f);
        bool wall = world.collideWalls(np, s.vel);
        s.pos.x = np.x; s.pos.z = np.z;
        float ground = world.supportHeight(s.pos.x, s.pos.z, s.pos.y + 0.3f);
        if (slope > 0.7f) { endManual(); break; } // transitions drop the manual
        if (wall && s.speed > 7.f) { bail("SKATED INTO A WALL"); break; }
        if (ground < s.pos.y - 0.3f) {
            Vec3 on = world.supportNormal(s.pos.x - s.vel.x * dt, s.pos.z - s.vel.z * dt, s.pos.y);
            s.vel.y = clampf(-(s.vel.x * on.x + s.vel.z * on.z) / std::max(on.y, 0.3f), -10.f, 10.f);
            s.state = S_AIR;
            s.airTime = 0; s.fallStart = s.pos.y;
            s.trickWindow = 0.25f;
            s.flip = F_NONE; s.flipPhase = 0; s.boardRoll = 0; s.boardSpin = 0;
            s.grab = G_NONE; s.spinAmt = 0;
            break;
        }
        s.pos.y = ground;
        // ticking score keeps the combo window open
        s.lineScore += (int)(60.f * dt * s.mult);
        s.comboT = 3.f;
        s.trickName = dir > 0 ? "MANUAL" : "NOSE MANUAL";
        s.trickT = 0.5f;
        break;
    }
    case S_GRIND: {
        auto& e = world.edges[s.grindEdge];
        Vec3 ab = e.b - e.a;
        Vec3 dir = ab.normalized();
        float sp = s.vel.dot(dir);   // signed speed along edge param
        Vec3 dirV = sp >= 0 ? dir : -dir;
        sp -= sp * 0.4f * dt; // grind friction
        if (in.jump) { endGrind(true); break; }
        s.vel = dir * sp;
        s.grindT += sp * dt / std::max(ab.len(), 0.01f);
        s.grindDist += std::abs(sp) * dt;
        // balance: drifts, corrected by steer input
        s.balance += (randf(s.flailSeed) - 0.5f) * 1.6f * dt + steer * 1.4f * dt;
        if (std::abs(s.balance) > 1.f) { bail("SLIPPED OUT"); break; }
        if (s.grindT >= 1.f || s.grindT <= 0.f) { endGrind(false); break; }
        Vec3 c = e.a + ab * s.grindT;
        s.pos = {c.x, c.y + 0.02f, c.z};
        if (e.rail && s.speed > 1.f) fx.sparks(s.pos - dirV * 0.1f, dirV * -1.f, 2);
        s.lean = dampf(s.lean, s.balance * 0.4f, 6, dt);
        break;
    }
    case S_BAIL: {
        s.bailT += dt;
        // body tumbles
        s.vel.y -= GRAV * dt;
        s.vel *= (1.f - 1.2f * dt);
        Vec3 np = s.pos + s.vel * dt;
        world.collideWalls(np, s.vel);
        s.pos = np;
        float ground = world.supportHeight(s.pos.x, s.pos.z, s.pos.y + 0.5f);
        if (s.pos.y < ground + 0.05f) {
            s.pos.y = ground + 0.05f;
            s.vel.y = std::abs(s.vel.y) > 2.f ? -s.vel.y * 0.3f : 0;
            s.vel.x *= 0.6f; s.vel.z *= 0.6f;
            if (randf(s.flailSeed) < 0.3f) fx.dust(s.pos, 2, 0.8f);
        }
        // loose board ballistic
        Board& b = s.looseBoard;
        b.vel.y -= GRAV * dt;
        b.pos += b.vel * dt;
        b.spin += dt * 9.f;
        float bg = world.supportHeight(b.pos.x, b.pos.z, b.pos.y + 0.3f);
        if (b.pos.y < bg + 0.03f) {
            b.pos.y = bg + 0.03f;
            b.vel.y = std::abs(b.vel.y) > 1.5f ? -b.vel.y * 0.35f : 0;
            b.vel.x *= 0.7f; b.vel.z *= 0.7f;
        }
        if (s.bailT > 1.5f) {
            // stand back up where we fell
            s.state = S_GROUND;
            s.pos.y = world.supportHeight(s.pos.x, s.pos.z, s.pos.y + 1.f);
            s.vel = {0, 0, 0};
            s.balance = 0; s.crouch = 0; s.lean = 0;
        }
        break;
    }
    }
    fx.update(dt);
}

void Game::drawBoard(std::vector<DrawItem>& out, const Mat4& bm, bool loose) const {
    Mat4 truckOff[2] = {
        Mat4::mul(bm, Mat4::translation({0.26f, 0, 0})),
        Mat4::mul(bm, Mat4::translation({-0.26f, 0, 0})),
    };
    Material deckMat{tex.deckArt, 1.f, {1,1,1}, 0.4f, 40.f, 1.f, 0.1f, 0.f};
    Material gripMat{tex.grip, 1.4f, {1,1,1}, 0.12f, 20.f, 1.f, 0.f, 0.f};
    Material truckMat{tex.metal, 1.f, {0.85f,0.86f,0.9f}, 1.1f, 120.f, 1.f, 0.9f, 0.f};
    Material wheelMat{0, 1.f, {0.93f,0.88f,0.75f}, 0.5f, 60.f, 1.f, 0.15f, 0.f};
    out.push_back({&mDeck, bm, deckMat});
    out.push_back({&mDeckTop, Mat4::mul(bm, Mat4::translation({0, 0.012f, 0})), gripMat});
    for (int tI = 0; tI < 2; tI++) {
        out.push_back({&mTruck, truckOff[tI], truckMat});
        for (int wS = -1; wS <= 1; wS += 2) {
            Mat4 wm = Mat4::mul(truckOff[tI], Mat4::translation({0, -0.055f, 0.115f * wS}));
            wm = Mat4::mul(wm, Mat4::rotateAxis({0, 0, 1}, s.wheelSpin + tI * 1.7f + wS));
            out.push_back({&mWheel, wm, wheelMat});
        }
    }
    (void)loose;
}

void Game::drawSkater(std::vector<DrawItem>& out) const {
    const Skater& s = this->s;
    Mat4 bm = boardMatrix();
    if (s.state != S_BAIL) drawBoard(out, bm, false);

    // ---- pose ----
    Vec3 f(std::cos(s.yaw), 0, std::sin(s.yaw));
    Vec3 rgt(-std::sin(s.yaw), 0, std::cos(s.yaw));
    float cr = s.crouchVis;
    bool air = (s.state == S_AIR);
    bool bail_ = (s.state == S_BAIL);
    float tuck = air ? clampf(s.airTime * 3.f, 0, 1) * 0.55f : 0;
    if (s.flip != F_NONE) tuck = 0.7f;
    float hipH = (0.95f - cr * 0.32f) + (air ? 0.15f : 0.f) - tuck * 0.10f;
    Vec3 hipPos = s.pos + Vec3(0, bail_ ? 0.35f : hipH, 0);
    if (bail_) {
        // ragdoll-ish tumble: body pitched over, flailing
        float sp = s.bailT * 6.f;
        hipPos = s.pos + Vec3(0, 0.3f + 0.1f * std::sin(sp * 2.f), 0);
    }
    Vec3 bodyUp(0, 1, 0);
    Vec3 leanF = f * (cr * 0.10f + (air ? 0.02f : 0.f)) + rgt * (s.lean * 0.12f);
    if (s.state == S_GRIND) leanF = leanF + rgt * (s.balance * 0.1f);
    if (s.state == S_MANUAL) leanF = leanF - f * (0.14f * s.manualDir) + rgt * (s.balance * 0.06f);
    if (bail_) leanF = f * 0.6f + rgt * 0.3f;
    Vec3 neck = hipPos + bodyUp * 0.46f + leanF;
    Vec3 head = neck + bodyUp * 0.17f;

    // feet on board (or flailing)
    Vec3 footL, footR;
    if (bail_) {
        unsigned sd = s.flailSeed;
        footL = hipPos + Vec3(randf(sd) * 1.2f - 0.6f, -0.2f + randf(sd) * 0.5f, randf(sd) * 1.2f - 0.6f);
        footR = hipPos + Vec3(randf(sd) * 1.2f - 0.6f, -0.3f + randf(sd) * 0.6f, randf(sd) * 1.2f - 0.6f);
    } else {
        Vec3 up(0, 1, 0);
        footL = bm.transformPoint({0.24f, 0.028f, 0.025f});
        footR = bm.transformPoint({-0.24f, 0.028f, -0.025f});
        footL = footL + up * 0.02f; footR = footR + up * 0.02f;
        if (s.pushAnim > 0) {
            // back foot pushes off behind
            float ph = s.pushAnim / 0.75f;
            footR = s.pos - f * (0.35f + 0.5f * (1 - ph)) + Vec3(0, 0.03f + 0.15f * ph, 0) - rgt * 0.12f;
        }
        if (s.grab == G_NOSE) footL = footL + f * -0.06f;
    }

    // legs via IK
    Vec3 hipL = hipPos - rgt * 0.10f, hipR = hipPos + rgt * 0.10f;
    Vec3 ankL = footL + Vec3(0, 0.085f, 0), ankR = footR + Vec3(0, 0.085f, 0);
    Vec3 kneeL = twoBoneIK(hipL, ankL, 0.42f, 0.42f, f * 0.7f - rgt * 0.15f);
    Vec3 kneeR = twoBoneIK(hipR, ankR, 0.42f, 0.42f, f * 0.7f + rgt * 0.15f);

    Material shirt{tex.fabric, 1.f, {1,1,1}, 0.12f, 16.f, 0.9f, 0.f, 0.f};
    Material denim{tex.denim, 1.f, {1,1,1}, 0.1f, 14.f, 0.9f, 0.f, 0.f};
    Material skin{0, 1.f, {0.78f, 0.58f, 0.44f}, 0.15f, 20.f, 0.9f, 0.f, 0.f};
    Material shoe{0, 1.f, {0.9f, 0.9f, 0.88f}, 0.3f, 40.f, 0.9f, 0.05f, 0.f};
    Material sole{0, 1.f, {0.22f, 0.18f, 0.15f}, 0.2f, 30.f, 0.9f, 0.f, 0.f};
    Material cap{0, 1.f, {0.12f, 0.14f, 0.18f}, 0.2f, 30.f, 0.9f, 0.f, 0.f};

    // thighs + shins + shoes
    out.push_back({&mCapsule, capsuleXform(hipL, kneeL, 0.082f), denim});
    out.push_back({&mCapsule, capsuleXform(kneeL, ankL, 0.062f), denim});
    out.push_back({&mCapsule, capsuleXform(hipR, kneeR, 0.082f), denim});
    out.push_back({&mCapsule, capsuleXform(kneeR, ankR, 0.062f), denim});
    for (int ft = 0; ft < 2; ft++) {
        Vec3 fp = ft == 0 ? footL : footR;
        float ry = s.yaw + (ft == 0 ? 0.15f : -0.15f);
        // gum sole + white upper
        Mat4 so = Mat4::translation(fp + Vec3(0, 0.012f, 0)) * Mat4::rotateY(ry);
        out.push_back({&mBox, Mat4::mul(so, Mat4::scale({0.29f, 0.028f, 0.115f})), sole});
        Mat4 sm = Mat4::translation(fp + Vec3(0, 0.055f, 0)) * Mat4::rotateY(ry);
        out.push_back({&mBox, Mat4::mul(sm, Mat4::scale({0.27f, 0.075f, 0.105f})), shoe});
    }
    // hips (denim) so the shirt reads as a shirt over pants
    out.push_back({&mCapsule, capsuleXform(hipPos - Vec3(0, 0.10f, 0), hipPos + Vec3(0, 0.07f, 0), 0.115f), denim});
    // torso + head + cap
    out.push_back({&mCapsule, capsuleXform(hipPos + Vec3(0, 0.05f, 0), neck, 0.125f), shirt});
    out.push_back({&mSphere, Mat4::mul(Mat4::translation(head),
                   Mat4::scale({0.115f, 0.125f, 0.115f})), skin});
    Mat4 capm = Mat4::translation(head + Vec3(0, 0.10f, 0)) * Mat4::rotateY(s.yaw);
    out.push_back({&mSphere, Mat4::mul(capm, Mat4::scale({0.125f, 0.075f, 0.125f})), cap});
    out.push_back({&mBox, Mat4::mul(Mat4::translation(head + f * 0.12f + Vec3(0, 0.095f, 0)),
                   Mat4::rotateY(s.yaw) * Mat4::scale({0.16f, 0.02f, 0.12f})), cap});

    // arms
    Vec3 shoL = neck - rgt * 0.20f + Vec3(0, -0.04f, 0);
    Vec3 shoR = neck + rgt * 0.20f + Vec3(0, -0.04f, 0);
    Vec3 handL, handR;
    if (bail_) {
        unsigned sd = s.flailSeed + 5;
        handL = shoL + Vec3(randf(sd) - 0.5f, randf(sd) - 0.6f, randf(sd) - 0.5f) * 1.1f;
        handR = shoR + Vec3(randf(sd) - 0.5f, randf(sd) - 0.7f, randf(sd) - 0.5f) * 1.1f;
    } else if (s.grab != G_NONE) {
        Vec3 gp;
        switch (s.grab) {
        case G_INDY:  gp = bm.transformPoint({0.10f, 0.0f, -0.11f}); break;
        case G_MELON: gp = bm.transformPoint({-0.05f, 0.0f, 0.11f}); break;
        case G_NOSE:  gp = bm.transformPoint({0.40f, 0.0f, 0.0f}); break;
        default:      gp = bm.transformPoint({-0.40f, 0.0f, 0.0f}); break;
        }
        handR = gp; // trailing hand grabs
        handL = shoL + Vec3(-0.25f, -0.28f, 0.15f) + f * 0.1f;
    } else {
        float swing = s.pushAnim > 0 ? std::sin((1 - s.pushAnim / 0.75f) * 6.28f) * 0.3f : 0;
        float outAmt = 0.22f + std::abs(s.lean) * 0.35f +
                       (s.state == S_GRIND ? 0.15f : 0) + (s.state == S_MANUAL ? 0.22f : 0);
        handL = shoL + (rgt * -outAmt) + Vec3(0, -0.30f + cr * -0.05f, 0) + f * (0.08f + swing);
        handR = shoR + (rgt * outAmt) + Vec3(0, -0.30f + cr * -0.05f, 0) + f * (0.05f - swing);
        if (s.state == S_GRIND) {
            float wb = s.balance * 0.35f;
            handL = handL + Vec3(0, wb, 0) + f * 0.12f;
            handR = handR + Vec3(0, -wb, 0) + f * -0.05f;
        }
        if (cr > 0.5f) { // hands toward knees when crouched
            handL = handL.lerp(kneeL + Vec3(0, 0.05f, 0), 0.35f);
            handR = handR.lerp(kneeR + Vec3(0, 0.05f, 0), 0.35f);
        }
    }
    Vec3 elbL = twoBoneIK(shoL, handL, 0.30f, 0.27f, Vec3(0, -1, 0) - f * 0.4f - rgt * 0.4f);
    Vec3 elbR = twoBoneIK(shoR, handR, 0.30f, 0.27f, Vec3(0, -1, 0) - f * 0.4f + rgt * 0.4f);
    out.push_back({&mCapsule, capsuleXform(shoL, elbL, 0.055f), shirt});
    out.push_back({&mCapsule, capsuleXform(elbL, handL, 0.045f), skin});
    out.push_back({&mCapsule, capsuleXform(shoR, elbR, 0.055f), shirt});
    out.push_back({&mCapsule, capsuleXform(elbR, handR, 0.045f), skin});

    // bail: draw loose board separately
    if (s.state == S_BAIL) drawBoard(out, bm, true);

    // session marker
    if (hasMarker) {
        Mat4 mm = Mat4::translation(markerPos + Vec3(0, 0.02f, 0));
        out.push_back({&mMarker, mm, {0, 1.f, {0.2f, 0.2f, 0.2f}, 0.4f, 30.f, 1.f, 0.3f, 0.f}});
        Mat4 fm = Mat4::mul(Mat4::translation(markerPos), Mat4::rotateY(markerYaw));
        out.push_back({&mFlag, fm, {0, 1.f, {1.0f, 0.45f, 0.1f}, 0.3f, 30.f, 1.f, 0.f, 0.6f}});
    }
}

void Game::collectDraws(std::vector<DrawItem>& out) const {
    drawSkater(out);
}
