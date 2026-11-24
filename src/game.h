// Game state: skater physics, trick/grind systems, bail, scoring, session marker,
// particles, and the animated skater + board model.
#pragma once
#include "math3d.h"
#include "world.h"
#include "glcore.h"
#include "texgen.h"
#include <vector>
#include <string>
#include <deque>

struct Input {
    bool push = false, brake = false;
    bool left = false, right = false;      // carve (A/D or arrows on ground)
    bool crouch = false;                   // hold to load ollie (Ctrl / C)
    bool jump = false;                     // quick pop / exit grind (Space)
    bool trick[4] = {false, false, false, false}; // arrows: L,R,Down,Up while airborne
    bool grab[4] = {false, false, false, false};  // Q,E,F,G in air
    bool manual = false, noseManual = false;      // R / V hold
    bool setMarker = false, goMarker = false;
};

struct Particle {
    Vec3 p, v;
    float life, maxLife, size;
    Vec3 col;
    int type; // 0 dust, 1 spark
};
struct Particles {
    std::deque<Particle> ps;
    unsigned seed = 5;
    void emit(const Vec3& p, const Vec3& v, float life, float size, const Vec3& c, int type);
    void dust(const Vec3& p, int n, float spread, const Vec3& baseVel = {0,0,0});
    void sparks(const Vec3& p, const Vec3& dir, int n);
    void update(float dt);
    void fill(SpriteBatch& sb) const;
};

enum SState { S_GROUND, S_AIR, S_GRIND, S_MANUAL, S_BAIL };
enum Flip { F_NONE, F_KICK, F_HEEL, F_SHOVE, F_TREFLIP };
enum Grab { G_NONE, G_INDY, G_MELON, G_NOSE, G_TAIL };

struct Board {
    Vec3 pos, vel;          // only used ballistically during bail
    float spin = 0;
};

struct Skater {
    Vec3 pos{0, 0, 0};      // board center
    Vec3 vel{0, 0, 0};
    float yaw = 0;          // heading (rad, forward = +x at yaw 0? we use -z? see below)
    int state = S_GROUND;
    float crouch = 0;       // 0..1 ollie charge
    float crouchVis = 0;    // smoothed for pose
    float lean = 0;         // carve lean
    // air/trick
    float airTime = 0;
    float trickWindow = 0;  // time after pop to register a flip
    float spinAmt = 0;      // radians rotated during current air
    Flip flip = F_NONE;
    float flipPhase = 0;    // 0..1 progress of flip anim
    float boardRoll = 0, boardSpin = 0; // displayed flip angles
    Grab grab = G_NONE;
    float popTimer = 0;
    float fallStart = 0;    // y at leaving ground
    // grind
    int grindEdge = -1;
    float grindT = 0;       // param along edge
    float balance = 0;      // -1..1, |1| = bail
    float grindDist = 0;
    bool grindBoardslide = false;
    int grindType = 0;      // 0=50-50 1=boardslide 2=smith 3=crooked 4=lipslide
    int manualDir = 0;      // +1 manual (back trucks), -1 nose manual
    // bail
    float bailT = 0;
    std::string bailText;
    Board looseBoard;
    unsigned flailSeed = 1;
    // score
    int lineScore = 0, totalScore = 0, mult = 1;
    float comboT = 0;
    std::string trickName;
    float trickT = 0;
    bool pendingLand = false;
    float speed = 0;
    float pushAnim = 0;
    float wheelSpin = 0;
};

struct Game {
    World world;
    TexSet tex;
    Skater s;
    Particles fx;
    // marker
    bool hasMarker = false;
    Vec3 markerPos;
    float markerYaw = 0;
    // camera fx
    float shake = 0, flash = 0;
    float fovBoost = 0;
    // skater/board meshes (built once)
    Mesh mCapsule, mSphere, mBox, mDeck, mDeckTop, mTruck, mWheel, mMarker, mFlag;

    void init(const TexSet& t);
    void update(const Input& in, float dt);
    void collectDraws(std::vector<DrawItem>& out) const;

    void doOllie(float power);
    void startFlip(Flip f);
    void tryGrind(float steer);
    void endGrind(bool popOut);
    void bail(const char* text);
    void land(bool bankLine = true);
    void endManual();
    void score(int pts, const char* name);
    void setMarkerHere();
    void teleportMarker();

    void drawSkater(std::vector<DrawItem>& out) const;
    void drawBoard(std::vector<DrawItem>& out, const Mat4& bm, bool loose) const;
    Mat4 boardMatrix() const;
};
