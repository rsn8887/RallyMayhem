// Rally Mayhem - tiny software synthesizer (all sound is generated at runtime)
// by rsn8887
#include "synth.h"
#include <cmath>
#include <cstring>

namespace snd {

enum { SQ, SAW, TRI, NOISE, SINE };

struct Voice {
    bool on = false;
    int wave = SQ;
    float freq = 440, slideMul = 1, vol = 0.2f, t = 0, dur = 0.1f, phase = 0, duty = 0.5f, nval = 0;
};

static const int NV = 28;
static Voice voices[NV];
Engine eng[5];
SDL_AudioDeviceID dev = 0;
static int rate = 44100;
static uint32_t nrng = 0x1234567;
static float skidVol = 0, rainVol = 0, lp = 0, skidLp = 0, rainLp = 0;
float skidTarget = 0, rainTarget = 0;
static bool musicOn = false;
bool mute = false;
static int musicStep = 0, musicCount = 0;
static float master = 0.6f;

static inline float frand() { nrng ^= nrng << 13; nrng ^= nrng >> 17; nrng ^= nrng << 5; return (nrng & 0xFFFF) / 32768.0f - 1.0f; }

// Not locked: only call from the audio thread or while the device is locked.
static void playRaw(int wave, float freq, float slide, float vol, float dur, float delay = 0, float duty = 0.5f) {
    int best = 0; float bestT = -1e9f;
    for (int i = 0; i < NV; i++) {
        if (!voices[i].on) { best = i; break; }
        float left = voices[i].t / voices[i].dur;
        if (left > bestT) { bestT = left; best = i; }
    }
    Voice& v = voices[best];
    v.on = true; v.wave = wave; v.freq = freq; v.slideMul = powf(slide, 1.0f / rate); v.vol = vol;
    v.t = -delay; v.dur = dur; v.phase = 0; v.duty = duty; v.nval = 0;
}
static void play(int wave, float freq, float slide, float vol, float dur, float delay = 0, float duty = 0.5f) {
    if (!dev || mute) return;
    SDL_LockAudioDevice(dev); playRaw(wave, freq, slide, vol, dur, delay, duty); SDL_UnlockAudioDevice(dev);
}
static void playUi(float freq, float dur, float delay = 0) {
    if (!dev) return;
    SDL_LockAudioDevice(dev); playRaw(SQ, freq, 1, 0.14f, dur, delay, 0.5f); SDL_UnlockAudioDevice(dev);
}

static inline float midi(float n) { return 440.0f * powf(2.0f, (n - 69.0f) / 12.0f); }

// A small looping chiptune for the title and results screens.
static void musicTick(int step) {
    static const int roots[4] = { 57, 53, 48, 55 };              // Am F C G
    static const int third[4] = { 3, 4, 4, 4 };
    static const int8_t lead[4][8] = { { 0, -99, 3, 7, -99, 5, 3, -99 }, { 0, -99, 3, 8, -99, 7, 3, 0 },
                                       { -2, -99, 3, 7, 10, -99, 7, -99 }, { 5, -99, 2, 5, 10, -99, 7, 5 } };
    int bar = (step / 16) & 3, s = step & 15, r = roots[bar];
    static const int arp[4] = { 0, 1, 2, 1 };
    int tones[3] = { r, r + third[bar], r + 7 };
    playRaw(SQ, midi((float)tones[arp[s & 3]] + 12), 1, 0.035f, 0.11f, 0, 0.25f);
    if (s == 0 || s == 3 || s == 6 || s == 8 || s == 11 || s == 14) playRaw(TRI, midi((float)r - 12), 1, 0.16f, 0.2f);
    if ((s & 1) == 0) { int n = lead[bar][s / 2]; if (n != -99) playRaw(SQ, midi(69.0f + n), 1, 0.07f, 0.22f, 0, 0.5f); }
    if (s == 0 || s == 8 || s == 10) playRaw(SINE, 130, 0.02f, 0.3f, 0.12f);
    if (s == 4 || s == 12) playRaw(NOISE, 5000, 0.3f, 0.12f, 0.12f);
    if (s & 1) playRaw(NOISE, 11000, 1, 0.03f, 0.03f);
}

static void callback(void*, Uint8* stream, int len) {
    int16_t* out = (int16_t*)stream;
    int n = len / 2;
    const float dt = 1.0f / rate;
    for (int i = 0; i < n; i++) {
        if (musicOn) { if (--musicCount <= 0) { musicCount = rate / 8; musicTick(musicStep++); } }
        float mix = 0;
        for (int k = 0; k < NV; k++) {
            Voice& v = voices[k];
            if (!v.on) continue;
            v.t += dt;
            if (v.t < 0) continue;
            if (v.t >= v.dur) { v.on = false; continue; }
            float env = 1.0f - v.t / v.dur; env *= env;
            if (v.t < 0.004f) env *= v.t / 0.004f;
            v.freq *= v.slideMul;
            v.phase += v.freq * dt;
            float s = 0;
            switch (v.wave) {
            case SQ:   v.phase -= floorf(v.phase); s = v.phase < v.duty ? 1.0f : -1.0f; break;
            case SAW:  v.phase -= floorf(v.phase); s = v.phase * 2 - 1; break;
            case TRI:  v.phase -= floorf(v.phase); s = fabsf(v.phase * 4 - 2) - 1; break;
            case SINE: v.phase -= floorf(v.phase); s = sinf(v.phase * 6.2831853f); break;
            case NOISE: if (v.phase >= 1) { v.phase -= floorf(v.phase); v.nval = frand(); } s = v.nval; break;
            }
            mix += s * env * v.vol;
        }
        for (int k = 0; k < 5; k++) {
            Engine& e = eng[k];
            e.freq += (e.tf - e.freq) * 0.0008f; e.vol += (e.tv - e.vol) * 0.0008f;
            if (e.vol < 0.0005f) continue;
            e.phase += e.freq * dt; if (e.phase >= 2) e.phase -= 2;
            float p1 = e.phase - floorf(e.phase), p2 = e.phase * 0.5f;
            mix += ((p1 * 2 - 1) * 0.6f + (p2 < 0.5f ? 0.5f : -0.5f)) * e.vol;
        }
        skidVol += (skidTarget - skidVol) * 0.001f;
        if (skidVol > 0.001f) { skidLp += (frand() - skidLp) * 0.35f; mix += (frand() - skidLp) * skidVol * 0.5f; }
        rainVol += (rainTarget - rainVol) * 0.0002f;
        if (rainVol > 0.001f) { rainLp += (frand() - rainLp) * 0.5f; mix += rainLp * rainVol; }
        lp += (mix - lp) * 0.55f;
        float o = lp * master;
        if (o > 1) o = 1;
        if (o < -1) o = -1;
        out[i] = (int16_t)(o * 30000);
    }
}

void init() {
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = 44100; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 1024; want.callback = callback;
    dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (dev) { rate = have.freq; SDL_PauseAudioDevice(dev, 0); }
}
void setMusic(bool on) {
    if (!dev || on == musicOn) return;
    SDL_LockAudioDevice(dev); musicOn = on; musicStep = 0; musicCount = 0; SDL_UnlockAudioDevice(dev);
}

// ---- sound effects ----
void beep(bool go)      { play(SQ, go ? 880.f : 440.f, 1, 0.22f, go ? 0.6f : 0.22f); }
void crash(float v)     { play(NOISE, 3500, 0.2f, 0.25f * v, 0.16f); play(SINE, 150, 0.4f, 0.25f * v, 0.1f); }
void pickup()           { play(SQ, 988, 1, 0.14f, 0.07f); play(SQ, 1319, 1, 0.14f, 0.09f, 0.06f); play(SQ, 1976, 1, 0.14f, 0.16f, 0.12f); }
void missile()          { play(NOISE, 9000, 0.15f, 0.25f, 0.5f); play(SAW, 1200, 0.25f, 0.12f, 0.4f); }
void shot()             { play(SQ, 1800, 0.02f, 0.16f, 0.12f, 0, 0.3f); play(NOISE, 8000, 0.3f, 0.12f, 0.06f); }
void explode()          { play(NOISE, 1800, 0.08f, 0.5f, 0.7f); play(SINE, 120, 0.3f, 0.5f, 0.5f); play(NOISE, 400, 0.5f, 0.3f, 0.9f); }
void splash()           { play(NOISE, 7000, 0.1f, 0.3f, 0.5f); play(SINE, 500, 0.2f, 0.15f, 0.25f); }
void turbo()            { play(SAW, 120, 5.0f, 0.16f, 0.7f); play(NOISE, 1500, 6.0f, 0.14f, 0.7f); }
void jump()             { play(SQ, 260, 4.0f, 0.13f, 0.2f, 0, 0.3f); }
void land()             { play(SINE, 140, 0.3f, 0.3f, 0.12f); play(NOISE, 2500, 0.3f, 0.12f, 0.08f); }
void horn()             { play(SAW, 311, 1, 0.14f, 0.9f); play(SAW, 370, 1, 0.14f, 0.9f); play(SQ, 155, 1, 0.08f, 0.9f); }
void bell()             { play(SINE, 1568, 1, 0.12f, 0.2f); play(SINE, 2349, 1, 0.06f, 0.15f); }
void lapChime()         { play(SQ, 784, 1, 0.14f, 0.1f); play(SQ, 1047, 1, 0.14f, 0.2f, 0.09f); }
void finish()           { const float f[5] = { 523, 659, 784, 1047, 1319 }; for (int i = 0; i < 5; i++) play(SQ, f[i], 1, 0.16f, i == 4 ? 0.5f : 0.12f, i * 0.1f); }
void mineDrop()         { play(SQ, 220, 0.5f, 0.14f, 0.12f); }
void menu()             { playUi(660, 0.05f); }
void join()             { playUi(523, 0.08f); playUi(784, 0.14f, 0.07f); }
void boost()            { play(SAW, 300, 3.0f, 0.12f, 0.3f); }

} // namespace snd
