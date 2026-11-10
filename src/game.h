// Rally Mayhem - everything the source files share: constants, types, global state, functions
// by rsn8887
#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "synth.h"
#include "tracks.h"
#ifdef __SWITCH__
#include <switch.h>
#endif

#define GAME_TITLE "RALLY MAYHEM"
#define GAME_VERSION "1.0.0"

// on-screen button names (SDL's "A"/"B"/"Y" are Cross/Circle/Triangle on the PS Vita)
#if defined(__vita__)
#define GAS_LABEL "X"
#define BRAKE_LABEL "O"
#define PAD_HELP "DPAD STEER  X GAS  O BRAKE  TRIANGLE RESET"
#else
#define GAS_LABEL "A"
#define BRAKE_LABEL "B"
#define PAD_HELP "PAD: DPAD STEER   A GAS   B BRAKE   Y RESET"
#endif
#if defined(__SWITCH__)
#define START_LABEL "+"
#else
#define START_LABEL "START"
#endif
#if defined(__SWITCH__) || defined(__vita__) || defined(__ANDROID__)
#define DESKTOP 0
#else
#define DESKTOP 1
#endif

// ------------------------------------------------------------------ basics
static const int W = 320, H = 240, N = W * H, HUD_H = 9;
static const float PI = 3.14159265f;
extern uint32_t fb[N];
extern int frameNo;
static constexpr uint32_t RGB(int r, int g, int b) { return 0xFF000000u | (r << 16) | (g << 8) | b; }
static inline uint32_t shade(uint32_t c, float f) {
    int r = (int)(((c >> 16) & 255) * f), g = (int)(((c >> 8) & 255) * f), b = (int)((c & 255) * f);
    return RGB(std::min(r, 255), std::min(g, 255), std::min(b, 255));
}
static inline uint32_t hash2(int x, int y, int s = 0) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float angDiff(float a, float b) { float d = a - b; while (d > PI) d -= 2 * PI; while (d < -PI) d += 2 * PI; return d; }
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed = 1) : s(seed ? seed : 1) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float f() { return (next() & 0xFFFFFF) / 16777216.0f; }
    int range(int n) { return (int)(next() % (uint32_t)n); }
};
extern Rng grng;
template <class F> static void sortSmall(int* o, int n, F before) { // insertion sort for a handful of items
    for (int i = 1; i < n; i++) { int v = o[i], j = i - 1; while (j >= 0 && before(v, o[j])) { o[j + 1] = o[j]; j--; } o[j + 1] = v; }
}
static inline bool inb(int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; }
static inline void pset(int x, int y, uint32_t c) { if (inb(x, y)) fb[y * W + x] = c; }
static inline void pdark(int x, int y, float f) { if (inb(x, y)) fb[y * W + x] = shade(fb[y * W + x], f); }
void rectf(int x, int y, int w, int h, uint32_t c);
static const uint32_t C_BLACK = RGB(0, 0, 0), C_WHITE = RGB(255, 255, 255), C_RED = RGB(236, 40, 40), C_YELLOW = RGB(255, 224, 40),
    C_GREY = RGB(128, 128, 128), C_ORANGE = RGB(255, 128, 0), C_CYAN = RGB(0, 255, 255), C_BLUE = RGB(0, 0, 255),
    C_GREEN = RGB(60, 230, 80), C_SKY = RGB(72, 140, 232), C_PASTELCYAN = RGB(150, 224, 255);
static const uint32_t CAR_COL[5] = { RGB(228, 36, 36), RGB(44, 84, 236), RGB(250, 214, 36), RGB(240, 84, 204), RGB(40, 210, 226) };
static const char* const CAR_NAME[5] = { "RED", "BLUE", "YELLOW", "PINK", "CYAN" };

// ------------------------------------------------------------------ font (SDL_ttf glyphs cached as bitmaps)
struct Glyph { int w = 0, h = 0, adv = 0; std::vector<uint8_t> px; };
struct PFont { Glyph g[128]; int height = 8; bool ok = false; };
extern PFont fontS, fontL;
bool loadFont(PFont& f, const std::string& path, int size);
int textW(const PFont& f, const char* s);
void drawTextRaw(const PFont& f, int x, int y, const char* s, uint32_t col);
void drawText(const PFont& f, int x, int y, const char* s, uint32_t col, bool shadow = true);
void drawTextC(const PFont& f, int cx, int y, const char* s, uint32_t col);

// ------------------------------------------------------------------ level data
enum { S_NONE, S_OFF, S_ROAD, S_DIRT, S_ICE, S_WATER, S_SHALLOW, S_PUDDLE, S_BOOST, S_PLANK, S_METAL };
static inline bool roadLike(uint8_t s) { return s >= S_ROAD && s != S_WATER; }
static const float CAR_HL = 4.5f, CAR_HW = 2.5f;                 // half length / width of a car (9x5 pixels)
static const int NSECT = 10;
static const float TRAIN_HW = 4, WAGON_LEN = 18, WAGON_STEP = 20; // trains are 9 pixels wide
static const int WAGONS = 5;
struct Sample { float x, y, tx, ty, w, h, bank, arc, curv, ramp; int f; uint8_t type; bool rail, over, portal; };
struct Train { bool horiz; float line, from, to, speed, period, phase, head; bool active, warn; int dir; float t; };
struct Crossing { int k, train; float along; bool ramp; };
struct Waver { int x, y; uint32_t col; int ph; };
struct Pt { int x, y; };
struct Duck { float cx, cy, r, ph; };
struct TreeTop { int x, y, r, kind; };
struct ItemBox { float x, y, z, timer; int type; };
struct Turntable { float x, y, r, speed, ang, now; };
struct Drawbridge {                     // a bridge section that lifts; barriers close while it is up
    int k0, k1; float period, phase, t, lift; bool open, warn;
    std::vector<int> span, barrier; std::vector<uint8_t> spanPos;
};
struct Ferry {                          // a raft shuttling between two docks across a gap in the road
    float ax, ay, bx, by, dx, dy, x, y, vx, vy, hl, hw, tLeft; int state, kA, kB;   // state: 0 at A, 1 to B, 2 at B, 3 to A
};
extern const TrackDef* def;
extern int theme, weather, levelNo;
extern bool night, flood;
extern float windX, windY, gust;
extern std::vector<Sample> sm;
extern int NS;
extern float trackLen, spacing;
extern uint8_t surf[2][N], solid[2][N], israil[2][N], water0[N], reserved[N], objMask[N], dynSolid[N], ttMask[N], spanMask[N], shadowM[N];
extern float ht[2][N], latm[2][N], castH[N];
extern int idxm[2][N], nearIdx[N];
extern uint32_t ground[N], overC[N];
extern uint8_t overA[N], lightStatic[N], lightBuf[N];
extern float overH[N];
extern std::vector<int> waterPix, tripPix;
extern std::vector<uint8_t> tripVal;
extern std::vector<Train> trains;
extern std::vector<Crossing> crossings;
extern std::vector<Waver> wavers;
extern std::vector<Pt> flags, smokers, signals, lamps;
extern std::vector<int> signalTrain;
extern std::vector<Duck> ducks;
extern std::vector<TreeTop> treeTops;
extern std::vector<ItemBox> items;
extern std::vector<Turntable> turntables;
extern std::vector<Drawbridge> bridges;
extern std::vector<Ferry> ferries;
extern int leafCount;
struct ThemeCol { uint32_t off, offA, offB, shore, road, railA, railB; };
static const ThemeCol THEMES[12] = {
    { RGB(60, 148, 52),   RGB(92, 180, 68),   RGB(44, 124, 44),   RGB(224, 208, 136), RGB(112, 112, 120), RGB(214, 40, 40),   RGB(236, 236, 236) },  // summer forest
    { RGB(112, 188, 76),  RGB(156, 220, 100), RGB(88, 164, 68),   RGB(232, 216, 150), RGB(116, 116, 124), RGB(214, 40, 40),   RGB(236, 236, 236) },  // spring
    { RGB(150, 128, 52),  RGB(196, 116, 40),  RGB(120, 100, 44),  RGB(214, 196, 130), RGB(108, 108, 116), RGB(214, 40, 40),   RGB(236, 236, 236) },  // autumn
    { RGB(236, 240, 248), RGB(210, 226, 246), RGB(192, 206, 236), RGB(196, 224, 244), RGB(124, 126, 136), RGB(214, 40, 40),   RGB(250, 250, 250) },  // winter
    { RGB(232, 200, 120), RGB(244, 218, 144), RGB(214, 178, 100), RGB(196, 156, 92),  RGB(120, 116, 116), RGB(150, 96, 48),   RGB(206, 156, 96) },   // Mexico
    { RGB(150, 150, 146), RGB(162, 162, 158), RGB(138, 138, 134), RGB(176, 176, 170), RGB(88, 88, 96),    RGB(236, 196, 40),  RGB(40, 40, 44) },     // city
    { RGB(96, 164, 64),   RGB(128, 192, 80),  RGB(76, 140, 56),   RGB(224, 208, 136), RGB(116, 114, 118), RGB(150, 96, 48),   RGB(236, 236, 236) },  // village
    { RGB(200, 60, 160),  RGB(250, 220, 60),  RGB(60, 200, 220),  RGB(255, 240, 150), RGB(70, 60, 90),    RGB(255, 220, 40),  RGB(255, 60, 200) },   // carnival
    { RGB(52, 152, 64),   RGB(64, 168, 76),   RGB(44, 136, 56),   RGB(224, 208, 136), RGB(78, 78, 86),    RGB(190, 196, 204), RGB(230, 60, 60) },    // F1 circuit
    { RGB(118, 130, 104), RGB(140, 150, 122), RGB(98, 108, 90),   RGB(160, 156, 146), RGB(108, 108, 112), RGB(232, 232, 232), RGB(120, 120, 120) },  // mountain
    { RGB(48, 128, 56),   RGB(72, 152, 68),   RGB(36, 104, 48),   RGB(210, 196, 130), RGB(104, 104, 112), RGB(214, 40, 40),   RGB(236, 236, 236) },  // Germany
    { RGB(86, 120, 56),   RGB(110, 144, 64),  RGB(66, 98, 48),    RGB(96, 104, 60),   RGB(112, 112, 120), RGB(120, 84, 48),   RGB(170, 130, 80) } }; // swamp
static inline bool sandy() { return theme == TH_MEXICO; }
static inline uint32_t waterCol(int k) {   // 0 deep, 1 light, 2 glint
    static const uint32_t w[3][3] = { { RGB(36, 96, 200), RGB(72, 140, 232), RGB(200, 230, 255) }, { RGB(50, 84, 58), RGB(66, 106, 66), RGB(132, 164, 110) }, { RGB(40, 84, 160), RGB(80, 130, 200), RGB(220, 236, 255) } };
    return w[theme == TH_SWAMP ? 1 : (theme == TH_WINTER ? 2 : 0)][k];
}
uint32_t hue(int v);

// ------------------------------------------------------------------ ground art
static inline bool onFerry(const Ferry& f, float x, float y) {
    float al = (x - f.x) * f.dx + (y - f.y) * f.dy, ac = -(x - f.x) * f.dy + (y - f.y) * f.dx;
    return fabsf(al) <= f.hl + 3 && fabsf(ac) <= f.hw;
}
void buildItems();
void buildLevel(int n);
static inline int pixAt(float x, float y) {
    int ix = (int)x, iy = (int)y;
    ix = ix < 0 ? 0 : (ix >= W ? W - 1 : ix); iy = iy < 0 ? 0 : (iy >= H ? H - 1 : iy);
    return iy * W + ix;
}
static inline int activeLayer(int p, float z) { return (surf[1][p] != S_NONE && z >= ht[1][p] - 3.0f) ? 1 : 0; }

// ------------------------------------------------------------------ particles
struct Particle { float x, y, vx, vy, life, maxLife; uint32_t col; float grav; };
extern std::vector<Particle> parts;
void addPart(float x, float y, float vx, float vy, float life, uint32_t col, float grav = 0);

// ------------------------------------------------------------------ cars
enum { WP_NONE, WP_MISSILE, WP_SHOT, WP_MINE, WP_TURBO, WP_COUNT };
struct Input { bool l = false, r = false, acc = false, brk = false, up = false, down = false, start = false, back = false, reset = false; };
struct Car {
    bool on = false, human = false, finished = false, air = false;
    int dev = -1, slot = 0;
    float x = 0, y = 0, a = 0, vx = 0, vy = 0, z = 0, vz = 0, slopeVz = 0, gvis = 0;
    int lap = 0, sector = 0, idx = 0, place = 0, weapon = 0, ammo = 0, points = 0, rank = 0;
    float lostT = 0, turbo = 0, spin = 0, dead = 0, invuln = 0, stuck = 0, rev = 0, ftime = 0, pitch = 0, roll = 0, skill = 1, lane = 0, crashCd = 0, aiFire = 0, power = 1;
    bool pUp = false, pDown = false;
};
struct Proj { int type; float x, y, vx, vy, z, life; int owner; };
struct Mine { float x, y, z, arm; int owner; };
extern Car cars[5];
extern std::vector<Proj> projs;
extern std::vector<Mine> mines;
extern float raceT;
extern int finishCount, raceLaps;
extern bool attract;
int carMask(float cx, float cy, float a, int (*out)[2]);
void startPose(int k, float& x, float& y, float& a);
void trainUpdate(float t);
Input aiInput(Car& c, float dt);
void stepCar(Car& c, Input in, float dt);
void collideCars();
void stepWeapons(float dt);
void computeRanks();

// ------------------------------------------------------------------ moving parts of a track
void updateMechanics(float dt, float t);

// ------------------------------------------------------------------ drawing
void drawIcon(int x, int y, int w, bool tile = false);
struct Drop { float x, y, sp; };
extern std::vector<Drop> drops, streaks;
struct Leaf { float x, y, ty, ph; uint32_t col; };
extern std::vector<Leaf> leaves;
void stepAmbient(float dt);
void drawWorld();

// ------------------------------------------------------------------ input devices
void addController(int joyIndex);
void removeController(SDL_JoystickID id);

// ------------------------------------------------------------------ game flow
extern int state, menuSel, optTrack, optLaps, curTrack;
extern bool optItems, paused, quit;
extern int optCpu, optDiff;
extern int joined[5];
void startRace(int track, bool isAttract);
void finishRace();
void updateRace(float frameDt);
void backdrop(float dim);
void drawHud();
void frame(float frameDt);
