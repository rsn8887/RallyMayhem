// Rally Mayhem - building a track: roads, scenery, lighting and the moving parts
// by rsn8887

#include "game.h"

// ------------------------------------------------------------------ track data

static const float DECK_H = 10, JUMP_H = 7, RAMP_LEN = 15;

struct Light { int x, y, r, amt; };

const TrackDef* def = nullptr;
int theme = 0, weather = 0, levelNo = 0;
bool night = false, flood = false;
float windX = 0, windY = 0, gust = 1;
std::vector<Sample> sm;
int NS = 1;
float trackLen = 1, spacing = 0.25f;
uint8_t surf[2][N], solid[2][N], israil[2][N], water0[N], reserved[N], objMask[N], dynSolid[N], ttMask[N], spanMask[N], shadowM[N];
float ht[2][N], latm[2][N], castH[N];
int idxm[2][N], nearIdx[N];
static uint16_t distRoad[N];
uint32_t ground[N], overC[N];
uint8_t overA[N], lightStatic[N], lightBuf[N];
float overH[N];
std::vector<int> waterPix, tripPix;
std::vector<uint8_t> tripVal;
std::vector<Train> trains;
std::vector<Crossing> crossings;
std::vector<Waver> wavers;
std::vector<Pt> flags, smokers, signals, lamps;
std::vector<int> signalTrain;
std::vector<Duck> ducks;
std::vector<TreeTop> treeTops;
std::vector<ItemBox> items;
static std::vector<Light> lights;
std::vector<Turntable> turntables;
std::vector<Drawbridge> bridges;
std::vector<Ferry> ferries;
int leafCount = 0;

uint32_t hue(int v) {             // 0..255 -> rainbow
    float h = (v & 255) / 255.0f * 6; int i = (int)h; float f = h - i; int q = (int)(255 * (1 - f)), t = (int)(255 * f);
    switch (i % 6) { case 0: return RGB(255, t, 0); case 1: return RGB(q, 255, 0); case 2: return RGB(0, 255, t); case 3: return RGB(0, q, 255); case 4: return RGB(t, 0, 255); default: return RGB(255, 0, q); }
}

static inline void gpx(int x, int y, uint32_t c) { if (inb(x, y) && y >= HUD_H) ground[y * W + x] = c; }
static inline void opx(int x, int y, uint32_t c, float h) { if (inb(x, y) && y >= HUD_H) { int p = y * W + x; overC[p] = c; overA[p] = 1; overH[p] = h; } }
static void markSolid(int x, int y, int w, int h, uint8_t s, float cast) {
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) if (inb(i, j)) { int p = j * W + i; solid[0][p] = s; castH[p] = std::max(castH[p], cast); objMask[p] = 1; }
}
static void markObj(int x, int y, int w, int h) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) if (inb(i, j)) objMask[j * W + i] = 1; }
static void markReserved(int x, int y, int w, int h) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) if (inb(i, j)) reserved[j * W + i] = 1; }
// Draws a small picture given as rows of characters; keys[i] is drawn in cols[i], '.' is transparent.
// With tall >= 0 the picture is drawn above the cars instead (they can drive behind it); a tall above 0
// also makes every pixel of it cast a shadow of that height (0: the shadow comes from its solid foot only).
static void sprite(int x, int y, const char* const* rows, int h, const char* keys, const uint32_t* cols, float tall = -1) {
    for (int j = 0; j < h; j++) for (int i = 0; rows[j][i]; i++) {
        const char* k = strchr(keys, rows[j][i]);
        if (rows[j][i] == '.' || !k) continue;
        if (tall < 0) gpx(x + i, y + j, cols[k - keys]);
        else { opx(x + i, y + j, cols[k - keys], 99); if (inb(x + i, y + j)) castH[(y + j) * W + x + i] = std::max(castH[(y + j) * W + x + i], tall); }
    }
}

static void stampBlob(float cx, float cy, float rx, float ry, int seed, int mode) {
    // mode 0: water, -1: land (islands on flooded tracks), otherwise a road patch of that surface type
    for (int y = (int)(cy - ry * 1.3f); y <= (int)(cy + ry * 1.3f); y++)
        for (int x = (int)(cx - rx * 1.3f); x <= (int)(cx + rx * 1.3f); x++) {
            if (!inb(x, y) || y < HUD_H) continue;
            float dx = (x - cx) / rx, dy = (y - cy) / ry, th = atan2f(dy, dx);
            float rr = 1 + 0.10f * sinf(3 * th + seed) + 0.06f * sinf(7 * th + seed * 2.3f);
            if (dx * dx + dy * dy > rr * rr) continue;
            int p = y * W + x;
            if (mode == 0) surf[0][p] = S_WATER;
            else if (mode == -1) surf[0][p] = S_OFF;
            else if (surf[0][p] == S_ROAD || surf[0][p] == S_DIRT) surf[0][p] = (uint8_t)mode;
        }
}
static void stampRiver(float x1, float y1, float x2, float y2, float w, int seed) {
    float len = hypotf(x2 - x1, y2 - y1), nx = -(y2 - y1) / len, ny = (x2 - x1) / len;
    for (float d = 0; d <= len; d += 0.5f) {
        float t = d / len, wob = sinf(d * 0.07f + seed) * 2.5f * sinf(t * PI);
        float cx = lerpf(x1, x2, t) + nx * wob, cy = lerpf(y1, y2, t) + ny * wob, r = w / 2;
        for (int y = (int)(cy - r); y <= (int)(cy + r); y++)
            for (int x = (int)(cx - r); x <= (int)(cx + r); x++)
                if (inb(x, y) && y >= HUD_H && (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) surf[0][y * W + x] = S_WATER;
    }
}

static void buildSamples() {
    const std::vector<CP>& P = def->p;
    int n = (int)P.size();
    std::vector<int> cpStart(n);
    sm.clear();
    for (int i = 0; i < n; i++) {
        const CP& p0 = P[(i + n - 1) % n], & p1 = P[i], & p2 = P[(i + 1) % n], & p3 = P[(i + 2) % n];
        float len = hypotf(p2.x - p1.x, p2.y - p1.y) * 1.1f;
        int m = std::max(8, (int)(len / 0.22f));
        cpStart[i] = (int)sm.size();
        for (int j = 0; j < m; j++) {
            float t = j / (float)m, t2 = t * t, t3 = t2 * t, st = t * t * (3 - 2 * t);
            Sample s{};
            s.x = 0.5f * (2 * p1.x + (-p0.x + p2.x) * t + (2 * p0.x - 5 * p1.x + 4 * p2.x - p3.x) * t2 + (-p0.x + 3 * p1.x - 3 * p2.x + p3.x) * t3);
            s.y = 0.5f * (2 * p1.y + (-p0.y + p2.y) * t + (2 * p0.y - 5 * p1.y + 4 * p2.y - p3.y) * t2 + (-p0.y + 3 * p1.y - 3 * p2.y + p3.y) * t3);
            s.w = lerpf(p1.w, p2.w, st); s.h = lerpf(p1.h, p2.h, st); s.f = p1.f;
            s.type = (p1.f & fD) ? S_DIRT : (p1.f & fI) ? S_ICE : S_ROAD;
            sm.push_back(s);
        }
    }
    NS = (int)sm.size();
    float arc = 0;
    for (int k = 0; k < NS; k++) { sm[k].arc = arc; const Sample& b = sm[(k + 1) % NS]; arc += hypotf(b.x - sm[k].x, b.y - sm[k].y); }
    trackLen = arc; spacing = trackLen / NS;
    for (int k = 0; k < NS; k++) {
        const Sample& a = sm[(k + NS - 3) % NS], & b = sm[(k + 3) % NS];
        float dx = b.x - a.x, dy = b.y - a.y, l = hypotf(dx, dy); if (l < 1e-5f) l = 1;
        sm[k].tx = dx / l; sm[k].ty = dy / l;
    }
    std::vector<float> cv(NS), tmp(NS);
    int win = std::max(4, (int)(4.0f / spacing));
    for (int k = 0; k < NS; k++) {
        const Sample& a = sm[(k + NS - win) % NS], & b = sm[(k + win) % NS];
        cv[k] = asinf(clampf(a.tx * b.ty - a.ty * b.tx, -1, 1)) / (2 * win * spacing);
    }
    auto smooth = [&](std::vector<float>& v, int r) {
        double acc = 0; for (int j = -r; j <= r; j++) acc += v[(j + NS * 4) % NS];
        for (int k = 0; k < NS; k++) { tmp[k] = (float)(acc / (2 * r + 1)); acc += v[(k + r + 1) % NS] - v[(k - r + NS * 4) % NS]; }
        v = tmp;
    };
    smooth(cv, (int)(8 / spacing));
    std::vector<float> bank(NS);
    for (int k = 0; k < NS; k++) { sm[k].curv = cv[k]; bank[k] = (sm[k].f & fK) ? clampf(cv[k] * 45, -1, 1) * 0.26f : 0; }
    smooth(bank, (int)(12 / spacing));
    for (int k = 0; k < NS; k++) sm[k].bank = bank[k];
    for (int i = 0; i < n; i++) {
        if (P[i].f & fJ) {
            int steps = (int)(RAMP_LEN / spacing);
            for (int j = 1; j <= steps; j++) { Sample& s = sm[(cpStart[i] - j + NS) % NS]; float r = 1 - j / (float)steps; s.h += JUMP_H * r; s.ramp = r + 0.001f; }
        }
        if (P[i].f & fO) { int steps = (int)(14 / spacing); for (int j = 0; j < steps; j++) sm[(cpStart[i] + j) % NS].type = S_BOOST; }
    }
    for (int k = 0; k < NS; k++) {
        Sample& s = sm[k];
        float nx = -s.ty, ny = s.tx;
        s.over = false;
        for (float d = -0.4f; d <= 0.41f; d += 0.4f) {
            int x = (int)(s.x + nx * d * s.w), y = (int)(s.y + ny * d * s.w);
            if (inb(x, y) && water0[y * W + x]) s.over = true;
        }
        bool autoRail = s.over && !(s.f & (fN | fF | fI));
        s.rail = (s.f & (fR | fB | fT)) || autoRail;
        bool t0 = (s.f & fT) != 0, tp = (sm[(k + NS - 6) % NS].f & fT) != 0, tn = (sm[(k + 6) % NS].f & fT) != 0;
        s.portal = t0 && (!tp || !tn);
    }
}

static void stampRoads() {
    for (int pass = 0; pass < 2; pass++)
        for (int k = 0; k < NS; k++) {
            const Sample& s = sm[k];
            if (s.f & (fG | fY)) continue;
            int ly = (s.f & fB) ? 1 : 0;
            float nx = -s.ty, ny = s.tx, half = s.w / 2, sgn = s.curv > 0 ? 1.f : -1.f, ab = fabsf(s.bank);
            if (pass == 0) {
                if (!s.rail) continue;
                for (int side = -1; side <= 1; side += 2)
                    for (float d = half + 0.3f; d <= half + 1.5f; d += 0.4f) {
                        int x = (int)floorf(s.x + nx * d * side), y = (int)floorf(s.y + ny * d * side);
                        if (!inb(x, y) || y < HUD_H) continue;
                        int p = y * W + x;
                        solid[ly][p] = 1; israil[ly][p] = 1; idxm[ly][p] = k;
                        if (ly == 1) surf[1][p] = S_ROAD;
                        ht[ly][p] = s.h + ab * (half - sgn * half * side);
                    }
            } else {
                for (float d = -half; d <= half + 0.01f; d += 0.4f) {
                    int x = (int)floorf(s.x + nx * d), y = (int)floorf(s.y + ny * d);
                    if (!inb(x, y) || y < HUD_H) continue;
                    int p = y * W + x;
                    uint8_t t = s.type;
                    if (ly == 0 && water0[p]) { if (s.f & fF) t = S_SHALLOW; else if (t != S_ICE) t = S_PLANK; }
                    surf[ly][p] = t; solid[ly][p] = 0; israil[ly][p] = 0; idxm[ly][p] = k; latm[ly][p] = d / half;
                    ht[ly][p] = s.h + ab * (half - sgn * d);
                }
            }
        }
    // fill pinholes left by rasterising
    for (int ly = 0; ly < 2; ly++)
        for (int y = HUD_H + 1; y < H - 1; y++)
            for (int x = 1; x < W - 1; x++) {
                int p = y * W + x;
                if (roadLike(surf[ly][p]) || israil[ly][p]) continue;
                static const int nb[4] = { -1, 1, -W, W };
                int cnt = 0, src = -1;
                for (int q = 0; q < 4; q++) { int pp = p + nb[q]; if (roadLike(surf[ly][pp]) && !israil[ly][pp] && surf[ly][pp] != S_SHALLOW) { cnt++; src = pp; } }
                if (cnt >= 3) { surf[ly][p] = surf[ly][src]; ht[ly][p] = ht[ly][src]; idxm[ly][p] = idxm[ly][src]; latm[ly][p] = latm[ly][src]; solid[ly][p] = 0; }
            }
}

static void bfsNearest() {
    std::vector<int> q; q.reserve(N);
    for (int p = 0; p < N; p++) {
        distRoad[p] = 0xFFFF; nearIdx[p] = 0;
        if (roadLike(surf[0][p]) || israil[0][p]) { distRoad[p] = 0; nearIdx[p] = idxm[0][p]; q.push_back(p); }
        else if (surf[1][p] != S_NONE) { distRoad[p] = 0; nearIdx[p] = idxm[1][p]; q.push_back(p); }
    }
    for (size_t i = 0; i < q.size(); i++) {
        int p = q[i], x = p % W, y = p / W;
        static const int dx[4] = { -1, 1, 0, 0 }, dy[4] = { 0, 0, -1, 1 };
        for (int k = 0; k < 4; k++) {
            int xx = x + dx[k], yy = y + dy[k];
            if (!inb(xx, yy)) continue;
            int pp = yy * W + xx;
            if (distRoad[pp] != 0xFFFF) continue;
            distRoad[pp] = distRoad[p] + 1; nearIdx[pp] = nearIdx[p]; q.push_back(pp);
        }
    }
}

// ------------------------------------------------------------------ ground art
static uint32_t offColor(int x, int y, uint32_t hs) {
    const ThemeCol& T = THEMES[theme];
    int v = hs % 100;
    switch (theme) {
    case TH_CITY: {
        if (x % 10 == 0 || y % 10 == 0) return T.offB;
        return v < 4 ? T.offA : T.off;
    }
    case TH_F1: return (((x + y) / 12) & 1) ? T.offA : T.off;
    case TH_MEXICO:
        if (v < 3) return T.offA;
        if (v < 5) return T.offB;
        return sinf(x * 0.06f + sinf(y * 0.045f) * 2.2f + y * 0.11f) > 0.72f ? RGB(240, 212, 132) : T.off;
    case TH_MOUNTAIN: {
        float a = sinf(x * 0.071f + sinf(y * 0.05f) * 2.4f) + sinf(y * 0.083f + sinf(x * 0.04f) * 2.0f + 1.7f);
        if (a > 1.35f) return v < 30 ? RGB(232, 236, 244) : C_WHITE;                   // snow patch
        if (a < -1.15f) return v < 25 ? RGB(150, 146, 140) : RGB(132, 128, 124);        // bare rock
        return v < 4 ? T.offA : (v < 8 ? T.offB : T.off);
    }
    case TH_SPRING:
        if (v == 0) return RGB(255, 180, 210);
        if (v == 1) return C_WHITE;
        if (v == 2) return RGB(255, 230, 80);
        return v < 7 ? T.offA : (v < 10 ? T.offB : T.off);
    case TH_CARNIVAL: return T.off;
    default: return v < 4 ? T.offA : (v < 7 ? T.offB : T.off);
    }
}

static uint32_t colorAt(int ly, int p) {
    int x = p % W, y = p / W;
    uint32_t hs = hash2(x, y, levelNo);
    const ThemeCol& T = THEMES[theme];
    if (israil[ly][p]) {
        const Sample& S = sm[idxm[ly][p]];
        if (S.f & fT) return RGB(96, 96, 100);
        if (theme == TH_CARNIVAL) return hue((int)(S.arc * 6));
        return ((int)(S.arc / 4) & 1) ? T.railA : T.railB;
    }
    uint8_t s = surf[ly][p];
    if (s == S_OFF) return offColor(x, y, hs);
    if (s == S_WATER) return waterCol(0);
    if (s == S_METAL) return RGB(36, 36, 40);
    const Sample& S = sm[idxm[ly][p]];
    float a = fabsf(latm[ly][p]), half = S.w / 2;
    int v = hs % 100;
    switch (s) {
    case S_ROAD: {
        if (idxm[ly][p] * spacing < 4.0f) return ((((int)(S.arc) / 2) + (int)floorf(latm[ly][p] * half / 2 + 50)) & 1) ? C_WHITE : RGB(16, 16, 16);
        if (S.ramp > 0.74f) return (((x + y) >> 1) & 1) ? RGB(250, 210, 30) : RGB(24, 24, 24);
        bool f1 = theme == TH_F1;
        if (!S.rail && fabsf(S.curv) > (f1 ? 0.012f : 0.022f) && a > 0.76f) {
            if (theme == TH_CARNIVAL) return hue((int)(S.arc * 8));
            return ((int)(S.arc / 4) & 1) ? RGB(216, 36, 36) : RGB(240, 240, 240);
        }
        if (!S.rail && a > 0.87f) return RGB(236, 236, 236);
        if (theme == TH_CITY && half > 9 && S.ramp <= 0 && a * half < 0.8f && ((int)(S.arc / 6) & 1)) return RGB(232, 200, 40);
        if (theme == TH_WINTER && v > 95) return RGB(236, 240, 248);
        if (v < 2) return shade(T.road, 0.62f);
        if (v < 4) return shade(T.road, 1.3f);
        if (S.rail && a > 0.9f) return shade(T.road, 0.78f);
        if (a > 0.72f) return shade(T.road, 0.9f);
        if (a < 0.26f) return shade(T.road, 1.07f);
        return T.road;
    }
    case S_DIRT: {
        if (a > 0.93f) return ((hs >> 10) & 3) ? RGB(120, 60, 0) : RGB(96, 48, 0);
        if (a > 0.80f) return ((hs >> 10) & 3) ? RGB(156, 78, 0) : RGB(208, 144, 72);
        if (fabsf(a - 0.42f) < 0.09f && ((hs >> 8) & 3)) return RGB(164, 80, 0);
        if (a < 0.10f && ((hs >> 8) & 1)) return RGB(216, 120, 24);
        return v < 5 ? RGB(128, 64, 0) : (v < 8 ? RGB(240, 150, 60) : RGB(192, 96, 0));
    }
    case S_ICE: return (((x + y * 2 + (hs & 1)) % 9) == 0 || v < 6) ? C_WHITE : (v < 20 ? RGB(170, 214, 244) : RGB(190, 230, 250));
    case S_SHALLOW: return v < 10 ? C_WHITE : (v < 30 ? RGB(150, 200, 250) : RGB(90, 160, 236));
    case S_PUDDLE: return v < 8 ? RGB(170, 200, 240) : (a > 0.9f ? RGB(84, 96, 140) : RGB(70, 84, 130));
    case S_BOOST: { int ph = (int)(S.arc + a * half * 0.8f) % 5; return ph < 2 ? RGB(255, 224, 0) : (ph < 4 ? RGB(232, 48, 28) : RGB(255, 140, 0)); }
    case S_PLANK: {
        if ((int)(S.arc) % 4 == 0) return RGB(92, 60, 30);
        if (a > 0.86f) return RGB(120, 80, 40);
        return v < 15 ? RGB(172, 124, 70) : (v < 30 ? RGB(140, 96, 50) : RGB(154, 108, 58));
    }
    }
    return T.road;
}

// kind: 0 leafy, 1 pine, 2 pink blossom, 3 white blossom, 4 autumn, 5 bare/snowy, 6 dark leafy
static void drawCanopy(int x, int y, int r, int kind) {
    uint32_t L, M, D, K;
    Rng rr(x * 131 + y * 17);
    switch (kind) {
    case 1: L = theme == TH_WINTER ? C_WHITE : RGB(76, 156, 96); M = RGB(36, 108, 70); D = RGB(22, 76, 54); K = RGB(14, 52, 40); break;
    case 2: L = RGB(255, 220, 236); M = RGB(246, 164, 204); D = RGB(208, 112, 162); K = RGB(150, 76, 120); break;
    case 3: L = C_WHITE; M = RGB(238, 234, 242); D = RGB(200, 194, 214); K = RGB(150, 146, 170); break;
    case 4: {
        static const uint32_t t[3][4] = { { RGB(255, 208, 72), RGB(240, 150, 40), RGB(196, 100, 28), RGB(130, 60, 20) }, { RGB(250, 130, 60), RGB(214, 64, 36), RGB(160, 36, 28), RGB(100, 24, 24) }, { RGB(250, 236, 110), RGB(220, 190, 60), RGB(170, 140, 40), RGB(110, 90, 30) } };
        int k = rr.range(3); L = t[k][0]; M = t[k][1]; D = t[k][2]; K = t[k][3];
    } break;
    case 5: L = C_WHITE; M = RGB(156, 156, 168); D = RGB(108, 108, 122); K = RGB(76, 76, 90); break;
    case 6: L = RGB(84, 160, 72); M = RGB(48, 120, 56); D = RGB(30, 88, 44); K = RGB(20, 60, 34); break;
    default:
        if (theme == TH_SPRING) { L = RGB(176, 236, 112); M = RGB(124, 204, 84); D = RGB(84, 164, 68); K = RGB(56, 120, 56); }
        else { L = RGB(124, 212, 84); M = RGB(72, 168, 60); D = RGB(44, 124, 48); K = RGB(28, 88, 40); }
    }
    int cy = y - 4;
    if (kind == 1) {
        static const int wd[10] = { 1, 1, 3, 3, 5, 5, 7, 7, 9, 9 };
        for (int j = 0; j < 10; j++) for (int i = -wd[j] / 2; i <= wd[j] / 2; i++) {
            uint32_t c = (i < 0 && (j & 1) == 0) ? L : (i == wd[j] / 2 ? K : (i > 0 ? D : M));
            if (j & 1 && i == -wd[j] / 2) c = D;
            opx(x + i, cy - 6 + j, c, 99);
            if (inb(x + i, cy - 6 + j)) castH[(cy - 6 + j) * W + x + i] = 5;
        }
        return;
    }
    for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        float d2 = dx * dx + dy * dy + (rr.range(5) - 2);
        if (d2 > r * r) continue;
        int t = dx + dy + rr.range(3) - 1;
        uint32_t c = t < -r / 2 - 1 ? L : (t < 1 ? M : (t < r ? D : K));
        if (rr.range(7) == 0) c = L;
        opx(x + dx, cy + dy, c, 99);
        if (inb(x + dx, cy + dy)) castH[(cy + dy) * W + x + dx] = 5;
    }
}
static void drawPalm(int x, int y) {
    gpx(x, y, RGB(150, 100, 50)); gpx(x, y + 1, RGB(120, 76, 40)); gpx(x + 1, y + 1, RGB(90, 56, 30)); gpx(x + 1, y, RGB(120, 76, 40));
    markSolid(x, y, 2, 2, 2, 4);
    int cy = y - 5;
    for (int d = 0; d < 8; d++) {
        float a = d * PI / 4 + 0.3f;
        for (int r = 0; r <= 6; r++) {
            int px = x + (int)lroundf(cosf(a) * r), py = cy + (int)lroundf(sinf(a) * r * 0.8f + r * r * 0.05f);
            uint32_t c = (d >= 3 && d <= 5) ? RGB(96, 208, 72) : (r < 2 ? RGB(70, 176, 60) : RGB(36, 124, 52));
            opx(px, py, c, 99); if (r > 1 && r < 6) opx(px, py + 1, RGB(24, 92, 44), 99);
            if (inb(px, py)) castH[py * W + px] = 6;
        }
    }
    opx(x, cy, RGB(170, 110, 40), 99); opx(x + 1, cy, RGB(130, 80, 30), 99);
}

static bool placeFree(int x, int y, int rx, int ry = -1) {
    if (ry < 0) ry = rx;
    for (int j = -ry; j <= ry; j++) for (int i = -rx; i <= rx; i++) {
        if (!inb(x + i, y + j) || y + j < HUD_H + 2) return false;
        int p = (y + j) * W + x + i;
        if (reserved[p] || water0[p] || surf[0][p] != S_OFF || surf[1][p] != S_NONE || solid[0][p]) return false;
    }
    return true;
}
static void addLeafTree(int x, int y, Rng& rng, int kind) {
    int r = kind == 1 ? 5 : 5 + rng.range(2);
    static const uint32_t tr[3] = { RGB(134, 84, 44), RGB(104, 64, 34), RGB(72, 44, 24) };
    gpx(x - 1, y, tr[0]); gpx(x, y, tr[1]); gpx(x + 1, y, tr[2]); gpx(x - 1, y + 1, tr[1]); gpx(x, y + 1, tr[1]); gpx(x + 1, y + 1, tr[2]);
    markSolid(x - 1, y, 3, 2, 2, 4);
    drawCanopy(x, y, r, kind);
    if (kind == 4 || kind == 0 || kind == 6) treeTops.push_back({ x, y - 4, r, kind });
    markReserved(x - 6, y - 10, 13, 14);
}
static void addLamp(int x, int y) {
    gpx(x, y, RGB(40, 40, 44)); for (int j = 1; j <= 4; j++) gpx(x, y - j, RGB(120, 120, 128));
    gpx(x - 1, y - 5, RGB(255, 244, 170)); gpx(x, y - 5, C_WHITE); gpx(x + 1, y - 5, RGB(255, 244, 170)); gpx(x, y - 6, RGB(90, 90, 96));
    markSolid(x, y, 1, 1, 1, 6); markReserved(x - 2, y - 7, 5, 9);
    lamps.push_back({ x, y - 5 }); lights.push_back({ x, y - 3, 24, 170 });
}
static void drawBuilding(int x, int y, int w, int h, Rng& rng) {
    static const uint32_t roofs[6] = { RGB(170, 170, 178), RGB(156, 132, 120), RGB(120, 140, 164), RGB(196, 180, 150), RGB(108, 110, 124), RGB(150, 84, 70) };
    uint32_t rc = roofs[rng.range(6)];
    int wall = 4;
    for (int j = 0; j < h - wall; j++) for (int i = 0; i < w; i++) {
        uint32_t c = rc;
        if (i == 0 || j == 0) c = shade(rc, 1.25f); else if (i == w - 1 || j == h - wall - 1) c = shade(rc, 0.7f);
        else if (i == 1 || j == 1) c = shade(rc, 1.1f);
        gpx(x + i, y + j, c);
    }
    int ux = x + 3 + rng.range(std::max(1, w - 9)), uy = y + 3 + rng.range(std::max(1, h - wall - 8));
    for (int j = 0; j < 3; j++) for (int i = 0; i < 4; i++) gpx(ux + i, uy + j, (j == 2 || i == 3) ? RGB(90, 90, 96) : (j == 0 ? RGB(230, 230, 234) : RGB(190, 190, 196)));
    if (w > 14) { int sx = x + w - 6, sy = y + 3; for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) gpx(sx + i, sy + j, (i + j) == 0 ? C_WHITE : RGB(120, 190, 230)); }
    for (int j = h - wall; j < h; j++) for (int i = 0; i < w; i++) {
        uint32_t c = shade(rc, 0.55f);
        bool win = (j == h - wall + 1 || j == h - wall + 2) && (i % 3) == 1 && i < w - 1;
        if (win) c = (night && rng.range(3)) ? RGB(255, 230, 130) : RGB(120, 156, 196);
        gpx(x + i, y + j, c);
    }
    markSolid(x, y, w, h, 2, 12); markReserved(x - 3, y - 3, w + 6, h + 6);
    lights.push_back({ x + w / 2, y + h, 12, 70 });
}
static void drawHouse(int x, int y, Rng& rng) {
    if (theme == TH_CITY) { drawBuilding(x, y, 18, 16, rng); return; }
    uint32_t r1 = RGB(226, 100, 66), r2 = RGB(186, 58, 42), ridge = RGB(250, 176, 136), wall = RGB(240, 232, 216), door = RGB(120, 72, 40), win = night ? RGB(255, 230, 130) : RGB(88, 152, 224);
    if (theme == TH_WINTER) { r1 = RGB(244, 247, 252); r2 = RGB(214, 224, 240); ridge = C_WHITE; }
    if (theme == TH_GERMANY) { r1 = RGB(160, 70, 50); r2 = RGB(116, 46, 38); ridge = RGB(196, 110, 84); }
    if (theme == TH_CARNIVAL) { r1 = RGB(250, 220, 60); r2 = RGB(240, 120, 40); wall = RGB(120, 220, 230); door = RGB(230, 60, 160); }
    if (theme == TH_MEXICO) {
        for (int j = 0; j < 9; j++) for (int i = 0; i < 16; i++) {
            uint32_t c = RGB(214, 170, 110);
            if (i == 0 || j == 0) c = RGB(240, 204, 150); else if (i == 15 || j == 8) c = RGB(170, 124, 76); else if (i == 1 || j == 1 || i == 14 || j == 7) c = RGB(196, 150, 96);
            gpx(x + i, y + j, c);
        }
        for (int j = 9; j < 14; j++) for (int i = 0; i < 16; i++) gpx(x + i, y + j, j == 9 ? RGB(226, 70, 56) : ((i == 0 || i == 15) ? RGB(200, 160, 110) : RGB(238, 204, 156)));
        for (int i = 2; i < 15; i += 4) gpx(x + i, y + 9, RGB(110, 70, 40));
        door = RGB(40, 96, 176);
    } else {
        for (int j = 0; j < 9; j++) for (int i = 0; i < 16; i++) {
            uint32_t c = j < 4 ? r1 : (j == 4 ? ridge : r2);
            if (j == 8) c = shade(r2, 0.7f);
            if ((i + j * 2) % 5 == 0 && j != 4) c = shade(c, 0.9f);
            gpx(x + i, y + j, c);
        }
        for (int j = 9; j < 14; j++) for (int i = 0; i < 16; i++) {
            uint32_t c = (i == 0 || i == 15) ? shade(wall, 0.75f) : wall;
            if (theme == TH_GERMANY && (i % 5 == 0 || j == 9 || j == 13 || (i + j) % 5 == 2)) c = RGB(84, 52, 32);
            gpx(x + i, y + j, c);
        }
        for (int j = 1; j < 4; j++) { gpx(x + 11, y + j, RGB(96, 96, 100)); gpx(x + 12, y + j, RGB(64, 64, 68)); }
        smokers.push_back({ x + 11, y });
    }
    for (int j = 10; j < 12; j++) for (int i = 0; i < 3; i++) { gpx(x + 2 + i, y + j, win); gpx(x + 11 + i, y + j, win); }
    for (int j = 11; j < 14; j++) { gpx(x + 7, y + j, door); gpx(x + 8, y + j, shade(door, 0.75f)); }
    markSolid(x, y, 16, 14, 2, 9); markReserved(x - 3, y - 3, 22, 20);
    lights.push_back({ x + 8, y + 14, 12, 90 });
}

// one piece of automatic scenery, depending on the theme
static bool addScenery(int x, int y, Rng& rng) {
    int k = rng.range(20);
    auto leaf = [&](int kind) { if (!placeFree(x, y, 5)) return false; addLeafTree(x, y, rng, kind); return true; };
    switch (theme) {
    case TH_SPRING: if (k < 7) return leaf(2); if (k < 11) return leaf(3); if (k < 17) return leaf(0); break;
    case TH_AUTUMN: if (k < 16) return leaf(4); if (k < 18) return leaf(5); break;
    case TH_WINTER: if (k < 14) return leaf(1); return leaf(5);
    case TH_SWAMP: if (k < 16) return leaf(6); break;
    case TH_GERMANY:
        if (k < 8) return leaf(1);
        if (k < 14) return leaf(6);
        if (k < 17 && placeFree(x + 8, y + 7, 12, 11)) { drawHouse(x, y, rng); return true; }
        break;
    case TH_VILLAGE:
        if (k < 5 && placeFree(x + 8, y + 7, 12, 11)) { drawHouse(x, y, rng); return true; }
        if (k < 9 && placeFree(x + 8, y + 5, 11, 8)) {      // fenced field
            bool wheat = rng.range(2);
            for (int j = 0; j < 11; j++) for (int i = 0; i < 17; i++) {
                uint32_t c = wheat ? ((j & 1) ? RGB(226, 196, 84) : RGB(196, 164, 60)) : ((i & 1) ? RGB(120, 176, 60) : RGB(84, 136, 48));
                if ((i == 0 || i == 16 || j == 0 || j == 10)) c = ((i + j) & 1) ? RGB(150, 100, 56) : RGB(110, 72, 40);
                gpx(x + i, y + j, c);
            }
            markObj(x, y, 17, 11); markReserved(x - 2, y - 2, 21, 15); return true;
        }
        if (k < 11 && placeFree(x + 2, y + 2, 4)) {
            static const char* hay[5] = { "yyyyy", "yoyoy", "yyyyy", "yoyoy", "ooooo" }; static const uint32_t c[2] = { RGB(244, 216, 96), RGB(204, 160, 56) };
            sprite(x, y, hay, 5, "yo", c); markSolid(x, y, 5, 5, 1, 3); markReserved(x - 2, y - 2, 9, 9); return true;
        }
        if (k < 18) return leaf(0);
        break;
    case TH_CITY:
        if (k < 12) {
            int w = 12 + rng.range(12), h = 12 + rng.range(8);
            if (placeFree(x + w / 2, y + h / 2, w / 2 + 3, h / 2 + 3)) { drawBuilding(x, y, w, h, rng); return true; }
            return false;
        }
        if (k < 15 && placeFree(x + 4, y + 2, 7, 5)) {      // parked car
            uint32_t c = hue(rng.range(256));
            for (int j = 0; j < 5; j++) for (int i = 0; i < 9; i++) {
                uint32_t col = shade(c, 0.9f);
                if ((j == 0 || j == 4) && (i == 1 || i == 2 || i == 6 || i == 7)) col = RGB(20, 20, 20);
                else if (j == 0 || j == 4) col = shade(c, 0.6f);
                else if (i >= 3 && i <= 5) col = (i == 5 || i == 3) ? RGB(40, 60, 100) : shade(c, 1.3f);
                gpx(x + i, y + j, col);
            }
            markSolid(x, y, 9, 5, 1, 3); markReserved(x - 2, y - 2, 13, 9); return true;
        }
        if (k < 17 && placeFree(x, y, 5)) {                  // tree in a planter
            for (int j = -2; j <= 2; j++) for (int i = -2; i <= 2; i++) gpx(x + i, y + j, (abs(i) == 2 || abs(j) == 2) ? RGB(190, 190, 186) : RGB(110, 80, 50));
            addLeafTree(x, y, rng, 0); return true;
        }
        if (placeFree(x, y - 2, 2, 4)) { addLamp(x, y); return true; }
        return false;
    case TH_CARNIVAL:
        if (k < 7 && placeFree(x, y, 5)) { drawPalm(x, y); markReserved(x - 6, y - 10, 13, 14); return true; }
        if (k < 12 && placeFree(x + 1, y - 2, 3, 5)) {       // balloons
            for (int b = 0; b < 3; b++) { uint32_t c = hue(rng.range(256)); int bx = x + b * 2 - 1, by = y - 6 + (b & 1); gpx(bx, by, c); gpx(bx + 1, by, shade(c, 0.8f)); gpx(bx, by + 1, shade(c, 0.8f)); gpx(bx + 1, by + 1, shade(c, 0.6f)); gpx(bx, by - 0, C_WHITE); }
            for (int j = -4; j <= 0; j++) gpx(x + 1, y + j, RGB(230, 230, 230));
            markReserved(x - 3, y - 8, 9, 11); return true;
        }
        if (placeFree(x + 3, y + 1, 6, 4)) {                 // dancers
            for (int b = 0; b < 3; b++) { uint32_t c = hue(rng.range(256)); int bx = x + b * 3, by = y + (b & 1); gpx(bx, by, RGB(240, 180, 140)); gpx(bx + 1, by, RGB(240, 180, 140)); gpx(bx, by + 1, c); gpx(bx + 1, by + 1, c); gpx(bx, by + 2, shade(c, 0.7f)); gpx(bx + 1, by + 2, shade(c, 0.7f)); wavers.push_back({ bx, by, c, rng.range(64) }); }
            markObj(x, y, 9, 4); markReserved(x - 2, y - 2, 13, 8); return true;
        }
        return false;
    case TH_F1:
        if (k < 5 && placeFree(x + 2, y + 2, 4)) {
            static const char* ty[5] = { ".kkk.", "kgkgk", "kkwkk", "kgkgk", ".kkk." }; static const uint32_t c[3] = { RGB(24, 24, 28), RGB(70, 70, 76), RGB(236, 236, 236) };
            sprite(x, y, ty, 5, "kgw", c); markSolid(x, y, 5, 5, 1, 3); markReserved(x - 2, y - 2, 9, 9); return true;
        }
        if (k < 11 && placeFree(x + 8, y + 2, 11, 5)) {      // advertising board
            uint32_t c = hue(rng.range(256));
            for (int j = 0; j < 5; j++) for (int i = 0; i < 17; i++) gpx(x + i, y + j, j == 4 ? RGB(40, 40, 44) : ((j == 2 && i > 2 && i < 14 && (i % 3)) ? C_WHITE : (j == 0 ? shade(c, 1.2f) : c)));
            markSolid(x, y, 17, 5, 2, 5); markReserved(x - 2, y - 2, 21, 9); return true;
        }
        if (k < 16) return leaf(0);
        break;
    case TH_MOUNTAIN:
        if (k < 7 && placeFree(x + 4, y + 3, 6, 5)) {
            static const char* b[6] = { "..wwww...", ".wlllllg.", "wllllllgd", "lllllggdd", "llgggggdd", ".gggddd.." }; static const uint32_t c[4] = { RGB(236, 236, 240), RGB(168, 164, 160), RGB(124, 120, 118), RGB(80, 78, 80) };
            sprite(x, y, b, 6, "wlgd", c); markSolid(x, y + 1, 9, 5, 2, 6); markReserved(x - 2, y - 2, 13, 10); return true;
        }
        if (k < 16) return leaf(1);
        break;
    case TH_MEXICO:
        if (k < 7 && placeFree(x + 3, y + 5, 4, 6)) {        // saguaro
            static const char* s[10] = { "...l...", "..llg..", "..llg.l", "l.llg.g", "l.llglg", "llllgg.", ".gllg..", "..llg..", "..llg..", "..llg.." }; static const uint32_t c[2] = { RGB(60, 204, 84), RGB(28, 140, 60) };
            // the top and the arms stand above the cars (a car can drive behind them); the foot is solid
            sprite(x, y, s, 7, "lg", c, 0); sprite(x, y + 7, s + 7, 3, "lg", c);
            if (k == 0) opx(x + 3, y - 1, RGB(255, 90, 200), 99);
            markSolid(x + 2, y + 7, 3, 1, 2, 0); markSolid(x + 2, y + 8, 3, 2, 2, 8);       // the shadow falls from the foot
            markReserved(x - 2, y - 2, 11, 14); return true;
        }
        if (k < 10 && placeFree(x + 2, y + 2, 3)) {          // barrel cactus
            static const char* s[5] = { ".ff.", "lggd", "lggd", "lggd", ".dd." }; static const uint32_t c[4] = { RGB(255, 90, 200), RGB(110, 220, 110), RGB(40, 164, 70), RGB(20, 104, 50) };
            sprite(x, y, s, 2, "flgd", c, 0); sprite(x, y + 2, s + 2, 3, "flgd", c);      // top above the cars, front solid
            markSolid(x, y + 2, 4, 3, 1, 3); markReserved(x - 2, y - 2, 8, 9); return true;
        }
        if (k < 13 && placeFree(x + 4, y + 3, 6, 5)) {       // boulder
            static const char* b[6] = { "..wwww...", ".wlllllg.", "wllllllgd", "lllllggdd", "llgggggdd", ".gggddd.." }; static const uint32_t c[4] = { RGB(244, 226, 200), RGB(204, 156, 108), RGB(164, 114, 74), RGB(106, 66, 42) };
            sprite(x, y, b, 6, "wlgd", c); markSolid(x, y + 1, 9, 5, 2, 6); markReserved(x - 2, y - 2, 13, 10); return true;
        }
        if (k < 15 && placeFree(x + 2, y + 2, 4)) {          // dead bush
            static const char* s[5] = { "b..b.b", ".b.bb.", "..bb..", ".bbb.b", "..b..." }; static const uint32_t c[1] = { RGB(140, 84, 36) };
            sprite(x, y, s, 5, "b", c); markObj(x, y, 6, 5); markReserved(x - 1, y - 1, 8, 7); return true;
        }
        if (k < 17 && placeFree(x + 2, y + 1, 4, 3)) {       // bleached skull
            static const char* s[4] = { "w...w", ".www.", ".kwk.", "..w.." }; static const uint32_t c[2] = { RGB(250, 246, 232), RGB(30, 30, 30) };
            sprite(x, y, s, 4, "wk", c); markObj(x, y, 5, 4); markReserved(x - 1, y - 1, 7, 6); return true;
        }
        if (placeFree(x, y, 5)) { drawPalm(x, y); markReserved(x - 6, y - 10, 13, 14); return true; }
        return false;
    default:
        if (k < 12) return leaf(0);
        if (k < 16) return leaf(1);
        break;
    }
    // bush (also the fallback for several themes)
    if (!placeFree(x + 1, y + 1, 3)) return false;
    uint32_t c = theme == TH_WINTER ? RGB(220, 230, 244) : (theme == TH_AUTUMN ? RGB(200, 110, 40) : RGB(60, 150, 60));
    for (int j = 0; j < 4; j++) for (int i = 0; i < 5; i++) if (!((j == 0 || j == 3) && (i == 0 || i == 4))) gpx(x + i, y + j, (i + j < 3) ? shade(c, 1.3f) : (i + j > 5 ? shade(c, 0.6f) : c));
    if (theme == TH_SPRING) { gpx(x + 1, y + 1, RGB(255, 150, 200)); gpx(x + 3, y + 2, C_WHITE); }
    markObj(x, y, 5, 4); castH[(y + 1) * W + x + 2] = 2; markReserved(x - 1, y - 1, 7, 6);
    return true;
}

static void placeObjects(Rng& rng) {
    // tree tops over the road (flag fC): the cars vanish beneath them
    for (int k = 0; k < NS; k += std::max(1, (int)(7 / spacing))) {
        const Sample& s = sm[k];
        if (!(s.f & fC)) continue;
        float d = (rng.f() * 2 - 1) * (s.w / 2 + 3);
        int x = (int)(s.x - s.ty * d), y = (int)(s.y + s.tx * d) + 4;
        if (!inb(x, y) || y < HUD_H + 12) continue;
        int kind = theme == TH_GERMANY || theme == TH_SWAMP ? 6 : (theme == TH_AUTUMN ? 4 : (theme == TH_SPRING ? 2 + rng.range(2) : 0));
        int r = 6 + rng.range(2);
        if (surf[0][y * W + x] == S_OFF && placeFree(x, y, 2)) { addLeafTree(x, y, rng, kind); continue; }
        drawCanopy(x, y, r, kind); treeTops.push_back({ x, y - 4, r, kind });
    }
    for (const Obj& o : def->o) {
        int x = (int)o.a, y = (int)o.b;
        switch (o.t) {
        case O_STAND: {
            int w = (int)o.c, h = (int)o.d;
            for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
                uint32_t c = RGB(70, 72, 84);
                if (j == 0 || i == 0) c = RGB(236, 236, 240); else if (j == h - 1 || i == w - 1) c = RGB(150, 150, 160);
                else if ((j - 1) % 4 == 3) c = RGB(120, 122, 136);
                gpx(x + i, y + j, c);
            }
            for (int j = 1; j + 3 < h; j += 4) for (int i = 2; i + 2 < w; i += 3) {
                if (rng.range(10) < 2) continue;
                uint32_t sc = hue(rng.range(256)), skin = rng.range(3) ? RGB(244, 190, 150) : RGB(150, 100, 70);
                if (rng.range(5) == 0) sc = C_WHITE;
                gpx(x + i, y + j, skin); gpx(x + i + 1, y + j, shade(skin, 0.85f));
                gpx(x + i, y + j + 1, sc); gpx(x + i + 1, y + j + 1, shade(sc, 0.8f)); gpx(x + i, y + j + 2, shade(sc, 0.7f)); gpx(x + i + 1, y + j + 2, shade(sc, 0.6f));
                if (rng.range(3) == 0) wavers.push_back({ x + i, y + j, skin, rng.range(64) });
            }
            markSolid(x, y, w, h, 2, 7); markReserved(x - 3, y - 3, w + 6, h + 6);
            lights.push_back({ x + w / 2, y + h / 2, std::max(w, h) / 2 + 8, 120 });
        } break;
        case O_HOUSE: drawHouse(x, y, rng); break;
        case O_TENT: {
            uint32_t tc = theme == TH_GERMANY ? RGB(60, 130, 230) : (theme == TH_MEXICO || theme == TH_CARNIVAL ? RGB(232, 60, 60) : RGB(60, 150, 236));
            for (int j = 0; j < 10; j++) for (int i = 0; i < 15; i++) {
                uint32_t c = ((i / 3) & 1) ? C_WHITE : tc;
                if (theme == TH_GERMANY) c = (((i + j) / 2) & 1) ? C_WHITE : tc;
                if (j < 4) c = shade(c, 1.0f); else if (j == 4) c = shade(c, 0.65f); else c = shade(c, 0.85f);
                if (j == 9) c = RGB(60, 60, 66);
                gpx(x + i, y + j, c);
            }
            markSolid(x, y, 15, 10, 2, 6); markReserved(x - 3, y - 3, 21, 16);
        } break;
        case O_HAY: {
            static const char* hay[5] = { "yyyyy", "yoyoy", "yyyyy", "yoyoy", "ooooo" }; static const uint32_t c[2] = { RGB(244, 216, 96), RGB(204, 160, 56) };
            sprite(x, y, hay, 5, "yo", c); markSolid(x, y, 5, 5, 1, 3); markReserved(x - 1, y - 1, 7, 7);
        } break;
        case O_TREE: addLeafTree(x, y, rng, theme == TH_AUTUMN ? 4 : 0); break;
        case O_ROCK: {
            static const char* b[5] = { ".www...", "wllllg.", "lllllgd", "llggggd", ".gggdd." }; static const uint32_t c[4] = { RGB(236, 236, 240), RGB(168, 164, 160), RGB(124, 120, 118), RGB(80, 78, 80) };
            sprite(x, y, b, 5, "wlgd", c); markSolid(x, y + 1, 7, 4, 1, 4); markReserved(x - 2, y - 2, 11, 9);
        } break;
        case O_FLAG: {
            for (int j = 0; j < 10; j++) gpx(x, y + j, j == 9 ? RGB(64, 64, 68) : RGB(230, 230, 234));
            flags.push_back({ x, y }); markSolid(x, y + 8, 1, 2, 1, 7); markReserved(x - 1, y - 1, 10, 12);
        } break;
        case O_SNOWMAN: {
            static const char* s[10] = { "..kkk..", ".kkkkk.", "..www..", ".wkwkw.", ".wwoww.", "..www..", ".wwwww.", "wwwkwws", "wwwwwws", ".wwwss." };
            static const uint32_t c[4] = { RGB(30, 30, 34), C_WHITE, RGB(255, 140, 30), RGB(200, 214, 236) };
            sprite(x, y, s, 10, "kwos", c); markSolid(x, y + 6, 7, 4, 1, 6); markObj(x, y, 7, 10); markReserved(x - 2, y - 2, 11, 14);
        } break;
        case O_LAMP: addLamp(x, y); break;
        default: break;
        }
    }
    // street lamps along the road in the city and on night tracks
    if (night || theme == TH_CITY) {
        int side = 1;
        for (int k = 0; k < NS; k += (int)(46 / spacing), side = -side) {
            const Sample& s = sm[k];
            if (s.f & (fG | fY | fB | fT)) continue;
            int x = (int)(s.x - s.ty * (s.w / 2 + 5) * side), y = (int)(s.y + s.tx * (s.w / 2 + 5) * side);
            if (placeFree(x, y - 2, 1, 4)) addLamp(x, y);
        }
    }
    // lakes get ducks
    for (const Obj& o : def->o) if (o.t == O_LAKE && theme != TH_WINTER && theme != TH_MEXICO && o.c > 14) {
        ducks.push_back({ o.a - o.c * 0.3f, o.b, std::min(o.c, o.d) * 0.3f, rng.f() * 6 });
        ducks.push_back({ o.a + o.c * 0.3f, o.b, std::min(o.c, o.d) * 0.25f, rng.f() * 6 });
    }
    // automatic scenery away from the road
    int placed = 0;
    for (int tries = 0; tries < 9000 && placed < def->trees; tries++) {
        int x = 8 + rng.range(W - 30), y = HUD_H + 14 + rng.range(H - HUD_H - 30);
        if (distRoad[y * W + x] < 12) continue;
        bool nearWater = false;
        for (int j = -6; j <= 6 && !nearWater; j += 3) for (int i = -6; i <= 6; i += 3) if (inb(x + i, y + j) && water0[(y + j) * W + x + i]) nearWater = true;
        if (nearWater) continue;
        if (addScenery(x, y, rng)) placed++;
    }
    // Mexico: wind ripples in the sand
    if (sandy()) for (int i = 0; i < 90; i++) {
        int x = 6 + rng.range(W - 20), y = HUD_H + 4 + rng.range(H - HUD_H - 8), len = 5 + rng.range(7);
        for (int k = 0; k < len; k++) {
            int xx = x + k, yy = y + (int)lroundf(sinf(k * 0.9f + i));
            if (!inb(xx, yy + 1) || yy < HUD_H) continue;
            int p = yy * W + xx;
            if (surf[0][p] != S_OFF || surf[1][p] != S_NONE || water0[p] || solid[0][p] || objMask[p] || reserved[p] || distRoad[p] < 4) continue;
            ground[p] = RGB(210, 176, 100);
            if (surf[0][p + W] == S_OFF && !objMask[p + W] && !solid[0][p + W] && !water0[p + W]) ground[p + W] = RGB(250, 228, 160);
        }
    }
    // swamp: reeds and lily pads on the water
    if (theme == TH_SWAMP) for (int i = 0; i < 420; i++) {
        int x = 3 + rng.range(W - 6), y = HUD_H + 4 + rng.range(H - HUD_H - 8), p = y * W + x;
        if (surf[0][p] != S_WATER || distRoad[p] < 4 || surf[1][p] != S_NONE) continue;
        if (rng.range(3) == 0) { gpx(x, y, RGB(120, 176, 70)); gpx(x + 1, y, RGB(90, 150, 60)); gpx(x, y + 1, RGB(70, 124, 54)); if (rng.range(3) == 0) gpx(x + 1, y - 1, RGB(255, 190, 220)); }
        else { for (int j = 0; j < 3 + rng.range(2); j++) gpx(x, y - j, j == 0 ? RGB(60, 84, 40) : RGB(150, 170, 80)); gpx(x, y - 4, RGB(110, 70, 40)); }
    }
    // small ground details
    for (int i = 0; i < 280; i++) {
        int x = 3 + rng.range(W - 6), y = HUD_H + 3 + rng.range(H - HUD_H - 6);
        if (distRoad[y * W + x] < 4 || !placeFree(x, y, 1)) continue;
        int k = rng.range(6);
        const ThemeCol& T = THEMES[theme];
        switch (theme) {
        case TH_CITY: if (k < 2) { gpx(x, y, RGB(110, 110, 108)); gpx(x + 1, y, RGB(110, 110, 108)); } else if (k == 2) { gpx(x, y, RGB(60, 60, 64)); gpx(x + 1, y, RGB(60, 60, 64)); gpx(x, y + 1, RGB(60, 60, 64)); gpx(x + 1, y + 1, RGB(90, 90, 96)); } break;   // cracks, manholes
        case TH_AUTUMN: { static const uint32_t lc[4] = { RGB(240, 140, 40), RGB(214, 60, 36), RGB(250, 220, 80), RGB(150, 50, 30) }; gpx(x, y, lc[k & 3]); if (k > 3) gpx(x + 1, y, lc[(k + 1) & 3]); } break;
        case TH_WINTER: if (k < 3) { gpx(x, y, RGB(200, 216, 240)); gpx(x + 1, y, RGB(180, 196, 230)); } else { gpx(x, y, RGB(130, 130, 140)); gpx(x, y - 1, C_WHITE); } break;
        case TH_MEXICO:
            if (k < 2) gpx(x, y, RGB(200, 150, 80));
            else if (k < 4) { gpx(x, y, RGB(150, 140, 50)); gpx(x + 1, y - 1, RGB(176, 166, 60)); gpx(x - 1, y - 1, RGB(150, 140, 50)); }
            else { gpx(x, y, RGB(200, 152, 104)); gpx(x + 1, y, RGB(160, 112, 72)); gpx(x, y - 1, RGB(240, 224, 200)); }
            break;
        case TH_MOUNTAIN: gpx(x, y, RGB(150, 146, 140)); gpx(x + 1, y, RGB(110, 108, 106)); if (k < 3) gpx(x, y - 1, RGB(220, 220, 224)); break;
        case TH_CARNIVAL: case TH_F1: break;
        default:
            if (k == 0) { gpx(x, y, RGB(255, 230, 80)); gpx(x, y + 1, T.offA); }
            else if (k == 1) { gpx(x, y, C_WHITE); gpx(x, y + 1, T.offA); }
            else if (k == 2) { gpx(x, y, RGB(255, 140, 210)); }
            else if (k == 3) { gpx(x, y, T.offA); gpx(x + 1, y - 1, T.offA); gpx(x - 1, y - 1, T.offA); }
            else if (k == 4) { gpx(x, y, T.offB); gpx(x + 1, y, T.offA); gpx(x, y - 1, T.offA); gpx(x + 1, y - 1, T.offB); }
            else gpx(x, y, RGB(150, 150, 150));
        }
    }
}

static void buildTrains() {
    trains.clear(); crossings.clear(); signals.clear(); signalTrain.clear();
    for (const Obj& o : def->o) {
        if (o.t != O_TRAIN) continue;
        Train t{};
        t.horiz = fabsf(o.b - o.d) < 0.5f;
        t.line = t.horiz ? o.b : o.a; t.from = t.horiz ? o.a : o.b; t.to = t.horiz ? o.c : o.d;
        t.dir = t.to > t.from ? 1 : -1; t.speed = 130; t.period = o.e; t.phase = o.f;
        int ti = (int)trains.size();
        trains.push_back(t);
        bool prevRoad = false;
        for (int a = 0; a < (t.horiz ? W : H); a++) {
            int x = t.horiz ? a : (int)t.line, y = t.horiz ? (int)t.line : a;
            if (y < HUD_H) continue;
            int p = y * W + x;
            bool rd = roadLike(surf[0][p]);
            for (int d = -4; d <= 4; d++) {
                int xx = t.horiz ? x : x + d, yy = t.horiz ? y + d : y;
                if (!inb(xx, yy)) continue;
                bool rail = (d == -3 || d == 3);
                if (rail) gpx(xx, yy, (a & 1) ? RGB(214, 214, 220) : RGB(180, 180, 190));
                else if (!rd && a % 4 == 0) gpx(xx, yy, RGB(120, 76, 40));
                else if (!rd && a % 4 == 1) gpx(xx, yy, RGB(90, 56, 30));
                else if (!rd && abs(d) < 3 && (hash2(xx, yy, 5) & 3) == 0) gpx(xx, yy, RGB(130, 126, 120));
                else if (rd && (d == -2 || d == 2)) gpx(xx, yy, RGB(60, 60, 64));
            }
            for (int d = -10; d <= 10; d++) { int xx = t.horiz ? x : x + d, yy = t.horiz ? y + d : y; if (inb(xx, yy)) reserved[yy * W + xx] = 1; }
            if (rd != prevRoad) {
                int sa = rd ? a - 3 : a + 2;
                for (int side = -1; side <= 1; side += 2) {
                    Pt s = t.horiz ? Pt{ sa, (int)t.line + side * 8 } : Pt{ (int)t.line + side * 8, sa };
                    signals.push_back(s); signalTrain.push_back(ti);
                }
            }
            prevRoad = rd;
        }
        int lastK = -100000;
        for (int k = 0; k < NS; k++) {
            const Sample& s = sm[k];
            if ((s.f & (fG | fB))) continue;
            float across = t.horiz ? s.y : s.x, along = t.horiz ? s.x : s.y;
            if (fabsf(across - t.line) < 0.6f && k - lastK > 60) {
                bool ramp = false;
                for (int j = 0; j < (int)(55 / spacing); j++) if (sm[(k - j + NS) % NS].ramp > 0) ramp = true;
                crossings.push_back({ k, ti, along, ramp }); lastK = k;
            }
        }
    }
}

// turntables, drawbridges and ferries (built after the roads are stamped)
static void buildMechanics() {
    turntables.clear(); bridges.clear(); ferries.clear();
    for (int p = 0; p < N; p++) { dynSolid[p] = 0; ttMask[p] = 0; spanMask[p] = 0; }
    for (const Obj& o : def->o) {
        if (o.t == O_TURNTABLE) {
            Turntable t{ o.a, o.b, o.c, o.d, 0, 0 };
            int id = (int)turntables.size() + 1, r = (int)o.c;
            for (int y = (int)o.b - r; y <= (int)o.b + r; y++) for (int x = (int)o.a - r; x <= (int)o.a + r; x++) {
                if (!inb(x, y) || y < HUD_H || (x - o.a) * (x - o.a) + (y - o.b) * (y - o.b) > o.c * o.c) continue;
                int p = y * W + x;
                if (!roadLike(surf[0][p])) { idxm[0][p] = nearIdx[p]; latm[0][p] = 0; }
                surf[0][p] = S_METAL; solid[0][p] = 0; israil[0][p] = 0; ht[0][p] = 0; ttMask[p] = (uint8_t)id;
            }
            markReserved((int)o.a - r - 18, (int)o.b - r - 18, 2 * r + 36, 2 * r + 36);
            turntables.push_back(t);
        } else if (o.t == O_DRAWBRIDGE) {
            Drawbridge d{}; d.period = o.d; d.phase = o.e; d.open = true;
            int best = 0; float bd = 1e9f;
            for (int k = 0; k < NS; k++) { float dd = hypotf(sm[k].x - o.a, sm[k].y - o.b); if (dd < bd) { bd = dd; best = k; } }
            int hl = (int)(o.c / spacing);
            d.k0 = std::max(0, best - hl); d.k1 = std::min(NS - 1, best + hl);
            int g0 = (int)(2 / spacing), g1 = (int)(4.4f / spacing), id = (int)bridges.size() + 1;
            for (int p = HUD_H * W; p < N; p++) {
                if (!roadLike(surf[0][p]) || israil[0][p]) continue;
                int i = idxm[0][p];
                if (i >= d.k0 && i <= d.k1 && surf[0][p] == S_PLANK) { d.span.push_back(p); d.spanPos.push_back((uint8_t)((i - d.k0) * 255 / std::max(1, d.k1 - d.k0))); spanMask[p] = (uint8_t)id; }
                else if ((i <= d.k0 - g0 && i >= d.k0 - g1) || (i >= d.k1 + g0 && i <= d.k1 + g1)) d.barrier.push_back(p);
            }
            bridges.push_back(d);
        } else if (o.t == O_FERRY) {
            Ferry f{};
            f.ax = o.a; f.ay = o.b; f.bx = o.c; f.by = o.d; f.x = f.ax; f.y = f.ay;
            float l = hypotf(f.bx - f.ax, f.by - f.ay); f.dx = (f.bx - f.ax) / l; f.dy = (f.by - f.ay) / l;
            f.hl = 18; f.hw = 14; f.state = 0; f.tLeft = 2.0f;
            int run = 0, want = (int)ferries.size(); f.kA = 0; f.kB = 0;
            for (int k = 1; k < NS; k++) {
                if ((sm[k].f & fY) && !(sm[k - 1].f & fY)) { if (run == want) f.kA = k - 1; }
                if (!(sm[k].f & fY) && (sm[k - 1].f & fY)) { if (run == want) f.kB = k; run++; }
            }
            f.hw = sm[f.kA].w / 2 + 2;
            ferries.push_back(f);
        }
    }
}

static void renderGround() {
    const ThemeCol& T = THEMES[theme];
    for (int p = 0; p < N; p++) ground[p] = colorAt(0, p);
    for (int y = HUD_H + 2; y < H - 1; y++) for (int x = 1; x < W - 1; x++) {
        int p = y * W + x;
        bool w = surf[0][p] == S_WATER;
        int nw = 0;
        for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) { uint8_t s = surf[0][p + j * W + i]; if (s == S_WATER || s == S_SHALLOW || s == S_PLANK || s == S_METAL) nw++; }
        uint32_t hs = hash2(x, y, levelNo + 9);
        if (w && nw < 9) ground[p] = waterCol(1);
        else if (w && (surf[0][p - W] == S_PLANK || surf[0][p - 2 * W] == S_PLANK) && (x % 7) < 2) ground[p] = RGB(52, 36, 22);       // stilts
        else if (surf[0][p] == S_OFF && nw > 0) ground[p] = T.shore;
        else if (surf[0][p] == S_OFF && distRoad[p] >= 1 && distRoad[p] <= 6 && surf[1][p] == S_NONE) {
            // verge: the ground changes towards the road edge
            int d = distRoad[p];
            bool dirt = sm[nearIdx[p]].type == S_DIRT;
            if (dirt && d == 1 && (hs % 3) == 0) ground[p] = RGB(140, 70, 0);
            else if (dirt && d == 2 && (hs % 6) == 0) ground[p] = RGB(176, 104, 24);
            else if (theme == TH_F1) ground[p] = d == 6 && (hs & 1) ? ground[p] : ((hs % 5) == 0 ? RGB(196, 176, 130) : ((hs % 7) == 0 ? RGB(236, 220, 176) : RGB(218, 200, 154)));   // gravel trap
            else if (theme == TH_CITY) ground[p] = d == 1 ? RGB(110, 110, 112) : (d <= 3 ? RGB(196, 196, 190) : ground[p]);          // kerbstone and pavement
            else if (theme == TH_CARNIVAL) ground[p] = d <= 2 ? RGB(250, 250, 250) : ground[p];
            else if (sandy()) ground[p] = d == 1 ? RGB(196, 152, 84) : (d == 2 ? RGB(214, 178, 100) : (d == 3 && (hs & 1) ? RGB(224, 190, 112) : ground[p]));
            else if (theme == TH_WINTER) ground[p] = d == 1 ? RGB(190, 202, 230) : (d == 2 ? RGB(216, 224, 242) : ground[p]);
            else if (d < 3) ground[p] = shade(T.off, d == 1 ? 0.74f : 0.88f);
        } else if (surf[0][p] == S_DIRT && fabsf(latm[0][p]) > 0.9f && (hs % 3) == 0) {
            ground[p] = sandy() ? RGB(196, 152, 84) : shade(T.off, 0.74f);
        }
    }
    // turntable pits and the rail stubs leading to them
    for (const Turntable& t : turntables) for (int a = 0; a < 4; a++) {
        float an = a * PI / 2 + PI / 4, cx = cosf(an), sy = sinf(an);
        for (int d = (int)t.r + 1; d < (int)t.r + 17; d++) for (int s = -3; s <= 3; s++) {
            int x = (int)(t.x + cx * d - sy * s), y = (int)(t.y + sy * d + cx * s);
            if (!inb(x, y) || y < HUD_H || surf[0][y * W + x] != S_OFF) continue;
            gpx(x, y, abs(s) == 3 ? RGB(200, 200, 208) : ((d % 4) == 0 ? RGB(110, 70, 40) : ground[y * W + x]));
            objMask[y * W + x] = 1;
        }
    }
}

static void finishGround() {
    // cliff faces south of raised ground
    for (int y = H - 1; y >= HUD_H; y--) for (int x = 0; x < W; x++) {
        int p = y * W + x;
        if (objMask[p]) continue;
        for (int k = 1; k <= 8 && y - k >= HUD_H; k++) {
            float d = ht[0][p - k * W] - ht[0][p];
            if (d >= 2 * k - 0.5f) { ground[p] = (d < 2 * k + 1.5f) ? RGB(176, 172, 168) : (((x / 2 + y) & 3) == 0 ? RGB(60, 58, 60) : RGB(104, 100, 100)); objMask[p] = 2; break; }
        }
    }
    // slope shading (light comes from the top left)
    for (int y = HUD_H + 1; y < H - 1; y++) for (int x = 1; x < W - 1; x++) {
        int p = y * W + x;
        if (objMask[p]) continue;
        float d = ht[0][p + W + 1] - ht[0][p - W - 1];
        if (fabsf(d) > 3.0f) { if (d < 0) ground[p] = shade(ground[p], 0.6f); continue; }
        if (fabsf(d) > 0.05f) ground[p] = shade(ground[p], clampf(1 + d * 0.36f, 0.55f, 1.45f));
    }
    // drop shadows
    for (int p = 0; p < N; p++) {
        castH[p] = std::max(castH[p], ht[0][p] + (israil[0][p] ? 2.0f : 0));
        if (surf[0][p] == S_PLANK && water0[p]) castH[p] = std::max(castH[p], 3.0f);
        if (surf[1][p] != S_NONE) castH[p] = std::max(castH[p], ht[1][p] + (israil[1][p] ? 2.0f : 0));
        shadowM[p] = 0;
    }
    for (int y = HUD_H; y < H; y++) for (int x = 0; x < W; x++) {
        int p = y * W + x;
        float h0 = std::max(ht[0][p], (solid[0][p] == 2 || objMask[p] == 1 || surf[0][p] == S_PLANK) ? castH[p] : ht[0][p] + (israil[0][p] ? 2.0f : 0));
        for (int k = 1; k <= 11; k++) {
            if (x - k < 0 || y - k < HUD_H) break;
            if (castH[p - k * W - k] > h0 + k * 1.15f) { ground[p] = shade(ground[p], 0.62f); shadowM[p] = 1; break; }
        }
    }
    // carnival: the open ground cycles through psychedelic colours at run time
    tripPix.clear(); tripVal.clear();
    if (theme == TH_CARNIVAL) for (int p = HUD_H * W; p < N; p++) {
        if (surf[0][p] != S_OFF || objMask[p] || solid[0][p] || distRoad[p] <= 2 || water0[p]) continue;
        int x = p % W, y = p / W;
        float v = sinf(x * 0.075f + sinf(y * 0.06f) * 2.4f) + sinf(y * 0.09f + sinf(x * 0.05f) * 2.0f) + sinf(hypotf(x - 160.f, y - 125.f) * 0.11f);
        tripPix.push_back(p); tripVal.push_back((uint8_t)(int)(v * 42 + 128 + (shadowM[p] ? 0 : 0)));
    }
}

static void buildOverlay() {
    for (int p = 0; p < N; p++) if (surf[1][p] != S_NONE) { overC[p] = colorAt(1, p); overA[p] = 1; overH[p] = ht[1][p]; }
    for (int y = H - 1; y >= HUD_H; y--) for (int x = 0; x < W; x++) {
        int p = y * W + x;
        if (surf[1][p] != S_NONE) continue;
        for (int k = 1; k <= 4 && y - k >= HUD_H; k++)
            if (surf[1][p - k * W] != S_NONE) { overC[p] = k == 1 ? RGB(190, 190, 196) : ((x % 8) < 2 ? RGB(150, 150, 156) : RGB(92, 92, 100)); overA[p] = 1; overH[p] = ht[1][p - k * W]; break; }
    }
    const ThemeCol& T = THEMES[theme];
    for (int k = 0; k < NS; k++) {
        const Sample& s = sm[k];
        if (!(s.f & fT)) continue;
        float nx = -s.ty, ny = s.tx, half = s.w / 2;
        for (float d = -half - 7; d <= half + 7; d += 0.4f) {
            int x = (int)floorf(s.x + nx * d), y = (int)floorf(s.y + ny * d);
            if (!inb(x, y) || y < HUD_H) continue;
            uint32_t hs = hash2(x, y, 77);
            bool lit = d * (ny - nx) <= 0;
            uint32_t c = hs % 6 == 0 ? T.offA : shade(T.off, lit ? 1.08f : 0.78f);
            if (fabsf(d) > half + 5.8f) c = RGB(40, 40, 44);
            else if (fabsf(d) > half + 3.0f) c = lit ? RGB(220, 220, 224) : RGB(110, 110, 116);
            else if (fabsf(d) > half) c = lit ? RGB(160, 160, 166) : RGB(132, 132, 138);
            if (s.portal) c = fabsf(d) <= half ? RGB(10, 10, 14) : RGB(176, 176, 182);
            opx(x, y, c, 99);
            castH[y * W + x] = std::max(castH[y * W + x], 6.0f);
        }
    }
}

void buildItems() {
    items.clear();
    const int cnt = 5;
    for (int i = 0; i < cnt; i++) {
        int k = (int)((i + 0.5f) * NS / cnt);
        for (int tries = 0; tries < NS; tries++, k = (k + 37) % NS) {
            const Sample& s = sm[k];
            bool bad = (s.f & (fG | fF | fT | fY)) || s.ramp > 0 || s.over || fabsf(s.h - sm[(k + 80) % NS].h) > 0.3f || s.type == S_BOOST || k * spacing < 36;
            for (const Crossing& c : crossings) if (abs(c.k - k) < (int)(28 / spacing)) bad = true;
            for (int j = 1; j < 56 && !bad; j++) if (sm[(k - (int)(j / spacing) + NS) % NS].ramp > 0) bad = true; // not in a landing zone
            int px = (int)s.x, py = (int)s.y;
            if (inb(px, py) && (ttMask[py * W + px] || !roadLike(surf[(s.f & fB) ? 1 : 0][py * W + px]))) bad = true;
            if (!bad) break;
        }
        const Sample& s = sm[k];
        float lat = ((i & 1) ? 1 : -1) * s.w * 0.2f;
        items.push_back({ s.x - s.ty * lat, s.y + s.tx * lat, s.h, 0, 1 + (int)(grng.next() % 4) });
    }
}

void buildLevel(int n) {
    levelNo = n; def = &TRACKS[n]; theme = def->theme; weather = def->weather;
    Rng rng(n * 7919 + 101);
    wavers.clear(); flags.clear(); smokers.clear(); ducks.clear(); treeTops.clear(); waterPix.clear(); lamps.clear(); lights.clear(); leafCount = 0;
    night = flood = false; windX = windY = 0; gust = 1;
    for (const Obj& o : def->o) { if (o.t == O_NIGHT) night = true; if (o.t == O_FLOOD) flood = true; if (o.t == O_WIND) { windX = o.a; windY = o.b; } }
    for (int p = 0; p < N; p++) {
        surf[0][p] = (flood && p >= HUD_H * W) ? S_WATER : S_OFF; surf[1][p] = S_NONE;
        for (int l = 0; l < 2; l++) { solid[l][p] = 0; israil[l][p] = 0; ht[l][p] = 0; latm[l][p] = 0; idxm[l][p] = 0; }
        reserved[p] = 0; objMask[p] = 0; castH[p] = 0; overA[p] = 0; overH[p] = 0; overC[p] = 0;
    }
    int seed = n * 13;
    for (const Obj& o : def->o) if (o.t == O_ISLAND) stampBlob(o.a, o.b, o.c, o.d, seed++, -1);
    for (const Obj& o : def->o) {
        if (o.t == O_LAKE) stampBlob(o.a, o.b, o.c, o.d, seed++, 0);
        if (o.t == O_RIVER) stampRiver(o.a, o.b, o.c, o.d, o.e, seed++);
    }
    for (int p = 0; p < N; p++) water0[p] = surf[0][p] == S_WATER;
    buildSamples();
    stampRoads();
    for (const Obj& o : def->o) {
        if (o.t == O_ICE) stampBlob(o.a, o.b, o.c, o.d, seed++, S_ICE);
        if (o.t == O_DIRT) stampBlob(o.a, o.b, o.c, o.d, seed++, S_DIRT);
        if (o.t == O_PUDDLE) stampBlob(o.a, o.b, o.c, o.d, seed++, S_PUDDLE);
        if (o.t == O_BOOST)
            for (int y = (int)(o.b - o.c); y <= (int)(o.b + o.c); y++) for (int x = (int)(o.a - o.c); x <= (int)(o.a + o.c); x++)
                if (inb(x, y) && (x - o.a) * (x - o.a) + (y - o.b) * (y - o.b) <= o.c * o.c + 1 && roadLike(surf[0][y * W + x]) && surf[0][y * W + x] != S_PLANK) surf[0][y * W + x] = S_BOOST;
    }
    bfsNearest();
    buildMechanics();
    renderGround();
    buildTrains();
    buildOverlay();
    placeObjects(rng);
    finishGround();
    buildItems();
    for (int p = 0; p < N; p++) if (surf[0][p] == S_WATER || surf[0][p] == S_SHALLOW) waterPix.push_back(p);
    // static light map for night tracks
    if (night) {
        for (int p = 0; p < N; p++) lightStatic[p] = 72;
        for (const Light& l : lights)
            for (int y = l.y - l.r; y <= l.y + l.r; y++) for (int x = l.x - l.r; x <= l.x + l.r; x++) {
                if (!inb(x, y)) continue;
                float d = hypotf((float)(x - l.x), (float)(y - l.y));
                if (d < l.r) { int v = lightStatic[y * W + x] + (int)(l.amt * (1 - d / l.r) * (1 - d / l.r)); lightStatic[y * W + x] = (uint8_t)std::min(255, v); }
            }
    }
}
