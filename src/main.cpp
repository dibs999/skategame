// Port Carverton Sessions — a Skate 3-inspired 3D skateboarding sim.
// SDL2 + OpenGL 3.3 core, procedural everything, no external assets.
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <cstdio>
#include <cmath>
#include <vector>
#include "math3d.h"
#include "glcore.h"
#include "shaders.h"
#include "texgen.h"
#include "geom.h"
#include "world.h"
#include "game.h"
#include "vfont.h"
#include "hud.h"
#include "audio.h"

int main(int argc, char** argv) {
    bool fullscreen = false;
    for (int i = 1; i < argc; i++)
        if (std::string(argv[i]) == "--fullscreen") fullscreen = true;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    g_audio.init();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    int W = 1440, H = 810;
    SDL_Window* win = SDL_CreateWindow("Port Carverton Sessions",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!win) { fprintf(stderr, "window: %s\n", SDL_GetError()); return 1; }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { fprintf(stderr, "glew init failed\n"); return 1; }
    glGetError();

    Shader lit, depth, sky, bright, blur, comp, line, point;
    if (!lit.compile(LIT_VS, LIT_FS)) return 1;
    if (!depth.compile(DEPTH_VS, DEPTH_FS)) return 1;
    if (!sky.compile(SKY_VS, SKY_FS)) return 1;
    if (!bright.compile(FSQ_VS, BRIGHT_FS)) return 1;
    if (!blur.compile(FSQ_VS, BLUR_FS)) return 1;
    if (!comp.compile(FSQ_VS, COMPOSITE_FS)) return 1;
    if (!line.compile(LINE_VS, LINE_FS)) return 1;
    if (!point.compile(POINT_VS, POINT_FS)) return 1;

    TexSet tex;
    generateAllTextures(tex);
    Game game;
    game.init(tex);

    // skybox cube
    Mesh skyCube;
    {
        MeshBuilder m;
        m.setColor({1, 1, 1});
        m.box({0, 0, 0}, {1, 1, 1});
        skyCube = m.build();
    }
    // fullscreen triangle
    GLuint fsqVao, fsqVbo;
    {
        float tri[] = {-1, -1, 3, -1, -1, 3};
        glGenVertexArrays(1, &fsqVao);
        glGenBuffers(1, &fsqVbo);
        glBindVertexArray(fsqVao);
        glBindBuffer(GL_ARRAY_BUFFER, fsqVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(tri), tri, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindVertexArray(0);
    }
    auto drawFSQ = [&] {
        glBindVertexArray(fsqVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
    };

    FBO shadowFbo, sceneFbo, bloomA, bloomB;
    shadowFbo.create(2048, 2048, true, false);
    sceneFbo.create(W, H, false, true);
    bloomA.create(W / 4, H / 4, false, true);
    bloomB.create(W / 4, H / 4, false, true);

    LineBatch lb; lb.init();
    SpriteBatch sb; sb.init();
    HUD hud;

    Vec3 sunDir = Vec3(0.55f, 0.62f, 0.35f).normalized(); // direction TO the sun
    Vec3 camPos = game.s.pos + Vec3(-3, 1, 0);
    Vec3 camTarget = game.s.pos;
    float camRoll = 0;

    Input in;
    bool prevJump = false, prevSet = false, prevGo = false, prevSpot[5] = {};
    bool running = true;
    Uint64 last = SDL_GetPerformanceCounter();
    double perfFreq = (double)SDL_GetPerformanceFrequency();
    float timeAcc = 0;

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float frameDt = (float)((now - last) / perfFreq);
        last = now;
        if (frameDt > 0.05f) frameDt = 0.05f;
        timeAcc += frameDt;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                W = ev.window.data1; H = ev.window.data2;
                sceneFbo.create(W, H, false, true);
                bloomA.create(W / 4, H / 4, false, true);
                bloomB.create(W / 4, H / 4, false, true);
            }
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
        }
        const Uint8* k = SDL_GetKeyboardState(nullptr);
        in.push = k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP];
        in.brake = k[SDL_SCANCODE_S] || (k[SDL_SCANCODE_DOWN] && game.s.state != S_AIR);
        in.left = k[SDL_SCANCODE_A] || (k[SDL_SCANCODE_LEFT] && game.s.state == S_GROUND);
        in.right = k[SDL_SCANCODE_D] || (k[SDL_SCANCODE_RIGHT] && game.s.state == S_GROUND);
        in.crouch = k[SDL_SCANCODE_LCTRL] || k[SDL_SCANCODE_RCTRL] || k[SDL_SCANCODE_C];
        in.jump = k[SDL_SCANCODE_SPACE];
        in.trick[0] = k[SDL_SCANCODE_LEFT];
        in.trick[1] = k[SDL_SCANCODE_RIGHT];
        in.trick[2] = k[SDL_SCANCODE_DOWN];
        in.trick[3] = k[SDL_SCANCODE_UP];
        in.grab[0] = k[SDL_SCANCODE_Q];
        in.grab[1] = k[SDL_SCANCODE_E];
        in.grab[2] = k[SDL_SCANCODE_F];
        in.grab[3] = k[SDL_SCANCODE_G];
        in.manual = k[SDL_SCANCODE_R];
        in.noseManual = k[SDL_SCANCODE_V];
        in.setMarker = k[SDL_SCANCODE_M] && !prevSet;
        in.goMarker = k[SDL_SCANCODE_T] && !prevGo;
        in.spot = -1;
        for (int i = 0; i < 5; i++)
            if (k[SDL_SCANCODE_1 + i] && !prevSpot[i]) in.spot = i;
        bool spotNow[5];
        for (int i = 0; i < 5; i++) { spotNow[i] = k[SDL_SCANCODE_1 + i]; prevSpot[i] = spotNow[i]; }
        bool jumpNow = in.jump;
        in.jump = in.jump && !prevJump;
        prevJump = jumpNow; prevSet = k[SDL_SCANCODE_M]; prevGo = k[SDL_SCANCODE_T];

        // physics substeps
        float rem = frameDt;
        while (rem > 0) {
            float dt = std::min(rem, 1.f / 120.f);
            game.update(in, dt);
            rem -= dt;
        }
        in.setMarker = in.goMarker = false; // edge-triggered consumed
        in.spot = -1;

        // ---------- camera: low "filmer" cam ----------
        {
            Vec3 f(std::cos(game.s.yaw), 0, std::sin(game.s.yaw));
            Vec3 rgt(-std::sin(game.s.yaw), 0, std::cos(game.s.yaw));
            float sp = game.s.speed;
            float dist = 2.6f + sp * 0.17f;
            Vec3 desired = game.s.pos - f * dist + Vec3(0, 0.62f + sp * 0.012f, 0)
                         + rgt * (game.s.lean * 0.5f);
            // keep camera above ground
            float gy = game.world.supportHeight(desired.x, desired.z, desired.y + 2.f);
            if (desired.y < gy + 0.35f) desired.y = gy + 0.35f;
            camPos = dampv(camPos, desired, 7.5f, frameDt);
            Vec3 look = game.s.pos + f * 1.0f + Vec3(0, 0.68f, 0);
            camTarget = dampv(camTarget, look, 10.f, frameDt);
            camRoll = dampf(camRoll, game.s.lean * 0.05f, 6, frameDt);
        }
        // camera shake
        float sh = game.shake;
        unsigned seedT = (unsigned)(timeAcc * 997) + 1;
        Vec3 shakeOff((randf(seedT) - 0.5f) * sh * 0.25f,
                      (randf(seedT) - 0.5f) * sh * 0.2f,
                      (randf(seedT) - 0.5f) * sh * 0.25f);
        Vec3 camFinal = camPos + shakeOff;

        // matricies
        float fov = (82.f + std::min(game.s.speed * 0.85f, 22.f) + game.fovBoost) * 3.14159f / 180.f;
        float aspect = (float)W / (float)H;
        Mat4 proj = Mat4::perspective(fov, aspect, 0.08f, 700.f);
        Mat4 view = Mat4::lookAt(camFinal, camTarget, {0, 1, 0});
        if (std::abs(camRoll) > 0.001f) {
            Vec3 fdir = (camTarget - camFinal).normalized();
            view = Mat4::rotateAxis(fdir, camRoll) * view;
        }
        Mat4 vp = Mat4::mul(proj, view);

        // shadow camera: ortho box following player
        Mat4 shadowVP;
        {
            Vec3 c = game.s.pos;
            Vec3 eye = c + sunDir * 45.f;
            Mat4 lv = Mat4::lookAt(eye, c, {0, 1, 0});
            Mat4 lp = Mat4::ortho(-28, 28, -28, 28, 5, 90);
            shadowVP = Mat4::mul(lp, lv);
        }

        // collect draws
        std::vector<DrawItem> draws;
        for (auto& d : game.world.items) draws.push_back(d);
        game.collectDraws(draws);

        // ---------- shadow pass ----------
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        shadowFbo.bind();
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.f, 4.f);
        depth.use();
        for (auto& d : draws) {
            if (!d.castShadow) continue;
            Mat4 mvp = Mat4::mul(shadowVP, d.model);
            depth.setMat4("uMVP", mvp);
            d.mesh->draw();
        }
        glDisable(GL_POLYGON_OFFSET_FILL);

        // ---------- scene pass ----------
        sceneFbo.bind();
        glClearColor(0.5f, 0.65f, 0.85f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // sky
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        sky.use();
        Mat4 skyM = Mat4::mul(Mat4::translation(camPos), Mat4::scale({600, 600, 600}));
        sky.setMat4("uMVP", Mat4::mul(vp, skyM));
        sky.setVec3("uSunDir", sunDir);
        sky.setF("uTime", timeAcc);
        skyCube.draw();
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        // lit scene
        lit.use();
        lit.setVec3("uSunDir", sunDir);
        lit.setVec3("uSunColor", {1.55f, 1.42f, 1.15f});
        lit.setVec3("uCamPos", camFinal);
        lit.setVec3("uAmbientTop", {0.26f, 0.33f, 0.45f});
        lit.setVec3("uAmbientBottom", {0.20f, 0.18f, 0.16f});
        lit.setVec3("uFogColor", {0.72f, 0.80f, 0.90f});
        lit.setF("uFogDensity", 0.0045f);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowFbo.tex);
        lit.setI("uShadowMap", 1);
        glActiveTexture(GL_TEXTURE0);
        for (auto& d : draws) {
            Mat4 mvp = Mat4::mul(vp, d.model);
            lit.setMat4("uMVP", mvp);
            lit.setMat4("uModel", d.model);
            float nm[9]; d.model.normalMat3(nm);
            lit.setMat3("uNormalMat", nm);
            lit.setMat4("uShadowVP", shadowVP);
            lit.setF("uTexScale", d.mat.texScale);
            lit.setF("uSpecular", d.mat.specular);
            lit.setF("uShininess", d.mat.shininess);
            lit.setF("uAO", d.mat.ao);
            lit.setF("uMetal", d.mat.metal);
            lit.setF("uEmissive", d.mat.emissive);
            if (d.mat.tex) {
                glBindTexture(GL_TEXTURE_2D, d.mat.tex);
                lit.setI("uTex", 0);
                lit.setF("uUseTex", 1.f);
            } else {
                lit.setF("uUseTex", 0.f);
            }
            d.mesh->draw();
        }
        // particles
        sb.data.clear();
        game.fx.fill(sb);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        point.use();
        point.setMat4("uVP", vp);
        sb.flush();
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        // ---------- bloom ----------
        glDisable(GL_DEPTH_TEST);
        bloomA.bind();
        bright.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneFbo.tex);
        bright.setI("uTex", 0);
        drawFSQ();
        for (int i = 0; i < 3; i++) {
            bloomB.bind();
            blur.use();
            glBindTexture(GL_TEXTURE_2D, bloomA.tex);
            blur.setI("uTex", 0);
            blur.setVec2("uDir", 1.f / bloomA.w, 0.f);
            drawFSQ();
            bloomA.bind();
            glBindTexture(GL_TEXTURE_2D, bloomB.tex);
            blur.setVec2("uDir", 0.f, 1.f / bloomB.h);
            drawFSQ();
        }

        // ---------- composite ----------
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, W, H);
        comp.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneFbo.tex);
        comp.setI("uScene", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomA.tex);
        comp.setI("uBloom", 1);
        comp.setF("uBloomAmt", 0.55f);
        comp.setF("uDistort", 0.030f + game.s.speed * 0.0025f); // fisheye feel, more at speed
        comp.setF("uVignette", 0.25f);
        comp.setF("uExposure", 1.35f);
        comp.setF("uFlash", game.flash * 0.4f);
        comp.setF("uTime", SDL_GetTicks() * 0.001f);
        drawFSQ();
        glEnable(GL_DEPTH_TEST);

        // ---------- HUD ----------
        Mat4 ortho = Mat4::ortho(0, (float)W, (float)H, 0, -1, 1);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        line.use();
        line.setMat4("uMVP", ortho);
        glLineWidth(2.5f);
        hud.draw(lb, game, W, H);
        lb.flush();
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);

        SDL_GL_SwapWindow(win);
    }

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
