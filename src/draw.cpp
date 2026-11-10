// Rally Mayhem - drawing the track, the cars, weather and night lighting
// by rsn8887

#include "game.h"

// ------------------------------------------------------------------ drawing
static inline uint32_t mixc(uint32_t a, uint32_t b, float t) {
    int r = (int)(((a >> 16) & 255) * (1 - t) + ((b >> 16) & 255) * t), g = (int)(((a >> 8) & 255) * (1 - t) + ((b >> 8) & 255) * t), bl = (int)((a & 255) * (1 - t) + (b & 255) * t);
    return RGB(r, g, bl);
}
static void drawCar(const Car& c) {
    if (c.dead > 0) return;
    if (c.invuln > 0 && (frameNo & 4)) return;
    uint32_t body = CAR_COL[c.slot], dark = shade(body, 0.5f), side = shade(body, 0.78f), roof = mixc(body, C_WHITE, 0.45f), roofHi = mixc(body, C_WHITE, 0.75f);
    float cs = cosf(c.a), sn = sinf(c.a), zv = c.z - c.gvis;
    // shadow (the pixel exact collision footprint, shifted)
    int m[96][2], n = carMask(c.x, c.y, c.a, m), so = 1 + (int)(zv * 0.6f);
    for (int i = 0; i < n; i++) pdark(m[i][0] + so, m[i][1] + 1 + (int)(zv * 0.2f), 0.55f);
    int cx = (int)floorf(c.x), cy = (int)floorf(c.y - zv);
    for (int py = cy - 11; py <= cy + 8; py++) for (int px = cx - 8; px <= cx + 8; px++) {
        for (int L = 2; L >= 0; L--) {
            float wx = px + 0.5f - c.x, wy = py + 0.5f - (c.y - zv) + L;
            float lx = wx * cs + wy * sn, ly = -wx * sn + wy * cs;
            float wy2 = wy + c.pitch * lx * 1.2f + c.roll * ly * 1.2f;
            lx = wx * cs + wy2 * sn; ly = -wx * sn + wy2 * cs;
            int ix = (int)floorf(lx + CAR_HL), iy = (int)floorf(ly + CAR_HW);
            if (ix < 0 || ix > 8 || iy < 0 || iy > 4) continue;
            uint32_t col = 0;
            if (L == 0) col = ((ix == 1 || ix == 2 || ix == 6 || ix == 7) && (iy == 0 || iy == 4)) ? RGB(16, 16, 18) : dark;
            else if (L == 1) {
                if (ix == 8) col = (iy == 0 || iy == 4) ? RGB(255, 244, 170) : side;
                else if (ix == 0) col = (iy == 0 || iy == 4) ? RGB(220, 24, 24) : dark;
                else if (iy == 0 || iy == 4) col = side;
                else col = (ix == 7 && iy == 2) ? mixc(body, C_WHITE, 0.3f) : body;
            } else {
                if (ix < 2 || ix > 5 || iy < 1 || iy > 3) continue;
                if (ix == 5) col = iy == 1 ? RGB(110, 150, 220) : RGB(28, 44, 92);
                else if (ix == 2) col = RGB(20, 30, 64);
                else col = iy == 1 ? roofHi : roof;
            }
            pset(px, py, col);
            break;
        }
    }
}
static bool carUnder(const Car& c) {
    for (int j = -5; j <= 5; j += 5) for (int i = -5; i <= 5; i += 5) {
        int x = (int)c.x + i, y = (int)c.y + j;
        if (inb(x, y) && overA[y * W + x] && c.z < overH[y * W + x] - 3) return true;
    }
    return false;
}
// 7x7 weapon symbol; with a background it becomes the 9x9 pick-up tile
void drawIcon(int x, int y, int w, bool tile) {
    static const char* ic[5][7] = {
        { ".......", ".......", ".......", ".......", ".......", ".......", "......." },
        { "...w...", "..www..", "..wbw..", "..www..", ".rwwwr.", ".r.o.r.", "...y..." },      // missile
        { ".......", "y.y.y..", "y.y.y..", "o.o.o..", "o.o.o..", "o.o.o..", "......." },      // shots
        { "..k.k..", ".kkkkk.", "kkkrkkk", ".krrrk.", "kkkrkkk", ".kkkkk.", "..k.k.." },      // mine
        { ".......", "w..y...", ".w..y..", "..w..y.", ".w..y..", "w..y...", "......." } };    // turbo
    static const uint32_t bg[5] = { 0, RGB(30, 44, 120), RGB(120, 36, 140), RGB(244, 208, 40), RGB(20, 132, 60) };
    if (tile) {
        for (int j = 0; j < 9; j++) for (int i = 0; i < 9; i++) pdark(x + i + 1, y + j + 2, 0.6f);
        bool bl = (frameNo >> 3) & 1;
        for (int j = 0; j < 9; j++) for (int i = 0; i < 9; i++) pset(x + i, y + j, (i == 0 || j == 0 || i == 8 || j == 8) ? (bl ? C_WHITE : RGB(190, 190, 200)) : bg[w]);
        x++; y++;
    }
    for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) {
        char ch = ic[w][j][i];
        if (ch == '.') continue;
        uint32_t c = ch == 'r' ? RGB(236, 40, 40) : ch == 'w' ? C_WHITE : ch == 'y' ? RGB(255, 232, 60) : ch == 'o' ? RGB(240, 150, 40) : ch == 'b' ? RGB(60, 120, 230) : RGB(30, 30, 36);
        if (!tile && ch == 'k') c = RGB(150, 150, 160);
        pset(x + i, y + j, c);
    }
}

std::vector<Drop> drops, streaks;
std::vector<Leaf> leaves;
static float birdX = -50, birdY = 60;
struct Tumble { float x, y, ph; };
static Tumble tumble[3] = { { 40, 70, 0 }, { 200, 150, 2 }, { 120, 215, 4 } };

void stepAmbient(float dt) {
    for (size_t i = 0; i < parts.size();) {
        Particle& p = parts[i];
        p.life -= dt; p.x += p.vx * dt; p.y += p.vy * dt; p.vy += p.grav * dt; p.vx *= 0.97f; if (p.grav == 0) p.vy *= 0.97f;
        if (p.life <= 0) { parts[i] = parts.back(); parts.pop_back(); } else i++;
    }
    // weather
    size_t want = weather == WX_RAIN ? 130 : (weather == WX_SNOW ? 110 : (weather == WX_CONFETTI ? 90 : 0));
    while (drops.size() < want) drops.push_back({ grng.f() * W, grng.f() * H, 0.6f + grng.f() * 0.6f });
    if (drops.size() > want) drops.resize(want);
    for (Drop& d : drops) {
        if (weather == WX_RAIN) { d.x -= 70 * dt * d.sp; d.y += 260 * dt * d.sp; }
        else { d.x += sinf(d.y * 0.08f + d.sp * 9) * 14 * dt - 5 * dt; d.y += (weather == WX_CONFETTI ? 34 : 26) * dt * d.sp; }
        if (d.y >= H) { d.y = HUD_H; d.x = grng.f() * (W + 60); }
        if (d.x < 0) d.x += W;
    }
    snd::rainTarget = ((weather == WX_RAIN || windX != 0 || windY != 0) && !attract) ? 0.05f : 0;
    // wind streaks
    want = (windX != 0 || windY != 0) ? 26 : 0;
    while (streaks.size() < want) streaks.push_back({ grng.f() * W, HUD_H + grng.f() * (H - HUD_H), 0.6f + grng.f() * 0.8f });
    if (streaks.size() > want) streaks.resize(want);
    for (Drop& d : streaks) {
        d.x += windX * gust * 9 * d.sp * dt; d.y += windY * gust * 9 * d.sp * dt;
        if (d.x < -6 || d.x > W + 6 || d.y < HUD_H || d.y > H) { bool edge = grng.range(2); d.x = edge ? (windX > 0 ? -5.0f : W + 5.0f) : grng.f() * W; d.y = edge ? HUD_H + grng.f() * (H - HUD_H) : (windY > 0 ? (float)HUD_H : H - 1.0f); }
    }
    // chimney smoke (drifts with the wind)
    if ((frameNo % 14) == 0) for (const Pt& s : smokers) addPart(s.x + 1.0f, s.y - 1.0f, 3 + grng.f() * 4 + windX * 0.6f, -7 - grng.f() * 4, 1.6f, grng.range(2) ? RGB(240, 240, 244) : RGB(170, 170, 178));
    // falling leaves in autumn; they stay where they land
    if (theme == TH_AUTUMN && !treeTops.empty() && (frameNo % 9) == 0 && leaves.size() < 40) {
        const TreeTop& t = treeTops[grng.range((int)treeTops.size())];
        static const uint32_t lc[4] = { RGB(240, 140, 40), RGB(214, 60, 36), RGB(250, 220, 80), RGB(150, 50, 30) };
        float x = t.x + (grng.f() - 0.5f) * 2 * t.r, y = t.y + (grng.f() - 0.5f) * t.r;
        leaves.push_back({ x, y, y + 6 + grng.f() * 22, grng.f() * 6, lc[grng.range(4)] });
    }
    // blossom petals in spring
    if (theme == TH_SPRING && !treeTops.empty() && (frameNo % 16) == 0 && leaves.size() < 24) {
        const TreeTop& t = treeTops[grng.range((int)treeTops.size())];
        float x = t.x + (grng.f() - 0.5f) * 2 * t.r, y = t.y + (grng.f() - 0.5f) * t.r;
        leaves.push_back({ x, y, y + 6 + grng.f() * 18, grng.f() * 6, grng.range(2) ? RGB(255, 200, 224) : C_WHITE });
    }
    for (size_t i = 0; i < leaves.size();) {
        Leaf& l = leaves[i];
        l.y += 9 * dt; l.ph += 4 * dt; l.x += sinf(l.ph) * 10 * dt + 2 * dt;
        if (l.y >= l.ty) {
            int x = (int)l.x, y = (int)l.y;
            if (inb(x, y) && y >= HUD_H) { int p = y * W + x; if (surf[0][p] != S_WATER && surf[0][p] != S_METAL && !solid[0][p] && !objMask[p] && leafCount < 2500) { ground[p] = l.col; leafCount++; } }
            leaves[i] = leaves.back(); leaves.pop_back();
        } else i++;
    }
    if (sandy()) for (Tumble& t : tumble) {
        t.x += (22 + 6 * sinf(t.ph * 0.3f)) * dt; t.ph += 6 * dt; t.y += sinf(t.ph * 0.21f) * 4 * dt;
        if (t.x > W + 8) { t.x = -8; t.y = HUD_H + 10 + grng.f() * (H - HUD_H - 20); }
    }
    if (birdX < -40) { if (grng.range(900) == 0 && weather == WX_CLEAR && !night) { birdX = W + 10; birdY = 30 + grng.f() * 170; } }
    else birdX -= 38 * dt;
}

// ---- night: everything is multiplied by a light map (street lamps, windows, head lights)
static void addGlow(float fx, float fy, int r, int amt) {
    int x0 = (int)fx, y0 = (int)fy;
    for (int y = y0 - r; y <= y0 + r; y++) for (int x = x0 - r; x <= x0 + r; x++) {
        if (!inb(x, y)) continue;
        int d2 = (x - x0) * (x - x0) + (y - y0) * (y - y0);
        if (d2 >= r * r) continue;
        int v = lightBuf[y * W + x] + amt * (r * r - d2) / (r * r);
        lightBuf[y * W + x] = (uint8_t)std::min(255, v);
    }
}
static void addBeam(float fx, float fy, float ang, int len) {
    float cs = cosf(ang), sn = sinf(ang);
    int x0 = (int)fx, y0 = (int)fy;
    for (int y = y0 - len; y <= y0 + len; y++) for (int x = x0 - len; x <= x0 + len; x++) {
        if (!inb(x, y)) continue;
        float dx = x - fx, dy = y - fy, al = dx * cs + dy * sn;
        if (al <= 0) continue;
        float d = sqrtf(dx * dx + dy * dy);
        if (d >= len) continue;
        float c = al / d;
        if (c < 0.88f) continue;
        int v = lightBuf[y * W + x] + (int)(250 * (1 - d / len) * std::min(1.0f, (c - 0.88f) / 0.07f));
        lightBuf[y * W + x] = (uint8_t)std::min(255, v);
    }
}
static void applyNight() {
    memcpy(lightBuf, lightStatic, N);
    for (const Car& c : cars) if (c.on && c.dead <= 0) { addGlow(c.x, c.y, 11, 120); addBeam(c.x + cosf(c.a) * 3, c.y + sinf(c.a) * 3, c.a, 52); }
    for (const Train& tr : trains) if (tr.active) { float hx = tr.horiz ? tr.head : tr.line, hy = tr.horiz ? tr.line : tr.head; addBeam(hx, hy, tr.horiz ? (tr.dir > 0 ? 0 : PI) : (tr.dir > 0 ? PI / 2 : -PI / 2), 70); addGlow(hx, hy, 14, 120); }
    for (const ItemBox& it : items) if (it.timer <= 0) addGlow(it.x, it.y, 9, 110);
    for (const Proj& pr : projs) addGlow(pr.x, pr.y, 10, 160);
    for (size_t i = 0; i < signals.size(); i++) if (trains[signalTrain[i]].warn && ((frameNo >> 3) & 1)) addGlow((float)signals[i].x, (float)signals[i].y, 8, 150);
    for (const Particle& p : parts) if (((p.col >> 16) & 255) > 230 && (p.col & 255) < 120) { int q = pixAt(p.x, p.y); lightBuf[q] = 255; }
    for (int p = HUD_H * W; p < N; p++) {
        int l = lightBuf[p];
        if (l >= 250) continue;
        uint32_t c = fb[p];
        int r = ((c >> 16) & 255) * l / 255, g = ((c >> 8) & 255) * l / 255, b = (c & 255) * std::min(255, l + 26) / 255;
        fb[p] = RGB(r, g, b);
    }
}

void drawWorld() {
    memcpy(fb, ground, sizeof(fb));
    // carnival ground
    if (!tripPix.empty()) {
        static uint32_t lut[256];
        for (int i = 0; i < 256; i++) lut[i] = shade(hue(i), 0.9f);
        int t = frameNo * 2;
        for (size_t i = 0; i < tripPix.size(); i++) { int p = tripPix[i]; uint32_t c = lut[(tripVal[i] * 2 + t) & 255]; fb[p] = shadowM[p] ? shade(c, 0.6f) : c; }
    }
    // water shimmer
    int t = frameNo >> 3;
    uint32_t wl = waterCol(1), wg = waterCol(2);
    for (int p : waterPix) {
        int x = p % W, y = p / W, row = y + (x >> 4);
        if ((row % 5) == 0 && ((x + t + row * 7) % 23) < 3) fb[p] = ((x + t + row * 7) % 23) == 1 ? wg : wl;
    }
    for (const Duck& d : ducks) {
        float a = d.ph + frameNo * 0.006f;
        int x = (int)(d.cx + cosf(a) * d.r * 1.6f), y = (int)(d.cy + sinf(a) * d.r);
        if (inb(x - 1, y) && inb(x + 2, y + 1) && surf[0][y * W + x] == S_WATER && surf[0][y * W + x + 1] == S_WATER) {
            bool right = -sinf(a) > 0;
            pset(x, y, C_WHITE); pset(x + 1, y, C_WHITE); pset(x, y + 1, RGB(214, 214, 224)); pset(x + 1, y + 1, RGB(214, 214, 224));
            pset(right ? x + 2 : x - 1, y, RGB(255, 150, 30)); pset(right ? x + 1 : x, y - 1, C_WHITE); pset(right ? x - 1 : x + 2, y + 1, wl);
        }
    }
    // spectators waving
    for (const Waver& w : wavers) {
        int ph = (frameNo / 8 + w.ph) & 15;
        if (ph < 6) { bool up = ph & 1; pset(w.x + (up ? 2 : -1), w.y - (up ? 1 : 0), w.col); if (ph & 2) pset(w.x + (up ? -1 : 2), w.y - 1, w.col); }
    }
    for (size_t i = 0; i < flags.size(); i++) {
        const Pt& f = flags[i];
        uint32_t col = theme == TH_GERMANY ? 0 : CAR_COL[i % 5];
        for (int k = 1; k <= 7; k++) {
            int wv = (int)floorf(sinf(frameNo * 0.15f - k * 0.8f + i) * 1.3f + 0.5f);
            for (int j = 0; j < 4; j++) {
                uint32_t c = col ? ((j == 3 && (k & 1)) ? C_WHITE : (j == 0 ? mixc(col, C_WHITE, 0.3f) : col)) : (j == 0 ? RGB(20, 20, 20) : (j < 3 ? RGB(220, 30, 30) : RGB(250, 204, 20)));
                pset(f.x + k, f.y + j + wv, c);
            }
        }
    }
    for (size_t i = 0; i < signals.size(); i++) {
        const Pt& s = signals[i];
        bool on = trains[signalTrain[i]].warn && ((frameNo >> 3) & 1);
        pset(s.x, s.y + 2, C_WHITE); pset(s.x, s.y + 3, RGB(96, 96, 100)); pset(s.x + 1, s.y + 2, RGB(200, 200, 204));
        uint32_t c = on ? RGB(255, 40, 40) : RGB(90, 10, 10);
        pset(s.x, s.y, c); pset(s.x + 1, s.y, c); pset(s.x, s.y + 1, c); pset(s.x + 1, s.y + 1, c);
        if (on) { pset(s.x - 1, s.y, RGB(255, 150, 150)); pset(s.x + 2, s.y + 1, RGB(255, 150, 150)); }
    }
    // turntables
    for (const Turntable& tt : turntables) {
        float cs = cosf(tt.ang), sn = sinf(tt.ang);
        int r = (int)tt.r;
        for (int y = (int)tt.y - r; y <= (int)tt.y + r; y++) for (int x = (int)tt.x - r; x <= (int)tt.x + r; x++) {
            if (!inb(x, y) || !ttMask[y * W + x]) continue;
            float dx = x + 0.5f - tt.x, dy = y + 0.5f - tt.y, d = sqrtf(dx * dx + dy * dy);
            float lx = dx * cs + dy * sn, ly = -dx * sn + dy * cs;
            uint32_t c = (((int)floorf(lx / 2) + (int)floorf(ly / 2)) & 1) ? RGB(132, 134, 144) : RGB(120, 122, 132);
            if (fabsf(ly) < 5 && ((int)floorf(lx + 100) % 4) == 0) c = RGB(104, 68, 40);
            if (fabsf(fabsf(ly) - 3) < 0.6f) c = RGB(220, 220, 228);
            if (d > tt.r - 3.5f) c = ((int)floorf((atan2f(ly, lx) + PI) * 5) & 1) ? RGB(244, 204, 40) : RGB(40, 40, 44);
            if (d > tt.r - 1.2f) c = RGB(60, 60, 66);
            if (d < 2) c = RGB(50, 50, 56);
            fb[y * W + x] = c;
        }
    }
    // drawbridges
    for (const Drawbridge& d : bridges) {
        if (d.lift > 0) {
            float vis = 0.5f * (1 - 0.85f * d.lift);
            for (size_t i = 0; i < d.span.size(); i++) {
                float f = d.spanPos[i] / 255.0f; int p = d.span[i];
                if (f < vis || f > 1 - vis) fb[p] = (fabsf(f - vis) < 0.05f || fabsf(f - (1 - vis)) < 0.05f) ? RGB(60, 40, 24) : shade(ground[p], 1 + 0.3f * d.lift);
                else fb[p] = ((p % W + p / W * 3 + (frameNo >> 3)) % 11) == 0 ? wl : waterCol(0);
            }
        }
        bool blink = (frameNo >> 3) & 1;
        for (int p : d.barrier) {
            int x = p % W, y = p / W;
            if (!d.open) fb[p] = (((x + y) / 2) & 1) ? RGB(236, 40, 40) : C_WHITE;
            else if (d.warn && blink && ((x + y) % 3) == 0) fb[p] = RGB(255, 60, 40);
        }
    }
    // ferries: a cable and the raft
    for (const Ferry& f : ferries) {
        float len = hypotf(f.bx - f.ax, f.by - f.ay);
        for (int k = -(int)f.hl - 4; k <= (int)(len + f.hl) + 4; k += 2) { int q = pixAt(f.ax + f.dx * k, f.ay + f.dy * k); if (surf[0][q] == S_WATER) fb[q] = RGB(30, 30, 36); }
        int R = (int)(f.hl + f.hw) + 3;
        for (int pass = 0; pass < 2; pass++)
            for (int y = (int)f.y - R; y <= (int)f.y + R; y++) for (int x = (int)f.x - R; x <= (int)f.x + R; x++) {
                float dx = x + 0.5f - f.x, dy = y + 0.5f - f.y, al = dx * f.dx + dy * f.dy, ac = -dx * f.dy + dy * f.dx;
                if (fabsf(al) > f.hl || fabsf(ac) > f.hw) continue;
                if (pass == 0) { pdark(x + 2, y + 3, 0.6f); continue; }
                uint32_t c = (((int)floorf(al + 100)) % 4 == 0) ? RGB(110, 72, 38) : ((hash2((int)floorf(al + 100), (int)floorf(ac + 100)) % 5) ? RGB(168, 120, 66) : RGB(150, 104, 56));
                if (fabsf(al) > f.hl - 1.5f || fabsf(ac) > f.hw - 1.5f) c = RGB(96, 62, 32);
                if (fabsf(al) > f.hl - 3 && fabsf(ac) > f.hw - 3) c = RGB(230, 200, 60);
                pset(x, y, c);
            }
    }
    // pick-ups and mines
    for (const ItemBox& it : items) if (it.timer <= 0) drawIcon((int)it.x - 4, (int)it.y - 4, it.type, true);
    for (const Mine& m : mines) {
        int x = (int)m.x, y = (int)m.y;
        static const char* ms[5] = { ".k.k.", "kkkkk", ".krk.", "kkkkk", ".k.k." };
        for (int j = 0; j < 5; j++) for (int i = 0; i < 5; i++) if (ms[j][i] != '.') { pdark(x - 1 + i, y - 1 + j, 0.7f); }
        for (int j = 0; j < 5; j++) for (int i = 0; i < 5; i++) if (ms[j][i] != '.') pset(x - 2 + i, y - 2 + j, ms[j][i] == 'r' ? ((m.arm <= 0 && (frameNo & 8)) ? RGB(255, 40, 40) : RGB(110, 20, 20)) : RGB(36, 36, 42));
    }
    if (sandy()) for (const Tumble& tb : tumble) {
        int x = (int)tb.x, y = (int)(tb.y - fabsf(sinf(tb.ph * 0.5f)) * 3), f = (int)(tb.ph) & 1;
        if (!inb(x, (int)tb.y) || surf[0][pixAt(tb.x, tb.y)] == S_WATER) continue;
        for (int i = -1; i <= 2; i++) pdark(x + i + 1, (int)tb.y + 3, 0.7f);
        static const char* tw[2][5] = { { ".bbb.", "b.l.b", "bl.lb", "b.l.b", ".bbb." }, { ".bbb.", "bl.lb", "b.l.b", "bl.lb", ".bbb." } };
        for (int j = 0; j < 5; j++) for (int i = 0; i < 5; i++) if (tw[f][j][i] != '.') pset(x - 2 + i, y - 2 + j, tw[f][j][i] == 'b' ? RGB(144, 88, 16) : RGB(208, 160, 72));
    }
    // ground level cars, then trains, then bridges and tree tops, then everything above
    bool under[5];
    for (int i = 0; i < 5; i++) { under[i] = cars[i].on && carUnder(cars[i]); if (cars[i].on && under[i]) drawCar(cars[i]); }
    for (const Train& tr : trains) {
        if (!tr.active) continue;
        int hw = (int)TRAIN_HW;
        for (int j = WAGONS - 1; j >= 0; j--) {
            float a0 = tr.head - tr.dir * (j * WAGON_STEP + WAGON_LEN), a1 = tr.head - tr.dir * (j * WAGON_STEP);
            int lo = (int)floorf(std::min(a0, a1)), hi = (int)floorf(std::max(a0, a1)), ln = (int)tr.line;
            static const uint32_t wc[4] = { RGB(150, 90, 50), RGB(50, 120, 220), RGB(50, 150, 80), RGB(230, 140, 40) };
            uint32_t body = j == 0 ? RGB(210, 40, 40) : wc[(j + levelNo) & 3];
            for (int al = lo; al <= hi; al++) for (int ac = -hw; ac <= hw; ac++) {
                int x = tr.horiz ? al : ln + ac, y = tr.horiz ? ln + ac : al;
                if (y >= HUD_H) pdark(x + 2, y + 3, 0.55f);
            }
            for (int al = lo; al <= hi; al++) for (int ac = -hw; ac <= hw; ac++) {
                int x = tr.horiz ? al : ln + ac, y = tr.horiz ? ln + ac : al;
                if (y < HUD_H) continue;
                uint32_t c = body;
                int fr = tr.dir > 0 ? hi - al : al - lo;
                if (al == lo || al == hi || ac == -hw || ac == hw) c = RGB(24, 24, 28);
                else if (ac == -hw + 1) c = mixc(body, C_WHITE, 0.35f);
                else if (ac == hw - 1) c = shade(body, 0.7f);
                else if (j == 0) c = fr < 5 ? RGB(50, 50, 56) : (fr == 7 || fr == 8 ? RGB(250, 210, 40) : (fr > 12 && abs(ac) < 2 ? RGB(40, 60, 110) : body));
                else if ((al - lo) % 5 == 0) c = shade(body, 0.72f);
                pset(x, y, c);
            }
            if (j == 0 && (frameNo % 4) == 0) { float hx = tr.horiz ? tr.head - tr.dir * 3 : tr.line, hy = tr.horiz ? tr.line : tr.head - tr.dir * 3; addPart(hx, hy - 2, (grng.f() - 0.5f) * 10, -12, 0.7f, grng.range(2) ? RGB(240, 240, 244) : RGB(170, 170, 178)); }
        }
    }
    for (int p = HUD_H * W; p < N; p++) if (overA[p]) fb[p] = overC[p];
    for (int i = 0; i < 5; i++) if (cars[i].on && !under[i]) drawCar(cars[i]);
    for (const Proj& pr : projs) {
        int x = (int)pr.x, y = (int)pr.y;
        float l = hypotf(pr.vx, pr.vy) + 0.01f, ux = pr.vx / l, uy = pr.vy / l;
        if (pr.type == WP_MISSILE) {
            for (int k = 0; k < 5; k++) pdark((int)(pr.x - ux * k) + 2, (int)(pr.y - uy * k) + 4, 0.6f);
            for (int k = 0; k < 5; k++) pset((int)(pr.x - ux * k), (int)(pr.y - uy * k) - 2, k == 0 ? RGB(236, 40, 40) : (k < 4 ? C_WHITE : RGB(255, 180, 40)));
        } else { pset(x, y, RGB(255, 240, 80)); pset((int)(pr.x - ux * 1.5f), (int)(pr.y - uy * 1.5f), C_WHITE); pset((int)(pr.x - ux * 3), (int)(pr.y - uy * 3), RGB(255, 160, 40)); }
    }
    for (const Particle& p : parts) pset((int)p.x, (int)p.y, p.col);
    for (const Leaf& l : leaves) pset((int)l.x, (int)l.y, l.col);
    if (birdX > -40) { int x = (int)birdX, y = (int)birdY, fl = (frameNo >> 3) & 1; pdark(x + 6, y + 12, 0.6f); pdark(x + 7, y + 12, 0.6f); for (int k = -3; k <= 3; k++) pset(x + k, y - (fl ? abs(k) : -abs(k)) / 2, RGB(20, 20, 24)); }
    if (night) applyNight();
    for (size_t i = 0; i < drops.size(); i++) {
        const Drop& d = drops[i];
        int x = (int)d.x, y = (int)d.y;
        if (y < HUD_H) continue;
        if (weather == WX_RAIN) { pset(x, y, RGB(190, 220, 255)); pset(x + 1, y - 2, RGB(130, 160, 230)); pset(x, y - 1, RGB(130, 160, 230)); }
        else if (weather == WX_CONFETTI) { uint32_t c = hue((int)i * 37); pset(x, y, c); if (((frameNo >> 2) + i) & 1) pset(x + 1, y, c); else pset(x, y + 1, c); }
        else { pset(x, y, C_WHITE); if (d.sp > 1.0f) pset(x + 1, y, RGB(220, 230, 250)); }
    }
    for (const Drop& d : streaks) {
        float l = hypotf(windX, windY) + 0.01f;
        for (int k = 0; k < 5; k++) { int q = pixAt(d.x - windX / l * k, d.y - windY / l * k); if (q >= HUD_H * W) fb[q] = mixc(fb[q], C_WHITE, 0.55f - k * 0.1f); }
    }
}
