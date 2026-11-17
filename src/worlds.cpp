// Rally Mayhem - the five worlds of tracks 31-50: sci-fi, water, candy, horror and sport.
// Their ground art and scenery, and their creatures and machines: zombies, rolling balls,
// bumpers, trampolines, whirlpools, warp gates, nets and giant donuts.
// by rsn8887

#include "game.h"

std::vector<Critter> critters;
std::vector<Roller> rollers;
std::vector<Bumper> bumpers;
std::vector<Pad> pads;
std::vector<Whirl> whirls;
std::vector<Warp> warps;
std::vector<int> seaPix, boostPix;
std::vector<uint8_t> boostVal;
float flashT = 0;

struct Fin { float cx, cy, r, ph; };                 // a shark circling
struct Blink { int x, y, ph; uint32_t col; };        // a light that flickers (pumpkins, eyes, beacons)
struct Dangler { float x, top, len, ph; };           // a spider on its thread
struct Ghost { float x, y, ph; };
struct Arch { float cx, cy, R, r; };                // a donut standing across the road
static std::vector<Arch> arches;
static std::vector<Fin> fins;
static std::vector<Blink> blinks;
static std::vector<Dangler> danglers;
static Ghost ghosts[2];
static int stains = 0;
static float nextFlash = 6;

static const uint32_t BLOOD[3] = { RGB(200, 16, 24), RGB(140, 8, 16), RGB(240, 60, 50) };
static const uint32_t GOO[3] = { RGB(120, 230, 60), RGB(60, 160, 40), RGB(190, 255, 120) };
static const uint32_t CHIPS[3] = { RGB(250, 250, 250), RGB(214, 40, 40), RGB(170, 170, 176) };
static const uint32_t BEAR[4] = { RGB(232, 40, 56), RGB(60, 200, 80), RGB(250, 200, 40), RGB(250, 130, 30) };
static const uint32_t POOL[8] = { RGB(250, 204, 40), RGB(40, 80, 220), RGB(226, 40, 40), RGB(130, 50, 170), RGB(250, 130, 30), RGB(30, 150, 80), RGB(140, 30, 40), RGB(30, 30, 36) };
static const uint32_t SPARK[3] = { RGB(255, 255, 255), RGB(120, 240, 255), RGB(255, 80, 220) };

static inline uint32_t mixc(uint32_t a, uint32_t b, float t) {
    int r = (int)(((a >> 16) & 255) * (1 - t) + ((b >> 16) & 255) * t), g = (int)(((a >> 8) & 255) * (1 - t) + ((b >> 8) & 255) * t), bl = (int)((a & 255) * (1 - t) + (b & 255) * t);
    return RGB(r, g, bl);
}

void worldClear() {
    critters.clear(); rollers.clear(); bumpers.clear(); pads.clear(); whirls.clear(); warps.clear();
    seaPix.clear(); boostPix.clear(); boostVal.clear(); fins.clear(); blinks.clear(); danglers.clear(); arches.clear();
    stains = 0; flashT = 0; nextFlash = 5;
}

// ------------------------------------------------------------------ ground colours

// the tennis court (style 2): clay inside the lines, white lines
static bool courtLine(int x, int y) {
    const int x0 = 46, x1 = 274, y0 = 62, y1 = 188, sy0 = y0 + 16, sy1 = y1 - 16, sx0 = 160 - 58, sx1 = 160 + 58;
    if (x < x0 || x > x1 || y < y0 || y > y1) return false;
    if (x == x0 || x == x1 || y == y0 || y == y1) return true;
    if ((y == sy0 || y == sy1)) return true;
    if ((x == sx0 || x == sx1) && y > sy0 && y < sy1) return true;
    if (y == 125 && x > sx0 && x < sx1) return true;
    return false;
}

uint32_t worldOff(int x, int y, uint32_t hs) {
    const ThemeCol& T = THEMES[theme];
    int v = hs % 100;
    switch (theme) {
    case TH_SCIFI: {
        if (style == 2) {                                   // rock and metal floating in space
            float a = sinf(x * 0.11f + sinf(y * 0.09f) * 2) + sinf(y * 0.13f + 1);
            return v < 6 ? RGB(104, 100, 128) : (a > 0.9f ? RGB(88, 84, 112) : RGB(72, 68, 96));
        }
        if (x % 16 == 0 || y % 16 == 0) return (x % 16 == 0 && y % 16 == 0) ? RGB(0, 176, 206) : (style == 3 ? RGB(110, 40, 30) : RGB(0, 70, 104));
        if (style == 3) return v < 3 ? RGB(80, 34, 24) : RGB(28, 14, 22);
        if (style == 1) return v < 3 ? RGB(40, 48, 60) : RGB(22, 26, 36);
        return v < 2 ? T.offA : T.off;
    }
    case TH_WATER: {
        // dry land: grass in the middle, a beach around it, surf where the sea meets it
        int near = 9, edge = 99;
        for (int j = -4; j <= 4; j++) for (int i = -4; i <= 4; i++) {
            if (!inb(x + i, y + j)) continue;
            int m = std::max(abs(i), abs(j));
            if (landM[(y + j) * W + x + i]) near = std::min(near, m); else edge = std::min(edge, m);
        }
        if (near == 0 && landM[y * W + x] == 2) {             // harbour quay: stone wall, concrete, a yellow safety line
            if (edge <= 1) return (x + y) % 5 == 0 ? RGB(176, 176, 172) : RGB(206, 206, 200);
            if (edge == 2) return RGB(104, 104, 108);
            if (edge == 4 && ((x + y) & 3) < 2) return RGB(236, 196, 40);
            if (x % 8 == 0 || y % 8 == 0) return RGB(140, 142, 140);
            return (hs % 13) == 0 ? RGB(150, 152, 150) : RGB(162, 164, 160);
        }
        if (near == 0 && landM[y * W + x] == 3) {             // wooden pier
            if (edge <= 1) return ((x + y) % 6 == 0) ? RGB(58, 38, 22) : RGB(100, 66, 36);
            if (x % 3 == 0) return RGB(118, 82, 46);
            return (hs % 5) == 0 ? RGB(160, 114, 64) : RGB(178, 130, 76);
        }
        if (near == 0) return edge <= 1 ? RGB(250, 240, 196) : (edge <= 3 ? ((hs % 9) == 0 ? RGB(214, 190, 124) : RGB(236, 216, 150)) : ((hs % 7) == 0 ? RGB(96, 190, 84) : ((hs % 5) == 0 ? RGB(50, 136, 60) : RGB(66, 160, 70))));
        if (near == 1) return RGB(206, 234, 250);
        if (near == 2) return RGB(96, 170, 232);
        float a = sinf(x * 0.085f + sinf(y * 0.06f) * 2.2f + y * 0.05f) + sinf(y * 0.19f + x * 0.03f);
        return a > 1.45f ? T.offA : (a < -1.5f ? T.offB : T.off);
    }
    case TH_CANDY: {
        float a = sinf(x * 0.07f + sinf(y * 0.055f) * 2.6f) + sinf(y * 0.085f + sinf(x * 0.045f) * 2.0f + 1.1f);
        return a > 1.3f ? T.offA : (a < -1.3f ? T.offB : T.off);
    }
    case TH_HORROR: {
        if (style == 2) {                                   // attic floor boards
            uint32_t hb = hash2(x / 7, 0, levelNo);
            if (x % 7 == 0) return RGB(52, 34, 20);
            if ((y + (int)(hb % 23)) % 31 == 0) return RGB(60, 40, 24);
            uint32_t base = (hb & 1) ? RGB(124, 86, 50) : RGB(112, 76, 44);
            return v < 8 ? shade(base, 0.88f) : base;
        }
        if (style == 3) {                                   // cracked earth
            float a = sinf(x * 0.21f + sinf(y * 0.17f) * 3) + sinf(y * 0.19f + sinf(x * 0.13f) * 3);
            if (fabsf(a) < 0.07f) return RGB(44, 32, 28);
            return v < 5 ? RGB(110, 86, 70) : (v < 10 ? RGB(84, 64, 54) : RGB(96, 74, 62));
        }
        if (style == 1) return v < 6 ? RGB(64, 54, 38) : (v < 12 ? RGB(28, 46, 32) : (v < 14 ? RGB(52, 76, 48) : RGB(38, 58, 40)));     // forest floor
        if (v == 7) return RGB(170, 92, 30);                                                          // a mown lawn with fallen leaves
        return v < 5 ? RGB(54, 88, 52) : (((y / 6) & 1) ? RGB(38, 68, 42) : RGB(33, 60, 38));
    }
    default: {                                              // sport
        int e = std::min(std::min(x, W - 1 - x), std::min(y - HUD_H, H - 1 - y));
        if (style == 0) {                                   // billiard table: wooden frame, cushions, felt
            if (e < 5) return (e == 2 && (x % 40 == 20 || y % 40 == 20)) ? C_WHITE : ((hs & 3) ? RGB(136, 84, 44) : RGB(112, 68, 36));
            if (e < 8) return e == 5 ? RGB(40, 150, 96) : RGB(12, 88, 52);
            return v < 6 ? T.offA : (v < 10 ? T.offB : T.off);
        }
        if (style == 1) {                                   // table tennis table
            if (e < 4) return RGB(14, 52, 110);
            if (e == 5 || e == 6 || y == 124 || y == 125) return RGB(240, 244, 250);
            return v < 4 ? RGB(30, 98, 176) : RGB(24, 88, 164);
        }
        if (style == 2) {                                   // tennis court
            if (e < 9) return e < 2 ? RGB(18, 64, 40) : (((e == 4 || e == 5) && (((x + 8) / 26 + (y + 8) / 26) & 1) && (x + y) % 3) ? RGB(232, 232, 226) : (e == 8 ? RGB(20, 70, 44) : RGB(30, 102, 62)));   // the green wall around a clay court
            if (courtLine(x, y)) return RGB(248, 246, 240);
            if (x > 46 && x < 274 && y > 62 && y < 188) return v < 6 ? RGB(214, 118, 74) : (v < 12 ? RGB(188, 96, 58) : RGB(200, 106, 64));
            return v < 6 ? RGB(190, 98, 60) : (v < 12 ? RGB(164, 78, 46) : RGB(176, 88, 52));
        }
        return ((x / 10) & 1) ? RGB(52, 150, 66) : RGB(44, 138, 60);                // stadium grass
    }
    }
}

bool worldRoad(int ly, int p, uint32_t& out) {
    int x = p % W, y = p / W;
    uint32_t hs = hash2(x, y, levelNo);
    int v = hs % 100;
    const ThemeCol& T = THEMES[theme];
    uint8_t s = surf[ly][p];
    if (israil[ly][p]) {
        const Sample& S = sm[idxm[ly][p]];
        if (S.f & fT) return false;
        switch (theme) {
        case TH_SCIFI: out = ((int)(S.arc / 3) % 4 == 0) ? C_WHITE : (((int)(S.arc / 3) & 1) ? T.railA : T.railB); break;
        case TH_WATER: out = ((int)(S.arc / 4) & 1) ? RGB(250, 140, 30) : T.railB; break;
        case TH_CANDY: out = ((int)(S.arc / 2.5f) & 1) ? T.railA : T.railB; break;
        case TH_HORROR: out = style == 2 ? (((int)(S.arc / 5) & 1) ? RGB(150, 104, 60) : RGB(96, 64, 36)) : (((int)(S.arc / 3) % 3 == 0) ? RGB(34, 32, 38) : T.railA); break;
        default: { int q = (int)(S.arc / 8) % 3; out = q == 0 ? T.railA : (q == 1 ? T.railB : RGB(220, 50, 50)); } break;
        }
        return true;
    }
    if (s == S_OFF || s == S_WATER || s == S_METAL || s == S_NONE) return false;
    const Sample& S = sm[idxm[ly][p]];
    float a = fabsf(latm[ly][p]), half = S.w / 2;
    if (s == S_SLIME) {
        if (theme == TH_CANDY) out = ((x * 3 + y * 5 + (int)(hs & 3)) % 11 == 0) ? RGB(255, 236, 150) : (v < 18 ? RGB(206, 128, 24) : RGB(232, 160, 40));      // caramel
        else out = v < 8 ? RGB(170, 255, 110) : (v < 30 ? RGB(46, 146, 34) : RGB(70, 188, 46));
        return true;
    }
    if (s == S_SHALLOW && liquid > LQ_COLD) { out = v < 10 ? waterCol(2) : (v < 40 ? waterCol(1) : mixc(waterCol(1), waterCol(2), 0.3f)); return true; }
    if (s == S_BOOST) {
        int ph = (((int)(S.arc * 2 - a * S.w * 0.9f)) % 12 + 12) % 12;
        out = theme == TH_SCIFI ? (ph < 3 ? C_WHITE : (ph < 6 ? RGB(0, 232, 255) : RGB(20, 60, 130))) : (ph < 4 ? RGB(255, 224, 0) : (ph < 8 ? RGB(232, 48, 28) : RGB(255, 140, 0)));
        return true;
    }
    if (s == S_PLANK) {
        if (theme != TH_SCIFI) return false;
        out = ((int)S.arc % 3 == 0) ? RGB(40, 56, 92) : (a > 0.84f ? RGB(0, 220, 255) : (v < 10 ? RGB(104, 118, 150) : RGB(88, 100, 132)));     // a grating over empty space
        return true;
    }
    if (s != S_ROAD) return false;
    if (idxm[ly][p] * spacing < 4.0f || S.ramp > 0.74f) return false;              // start line, jump markings
    switch (theme) {
    case TH_SCIFI: {
        bool seam = (int)(S.arc / 10) != (int)((S.arc - 1) / 10);
        if (a > 0.86f) out = S.rail ? shade(T.road, 0.7f) : (((int)(S.arc / 2) & 1) ? RGB(0, 232, 255) : RGB(0, 120, 176));
        else if (a > 0.76f) out = RGB(36, 40, 60);
        else if (seam) out = RGB(44, 48, 70);
        else if (a < 0.07f && ((int)(S.arc / 5) & 1)) out = RGB(240, 60, 200);
        else out = v < 3 ? RGB(88, 96, 124) : (a < 0.4f ? RGB(70, 76, 102) : T.road);
        if (S.ramp > 0) out = shade(out, 1.2f);
    } break;
    case TH_WATER: {
        if (S.ramp > 0.02f) { out = ((int)S.arc % 3 == 0) ? RGB(120, 84, 44) : (v < 20 ? RGB(196, 150, 90) : RGB(176, 130, 74)); break; }     // wooden ski jump
        if (!S.rail && a > 0.86f) { int q = (int)(S.arc / 2.2f) % 5; out = q == 0 ? T.railA : (q == 1 ? T.railB : shade(T.road, 0.92f)); break; }  // a line of buoys
        out = v < 2 ? RGB(130, 196, 244) : (a < 0.3f ? shade(T.road, 1.05f) : T.road);
    } break;
    case TH_CANDY: {
        bool groove = (int)(S.arc / 7) != (int)((S.arc - 1) / 7) || fabsf(a * half - half * 0.5f) < 0.5f;
        if (!S.rail && a > 0.85f) out = RGB(255, 244, 224);
        else if (groove) out = RGB(84, 48, 28);
        else out = v < 4 ? RGB(146, 92, 58) : (v < 8 ? RGB(96, 56, 34) : T.road);
    } break;
    case TH_HORROR: {
        if (style == 2) {                                   // an old carpet across the attic floor
            if (a > 0.84f) out = ((int)(S.arc / 2) & 1) ? RGB(214, 170, 60) : RGB(150, 110, 40);
            else out = ((int)(S.arc / 3 + a * half) % 5 == 0) ? RGB(96, 22, 30) : (v < 6 ? RGB(150, 44, 50) : RGB(128, 32, 40));
            break;
        }
        float cr = sinf(x * 0.31f + sinf(y * 0.23f) * 3) + sinf(y * 0.27f + sinf(x * 0.19f) * 3);
        if (style == 0 && a < 0.07f && ((int)(S.arc / 6) & 1)) out = RGB(196, 166, 50);          // a suburban street
        else if (!S.rail && a > 0.88f) out = ((int)(S.arc / 4) % 3 == 0) ? T.road : RGB(150, 146, 130);
        else if (fabsf(cr) < 0.05f) out = RGB(36, 34, 40);
        else out = v < 1 ? RGB(120, 16, 22) : (v < 6 ? RGB(52, 50, 58) : (v < 10 ? RGB(80, 76, 86) : T.road));
    } break;
    default: {
        if (style == 3) {                                   // running track: lanes
            float dd = latm[ly][p] * half + half;
            bool line = fmodf(dd + 0.4f, 6.5f) < 0.9f || a > 0.95f;
            out = line ? RGB(244, 244, 244) : (v < 6 ? RGB(182, 62, 44) : (v < 12 ? RGB(208, 84, 62) : RGB(196, 72, 52)));
        } else if (style == 2) out = (!S.rail && a > 0.88f) ? RGB(248, 246, 240) : (v < 5 ? RGB(236, 150, 104) : (((int)(S.arc / 3) & 1) ? RGB(224, 134, 90) : RGB(218, 128, 84)));     // swept clay
        else if (style == 1) out = (!S.rail && a > 0.88f) ? RGB(250, 250, 250) : (v < 5 ? RGB(60, 132, 204) : RGB(50, 122, 196));
        else out = (!S.rail && a > 0.88f) ? RGB(228, 238, 228) : (v < 6 ? RGB(44, 160, 100) : T.road);
    } break;
    }
    return true;
}

void worldVerge(int p, int d, uint32_t hs) {
    const ThemeCol& T = THEMES[theme];
    switch (theme) {
    case TH_SCIFI: if (style != 2) { if (d == 1) ground[p] = RGB(0, 150, 190); else if (d == 2) ground[p] = RGB(0, 74, 104); } break;
    case TH_CANDY: if (d == 1) ground[p] = (hs % 4) == 0 ? RGB(255, 244, 224) : shade(T.off, 0.8f); else if (d == 2) ground[p] = shade(T.off, 0.9f); break;
    case TH_HORROR:
        if (style == 0) { if (d == 1) ground[p] = RGB(150, 150, 146); else if (d <= 4) ground[p] = ((p % W) % 6 == 0 || (hs % 17) == 0) ? RGB(84, 84, 90) : RGB(106, 106, 112); }      // kerb and pavement
        else if (d < 3) ground[p] = shade(ground[p], d == 1 ? 0.72f : 0.86f);
        break;
    case TH_SPORT: if (style == 3 && d == 1) ground[p] = RGB(236, 236, 236); else if (style == 3 && d == 2) ground[p] = RGB(40, 120, 52); break;
    default: break;
    }
}

// ------------------------------------------------------------------ scenery

static void paintHouse(int x, int y, uint32_t r1, uint32_t r2, uint32_t ridge, uint32_t wall, uint32_t door, uint32_t win, bool dots, bool smoke) {
    for (int j = 0; j < 9; j++) for (int i = 0; i < 16; i++) {
        uint32_t c = j < 4 ? r1 : (j == 4 ? ridge : r2);
        if (j == 8) c = shade(r2, 0.7f);
        if (dots && (i * 3 + j * 5) % 7 == 0 && j != 4) c = hue((i * 37 + j * 91 + x) & 255);
        else if ((i + j * 2) % 5 == 0 && j != 4) c = shade(c, 0.9f);
        gpx(x + i, y + j, c);
    }
    for (int j = 9; j < 14; j++) for (int i = 0; i < 16; i++) gpx(x + i, y + j, (i == 0 || i == 15) ? shade(wall, 0.75f) : wall);
    for (int j = 1; j < 4; j++) { gpx(x + 11, y + j, RGB(96, 96, 100)); gpx(x + 12, y + j, RGB(64, 64, 68)); }
    if (smoke) smokers.push_back({ x + 11, y });
    for (int j = 10; j < 12; j++) for (int i = 0; i < 3; i++) { gpx(x + 2 + i, y + j, win); gpx(x + 11 + i, y + j, win); }
    for (int j = 11; j < 14; j++) { gpx(x + 7, y + j, door); gpx(x + 8, y + j, shade(door, 0.75f)); }
    markSolid(x, y, 16, 14, 2, 9); markReserved(x - 3, y - 3, 22, 20);
    addLight(x + 8, y + 14, 12, 90);
}

bool worldHouse(int x, int y, Rng& rng) {
    switch (theme) {
    case TH_CANDY:      // gingerbread house
        paintHouse(x, y, RGB(252, 252, 252), RGB(232, 224, 236), C_WHITE, RGB(176, 108, 54), RGB(232, 40, 56), RGB(255, 226, 120), true, true);
        for (int i = 0; i < 16; i += 2) gpx(x + i, y + 9, C_WHITE);
        return true;
    case TH_HORROR:
        if (style == 2) {                                   // an old trunk
            for (int j = 0; j < 9; j++) for (int i = 0; i < 14; i++) {
                uint32_t c = j < 5 ? RGB(110, 70, 40) : RGB(84, 52, 30);
                if (i == 0 || i == 13 || j == 0 || j == 8) c = RGB(48, 30, 18); else if (i == 4 || i == 9) c = RGB(170, 140, 60); else if (j == 5) c = RGB(60, 38, 22);
                gpx(x + i, y + j, c);
            }
            gpx(x + 6, y + 5, RGB(230, 200, 80)); gpx(x + 7, y + 5, RGB(230, 200, 80));
            markSolid(x, y, 14, 9, 2, 5); markReserved(x - 2, y - 2, 18, 13);
            return true;
        }
        if (style == 0) {                                   // a family home, decorated for the night
            static const uint32_t roofs[4][2] = { { RGB(130, 64, 52), RGB(96, 44, 38) }, { RGB(76, 86, 110), RGB(54, 62, 84) }, { RGB(96, 96, 70), RGB(70, 70, 50) }, { RGB(70, 70, 76), RGB(48, 48, 54) } };
            static const uint32_t walls[3] = { RGB(206, 200, 184), RGB(176, 188, 206), RGB(214, 196, 150) };
            int r = rng.range(4);
            paintHouse(x, y, roofs[r][0], roofs[r][1], shade(roofs[r][0], 1.25f), walls[rng.range(3)], RGB(120, 50, 40), RGB(255, 214, 90), false, true);
            gpx(x + 5, y + 13, RGB(244, 130, 24)); gpx(x + 5, y + 12, RGB(60, 130, 50)); gpx(x + 10, y + 13, RGB(244, 130, 24));        // pumpkins on the porch
            blinks.push_back({ x + 5, y + 13, rng.range(64), RGB(255, 236, 110) });
            for (int i = 3; i < 13; i += 2) gpx(x + i, y + 9, hue(i * 40 + x));                                                     // a string of lights
            return true;
        }
        if (style == 1) {                                   // a log cabin
            paintHouse(x, y, RGB(52, 84, 58), RGB(36, 62, 44), RGB(74, 110, 76), RGB(132, 88, 48), RGB(60, 36, 20), RGB(255, 214, 90), false, true);
            for (int j = 9; j < 14; j += 2) for (int i = 1; i < 15; i++) if (!(j > 9 && ((i >= 2 && i <= 4) || (i >= 11 && i <= 13) || i == 7 || i == 8))) gpx(x + i, y + j, RGB(96, 62, 34));
            return true;
        }
        paintHouse(x, y, RGB(66, 56, 88), RGB(46, 38, 62), RGB(90, 80, 112), RGB(92, 88, 98), RGB(20, 18, 22), RGB(255, 214, 90), false, false);
        blinks.push_back({ x + 3, y + 10, rng.range(64), RGB(255, 236, 140) }); blinks.push_back({ x + 12, y + 11, rng.range(64), RGB(255, 236, 140) });
        gpx(x + 5, y + 12, RGB(60, 56, 64)); gpx(x + 9, y + 9, RGB(60, 56, 64));       // cracks
        return true;
    case TH_SCIFI: {    // a tower: its roof, and the front with rows of lit windows
        int w = 12 + rng.range(5), roofH = 7, h = 14 + rng.range(9);
        uint32_t metal = rng.range(2) ? RGB(70, 78, 110) : RGB(86, 74, 112), neon = rng.range(2) ? RGB(0, 232, 255) : RGB(255, 60, 200);
        for (int j = 0; j < roofH; j++) for (int i = 0; i < w; i++) {
            uint32_t c = shade(metal, 1.25f);
            if (i == 0 || j == 0) c = shade(metal, 1.7f); else if (i == w - 1 || j == roofH - 1) c = shade(metal, 0.9f);
            else if (abs(i - w / 2) + abs(j - roofH / 2) * 2 == 4) c = neon;                       // landing mark
            else if (i == w / 2 && j == roofH / 2) c = C_WHITE;
            gpx(x + i, y + j, c);
        }
        for (int j = roofH; j < h; j++) for (int i = 0; i < w; i++) {
            uint32_t c = shade(metal, 0.5f);
            if (i == 0) c = shade(metal, 0.7f); else if (i == w - 1) c = shade(metal, 0.36f);
            else if (j == h - 1) c = neon;
            else if (((j - roofH) & 1) == 0 && i % 2 == 1) { uint32_t hw = hash2(x + i, y + j, 5); c = (hw % 5) == 0 ? shade(metal, 0.3f) : ((hw % 7) == 0 ? RGB(255, 226, 120) : ((hw & 8) ? RGB(120, 236, 255) : RGB(60, 150, 220))); }
            gpx(x + i, y + j, c);
        }
        for (int j = 1; j <= 5; j++) opx(x + 2, y - j, j == 5 ? RGB(255, 60, 60) : RGB(170, 176, 200), 99);       // aerial
        blinks.push_back({ x + 2, y - 5, rng.range(64), RGB(255, 80, 80) });
        markSolid(x, y, w, h, 2, 13); markReserved(x - 3, y - 7, w + 6, h + 10);
        addLight(x + w / 2, y + h, 14, 110);
        return true;
    }
    case TH_WATER: {    // a beach hut (looks best on an island)
        for (int j = 0; j < 11; j++) for (int i = 0; i < 13; i++) {
            uint32_t c = j < 6 ? (((i / 2) & 1) ? RGB(236, 60, 50) : C_WHITE) : RGB(232, 200, 150);
            if (j == 5) c = RGB(150, 40, 36); else if (j >= 6 && (i == 0 || i == 12)) c = RGB(170, 130, 90);
            if (j >= 8 && (i == 6 || i == 7)) c = RGB(60, 120, 180);
            gpx(x + i, y + j, c);
        }
        markSolid(x, y, 13, 11, 2, 7); markReserved(x - 2, y - 2, 17, 15);
        return true;
    }
    default: return false;
    }
}

static void deadTree(int x, int y, Rng& rng) {
    uint32_t bark = RGB(46, 38, 36), barkL = RGB(74, 62, 56);
    gpx(x, y, barkL); gpx(x + 1, y, bark); gpx(x, y + 1, bark); gpx(x + 1, y + 1, RGB(30, 24, 24));
    markSolid(x, y, 2, 2, 2, 0);
    for (int i = -1; i <= 2; i++) if (inb(x + i, y + 1)) castH[(y + 1) * W + x + i] = std::max(castH[(y + 1) * W + x + i], i == 0 || i == 1 ? 9.0f : 4.0f);
    for (int j = 1; j <= 7; j++) { opx(x, y - j, j < 5 ? barkL : bark, 99); if (j < 5) opx(x + 1, y - j, bark, 99); }
    for (int b = 0; b < 5; b++) {
        int by = y - 3 - b - rng.range(2), dir = (b & 1) ? 1 : -1, len = 2 + rng.range(4);
        for (int k = 1; k <= len; k++) { opx(x + dir * k + (dir > 0 ? 1 : 0), by - (k + 1) / 2, k == len ? barkL : bark, 99); if (k == len - 1 && rng.range(2)) opx(x + dir * k + (dir > 0 ? 1 : 0), by - (k + 1) / 2 - 1, bark, 99); }
    }
    markReserved(x - 6, y - 12, 14, 15);
}

bool worldScenery(int x, int y, Rng& rng) {
    int k = rng.range(20);
    switch (theme) {
    case TH_SCIFI:
        if (k < 5 && placeFree(x + 1, y - 3, 2, 5)) {        // beacon mast
            for (int j = 0; j < 8; j++) { gpx(x + 1, y - j, j & 1 ? RGB(150, 156, 176) : RGB(110, 116, 140)); }
            gpx(x, y, RGB(70, 74, 96)); gpx(x + 2, y, RGB(50, 54, 72)); gpx(x, y - 5, RGB(150, 156, 176)); gpx(x + 2, y - 5, RGB(90, 96, 120));
            markSolid(x, y, 3, 1, 1, 7); markReserved(x - 2, y - 10, 7, 13);
            blinks.push_back({ x + 1, y - 8, rng.range(64), (k & 1) ? RGB(255, 60, 60) : RGB(0, 255, 200) }); addLight(x + 1, y - 6, 12, 90);
            return true;
        }
        if (k < 9 && placeFree(x + 2, y + 3, 4, 5)) {        // energy crystal
            static const char* s[8] = { "..w..", ".wlc.", ".wlc.", "wllcb", "wllcb", ".lccb", ".lcb.", "..b.." }; static const uint32_t c[4] = { C_WHITE, RGB(150, 250, 255), RGB(0, 200, 236), RGB(0, 100, 160) };
            static const uint32_t c2[4] = { C_WHITE, RGB(255, 170, 240), RGB(236, 60, 200), RGB(130, 20, 120) };
            sprite(x, y, s, 8, "wlcb", (k & 1) ? c : c2); markSolid(x, y + 3, 5, 5, 1, 6); markReserved(x - 2, y - 2, 9, 12); addLight(x + 2, y + 4, 10, 120);
            return true;
        }
        if (k < 12 && placeFree(x + 8, y + 10, 12, 16)) return worldHouse(x, y, rng);
        if (k < 14 && placeFree(x + 7, y + 7, 10, 10)) {     // landing pad with a parked shuttle
            for (int j = -7; j <= 7; j++) for (int i = -7; i <= 7; i++) {
                float d = hypotf((float)i, (float)j);
                if (d > 7.4f) continue;
                gpx(x + 7 + i, y + 7 + j, d > 6.3f ? (((i + j + 20) / 2) & 1 ? RGB(240, 200, 40) : RGB(30, 32, 44)) : (fabsf(d - 4) < 0.6f ? RGB(0, 200, 236) : RGB(54, 60, 84)));
            }
            static const char* sh[5] = { "ww.....", "wbbbb..", ".bcbbbw", "wbbbb..", "ww....." }; uint32_t c[3] = { RGB(230, 236, 250), hue(rng.range(256)), RGB(120, 236, 255) };
            sprite(x + 4, y + 5, sh, 5, "wbc", c); markSolid(x + 4, y + 5, 7, 5, 1, 3); markObj(x, y, 15, 15); markReserved(x - 2, y - 2, 19, 19);
            return true;
        }
        if (k < 15 && placeFree(x + 4, y - 2, 6, 6)) {       // hologram board
            for (int j = 0; j < 4; j++) { gpx(x + 4, y - j, RGB(120, 126, 150)); }
            for (int j = 0; j < 5; j++) for (int i = 0; i < 9; i++) opx(x + i, y - 9 + j, (j == 0 || j == 4 || i == 0 || i == 8) ? RGB(40, 50, 90) : hue((i * 30 + j * 50 + x * 7) & 255), 99);
            for (int i = 1; i < 8; i += 2) blinks.push_back({ x + i, y - 7, rng.range(64), C_WHITE });
            markSolid(x + 4, y, 1, 1, 1, 8); markReserved(x - 2, y - 11, 13, 14); addLight(x + 4, y - 6, 12, 120);
            return true;
        }
        if (k < 17 && placeFree(x + 3, y + 3, 5, 5)) {       // radar dish
            static const char* s[7] = { ".wwwww.", "wlllllg", "wllkllg", ".lllgg.", "..ggg..", "...m...", "..mmm.." }; static const uint32_t c[5] = { RGB(236, 240, 250), RGB(170, 180, 206), RGB(110, 120, 150), RGB(255, 80, 60), RGB(70, 74, 96) };
            sprite(x, y, s, 7, "wlgkm", c); markSolid(x, y + 3, 7, 4, 1, 5); markReserved(x - 2, y - 2, 11, 11); return true;
        }
        if (placeFree(x + 1, y + 1, 3)) {                    // floor light
            gpx(x, y, RGB(0, 232, 255)); gpx(x + 1, y, C_WHITE); gpx(x, y + 1, RGB(0, 140, 190)); gpx(x + 1, y + 1, RGB(0, 232, 255));
            markObj(x, y, 2, 2); markReserved(x - 2, y - 2, 6, 6); addLight(x, y, 9, 100); return true;
        }
        return false;
    case TH_WATER:
        if (k < 6 && placeFree(x + 4, y + 3, 7, 6)) {        // rock in the surf
            static const char* b[6] = { "..wwww...", ".wlllllg.", "wllllllgd", "lllllggdd", "llgggggdd", ".gggddd.." }; static const uint32_t c[4] = { RGB(214, 214, 220), RGB(150, 146, 144), RGB(108, 104, 104), RGB(66, 64, 70) };
            for (int i = -1; i < 10; i++) { if ((i + x) % 3) gpx(x + i, y + 6, RGB(214, 236, 250)); if ((i + y) % 4 == 0) gpx(x + i, y + 7, RGB(150, 204, 240)); }
            gpx(x - 1, y + 4, RGB(214, 236, 250)); gpx(x + 9, y + 5, RGB(214, 236, 250));
            sprite(x, y, b, 6, "wlgd", c); markSolid(x, y + 1, 9, 5, 2, 5); markReserved(x - 3, y - 2, 15, 11); return true;
        }
        if (k < 10 && placeFree(x + 3, y + 5, 6, 7)) {       // sailing boat at anchor
            static const char* s[10] = { "...m...", "..wm...", "..wmw..", ".wwmww.", ".wwmww.", "wwwmwww", "...m...", "rrrrrrr", ".rrrrr.", "..ddd.." }; uint32_t c[4] = { RGB(250, 250, 250), RGB(110, 80, 50), hue(rng.range(256)), RGB(40, 60, 110) };
            sprite(x, y, s, 10, "wmrd", c); markSolid(x, y + 6, 7, 4, 2, 6); markObj(x, y, 7, 10); markReserved(x - 3, y - 2, 13, 15); return true;
        }
        if (k < 15 && placeFree(x, y, 10)) { fins.push_back({ (float)x, (float)y, 4.0f + rng.range(5), rng.f() * 6 }); markReserved(x - 10, y - 8, 21, 17); return true; }
        if (k < 18 && placeFree(x + 1, y + 1, 3)) {          // marker buoy
            gpx(x, y, RGB(236, 50, 40)); gpx(x + 1, y, RGB(180, 30, 30)); gpx(x, y + 1, C_WHITE); gpx(x + 1, y + 1, RGB(200, 200, 210)); gpx(x, y + 2, RGB(236, 50, 40)); gpx(x + 1, y + 2, RGB(180, 30, 30));
            gpx(x - 1, y + 3, RGB(170, 214, 244)); gpx(x + 2, y + 3, RGB(170, 214, 244));
            markSolid(x, y, 2, 3, 1, 3); markReserved(x - 2, y - 3, 6, 9); blinks.push_back({ x, y - 1, rng.range(64), RGB(255, 240, 120) }); return true;
        }
        if (placeFree(x + 4, y + 2, 8, 7)) {                 // sand bank with a palm
            for (int j = -3; j <= 3; j++) for (int i = -6; i <= 6; i++) {
                float d = (i / 6.5f) * (i / 6.5f) + (j / 3.5f) * (j / 3.5f);
                if (d > 1) continue;
                gpx(x + 4 + i, y + 2 + j, d > 0.72f ? RGB(170, 214, 244) : (d > 0.5f ? RGB(250, 238, 190) : RGB(236, 216, 150)));
                if (d <= 0.72f && inb(x + 4 + i, y + 2 + j)) { int q = (y + 2 + j) * W + x + 4 + i; solid[0][q] = 2; objMask[q] = 1; }
            }
            drawPalm(x + 4, y + 2); markReserved(x - 6, y - 10, 21, 18); return true;
        }
        return false;
    case TH_CANDY:
        if (k < 5 && placeFree(x, y - 3, 4, 6)) {            // lollipop
            uint32_t c1 = hue(rng.range(256)), c2 = C_WHITE;
            for (int j = 0; j < 5; j++) gpx(x, y - j, j == 0 ? RGB(190, 190, 200) : C_WHITE);
            for (int j = -3; j <= 3; j++) for (int i = -3; i <= 3; i++) {
                float d = sqrtf((float)(i * i + j * j));
                if (d > 3.4f) continue;
                float an = atan2f((float)j, (float)i) + d * 1.3f;
                uint32_t c = (sinf(an * 2) > 0) ? c1 : c2;
                if (i + j > 2) c = shade(c, 0.78f); else if (i + j < -3) c = mixc(c, C_WHITE, 0.4f);
                opx(x + i, y - 7 + j, c, 99);
            }
            markSolid(x, y, 1, 1, 1, 0); if (inb(x, y)) castH[y * W + x] = 8; if (inb(x + 1, y)) castH[y * W + x + 1] = 8;
            markReserved(x - 5, y - 12, 11, 15); return true;
        }
        if (k < 8 && placeFree(x + 2, y - 3, 3, 6)) {        // candy cane
            static const char* s[10] = { ".rww.", "w..rr", "r...w", "w...w", "....r", "....r", "....w", "....w", "....r", "....r" }; static const uint32_t c[2] = { RGB(236, 40, 56), C_WHITE };
            sprite(x, y - 9, s, 8, "rw", c, 0); sprite(x, y - 1, s + 8, 2, "rw", c);
            markSolid(x + 4, y - 1, 1, 2, 1, 8); markReserved(x - 2, y - 11, 9, 14); return true;
        }
        if (k < 12 && placeFree(x + 2, y + 3, 4, 5)) {       // gummi bear
            static const char* s[7] = { "b...b", "blllb", ".lbl.", "blllb", ".lll.", ".bbb.", "b...b" };
            uint32_t base = BEAR[rng.range(4)], c[2] = { shade(base, 0.72f), mixc(base, C_WHITE, 0.25f) };
            sprite(x, y, s, 7, "bl", c); gpx(x + 1, y + 1, C_WHITE);
            markSolid(x, y + 1, 5, 6, 1, 4); markReserved(x - 2, y - 2, 9, 11); return true;
        }
        if (k < 15 && placeFree(x + 3, y + 4, 5, 6)) {       // cup cake
            static const char* s[9] = { "...r...", "..www..", ".wwpww.", "wwpwwpw", "wpwwwww", ".ybyby.", ".ybyby.", ".ybyby.", "..byb.." };
            uint32_t c[5] = { RGB(232, 30, 50), rng.range(2) ? C_WHITE : RGB(170, 230, 255), rng.range(2) ? RGB(255, 150, 200) : RGB(255, 226, 120), RGB(250, 214, 110), RGB(196, 150, 70) };
            sprite(x, y, s, 9, "rwpyb", c); markSolid(x, y + 3, 7, 6, 1, 5); markReserved(x - 2, y - 2, 11, 13); return true;
        }
        if (k < 17 && placeFree(x, y, 5)) { addLeafTree(x, y, rng, 2); return true; }     // candy floss tree
        if (k < 19 && placeFree(x + 3, y + 2, 5, 4)) {       // a small donut
            static const char* s[5] = { ".iiiii.", "iiidiii", "iid.dii", "biidiib", ".bbbbb." }; uint32_t c[3] = { rng.range(2) ? RGB(255, 130, 190) : RGB(120, 70, 40), RGB(180, 120, 60), RGB(226, 170, 100) };
            sprite(x, y, s, 5, "idb", c); gpx(x + 1, y + 1, hue(rng.range(256))); gpx(x + 5, y, hue(rng.range(256))); gpx(x + 4, y + 2, hue(rng.range(256)));
            markSolid(x, y, 7, 5, 1, 2); markReserved(x - 2, y - 2, 11, 9); return true;
        }
        if (placeFree(x + 2, y + 4, 4, 6)) {                 // ice cream cone
            static const char* s[9] = { ".ppp.", "ppppw", "pwppp", "wwwww", "ywywy", ".ywy.", ".yyy.", "..y..", "..y.." }; uint32_t c[3] = { rng.range(2) ? RGB(255, 160, 200) : RGB(150, 230, 190), C_WHITE, RGB(226, 170, 90) };
            sprite(x, y, s, 9, "pwy", c); markSolid(x, y + 3, 5, 6, 1, 5); markReserved(x - 2, y - 2, 9, 13); return true;
        }
        return false;
    case TH_HORROR:
        if (style == 2) {                                    // attic junk
            if (k < 8 && placeFree(x + 3, y + 3, 5, 5)) {    // crate
                for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) gpx(x + i, y + j, (i == 0 || j == 0 || i == 6 || j == 6) ? RGB(70, 46, 26) : ((i == j || i == 6 - j) ? RGB(96, 64, 36) : RGB(150, 106, 60)));
                markSolid(x, y, 7, 7, 2, 5); markReserved(x - 2, y - 2, 11, 11); return true;
            }
            if (k < 14 && placeFree(x + 4, y + 4, 6, 6)) {   // cobweb
                for (int r = 2; r <= 8; r += 3) for (int i = 0; i <= r; i++) gpx(x + i, y + r - i, RGB(206, 206, 214));
                for (int i = 0; i < 9; i++) { gpx(x + i, y, RGB(180, 180, 190)); gpx(x, y + i, RGB(180, 180, 190)); if (i < 7) gpx(x + i, y + i, RGB(160, 160, 170)); }
                markObj(x, y, 9, 9); markReserved(x - 1, y - 1, 11, 11); return true;
            }
            if (placeFree(x + 2, y + 2, 4, 4)) {             // paint pots
                for (int b = 0; b < 2; b++) { uint32_t c = hue(rng.range(256)); int bx = x + b * 4; gpx(bx, y, c); gpx(bx + 1, y, c); gpx(bx + 2, y, shade(c, 0.7f)); for (int j = 1; j < 4; j++) { gpx(bx, y + j, RGB(190, 190, 196)); gpx(bx + 1, y + j, RGB(150, 150, 158)); gpx(bx + 2, y + j, RGB(100, 100, 110)); } }
                markSolid(x, y, 7, 4, 1, 3); markReserved(x - 2, y - 2, 11, 8); return true;
            }
            return false;
        }
        if (style == 0) {                                    // a suburb on Halloween night
            if (k < 6 && placeFree(x + 8, y + 7, 12, 11)) return worldHouse(x, y, rng);
            if (k < 10 && placeFree(x, y, 5)) { addLeafTree(x, y, rng, 4); return true; }
            if (k < 13 && placeFree(x + 5, y + 1, 7, 3)) {   // picket fence
                for (int i = 0; i < 11; i++) { gpx(x + i, y + 1, RGB(214, 214, 206)); if (i % 2 == 0) { gpx(x + i, y, RGB(240, 240, 232)); gpx(x + i, y + 2, RGB(170, 170, 166)); } }
                markSolid(x, y + 1, 11, 1, 1, 2); markReserved(x - 1, y - 1, 13, 5); return true;
            }
            if (k < 14 && placeFree(x + 1, y - 1, 2, 4)) {   // letter box
                gpx(x, y, RGB(90, 60, 34)); gpx(x, y - 1, RGB(90, 60, 34)); gpx(x - 1, y - 2, RGB(60, 90, 160)); gpx(x, y - 2, RGB(80, 120, 200)); gpx(x + 1, y - 2, RGB(50, 70, 130)); gpx(x + 1, y - 3, RGB(230, 40, 40));
                markSolid(x, y, 1, 1, 1, 3); markReserved(x - 2, y - 4, 5, 6); return true;
            }
        } else if (style == 1) {                             // a summer camp in the woods
            if (k < 9 && placeFree(x, y, 5)) { addLeafTree(x, y, rng, 1); return true; }
            if (k < 11 && placeFree(x + 8, y + 7, 12, 11)) return worldHouse(x, y, rng);
            if (k < 13 && placeFree(x + 5, y + 4, 7, 6)) {   // tent
                static const char* t[8] = { "....ll.....", "...lldd....", "..lllddd...", ".llllkddd..", ".lllkkkddd.", "llllkkkdddd", "llllkkkdddd", "sss.....sss" }; uint32_t c[4] = { rng.range(2) ? RGB(220, 150, 50) : RGB(70, 150, 110), 0, RGB(20, 18, 22), RGB(60, 50, 40) };
                c[1] = shade(c[0], 0.62f);
                sprite(x, y, t, 8, "ldks", c); markSolid(x, y + 2, 11, 5, 2, 5); markReserved(x - 2, y - 2, 15, 12); return true;
            }
            if (k < 15 && placeFree(x + 3, y + 2, 5, 5)) {   // camp fire
                static const char* f[5] = { "s.....s", ".bb.bb.", "..bbb..", ".bb.bb.", "s.....s" }; static const uint32_t c[2] = { RGB(120, 116, 110), RGB(84, 52, 28) };
                sprite(x, y, f, 5, "sb", c); markSolid(x + 1, y + 1, 5, 3, 1, 1); markReserved(x - 3, y - 4, 13, 12);
                blinks.push_back({ x + 3, y + 2, rng.range(64), RGB(255, 200, 40) }); blinks.push_back({ x + 3, y + 1, rng.range(64), RGB(255, 110, 20) }); blinks.push_back({ x + 2, y + 2, rng.range(64), RGB(255, 240, 150) }); blinks.push_back({ x + 4, y + 1, rng.range(64), RGB(255, 150, 30) });
                smokers.push_back({ x + 2, y }); addLight(x + 3, y + 2, 18, 170); return true;
            }
            if (k < 16 && placeFree(x + 5, y + 1, 7, 3)) {   // canoe
                static const char* cn[3] = { ".rrrrrrrrr.", "rdddwdddddr", ".rrrrrrrrr." }; uint32_t c[3] = { rng.range(2) ? RGB(200, 50, 40) : RGB(50, 130, 80), RGB(60, 40, 26), RGB(200, 170, 110) };
                sprite(x, y, cn, 3, "rdw", c); markSolid(x, y, 11, 3, 1, 2); markReserved(x - 2, y - 2, 15, 7); return true;
            }
        }
        if (k < (style == 3 ? 6 : 18) && style != 0 && placeFree(x, y - 2, 5, 6)) { deadTree(x, y, rng); return true; }
        if (style != 3) {
            if (placeFree(x + 2, y + 2, 4, 4)) {             // pumpkin lantern
                static const char* s[5] = { "..g..", ".ooo.", "oyoyd", "oooyd", ".odd." }; static const uint32_t c[4] = { RGB(60, 130, 50), RGB(244, 130, 24), RGB(255, 236, 110), RGB(170, 76, 14) };
                sprite(x, y, s, 5, "goyd", c); markSolid(x, y + 1, 5, 4, 1, 3); markReserved(x - 2, y - 2, 9, 9);
                blinks.push_back({ x + 2, y + 2, rng.range(64), RGB(255, 250, 170) }); addLight(x + 2, y + 2, 13, 150); return true;
            }
            return false;
        }
        if (k < 9 && placeFree(x + 2, y + 3, 4, 5)) {        // grave stone
            static const char* s[7] = { ".lll.", "lgggd", "lgkgd", "lkkkd", "lgkgd", "lgggd", "mmmmm" }; static const uint32_t c[5] = { RGB(176, 176, 184), RGB(132, 132, 142), RGB(84, 84, 96), RGB(60, 60, 70), RGB(70, 52, 40) };
            sprite(x, y, s, 7, "lgdkm", c); markSolid(x, y + 2, 5, 5, 1, 5); markReserved(x - 2, y - 2, 9, 12); return true;
        }
        if (k < 11 && placeFree(x + 2, y + 3, 4, 5)) {       // cross
            static const char* s[8] = { "..l..", "..ld.", "lllld", ".dldd", "..ld.", "..ld.", "..ld.", ".mmm." }; static const uint32_t c[3] = { RGB(170, 164, 150), RGB(96, 92, 88), RGB(70, 52, 40) };
            sprite(x, y, s, 8, "ldm", c); markSolid(x + 1, y + 5, 3, 3, 1, 6); markReserved(x - 2, y - 2, 9, 12); return true;
        }
        if (k < 14 && placeFree(x + 2, y + 2, 4, 4)) {       // pumpkin lantern
            static const char* s[5] = { "..g..", ".ooo.", "oyoyd", "oooyd", ".odd." }; static const uint32_t c[4] = { RGB(60, 130, 50), RGB(244, 130, 24), RGB(255, 236, 110), RGB(170, 76, 14) };
            sprite(x, y, s, 5, "goyd", c); markSolid(x, y + 1, 5, 4, 1, 3); markReserved(x - 2, y - 2, 9, 9);
            blinks.push_back({ x + 2, y + 2, rng.range(64), RGB(255, 250, 170) }); addLight(x + 2, y + 2, 13, 150); return true;
        }
        if (k < 16 && placeFree(x + 2, y + 1, 4, 3)) {       // skull and bones
            static const char* s[4] = { "w...w", ".www.", ".kwk.", "w.w.w" }; static const uint32_t c[2] = { RGB(232, 228, 210), RGB(30, 30, 30) };
            sprite(x, y, s, 4, "wk", c); markObj(x, y, 5, 4); markReserved(x - 1, y - 1, 7, 6); return true;
        }
        if (k < 17 && placeFree(x + 8, y + 7, 12, 11)) return worldHouse(x, y, rng);
        if (placeFree(x + 2, y + 1, 4, 3)) {                 // something watching from a dark bush
            for (int j = 0; j < 4; j++) for (int i = 0; i < 6; i++) if (!((j == 0 || j == 3) && (i == 0 || i == 5))) gpx(x + i, y + j, (i + j < 3) ? RGB(44, 56, 44) : RGB(26, 32, 30));
            markObj(x, y, 6, 4); markReserved(x - 1, y - 1, 8, 6);
            blinks.push_back({ x + 1, y + 1, rng.range(64), RGB(255, 40, 30) }); blinks.push_back({ x + 3, y + 1, blinks.back().ph, RGB(255, 40, 30) }); return true;
        }
        return false;
    default:
        return false;       // nothing grows on a table, a court or in a stadium
    }
}

// a small person seen from above (spectators, ball kids, fishermen); some of them wave
static void person(int x, int y, uint32_t shirt, Rng& rng, bool wave = true) {
    uint32_t skin = rng.range(3) ? RGB(244, 190, 150) : RGB(150, 100, 70);
    gpx(x, y, skin); gpx(x + 1, y, shade(skin, 0.85f)); gpx(x, y + 1, shirt); gpx(x + 1, y + 1, shade(shirt, 0.8f)); gpx(x, y + 2, shade(shirt, 0.6f)); gpx(x + 1, y + 2, shade(shirt, 0.5f));
    if (wave && rng.range(2)) wavers.push_back({ x, y, skin, rng.range(64) });
}
// objects that look different in the new worlds; false = draw the standard one
bool worldObject(const Obj& o, Rng& rng) {
    int x = (int)o.a, y = (int)o.b;
    if (theme == TH_WATER && o.t == O_TENT) {               // a lighthouse
        static const char* lh[16] = { "...y...", "..kyk..", "..kkk..", ".wwwww.", "..rrr..", "..rrd..", "..wwd..", "..wwd..", ".rrrdd.", ".rrrdd.", ".wwwdd.", ".wwwdd.", ".rrrdd.", "ggggggg", "ggggggd", ".ddddd." };
        static const uint32_t c[6] = { RGB(255, 244, 150), RGB(40, 44, 56), RGB(250, 250, 250), RGB(226, 50, 44), RGB(150, 150, 160), RGB(190, 190, 186) };
        sprite(x, y - 13, lh, 11, "ykwrdg", c, 0); sprite(x, y - 2, lh + 11, 5, "ykwrdg", c);
        markSolid(x, y - 1, 7, 4, 2, 0); if (inb(x + 3, y + 2)) { castH[(y + 2) * W + x + 2] = 11; castH[(y + 2) * W + x + 3] = 11; castH[(y + 2) * W + x + 4] = 11; }
        markReserved(x - 3, y - 15, 13, 20); blinks.push_back({ x + 3, y - 13, rng.range(64), C_WHITE }); blinks.push_back({ x + 3, y - 12, blinks.back().ph, RGB(255, 244, 150) }); addLight(x + 3, y - 10, 30, 160);
        return true;
    }
    if (theme == TH_SPORT && style == 3 && o.t == O_FLAG) return false;
    return false;
}

// is a point inside the loop of the road? (even-odd rule on the centre line)
static bool insideTrack(float x, float y) {
    bool in = false;
    int step = std::max(1, (int)(4 / spacing));
    for (int k = 0; k < NS; k += step) {
        const Sample& a = sm[k], & b = sm[(k + step) % NS];
        if ((a.y > y) != (b.y > y) && x < a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y)) in = !in;
    }
    return in;
}

void worldDetails(Rng& rng) {
    auto freeOff = [](int x, int y) { int p = y * W + x; return inb(x, y) && y > HUD_H + 1 && surf[0][p] == S_OFF && surf[1][p] == S_NONE && !objMask[p] && !solid[0][p] && !reserved[p] && !water0[p]; };
    if (theme == TH_WATER) {
        // the harbour: containers, a crane and bollards on the quays, boats moored along them, people on the piers
        for (const Obj& o : def->o) {
            if (o.t == O_QUAY) {
                int qx = (int)o.a, qy = (int)o.b, qw = (int)o.c, qh = (int)o.d;
                auto onQuay = [&](int x, int y, int w, int h) { for (int j = -1; j <= h; j++) for (int i = -1; i <= w; i++) { int xx = x + i, yy = y + j; if (!inb(xx, yy) || yy < HUD_H + 1 || landM[yy * W + xx] != 2 || reserved[yy * W + xx]) return false; } return x >= qx + 6 && y >= qy + 6 && x + w <= qx + qw - 6 && y + h <= qy + qh - 6; };
                for (int tries = 0; tries < 260; tries++) {
                    int x = qx + 6 + rng.range(std::max(1, qw - 12)), y = qy + 6 + rng.range(std::max(1, qh - 12)), k = rng.range(10);
                    if (k < 6 && onQuay(x, y, 10, 5)) {      // container
                        uint32_t c = hue(rng.range(256));
                        for (int j = 0; j < 5; j++) for (int i = 0; i < 10; i++) gpx(x + i, y + j, j == 4 ? shade(c, 0.4f) : ((j == 0 || i == 0) ? shade(c, 0.95f) : (i % 2 ? shade(c, 0.62f) : shade(c, 0.75f))));
                        gpx(x + 4, y + 2, C_WHITE); gpx(x + 5, y + 2, C_WHITE);
                        markSolid(x, y, 10, 5, 2, 7); markReserved(x - 1, y - 1, 12, 7);
                    } else if (k < 7 && onQuay(x, y, 14, 9)) {       // warehouse
                        for (int j = 0; j < 9; j++) for (int i = 0; i < 14; i++) gpx(x + i, y + j, j >= 6 ? ((i % 4 == 1 && j < 8) ? RGB(60, 80, 110) : RGB(150, 70, 56)) : (j == 3 ? RGB(230, 232, 236) : ((i + j) % 3 == 0 ? RGB(150, 160, 176) : RGB(176, 186, 200))));
                        markSolid(x, y, 14, 9, 2, 10); markReserved(x - 2, y - 2, 18, 13);
                    } else if (k < 8 && onQuay(x, y - 6, 13, 12)) {  // harbour crane
                        static const char* cr[13] = { ".....yy......", "....yyyyyyyyy", "....yy.....k.", "....yy.....k.", "..kkyykk...k.", "..k.yy.k..hhh", "..k.yy.k.....", "..kyyyyk.....", "..k.yy.k.....", "..kyyyyk.....", "..k.yy.k.....", ".kkkkkkkk....", ".dd....dd...." };
                        static const uint32_t c[4] = { RGB(250, 196, 30), RGB(60, 64, 76), RGB(226, 50, 44), RGB(30, 32, 40) };
                        sprite(x, y - 6, cr, 11, "ykhd", c, 0); sprite(x, y + 5, cr + 11, 2, "ykhd", c);
                        markSolid(x + 1, y + 5, 8, 2, 2, 12); markReserved(x - 2, y - 8, 17, 17);
                    }
                }
                // bollards on the edge and boats moored against it
                for (int e = 0; e < 4; e++) {
                    int len = (e < 2) ? qw : qh;
                    for (int a = 8; a < len - 8; a += 12) {
                        int bx = e == 0 ? qx + a : (e == 1 ? qx + a : (e == 2 ? qx + 3 : qx + qw - 5)), by = e == 0 ? qy + 3 : (e == 1 ? qy + qh - 5 : (e == 2 ? qy + a : qy + a));
                        if (inb(bx, by) && by > HUD_H + 1 && landM[by * W + bx] == 2) { gpx(bx, by, RGB(70, 72, 80)); gpx(bx + 1, by, RGB(40, 42, 50)); gpx(bx, by + 1, RGB(40, 42, 50)); gpx(bx + 1, by + 1, RGB(24, 26, 32)); gpx(bx, by - 1, RGB(120, 122, 130)); }
                    }
                    for (int a = 14; a < len - 14; a += 22 + rng.range(10)) {
                        bool horiz = e < 2;
                        int bw = horiz ? 11 : 5, bh = horiz ? 5 : 11;
                        int bx = horiz ? qx + a : (e == 2 ? qx - bw - 2 : qx + qw + 2), by = horiz ? (e == 0 ? qy - bh - 2 : qy + qh + 2) : qy + a;
                        bool ok = true;
                        for (int j = -3; j < bh + 3 && ok; j++) for (int i = -3; i < bw + 3; i++) { int xx = bx + i, yy = by + j; if (!inb(xx, yy) || yy < HUD_H + 2 || surf[0][yy * W + xx] != S_OFF || solid[0][yy * W + xx] || reserved[yy * W + xx] || distRoad[yy * W + xx] < 5) { ok = false; break; } }
                        if (!ok) continue;
                        uint32_t hull = rng.range(3) ? C_WHITE : hue(rng.range(256)), deck = hue(rng.range(256));
                        for (int j = 0; j < bh; j++) for (int i = 0; i < bw; i++) {
                            int al = horiz ? i : j, ac = horiz ? j : i, L = horiz ? bw : bh;
                            if ((al == 0 || al == L - 1) && ac != 2) continue;
                            if (al == L - 2 && (ac == 0 || ac == 4)) continue;
                            uint32_t c = (ac == 0 || ac == 4 || al == 0 || al >= L - 2) ? hull : (al > 2 && al < 6 ? (ac == 2 ? RGB(60, 90, 140) : C_WHITE) : shade(deck, 0.9f));
                            gpx(bx + i, by + j, c);
                            if (inb(bx + i, by + j)) { int q = (by + j) * W + bx + i; solid[0][q] = 2; objMask[q] = 1; }
                        }
                        markReserved(bx - 3, by - 3, bw + 6, bh + 6);
                    }
                }
            }
            if (o.t == O_PIER) {
                bool vert = fabsf(o.d - o.b) > fabsf(o.c - o.a);
                float len = hypotf(o.c - o.a, o.d - o.b);
                for (float a = 6; a < len - 3; a += 9 + rng.range(8)) {
                    int px = (int)(o.a + (o.c - o.a) * a / len) + (vert ? rng.range(3) - 2 : 0), py = (int)(o.b + (o.d - o.b) * a / len) + (vert ? 0 : rng.range(2) - 2);
                    if (inb(px, py) && landM[py * W + px] == 3 && !reserved[py * W + px]) { person(px, py, hue(rng.range(256)), rng); markReserved(px - 1, py - 1, 4, 5); }
                }
                blinks.push_back({ (int)o.c, (int)o.d, rng.range(64), RGB(255, 240, 120) }); gpx((int)o.c, (int)o.d + 1, RGB(60, 60, 70));
            }
        }
        // a few palms on dry land
        for (int i = 0; i < 400; i++) {
            int x = 8 + rng.range(W - 16), y = HUD_H + 14 + rng.range(H - HUD_H - 20);
            bool ok = true;
            for (int j = -6; j <= 3 && ok; j++) for (int k = -6; k <= 6; k++) if (!inb(x + k, y + j) || landM[(y + j) * W + x + k] != 1 || reserved[(y + j) * W + x + k]) { ok = false; break; }
            if (ok) { drawPalm(x, y); markReserved(x - 9, y - 12, 19, 18); }
        }
        return;
    }
    if (theme == TH_SPORT && style == 0) {      // the frame of the billiard table is a wall
        markSolid(0, HUD_H, W, 7, 2, 4); markSolid(0, H - 7, W, 7, 2, 4); markSolid(0, HUD_H, 7, H - HUD_H, 2, 4); markSolid(W - 7, HUD_H, 7, H - HUD_H, 2, 4);
    }
    if (theme == TH_SPORT && style == 0) {      // a cue and a cube of chalk lie on the cloth
        for (int k = 0; k < 96; k++) {
            int x = 96 + k, y = 172 - k / 6;
            uint32_t c = k < 3 ? RGB(120, 200, 240) : (k < 6 ? C_WHITE : (k < 60 ? RGB(226, 190, 130) : (k % 6 == 0 ? RGB(230, 230, 230) : RGB(70, 40, 30))));
            if (!freeOff(x, y) || distRoad[y * W + x] < 3) continue;
            gpx(x, y, c); if (k > 40 && freeOff(x, y + 1)) gpx(x, y + 1, shade(c, 0.7f));
            objMask[y * W + x] = 1; castH[y * W + x] = 1.2f;
        }
        for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) if (freeOff(120 + i, 96 + j)) { gpx(120 + i, 96 + j, (i == 3 || j == 3) ? RGB(20, 70, 140) : ((i == 1 && j == 1) ? RGB(60, 110, 180) : RGB(40, 110, 200))); objMask[(96 + j) * W + 120 + i] = 1; castH[(96 + j) * W + 120 + i] = 2; }
    }
    if (theme == TH_SPORT && style == 2) {      // umpire's chair, benches, ball kids and a scoreboard
        static const char* ch[9] = { ".wwww.", "wgggdw", "wgggdw", ".wwww.", ".m..m.", ".m..m.", ".mmmm.", ".m..m.", "mm..mm" }; static const uint32_t cc[4] = { RGB(240, 240, 236), RGB(40, 120, 80), RGB(24, 80, 54), RGB(150, 154, 164) };
        if (placeFree(163, 200, 4, 6)) { sprite(160, 195, ch, 9, "wgdm", cc); markSolid(160, 199, 6, 5, 2, 8); markReserved(157, 192, 12, 14); person(162, 196, C_WHITE, rng, false); }
        for (int b = 0; b < 4; b++) {
            int bx = b < 2 ? 90 + b * 110 : 90 + (b - 2) * 110, by = b < 2 ? 22 : 222;
            if (!placeFree(bx + 6, by + 1, 8, 3)) continue;
            for (int i = 0; i < 13; i++) { gpx(bx + i, by, RGB(60, 150, 110)); gpx(bx + i, by + 1, RGB(40, 110, 80)); } gpx(bx + 1, by + 2, RGB(40, 44, 50)); gpx(bx + 11, by + 2, RGB(40, 44, 50));
            person(bx + 3, by - 1, C_WHITE, rng, false); person(bx + 8, by - 1, RGB(250, 230, 80), rng, false);
            markSolid(bx, by, 13, 2, 1, 2); markReserved(bx - 2, by - 3, 17, 8);
        }
        static const int kid[6][2] = { { 22, 60 }, { 22, 186 }, { 296, 60 }, { 296, 186 }, { 150, 26 }, { 170, 222 } };
        for (int k = 0; k < 6; k++) if (placeFree(kid[k][0], kid[k][1] + 1, 2)) { person(kid[k][0], kid[k][1], RGB(60, 90, 200), rng); markObj(kid[k][0], kid[k][1], 2, 3); markReserved(kid[k][0] - 1, kid[k][1] - 1, 4, 5); }
        if (placeFree(40, 24, 13, 6)) {
            for (int j = 0; j < 9; j++) for (int i = 0; i < 24; i++) gpx(28 + i, 19 + j, (i == 0 || j == 0 || i == 23 || j == 8) ? RGB(60, 64, 72) : ((j == 2 || j == 5) && i > 2 && i < 21 && (i % 4) ? (i > 14 ? RGB(255, 226, 60) : RGB(240, 240, 240)) : RGB(16, 18, 26)));
            markSolid(28, 19, 24, 9, 2, 6); markReserved(26, 17, 28, 13);
        }
    }
    if (theme == TH_SPORT && style == 1) {      // two bats lying on the table
        for (int b = 0; b < 2; b++) {
            int cx = b ? 244 : 76, cy = b ? 122 : 118; uint32_t rub = b ? RGB(40, 44, 56) : RGB(214, 40, 44);
            for (int j = -10; j <= 10; j++) for (int i = -10; i <= 10; i++) {
                float d = hypotf((float)i, (float)j);
                if (d > 9.6f) continue;
                gpx(cx + i, cy + j, d > 8.4f ? RGB(196, 150, 96) : (i + j < -6 ? shade(rub, 1.25f) : (i + j > 7 ? shade(rub, 0.75f) : rub)));
            }
            for (int k = 8; k < 20; k++) for (int t = -1; t <= 1; t++) { int hx = cx + (b ? -k : k) * 7 / 10 + (b ? -t : t), hy = cy + k * 7 / 10 - t; gpx(hx, hy, t == -1 ? RGB(226, 180, 120) : (t == 0 ? RGB(196, 150, 96) : RGB(140, 100, 60))); gpx(hx, hy + 1, RGB(140, 100, 60)); }
            markSolid(cx - 9, cy - 9, 19, 19, 2, 2); markSolid(b ? cx - 15 : cx + 5, cy + 5, 11, 10, 2, 2); markReserved(cx - 16, cy - 12, 33, 30);
        }
    }
    if (theme == TH_SPORT && style == 3) {      // a football pitch inside the running track
        for (int y = HUD_H + 2; y < H - 1; y++) for (int x = 1; x < W - 1; x++) {
            if (!freeOff(x, y) || distRoad[y * W + x] < 7 || !insideTrack((float)x, (float)y)) continue;
            int p = y * W + x;
            float dc = hypotf(x - 160.0f, y - 125.0f);
            bool line = x == 160 || fabsf(dc - 13) < 0.6f || dc < 1.2f || distRoad[p] == 9;
            ground[p] = line ? RGB(240, 244, 240) : (((x / 12) & 1) ? RGB(60, 164, 70) : RGB(48, 146, 60));
        }
        // field events on the grass: long jump, high jump, shot put, goals
        auto block = [&](int x0, int y0, int w, int h, uint32_t a, uint32_t b, bool sol) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) if (freeOff(x0 + i, y0 + j)) { gpx(x0 + i, y0 + j, (i == 0 || j == 0 || i == w - 1 || j == h - 1) ? b : (((i + j) % 5) == 0 ? shade(a, 0.9f) : a)); objMask[(y0 + j) * W + x0 + i] = 1; if (sol) castH[(y0 + j) * W + x0 + i] = 2.5f; } };
        block(96, 100, 30, 4, RGB(196, 72, 52), RGB(244, 244, 244), false);          // run-up
        block(126, 98, 14, 8, RGB(240, 216, 150), RGB(250, 250, 250), false);        // sand pit
        block(196, 140, 14, 9, RGB(60, 110, 220), RGB(30, 60, 150), true);           // high jump mat
        for (int i = 0; i < 16; i++) if (freeOff(195 + i, 137)) gpx(195 + i, 137, (i == 0 || i == 15) ? RGB(60, 64, 72) : ((i / 3) & 1 ? RGB(226, 50, 44) : C_WHITE));
        for (int j = -5; j <= 5; j++) for (int i = -5; i <= 5; i++) { float d = hypotf((float)i, (float)j); if (d < 5.4f && freeOff(200 + i, 104 + j)) gpx(200 + i, 104 + j, d > 4.2f ? C_WHITE : RGB(150, 150, 150)); }
        for (int k = 6; k < 26; k++) { if (freeOff(200 + k, 104 - k / 3)) gpx(200 + k, 104 - k / 3, C_WHITE); if (freeOff(200 + k, 104 + k / 3)) gpx(200 + k, 104 + k / 3, C_WHITE); }
        for (int g = 0; g < 2; g++) for (int j = -6; j <= 6; j++) for (int i = 0; i < 3; i++) { int gx = g ? 236 + i : 82 + i; if (freeOff(gx, 125 + j)) gpx(gx, 125 + j, (abs(j) == 6 || i == (g ? 2 : 0)) ? C_WHITE : (((i + j) & 1) ? RGB(200, 210, 200) : ground[(125 + j) * W + gx])); }
        for (int k = 0; k < 9; k++) if (placeFree(100 + k * 14, 150, 2)) { person(100 + k * 14 + rng.range(5), 146 + rng.range(8), k < 5 ? RGB(226, 50, 44) : RGB(60, 90, 220), rng, false); }
    }
    for (int i = 0; i < 300; i++) {
        int x = 3 + rng.range(W - 6), y = HUD_H + 3 + rng.range(H - HUD_H - 6);
        if (distRoad[y * W + x] < 4 || !freeOff(x, y) || !freeOff(x + 1, y) || !freeOff(x, y - 1)) continue;
        int k = rng.range(6);
        switch (theme) {
        case TH_CANDY: { uint32_t c = hue(rng.range(256)); gpx(x, y, c); if (k < 3) gpx(x + 1, y, c); else gpx(x, y - 1, c); } break;      // sprinkles
        case TH_HORROR:
            if (style == 2) { if (k < 3) gpx(x, y, RGB(60, 40, 24)); else { gpx(x, y, RGB(150, 110, 66)); gpx(x + 1, y, RGB(90, 60, 34)); } }
            else if (k == 0) { gpx(x, y, RGB(226, 222, 204)); gpx(x + 1, y, RGB(226, 222, 204)); gpx(x - 1, y - 1, RGB(190, 186, 170)); }             // a bone
            else if (k == 1) { gpx(x, y, RGB(120, 12, 18)); gpx(x + 1, y, RGB(150, 20, 24)); }
            else if (k < 4) { gpx(x, y, RGB(34, 40, 32)); gpx(x, y - 1, RGB(40, 48, 36)); gpx(x + 1, y - 1, RGB(34, 40, 32)); }
            else gpx(x, y, RGB(96, 92, 100));
            break;
        case TH_SCIFI: if (style == 2) { gpx(x, y, RGB(50, 46, 70)); gpx(x + 1, y, RGB(50, 46, 70)); gpx(x, y - 1, RGB(110, 106, 136)); } else if (k < 2) gpx(x, y, k ? RGB(0, 200, 230) : RGB(230, 60, 200)); break;
        case TH_SPORT: if (style >= 2 && k < 2 && !(style == 2 && x > 40 && x < 280 && y > 56 && y < 194)) { gpx(x, y, k ? C_WHITE : RGB(255, 230, 80)); } break;
        default: break;
        }
    }
}

// ------------------------------------------------------------------ building the machines

void worldBuildEarly() {
    // nets: a low barrier across the road; the samples under it are marked so that CPU cars and
    // the respawn code treat it like a gap (a run-up is needed)
    for (const Obj& o : def->o) {
        if (o.t != O_NET) continue;
        float len = hypotf(o.c - o.a, o.d - o.b);
        bool vert = fabsf(o.d - o.b) > fabsf(o.c - o.a);
        for (float d = 0; d <= len; d += 0.5f) {
            int x = (int)(o.a + (o.c - o.a) * d / len), y = (int)(o.b + (o.d - o.b) * d / len);
            for (int t = 0; t < 2; t++) { int xx = x + (vert ? t : 0), yy = y + (vert ? 0 : t); if (inb(xx, yy) && yy >= HUD_H) netMask[yy * W + xx] = 1; }
        }
        for (int k = 0; k < NS; k++) {
            float px = sm[k].x - o.a, py = sm[k].y - o.b, t = clampf((px * (o.c - o.a) + py * (o.d - o.b)) / (len * len), 0, 1);
            if (hypotf(px - (o.c - o.a) * t, py - (o.d - o.b) * t) < 1.5f) sm[k].f |= fHOP;
        }
    }
    // warp gates: one at each end of an fW stretch
    for (int k = 1; k < NS; k++) {
        if (!(sm[k].f & fW) || (sm[k - 1].f & fW)) continue;
        int e = k; while (e < NS && (sm[e].f & fW)) e++;
        Warp w{};
        w.kA = k - 1; w.kB = (e + (int)(4 / spacing)) % NS;
        const Sample& A = sm[w.kA], & B = sm[w.kB];
        w.ax = A.x; w.ay = A.y; w.bx = B.x; w.by = B.y; w.aa = atan2f(A.ty, A.tx); w.ab = atan2f(B.ty, B.tx); w.hw = A.w / 2;
        warps.push_back(w);
    }
}

// A giant donut standing upright, half buried: an arch across a road that runs up or down the screen.
// (cx, cy) is where the road passes under it; the cars drive through the hole and are hidden behind the dough.
static void buildDonut(float cx, float cy, float R, float r, Rng& rng) {
    uint32_t dough = RGB(232, 176, 100), icing = rng.range(2) ? RGB(255, 120, 186) : RGB(126, 72, 42);
    if (rng.range(3) == 0) icing = RGB(252, 250, 244);
    int seed = rng.range(1000);
    // the far side first (two pixels up the screen, darker), then the near side: that gives it its thickness
    for (int pass = 0; pass < 2; pass++) {
        int lift = pass == 0 ? 3 : 0;
        for (int h = 0; h <= (int)R + 1; h++) for (int x = (int)(cx - R) - 1; x <= (int)(cx + R) + 1; x++) {
            float dx = x + 0.5f - cx, d = hypotf(dx, h + 0.5f);
            if (d > R || d < r) continue;
            int y = (int)cy - h - lift;
            float t = (d - r) / (R - r), th = atan2f(h + 0.5f, dx);
            // a round tube lit from the top left
            float lit = 0.74f + 0.26f * sinf(t * PI) + 0.22f * (cosf(th) * -0.6f + sinf(th) * 0.8f) * (t - 0.35f) * 2;
            float drip = 0.36f + 0.13f * sinf(th * 9 + seed) + 0.07f * sinf(th * 17 + seed * 3);
            bool ice = t > drip && h > 2;
            uint32_t c = shade(ice ? icing : dough, clampf(lit, 0.5f, 1.35f));
            if (ice && t > drip + 0.12f && (hash2(x, h, seed) % 6) == 0) c = hue(hash2(x, h, seed + 1) & 255);      // sprinkles
            if (ice && fabsf(t - drip) < 0.07f) c = shade(icing, 0.72f);
            if (t < 0.10f) c = shade(dough, 0.55f); else if (t > 0.93f) c = shade(ice ? icing : dough, 0.62f);
            if (pass == 0) c = shade(t < 0.5f ? dough : (h > 2 ? icing : dough), 0.5f);
            opx(x, y, c, 99);
            if (inb(x, y)) reserved[y * W + x] = 1;
        }
    }
    // its two feet are solid, with crumbs around them
    for (int side = -1; side <= 1; side += 2) {
        int x0 = (int)(cx + side * (r + R) / 2 - (R - r) / 2), w = (int)(R - r) + 1;
        markSolid(x0, (int)cy - 3, w, 5, 2, 0);
        for (int i = -2; i < w + 2; i++) for (int j = 1; j <= 3; j++) if ((hash2(x0 + i, j, seed) % (j + 1)) == 0 && inb(x0 + i, (int)cy + j) && !roadLike(surf[0][((int)cy + j) * W + x0 + i])) gpx(x0 + i, (int)cy + j, (hash2(x0 + i, j, seed + 2) & 1) ? RGB(206, 150, 84) : RGB(250, 226, 170));
    }
    markReserved((int)(cx - R) - 4, (int)(cy - R) - 6, (int)(2 * R) + 26, (int)R + 26);
    arches.push_back({ cx, cy, R, r });
}
// the shadow of an arch falls on the ground to its lower right
void worldBuildLate() {
    static uint8_t done[N];
    if (arches.empty()) return;
    memset(done, 0, sizeof(done));
    for (const Arch& a : arches)
        for (float h = 0; h <= a.R; h += 0.5f) for (float dx = -a.R; dx <= a.R; dx += 0.5f) {
            float d = hypotf(dx, h);
            if (d > a.R || d < a.r) continue;
            for (int dep = 0; dep <= 3; dep += 3) {          // near and far side of the tube
                int x = (int)(a.cx + dx + h / 1.15f), y = (int)(a.cy - dep + h / 1.15f);
                if (!inb(x, y) || y < HUD_H) continue;
                int p = y * W + x;
                if (done[p] || shadowM[p] || solid[0][p] == 2 || overA[p]) continue;
                done[p] = 1; shadowM[p] = 1; ground[p] = shade(ground[p], 0.62f);
            }
        }
}

static void drawNet(const Obj& o) {
    float len = hypotf(o.c - o.a, o.d - o.b);
    for (int p = HUD_H * W; p < N; p++) {
        if (!netMask[p]) continue;
        int x = p % W, y = p / W;
        float px = x - o.a, py = y - o.b, t = (px * (o.c - o.a) + py * (o.d - o.b)) / (len * len);
        if (t < -0.02f || t > 1.02f || hypotf(px - (o.c - o.a) * t, py - (o.d - o.b) * t) > 2.5f) continue;
        bool second = netMask[p - 1] && netMask[p - W] ? true : (fabsf(o.d - o.b) > fabsf(o.c - o.a) ? netMask[p - 1] : netMask[p - W]);
        int a = (int)(t * len);
        uint32_t c;
        switch (theme) {
        case TH_WATER: c = (hash2(x, y, 9) % 3) ? (second ? RGB(96, 92, 96) : RGB(150, 146, 144)) : RGB(214, 236, 250); break;          // a reef
        case TH_CANDY: c = ((a / 2) & 1) ? RGB(40, 30, 34) : RGB(236, 40, 56); break;                                                 // liquorice
        case TH_HORROR: c = (a % 4 == 0) ? RGB(226, 222, 204) : (second ? RGB(40, 38, 44) : RGB(84, 80, 90)); break;                   // iron fence
        case TH_SCIFI: c = ((a + (second ? 1 : 0)) & 1) ? RGB(255, 60, 200) : RGB(255, 190, 240); break;                               // force field
        default: c = second ? RGB(120, 124, 130) : ((a & 1) ? C_WHITE : RGB(214, 218, 224)); break;                                    // a net
        }
        if (a < 2 || a > (int)len - 2) c = theme == TH_SPORT ? RGB(60, 64, 72) : shade(c, 0.6f);          // posts
        ground[p] = c; objMask[p] = 1; castH[p] = std::max(castH[p], 3.5f); reserved[p] = 1;
        if (theme == TH_WATER) {        // more rocks and surf around the reef (the barrier itself stays thin)
            for (int q = 0; q < 3; q++) {
                uint32_t h = hash2(x, y, 20 + q); int xx = x + (int)(h % 7) - 3, yy = y + (int)((h >> 4) % 5) - 2;
                if (!inb(xx, yy) || yy < HUD_H || netMask[yy * W + xx] || objMask[yy * W + xx]) continue;
                ground[yy * W + xx] = (h >> 8) % 3 == 0 ? RGB(214, 236, 250) : ((h >> 8) % 3 == 1 ? RGB(120, 116, 118) : RGB(84, 82, 90)); objMask[yy * W + xx] = 1;
            }
        } else if (theme == TH_SPORT && inb(x + 2, y) && !netMask[p + 1] && !netMask[p + 2] && a >= 2 && a <= (int)len - 2) {
            ground[p + 1] = (a & 1) ? RGB(60, 110, 70) : RGB(40, 84, 54); objMask[p + 1] = 1;                // the mesh below the white band
        }
    }
}

void worldBuild(Rng& rng) {
    for (const Obj& o : def->o) {
        switch (o.t) {
        case O_DONUT: buildDonut(o.a, o.b, o.c, o.d, rng); break;
        case O_BUMPER: bumpers.push_back({ o.a, o.b, o.c, 0 }); break;
        case O_BOUNCE: pads.push_back({ o.a, o.b, o.c, 0 }); break;
        case O_WHIRL: whirls.push_back({ o.a, o.b, o.c }); break;
        case O_NET: drawNet(o); break;
        case O_ROLLER: {
            Roller r{};
            r.ax = o.a; r.ay = o.b; r.bx = o.c; r.by = o.d; r.speed = o.e > 0 ? o.e : 60; r.oneWay = o.f != 0; r.dir = 1; r.x = r.ax; r.y = r.ay;
            r.u = rng.f() * 0.9f;
            r.r = theme == TH_SPORT ? (style == 0 ? 4.0f : 2.2f) : (theme == TH_HORROR ? 5.0f : (theme == TH_WATER ? 4.5f : 3.5f));
            rollers.push_back(r);
        } break;
        case O_HURDLE: {
            const Sample& s = sm[nearIdx[pixAt(o.a, o.b)]];
            critters.push_back({ o.a, o.b, o.a, o.b, 0, 0, -s.ty, s.tx, 3 });
        } break;
        case O_ZOMBIES: {
            int kind = theme == TH_CANDY ? 2 : (theme == TH_HORROR && style == 2 ? 1 : 0), made = 0;
            for (int tries = 0; tries < 900 && made < (int)o.a; tries++) {
                int k = rng.range(NS);
                const Sample& s = sm[k];
                if (k * spacing < 46 || (NS - k) * spacing < 24 || (s.f & (fG | fB | fT | fY | fHOP)) || s.ramp > 0) continue;
                float side = rng.range(2) ? 1.0f : -1.0f, d = s.w / 2 + 4 + rng.f() * 8;
                if (kind == 1 && rng.range(2)) d = rng.f() * s.w * 0.3f;            // spiders also live on the planks
                int x = (int)(s.x - s.ty * d * side), y = (int)(s.y + s.tx * d * side);
                if (!inb(x, y) || y < HUD_H + 6 || y > H - 5 || x < 5 || x > W - 6) continue;
                int p = y * W + x;
                if (solid[0][p] || surf[0][p] == S_WATER || surf[0][p] == S_NONE || surf[1][p] != S_NONE || objMask[p]) continue;
                bool crowded = false;
                for (const Critter& c : critters) if (hypotf(c.hx - x, c.hy - y) < 16) crowded = true;
                if (crowded) continue;
                if (kind == 0 && surf[0][p] == S_OFF) {     // an open grave to climb out of
                    for (int j = -1; j <= 1; j++) for (int i = -3; i <= 3; i++) if (abs(i) + abs(j) * 2 < 5 && inb(x + i, y + j) && surf[0][(y + j) * W + x + i] == S_OFF && !objMask[(y + j) * W + x + i]) gpx(x + i, y + j, (abs(i) == 3 || j == -1) ? RGB(84, 62, 46) : RGB(26, 20, 20));
                }
                critters.push_back({ (float)x, (float)y, (float)x, (float)y, rng.f() * 20, 0, 0, 0, kind });
                made++;
            }
        } break;
        default: break;
        }
    }
    if (theme == TH_HORROR) {
        ghosts[0] = { 60, 80, 0 }; ghosts[1] = { 250, 170, 3 };
        if (style == 2) for (int i = 0; i < 9; i++) danglers.push_back({ 14.0f + rng.range(W - 28), (float)HUD_H, 30.0f + rng.range(150), rng.f() * 6 });
    }
}

// ------------------------------------------------------------------ simulation

static void stain(float x, float y, const uint32_t* cols, int n) {
    for (int i = 0; i < n && stains < 2600; i++) {
        int px = (int)(x + (grng.f() - 0.5f) * 9), py = (int)(y + (grng.f() - 0.5f) * 7);
        if (!inb(px, py) || py < HUD_H) continue;
        int p = py * W + px;
        if (surf[0][p] == S_WATER || surf[0][p] == S_NONE || objMask[p] || solid[0][p] || surf[1][p] != S_NONE) continue;
        ground[p] = shade(cols[grng.range(2)], 0.8f); stains++;
    }
}

void worldUpdate(float dt) {
    for (Critter& z : critters) {
        if (z.dead > 0) {
            z.dead -= dt;
            if (z.dead <= 0) {
                bool carNear = false;
                for (const Car& c : cars) if (c.on && hypotf(c.x - z.hx, c.y - z.hy) < 22) carNear = true;
                if (carNear) z.dead = 0.6f; else { z.x = z.hx; z.y = z.hy; z.ph = 0; }
            }
            continue;
        }
        if (z.on >= 0) {                                     // clinging to a car: it drags and swerves
            Car& c = cars[z.on];
            float sp = hypotf(c.vx, c.vy);
            z.hold -= dt; z.x = c.x; z.y = c.y; z.ph += dt * 10;
            if (c.dead > 0 || !c.on) { z.on = -1; z.dead = 4; continue; }
            float k = 1 - 1.7f * dt; c.vx *= k; c.vy *= k; c.a += sinf(z.ph * 2.3f) * 0.9f * dt;
            if (z.hold <= 0 || sp > 62 || c.air) {           // shaken off
                z.on = -1; z.dead = 5 + grng.f() * 4; snd::squish(z.x);
                burst(z.x, z.y, 14, 40, 0.5f, BLOOD, 3, 70); stain(z.x, z.y, BLOOD, 10);
            }
            continue;
        }
        float mvx = 0, mvy = 0;
        if (z.kind != 3) {
            // run at the nearest car, or shuffle about near home
            const Car* best = nullptr; float bd = z.kind == 0 ? 78.0f : (z.kind == 1 ? 34.0f : 0.0f);
            for (const Car& c : cars) if (c.on && c.dead <= 0 && c.z - c.gvis < 3 && raceT > 0) { float d = hypotf(c.x - z.x, c.y - z.y); if (d < bd) { bd = d; best = &c; } }
            float tx = z.hx + cosf(z.ph * 0.37f + z.hx) * 13, ty = z.hy + sinf(z.ph * 0.29f + z.hy) * 9, sp = z.kind == 2 ? 7.0f : 6.0f;
            if (best && hypotf(z.x - z.hx, z.y - z.hy) < 90) { tx = best->x + best->vx * 0.35f; ty = best->y + best->vy * 0.35f; sp = z.kind == 1 ? 17.0f : 27.0f; }
            float dx = tx - z.x, dy = ty - z.y, d = hypotf(dx, dy);
            if (d > 1) {
                mvx = dx / d * sp; mvy = dy / d * sp;
                int q = pixAt(z.x + mvx * dt * 6, z.y + mvy * dt * 6);
                bool ok = !solid[0][q] && surf[0][q] != S_WATER && surf[0][q] != S_NONE && !netMask[q] && surf[1][q] == S_NONE && q >= (HUD_H + 3) * W;
                if (ok) { z.x += mvx * dt; z.y += mvy * dt; }
                z.dx = mvx;                                    // which way it faces
            }
            z.ph += dt * (best ? 12 : 3);
        }
        for (Car& c : cars) {
            if (!c.on || c.dead > 0 || c.z - ht[0][pixAt(c.x, c.y)] > 3) continue;
            float d;
            if (z.kind == 3) {                               // a bar across the lane
                float px = c.x - z.x, py = c.y - z.y, t = clampf(px * z.dx + py * z.dy, -4, 4);
                d = hypotf(px - z.dx * t, py - z.dy * t) + 0.6f;
            } else d = hypotf(c.x - z.x, c.y - z.y);
            if (d > 4.8f) continue;
            float sp = hypotf(c.vx, c.vy);
            if (z.kind == 3) {
                if (sp < 12) continue;
                c.vx *= 0.72f; c.vy *= 0.72f; z.dead = 7; snd::clack(z.x);
                burst(z.x, z.y, 6, 30, 0.3f, CHIPS, 3, 60);
            } else if (z.kind == 0 && sp < 46 && raceT > 0 && c.invuln <= 0) {
                bool taken = false;
                for (const Critter& o : critters) if (o.on == c.slot) taken = true;
                if (taken) continue;
                z.on = c.slot; z.hold = 1.7f; snd::crash(0.5f, c.x);         // it jumps onto the car
            } else {
                const uint32_t* cols = z.kind == 0 ? BLOOD : (z.kind == 1 ? GOO : BEAR);
                float slow = z.kind == 0 ? 0.6f : 0.68f;
                c.vx *= slow; c.vy *= slow; z.dead = 6 + grng.f() * 5; snd::squish(z.x);
                burst(z.x, z.y, 22, 46, 0.55f, cols, 3, 70);
                stain(z.x + c.vx * 0.03f, z.y + c.vy * 0.03f, cols, 18);
            }
            break;
        }
    }
    for (Roller& r : rollers) {
        float lx = r.bx - r.ax, ly = r.by - r.ay, len = hypotf(lx, ly);
        if (r.cd > 0) r.cd -= dt;
        if (r.wait > 0) { r.wait -= dt; continue; }
        r.u += r.dir * r.speed * dt / len;
        if (r.u >= 1) { if (r.oneWay) { r.u = 0; r.wait = 1.4f; } else { r.u = 1; r.dir = -1; } }
        if (r.u <= 0) { r.u = 0; r.dir = 1; }
        r.x = r.ax + lx * r.u; r.y = r.ay + ly * r.u; r.vx = lx / len * r.speed * r.dir; r.vy = ly / len * r.speed * r.dir;
        if (r.cd > 0) continue;
        for (Car& c : cars) {
            if (!c.on || c.dead > 0 || c.invuln > 0 || c.z - ht[0][pixAt(c.x, c.y)] > 4) continue;
            float dx = c.x - r.x, dy = c.y - r.y, d = hypotf(dx, dy);
            if (d > r.r + 3.6f) continue;
            if (d < 0.1f) { dx = 1; dy = 0; d = 1; }
            if (theme == TH_HORROR && style == 2) { hitCar(c, 0.8f, 0.8f, 0); snd::warp(c.x); burst(c.x, c.y, 10, 30, 0.4f, SPARK, 1); }       // a ghost: it goes straight through
            else if (theme == TH_HORROR) { hitCar(c, 0.9f, 0.6f, 0); snd::crash(0.9f, c.x); burst(c.x, c.y, 10, 40, 0.3f, SPARK, 1); }                  // the scarecrow swings its scythe
            else { c.vx += r.vx * 0.8f + dx / d * 46; c.vy += r.vy * 0.8f + dy / d * 46; hitCar(c, 0.4f, 1.0f, 0); snd::crash(0.7f, c.x); burst(c.x, c.y, 6, 30, 0.25f, SPARK, 1); }
            r.cd = 0.6f; break;
        }
    }
    for (Bumper& b : bumpers) if (b.flash > 0) b.flash -= dt;
    for (Pad& p : pads) if (p.squash > 0) p.squash -= dt;
    for (Warp& w : warps) if (w.flash > 0) w.flash -= dt;
}

void worldCar(Car& c, float dt) {
    float sp = hypotf(c.vx, c.vy);
    for (Bumper& b : bumpers) {
        float dx = c.x - b.x, dy = c.y - b.y, d = hypotf(dx, dy), reach = b.r + 3.2f;
        if (d > reach || c.z - c.gvis > 5) continue;
        if (d < 0.1f) { dx = 1; dy = 0; d = 1; }
        float nx = dx / d, ny = dy / d, vn = c.vx * nx + c.vy * ny;
        c.x = b.x + nx * reach; c.y = b.y + ny * reach;
        if (vn < 0) { c.vx -= 2 * vn * nx; c.vy -= 2 * vn * ny; vn = -vn; }
        if (vn < 85) { c.vx += nx * (85 - vn); c.vy += ny * (85 - vn); }
        if (b.flash <= 0) { snd::boing(b.x); burst(b.x + nx * b.r, b.y + ny * b.r, 5, 30, 0.25f, SPARK, 3); }
        b.flash = 0.25f;
    }
    if (c.air) return;
    for (Pad& p : pads) if (hypotf(c.x - p.x, c.y - p.y) < p.r && sp > 12) { c.air = true; c.vz = 80; c.slopeVz = 0; p.squash = 0.35f; snd::boing(p.x); break; }
    for (const Whirl& w : whirls) {
        float dx = c.x - w.x, dy = c.y - w.y, d = hypotf(dx, dy);
        if (d > w.r || d < 0.5f) continue;
        float k = 1 - d / w.r, nx = dx / d, ny = dy / d;
        c.vx += (-nx * 70 - ny * 95) * k * dt; c.vy += (-ny * 70 + nx * 95) * k * dt;
        c.a += 1.5f * k * dt;
    }
    for (Warp& w : warps) {
        float ca = cosf(w.aa), sa = sinf(w.aa), dx = c.x - w.ax, dy = c.y - w.ay;
        float al = dx * ca + dy * sa, lat = -dx * sa + dy * ca;
        if (al < -1.5f || al > 6 || fabsf(lat) > w.hw + 3 || c.vx * ca + c.vy * sa < 4) continue;
        float da = w.ab - w.aa, cb = cosf(w.ab), sb = sinf(w.ab), cd = cosf(da), sd = sinf(da);
        static const uint32_t wc[3] = { RGB(255, 255, 255), RGB(120, 240, 255), RGB(200, 120, 255) };
        burst(c.x, c.y, 16, 50, 0.4f, wc, 3);
        c.x = w.bx - sb * lat * 0.8f; c.y = w.by + cb * lat * 0.8f; c.a += da;
        float vx = c.vx * cd - c.vy * sd; c.vy = c.vx * sd + c.vy * cd; c.vx = vx;
        c.idx = w.kB; c.z = c.gvis = sm[w.kB].h; c.lostT = 0;
        int sec = c.idx * NSECT / NS; if (sec != (c.sector + 1) % NSECT) c.sector = sec;     // the gate skipped a sector or two
        burst(c.x, c.y, 16, 50, 0.4f, wc, 3);
        w.flash = 0.3f; snd::warp(c.x);
        break;
    }
}

void worldAmbient(float dt) {
    if (weather == WX_STORM) {
        if (flashT > 0) flashT -= dt;
        nextFlash -= dt;
        if (nextFlash <= 0) { nextFlash = 4 + grng.f() * 7; flashT = 0.32f; if (!attract) snd::thunder(); }
    }
    for (Dangler& d : danglers) d.ph += dt * 0.5f;
    for (Ghost& g : ghosts) g.ph += dt;
    // hovercraft and boats leave a trail
    for (const Car& c : cars) {
        if (!c.on || c.dead > 0 || vehicle == VH_CAR) continue;
        float sp = hypotf(c.vx, c.vy), fx = cosf(c.a), fy = sinf(c.a);
        if (vehicle == VH_BOAT && sp > 14 && !c.air && (frameNo & 1)) {
            float side = (frameNo & 2) ? 1.0f : -1.0f;
            addPart(c.x - fx * 4 - fy * 2 * side, c.y - fy * 4 + fx * 2 * side, -fx * 6 - fy * side * (10 + sp * 0.12f), -fy * 6 + fx * side * (10 + sp * 0.12f), 0.55f, (frameNo & 4) ? C_WHITE : RGB(170, 214, 244));
        }
        if (vehicle == VH_HOVER && sp > 10 && (frameNo % 3) == 0) addPart(c.x - fx * 5, c.y - fy * 5 - 2, -fx * 14 + (grng.f() - 0.5f) * 8, -fy * 14 + (grng.f() - 0.5f) * 8, 0.22f, (frameNo & 1) ? RGB(120, 240, 255) : RGB(40, 120, 255));
    }
}

// ------------------------------------------------------------------ drawing

static void disc(float cx, float cy, float r, uint32_t base, float lightK) {
    for (int y = (int)(cy - r); y <= (int)(cy + r); y++) for (int x = (int)(cx - r); x <= (int)(cx + r); x++) {
        float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = hypotf(dx, dy);
        if (d > r) continue;
        float l = 1.0f - lightK * (dx + dy) / (r * 1.6f) - 0.25f * (d / r) * (d / r);
        pset(x, y, shade(base, clampf(l, 0.45f, 1.5f)));
    }
}
static void discShadow(float cx, float cy, float r) {
    for (int y = (int)(cy - r); y <= (int)(cy + r); y++) for (int x = (int)(cx - r); x <= (int)(cx + r); x++)
        if (hypotf(x + 0.5f - cx, y + 0.5f - cy) <= r && y >= HUD_H) pdark(x, y, 0.6f);
}

void worldDrawLow(int stage) {
    if (stage == 0) {
    // the open sea moves
    if (!seaPix.empty()) {
        int t = frameNo >> 2;
        for (int p : seaPix) {
            int x = p % W, y = p / W, row = y + (x >> 5);
            int ph = (x + t + row * 11) % 19;
            if ((row & 3) == 0 && ph < 5) fb[p] = shade(fb[p], ph == 2 ? 1.34f : 1.16f);
            else if ((row & 3) == 2 && ((x * 2 - t + row * 7) % 37 + 37) % 37 < 3) fb[p] = shade(fb[p], 0.86f);
        }
    }
    // turbo pads: chevrons run forwards
    if (!boostPix.empty()) {
        int t = frameNo / 2;
        for (size_t i = 0; i < boostPix.size(); i++) {
            int ph = ((boostVal[i] - t) % 12 + 12) % 12, p = boostPix[i];
            uint32_t c = theme == TH_SCIFI ? (ph < 3 ? C_WHITE : (ph < 6 ? RGB(0, 232, 255) : RGB(20, 60, 130))) : (ph < 4 ? RGB(255, 224, 0) : (ph < 8 ? RGB(232, 48, 28) : RGB(255, 140, 0)));
            fb[p] = shadowM[p] ? shade(c, 0.62f) : c;
        }
    }
    for (const Whirl& w : whirls) {
        float t = frameNo * 0.06f;
        for (int y = (int)(w.y - w.r); y <= (int)(w.y + w.r); y++) for (int x = (int)(w.x - w.r); x <= (int)(w.x + w.r); x++) {
            if (!inb(x, y) || y < HUD_H) continue;
            float dx = x + 0.5f - w.x, dy = y + 0.5f - w.y, d = hypotf(dx, dy);
            if (d > w.r || solid[0][y * W + x]) continue;
            float arm = sinf(atan2f(dy, dx) * 2 - d * 0.55f + t * 2.2f), k = 1 - d / w.r;
            uint32_t c = fb[y * W + x];
            if (arm > 0.72f) c = mixc(c, C_WHITE, 0.25f + 0.6f * k); else c = shade(c, 1 - 0.45f * k);
            if (d < 1.6f) c = RGB(8, 30, 80);
            fb[y * W + x] = c;
        }
    }
    for (const Pad& p : pads) {
        float sq = p.squash > 0 ? sinf(p.squash * 40) * 1.5f : sinf(frameNo * 0.12f) * 0.4f, r = p.r + sq;
        for (int y = (int)(p.y - r); y <= (int)(p.y + r); y++) for (int x = (int)(p.x - r); x <= (int)(p.x + r); x++) {
            float dx = x + 0.5f - p.x, dy = y + 0.5f - p.y, d = hypotf(dx, dy);
            if (d > r) continue;
            uint32_t c = d > r - 1.2f ? RGB(150, 20, 60) : ((((int)(d / 2.2f)) & 1) ? RGB(255, 110, 170) : RGB(236, 50, 110));
            if (dx + dy < -r * 0.6f) c = mixc(c, C_WHITE, 0.5f);
            pset(x, y, c);
        }
    }
    return;
    }
    for (const Fin& f : fins) {
        float a = f.ph + frameNo * 0.012f;
        int x = (int)(f.cx + cosf(a) * f.r * 1.5f), y = (int)(f.cy + sinf(a) * f.r);
        if (!inb(x - 2, y - 3) || !inb(x + 2, y + 1) || y < HUD_H + 3) continue;
        bool right = -sinf(a) > 0;
        pset(x, y - 2, RGB(70, 80, 96)); pset(x, y - 1, RGB(90, 100, 118)); pset(x + (right ? -1 : 1), y - 1, RGB(60, 70, 86)); pset(x, y, RGB(90, 100, 118)); pset(x + (right ? -1 : 1), y, RGB(60, 70, 86));
        pset(x + (right ? -2 : 2), y + 1, RGB(214, 236, 250)); pset(x + (right ? -3 : 3), y + 1, RGB(170, 214, 244)); pset(x + (right ? 1 : -1), y + 1, RGB(214, 236, 250));
    }
    for (const Blink& b : blinks) {
        int ph = (frameNo / 5 + b.ph) & 63;
        bool eyes = ((b.col >> 16) & 255) > 240 && ((b.col >> 8) & 255) < 60;
        if (eyes ? (ph < 44) : (ph % 7 != 3 || theme == TH_HORROR)) pset(b.x, b.y, (ph & 1) && !eyes ? shade(b.col, 0.82f) : b.col);
    }
    // warp gates
    for (const Warp& w : warps) for (int e = 0; e < 2; e++) {
        float gx = e ? w.bx : w.ax, gy = e ? w.by : w.ay, an = e ? w.ab : w.aa, nx = -sinf(an), ny = cosf(an);
        for (float d = -w.hw - 2; d <= w.hw + 2; d += 0.5f) for (int k = -1; k <= 1; k++) {
            int x = (int)(gx + nx * d + cosf(an) * k * 0.9f), y = (int)(gy + ny * d + sinf(an) * k * 0.9f);
            bool post = fabsf(d) > w.hw;
            uint32_t c = post ? RGB(190, 200, 220) : hue((int)(d * 9 + frameNo * (e ? -6 : 6)) & 255);
            if (!post && (k == 0 || w.flash > 0)) c = mixc(c, C_WHITE, 0.6f);
            if (y >= HUD_H) pset(x, y, c);
        }
    }
    // zombies and their friends
    for (const Critter& z : critters) {
        int x = (int)z.x, y = (int)z.y;
        if (z.kind == 3) {
            for (int k = -4; k <= 4; k++) {
                int hx = (int)(z.x + z.dx * k), hy = (int)(z.y + z.dy * k);
                if (z.dead > 0) { pset(hx + 2, hy + 1, (k & 1) ? RGB(150, 150, 156) : RGB(120, 60, 60)); continue; }      // knocked over
                pdark(hx + 2, hy + 2, 0.55f); pdark(hx + 3, hy + 3, 0.55f);
                uint32_t c = abs(k) == 4 ? RGB(30, 32, 40) : (((k + 4) / 2) & 1 ? C_WHITE : RGB(30, 32, 40));
                pset(hx, hy, shade(c, 0.8f)); pset(hx, hy - 1, c); pset(hx, hy - 2, abs(k) == 4 ? c : C_WHITE);
            }
            continue;
        }
        if (z.dead > 0 || z.on >= 0) continue;
        int f = ((int)z.ph) & 1;
        if (z.kind == 0) {
            float rise = clampf(z.ph * 0.5f, 0, 1);       // climbs out of the ground first
            int dir = z.dx < 0 ? -1 : 1;                  // faces the way it runs
            bool alt = hash2((int)z.hx, (int)z.hy) & 1, alt2 = hash2((int)z.hx, (int)z.hy, 3) & 1;
            uint32_t skin = alt2 ? RGB(150, 226, 120) : RGB(170, 210, 190), skinD = shade(skin, 0.7f), shirt = alt ? RGB(170, 60, 60) : RGB(70, 110, 190), pants = alt2 ? RGB(60, 60, 90) : RGB(100, 76, 50);
            pdark(x, y + 1, 0.55f); pdark(x + 1, y + 1, 0.55f); pdark(x + 2, y + 1, 0.55f); pdark(x + 2, y, 0.55f);
            pset(x, y - 8, RGB(40, 44, 30)); pset(x + 1, y - 8, RGB(40, 44, 30)); pset(x + (dir > 0 ? 0 : 1), y - 7, RGB(40, 44, 30));          // hair
            pset(x + (dir > 0 ? 1 : 0), y - 7, skin); pset(x, y - 6, dir > 0 ? skinD : skin); pset(x + 1, y - 6, dir > 0 ? skin : skinD);        // head
            pset(x + (dir > 0 ? 1 : 0), y - 7, (frameNo >> 3) & 1 ? RGB(255, 40, 30) : RGB(255, 230, 60));                                       // eye
            if (rise > 0.35f) {
                pset(x, y - 5, shirt); pset(x + 1, y - 5, shade(shirt, 0.75f)); pset(x, y - 4, shade(shirt, 0.9f)); pset(x + 1, y - 4, shade(shirt, 0.65f));
                pset(x + (hash2(x, 1) & 1), y - 4, RGB(120, 12, 18));                                                                             // blood on the shirt
                pset(x + (dir > 0 ? 2 : -1), y - 5, skin); pset(x + (dir > 0 ? 3 : -2), y - 5 - f, skin); pset(x + (dir > 0 ? 2 : -1), y - 4 + f, skinD);      // both arms reach forward
            }
            if (rise > 0.7f) {
                pset(x, y - 3, pants); pset(x + 1, y - 3, shade(pants, 0.8f));
                pset(x + (f ? -1 : 0), y - 2, pants); pset(x + (f ? 2 : 1), y - 2, shade(pants, 0.8f)); pset(x + (f ? -1 : 0), y - 1, pants); pset(x + (f ? 2 : 1), y - 1, shade(pants, 0.8f));
                pset(x + (f ? -1 : 0) + dir, y, RGB(30, 28, 30)); pset(x + (f ? 2 : 1), y, RGB(30, 28, 30));
            }
        } else if (z.kind == 1) {
            pdark(x + 1, y + 2, 0.65f); pdark(x, y + 2, 0.65f);
            pset(x, y, RGB(22, 20, 26)); pset(x + 1, y, RGB(22, 20, 26)); pset(x, y - 1, RGB(40, 36, 46)); pset(x + 1, y - 1, RGB(22, 20, 26)); pset(x, y + 1, RGB(22, 20, 26));
            pset(x - 1, y - 1 + f, RGB(22, 20, 26)); pset(x + 2, y - f, RGB(22, 20, 26)); pset(x - 2, y + f, RGB(22, 20, 26)); pset(x + 3, y + 1 - f, RGB(22, 20, 26)); pset(x - 1, y + 1, RGB(22, 20, 26)); pset(x + 2, y + 1, RGB(22, 20, 26));
            pset(x, y - 1, RGB(255, 40, 40));
        } else {
            uint32_t base = BEAR[hash2((int)z.hx, (int)z.hy) & 3];
            pdark(x + 1, y + 2, 0.62f); pdark(x + 2, y + 2, 0.62f);
            pset(x - 1, y - 4, shade(base, 0.7f)); pset(x + 1, y - 4, shade(base, 0.7f));
            for (int j = -3; j <= 0; j++) for (int i = -1; i <= 1; i++) pset(x + i, y + j, (i == -1 && j < -1) ? mixc(base, C_WHITE, 0.4f) : (i == 1 ? shade(base, 0.72f) : base));
            pset(x - 2, y - 2 + f, base); pset(x + 2, y - 1 - f, shade(base, 0.72f)); pset(x - 1, y + 1, shade(base, 0.72f)); pset(x + 1, y + 1, shade(base, 0.6f));
            pset(x, y - 3, RGB(40, 20, 20));
        }
    }
    for (const Bumper& b : bumpers) {
        float r = b.r + (b.flash > 0 ? 1.0f : 0);
        discShadow(b.x + 2, b.y + 3, r);
        uint32_t base = theme == TH_CANDY ? hue((int)(b.x * 7 + b.y * 3) & 255) : (theme == TH_SPORT ? POOL[(&b - &bumpers[0]) & 7] : (theme == TH_WATER ? RGB(236, 50, 40) : (theme == TH_HORROR ? RGB(244, 130, 24) : RGB(0, 200, 236))));
        if (b.flash > 0) base = mixc(base, C_WHITE, 0.6f);
        disc(b.x, b.y, r, base, 0.55f);
        pset((int)(b.x - r * 0.4f), (int)(b.y - r * 0.45f), C_WHITE);
        if (theme == TH_SPORT) { for (int j = -1; j <= 0; j++) for (int i = -1; i <= 0; i++) pset((int)b.x + i, (int)b.y + j, C_WHITE); }
        else if (theme == TH_WATER) { for (int i = -(int)r + 1; i < (int)r; i++) pset((int)b.x + i, (int)b.y, C_WHITE); }
        else if (theme == TH_HORROR) { pset((int)b.x - 1, (int)b.y - 1, RGB(255, 240, 120)); pset((int)b.x + 1, (int)b.y - 1, RGB(255, 240, 120)); for (int i = -1; i <= 1; i++) pset((int)b.x + i, (int)b.y + 1, RGB(255, 240, 120)); }
    }
    for (const Roller& r : rollers) {
        if (r.wait > 0) continue;
        if (theme == TH_HORROR && style == 2) continue;      // ghosts float above everything
        if (theme == TH_HORROR) {                            // a scarecrow with a pumpkin head and a scythe walks its beat
            int x = (int)r.x, y = (int)r.y + 3, f = ((int)(r.u * hypotf(r.bx - r.ax, r.by - r.ay) / 3)) & 1, dir = r.vx < 0 || (r.vx == 0 && r.vy < 0) ? -1 : 1;
            for (int i = -1; i <= 3; i++) pdark(x + i, y + 1, 0.55f);
            for (int j = -10; j <= -8; j++) for (int i = -1; i <= 2; i++) if (!((j == -10) && (i == -1 || i == 2))) pset(x + i, y + j, (i == 2 || j == -8) ? RGB(190, 90, 14) : RGB(250, 140, 26));
            pset(x, y - 11, RGB(60, 130, 50)); pset(x, y - 9, RGB(255, 244, 120)); pset(x + 1, y - 9, RGB(255, 244, 120)); pset(x + ((frameNo >> 4) & 1), y - 8, RGB(255, 244, 120));
            for (int j = -7; j <= -3; j++) for (int i = -1; i <= 2; i++) pset(x + i, y + j, i == 2 ? RGB(40, 30, 44) : ((i + j) & 3) == 0 ? RGB(110, 84, 60) : RGB(70, 54, 76));
            pset(x - 2, y - 6, RGB(214, 190, 110)); pset(x + 3, y - 6, RGB(214, 190, 110)); pset(x - 1 + f, y - 2, RGB(50, 40, 50)); pset(x + 2 - f, y - 2, RGB(50, 40, 50)); pset(x - 1 + f, y - 1, RGB(214, 190, 110)); pset(x + 2 - f, y - 1, RGB(214, 190, 110));
            int sx = x + dir * 4;                            // the scythe
            for (int j = -12; j <= -1; j++) pset(sx, y + j, RGB(120, 84, 48));
            for (int i = 1; i <= 4; i++) { pset(sx - dir * i, y - 12 + (i > 2 ? 1 : 0), RGB(226, 232, 240)); if (i < 4) pset(sx - dir * i, y - 11 + (i > 2 ? 1 : 0), RGB(150, 160, 176)); }
            continue;
        }
        float hop = (theme == TH_SPORT && style != 0) ? fabsf(sinf(r.u * hypotf(r.bx - r.ax, r.by - r.ay) * 0.09f)) * 6 : 0;
        discShadow(r.x + 1.5f + hop * 0.4f, r.y + 2, r.r);
        uint32_t base = theme == TH_SPORT ? (style == 0 ? RGB(246, 244, 230) : (style == 1 ? RGB(250, 250, 250) : RGB(214, 236, 50))) : (theme == TH_CANDY ? hue((frameNo * 3 + (int)r.ax) & 255) : (theme == TH_WATER ? RGB(132, 90, 50) : RGB(255, 80, 220)));
        disc(r.x, r.y - hop, r.r, base, 0.5f);
        if (theme == TH_SPORT && style == 0) { int rx = (int)(r.x + cosf(r.u * 30) * 1.5f), ry = (int)(r.y + sinf(r.u * 30) * 1.5f); pset(rx, ry, RGB(226, 40, 40)); }      // the cue ball has a red dot
        if (theme == TH_WATER) { pset((int)r.x - 2, (int)r.y, RGB(90, 60, 34)); pset((int)r.x + 1, (int)r.y - 1, RGB(170, 124, 70)); pset((int)(r.x - r.r), (int)(r.y + r.r), RGB(214, 236, 250)); pset((int)(r.x + r.r), (int)(r.y + r.r), RGB(214, 236, 250)); }
        if (theme == TH_SCIFI) pset((int)r.x, (int)r.y, C_WHITE);
    }
}

void worldDrawHigh() {
    for (const Critter& z : critters) if (z.on >= 0 && z.kind == 0) {      // a zombie hanging on to a car
        int x = (int)z.x, y = (int)z.y - 2, f = ((int)z.ph) & 1;
        pset(x, y - 4, RGB(40, 44, 30)); pset(x + 1, y - 4, RGB(40, 44, 30)); pset(x, y - 3, RGB(150, 226, 120)); pset(x + 1, y - 3, RGB(104, 160, 90)); pset(x + f, y - 3, RGB(255, 40, 30));
        pset(x, y - 2, RGB(170, 60, 60)); pset(x + 1, y - 2, RGB(120, 40, 40)); pset(x - 1 - f, y - 2, RGB(150, 226, 120)); pset(x + 2 + f, y - 2, RGB(104, 160, 90)); pset(x, y - 1, RGB(60, 60, 90)); pset(x + 1, y - 1, RGB(44, 44, 70));
    }
    if (theme != TH_HORROR) return;
    // spiders let themselves down on a thread and climb up again
    for (const Dangler& d : danglers) {
        float len = d.len * (0.55f + 0.45f * sinf(d.ph));
        int x = (int)d.x, y = (int)(d.top + len);
        for (int j = HUD_H; j < y; j++) if ((j & 1) || j > y - 6) pset(x, j, RGB(200, 200, 210));
        pdark(x + 5, y + 8, 0.62f); pdark(x + 6, y + 8, 0.62f); pdark(x + 5, y + 9, 0.62f); pdark(x + 6, y + 9, 0.62f);
        int f = (frameNo >> 3) & 1;
        for (int j = 0; j < 3; j++) for (int i = -1; i <= 1; i++) pset(x + i, y + j, RGB(24, 22, 28));
        pset(x, y + 3, RGB(24, 22, 28)); pset(x - 2, y + f, RGB(24, 22, 28)); pset(x + 2, y + 1 - f, RGB(24, 22, 28)); pset(x - 2, y + 2 + f, RGB(24, 22, 28)); pset(x + 2, y + 3 - f, RGB(24, 22, 28));
        pset(x - 1, y + 2, RGB(255, 40, 40)); pset(x + 1, y + 2, RGB(255, 40, 40));
    }
    auto ghost = [](float gx, float gy, float ph) {
        int x = (int)gx, y = (int)(gy + sinf(ph * 2) * 2);
        static const char* g[9] = { "..www..", ".wwwww.", "wwkwkww", "wwkwkww", "wwwwwww", "wwwkwww", "wwwwwww", "wwwwwww", "w.w.w.w" };
        for (int j = 0; j < 9; j++) for (int i = 0; i < 7; i++) {
            if (g[j][i] == '.' || !inb(x - 3 + i, y - 4 + j) || y - 4 + j < HUD_H) continue;
            int p = (y - 4 + j) * W + x - 3 + i;
            if (j == 8 && ((i / 2 + (frameNo >> 3)) & 1)) continue;
            fb[p] = g[j][i] == 'k' ? RGB(20, 20, 40) : mixc(fb[p], RGB(236, 244, 255), 0.72f);
        }
        for (int i = -2; i <= 2; i++) pdark(x + i + 4, y + 11, 0.75f);
    };
    if (style == 3) for (const Ghost& g : ghosts) ghost(160 + sinf(g.ph * 0.17f + g.x) * 140, 125 + sinf(g.ph * 0.23f + g.y) * 95, g.ph);
    if (style == 2) for (const Roller& r : rollers) if (r.wait <= 0) ghost(r.x, r.y - 3, r.u * 20);
}
