// Procedural audio: rolling rumble, wind, grind scrape, one-shot
// pops/landings/bails — all synthesized in the SDL audio callback, no assets.
#pragma once
#include <SDL2/SDL.h>
#include <atomic>
#include <cmath>
#include <cstdint>

struct AudioFx {
    std::atomic<float> roll{0}, wind{0}, grind{0};
    std::atomic<unsigned> ev{0};
    enum Ev : unsigned { POP = 1, LAND = 2, LANDHARD = 4, BAIL = 8 };

    bool init() {
        SDL_AudioSpec want{}, got{};
        want.freq = 44100; want.format = AUDIO_F32SYS; want.channels = 1;
        want.samples = 512; want.callback = &AudioFx::cb; want.userdata = this;
        dev = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
        if (!dev) return false;
        SDL_PauseAudioDevice(dev, 0);
        return true;
    }
    void shutdown() { if (dev) SDL_CloseAudioDevice(dev); }
    void fire(Ev e) { ev.fetch_or(e, std::memory_order_relaxed); }

private:
    SDL_AudioDeviceID dev = 0;
    unsigned rng = 0x9E3779B9u;
    float lp1 = 0, lp2 = 0, hpLp = 0, osc = 0, ph = 0;
    float popE = 0, landE = 0, hardE = 0, bailE = 0;

    float noise() {
        rng = rng * 1664525u + 1013904223u;
        return ((rng >> 8) & 0xFFFF) / 32768.f - 1.f;
    }
    static void cb(void* u, Uint8* b, int len) { ((AudioFx*)u)->mix((float*)b, len / 4); }
    void mix(float* out, int n) {
        unsigned e = ev.exchange(0, std::memory_order_relaxed);
        if (e & POP) popE = 1;
        if (e & LAND) landE = 1;
        if (e & LANDHARD) hardE = 1;
        if (e & BAIL) bailE = 1;
        float rl = roll.load(std::memory_order_relaxed);
        float wl = wind.load(std::memory_order_relaxed);
        float gl = grind.load(std::memory_order_relaxed);
        for (int i = 0; i < n; i++) {
            float s = 0.f;
            float nz = noise();
            // wheels on concrete: double-lowpassed noise, slow texture wobble
            lp1 += 0.10f * (nz - lp1);
            lp2 += 0.06f * (lp1 - lp2);
            ph += 0.0012f;
            s += lp2 * rl * (1.4f + 0.4f * std::sin(ph));
            // wind: highpassed noise
            hpLp += 0.02f * (nz - hpLp);
            s += (nz - hpLp) * 0.35f * wl;
            // grind scrape: gritty hi-noise + ringing component
            if (gl > 0.001f) {
                osc += (nz * 0.9f - osc) * 0.25f;
                s += ((nz - lp1) * 0.55f + osc * 0.5f) * gl;
            }
            // one-shots: decaying pitch-swept bursts
            if (popE > 0.001f)  { s += popE * (nz * 0.4f + std::sin(popE * 300.f) * 0.3f); popE *= 0.9965f; }
            if (landE > 0.001f) { s += landE * (std::sin(landE * 500.f) * 0.45f + nz * 0.3f); landE *= 0.9955f; }
            if (hardE > 0.001f) { s += hardE * (std::sin(hardE * 380.f) * 0.6f + nz * 0.45f); hardE *= 0.9945f; }
            if (bailE > 0.001f) { s += bailE * (nz * 0.5f + std::sin(bailE * 260.f) * 0.4f); bailE *= 0.996f; }
            s *= 0.6f;
            out[i] = s > 1 ? 1 : (s < -1 ? -1 : s);
        }
    }
};

inline AudioFx g_audio;
