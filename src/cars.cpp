// Rally Mayhem - cars: physics, collisions, CPU drivers, weapons, trains and other moving parts
// by rsn8887

#include "game.h"

// ------------------------------------------------------------------ particles
std::vector<Particle> parts;
void addPart(float x, float y, float vx, float vy, float life, uint32_t col, float grav) {
    if (parts.size() < 700) parts.push_back({ x, y, vx, vy, life, life, col, grav });
}
static void burst(float x, float y, int n, float sp, float life, const uint32_t* cols, int nc, float grav = 0) {
    for (int i = 0; i < n; i++) { float a = grng.f() * 2 * PI, s = sp * (0.3f + grng.f()); addPart(x, y, cosf(a) * s, sinf(a) * s, life * (0.5f + grng.f() * 0.7f), cols[grng.range(nc)], grav); }
}
static const uint32_t FIRE_COLS[4] = { RGB(255, 255, 0), RGB(255, 128, 0), RGB(255, 0, 0), RGB(255, 255, 255) };
static const uint32_t WATER_COLS[3] = { RGB(255, 255, 255), RGB(170, 220, 255), RGB(72, 140, 232) };
static const uint32_t SMOKE_COLS[2] = { RGB(128, 128, 128), RGB(64, 64, 64) };
static void explosion(float x, float y) { burst(x, y, 40, 60, 0.6f, FIRE_COLS, 4); burst(x, y, 14, 25, 1.0f, SMOKE_COLS, 2); snd::explode(); }

// ------------------------------------------------------------------ cars
Car cars[5];
std::vector<Proj> projs;
std::vector<Mine> mines;
float raceT = 0;
int finishCount = 0, raceLaps = 5;
bool attract = true;

int carMask(float cx, float cy, float a, int (*out)[2]) {
    float c = cosf(a), s = sinf(a);
    int n = 0, x0 = (int)floorf(cx - 6), y0 = (int)floorf(cy - 6);
    for (int y = y0; y <= y0 + 12; y++) for (int x = x0; x <= x0 + 12; x++) {
        float wx = x + 0.5f - cx, wy = y + 0.5f - cy, lx = wx * c + wy * s, ly = -wx * s + wy * c;
        if (fabsf(lx) <= CAR_HL && fabsf(ly) <= CAR_HW && n < 96) { out[n][0] = x; out[n][1] = y; n++; }
    }
    return n;
}
static inline bool blockedPix(int x, int y, float z) {
    if (x < 0 || x >= W || y < HUD_H || y >= H) return true;
    int p = y * W + x, ly = activeLayer(p, z);
    uint8_t s = solid[ly][p];
    if (s == 2 || (dynSolid[p] && ly == 0 && z < 4)) return true;
    float h = ht[ly][p];
    if (s == 1 && z < h + 3.0f) return true;
    return h - z > 3.3f;
}
static bool carBlocked(const Car& c, float x, float y, float a, float* nx = nullptr, float* ny = nullptr) {
    int m[96][2], n = carMask(x, y, a, m), hits = 0;
    float sx = 0, sy = 0;
    for (int i = 0; i < n; i++) if (blockedPix(m[i][0], m[i][1], c.z)) { hits++; sx += m[i][0] + 0.5f - x; sy += m[i][1] + 0.5f - y; }
    if (!hits) return false;
    if (nx) {
        float l = hypotf(sx, sy);
        if (l < 0.05f) { float sp = hypotf(c.vx, c.vy); if (sp > 1) { *nx = -c.vx / sp; *ny = -c.vy / sp; } else { *nx = -cosf(a); *ny = -sinf(a); } }
        else { *nx = -sx / l; *ny = -sy / l; }
    }
    return true;
}

void startPose(int k, float& x, float& y, float& a) {
    float back = 10 + k * 11.0f;
    int si = ((NS - (int)(back / spacing)) % NS + NS) % NS;
    const Sample& s = sm[si];
    float lat = ((k & 1) ? 1 : -1) * s.w * 0.2f;
    x = s.x - s.ty * lat; y = s.y + s.tx * lat; a = atan2f(s.ty, s.tx);
}
static void respawn(Car& c) {
    int k = ((c.idx - (int)(28 / spacing)) % NS + NS) % NS;
    for (int tries = 0; tries < NS; tries += 8, k = (k - 8 + NS) % NS) {
        const Sample& s = sm[k];
        bool bad = (s.f & (fG | fF | fY)) || s.ramp > 0 || (s.over && s.type == S_ICE);
        { int q = pixAt(s.x, s.y); if (spanMask[q] || ttMask[q]) bad = true; }
        for (const Crossing& cr : crossings) if (abs(cr.k - k) < (int)(14 / spacing)) bad = true;
        for (int j = 4; j <= 84 && !bad; j += 4) if (sm[(k + (int)(j / spacing)) % NS].f & fG) bad = true;
        if (!bad) break;
    }
    const Sample& s = sm[k];
    c.x = s.x; c.y = s.y; c.a = atan2f(s.ty, s.tx); c.vx = c.vy = c.vz = 0; c.z = s.h; c.gvis = s.h; c.air = false;
    c.spin = 0; c.invuln = 2.0f; c.idx = k; c.stuck = 0; c.lostT = 0; c.turbo = 0; c.slopeVz = 0;
}
static void killCar(Car& c, float t) { if (c.dead > 0) return; c.dead = t; c.vx = c.vy = 0; c.turbo = 0; }
static void hitCar(Car& c, float spinT, float slow, float pop) {
    if (c.invuln > 0 || c.dead > 0) return;
    c.spin = std::max(c.spin, spinT); c.vx *= slow; c.vy *= slow;
    if (pop > 0 && !c.air) { c.air = true; c.vz = pop; }
}

void trainUpdate(float t) {
    for (Train& tr : trains) {
        float len = WAGONS * WAGON_STEP, ct = fmodf(t + tr.phase + 1000 * tr.period, tr.period);
        float travel = (fabsf(tr.to - tr.from) + len) / tr.speed;
        bool was = tr.active, wasWarn = tr.warn;
        tr.t = ct; tr.active = ct < travel; tr.head = tr.from + tr.dir * tr.speed * ct;
        tr.warn = tr.active || ct > tr.period - 1.8f;
        if (!attract) {
            if (tr.active && !was) snd::horn();
            if (tr.warn && (!wasWarn || ((int)(ct * 4) != (int)((ct - 1.0f / 120) * 4) && !tr.active))) snd::bell();
        }
    }
}
// time until the train's head / tail pass a point on its line (seconds, may be negative)
static void trainTimes(const Train& tr, float along, float& tHead, float& tTail) {
    float start = tr.active ? -tr.t : (tr.period - tr.t);
    tHead = start + fabsf(along - tr.from) / tr.speed; tTail = tHead + WAGONS * WAGON_STEP / tr.speed;
}
static bool trainHits(const Car& c) {
    if (trains.empty() || c.z - ht[0][pixAt(c.x, c.y)] > 5.5f) return false;
    int m[96][2], n = -1;
    for (const Train& tr : trains) {
        if (!tr.active) continue;
        float across = tr.horiz ? c.y : c.x;
        if (fabsf(across - tr.line) > 11) continue;
        if (n < 0) n = carMask(c.x, c.y, c.a, m);
        for (int j = 0; j < WAGONS; j++) {
            float a0 = tr.head - tr.dir * (j * WAGON_STEP + WAGON_LEN), a1 = tr.head - tr.dir * (j * WAGON_STEP);
            float lo = std::min(a0, a1), hi = std::max(a0, a1);
            for (int i = 0; i < n; i++) {
                float al = (tr.horiz ? m[i][0] : m[i][1]) + 0.5f, ac = (tr.horiz ? m[i][1] : m[i][0]) + 0.5f;
                if (al >= lo && al <= hi && fabsf(ac - tr.line - 0.5f) <= TRAIN_HW + 0.5f) return true;
            }
        }
    }
    return false;
}

static void fireWeapon(Car& c, bool forward) {
    float fx = cosf(c.a), fy = sinf(c.a), dir = forward ? 1.f : -1.f;
    switch (c.weapon) {
    case WP_MISSILE:
        projs.push_back({ WP_MISSILE, c.x + fx * 6 * dir, c.y + fy * 6 * dir, fx * dir * 150 + c.vx * 0.5f, fy * dir * 150 + c.vy * 0.5f, c.z + 2, 2.6f, c.slot });
        snd::missile(); c.weapon = WP_NONE; break;
    case WP_SHOT:
        projs.push_back({ WP_SHOT, c.x + fx * 6 * dir, c.y + fy * 6 * dir, fx * dir * 300, fy * dir * 300, c.z, 1.0f, c.slot });
        snd::shot(); if (--c.ammo <= 0) c.weapon = WP_NONE; break;
    case WP_MINE: {
        float d = forward ? 26.f : -9.f;
        mines.push_back({ c.x + fx * d, c.y + fy * d, c.z, forward ? 0.9f : 0.5f, c.slot });
        snd::mineDrop(); c.weapon = WP_NONE;
    } break;
    case WP_TURBO: c.turbo = 2.2f; snd::turbo(); c.weapon = WP_NONE; break;
    }
}

Input aiInput(Car& c, float dt) {
    Input in;
    float sp = hypotf(c.vx, c.vy), vfw = c.vx * cosf(c.a) + c.vy * sinf(c.a);   // vfw: speed along the car's nose
    int p = pixAt(c.x, c.y), ly = activeLayer(p, c.z);
    uint8_t sf = surf[ly][p];
    float look = 13 + sp * 0.20f;
    int k = (c.idx + (int)(look / spacing)) % NS;
    const Sample& T = sm[k];
    float lane = c.lane * T.w * 0.22f;
    bool rampAhead = false, gapAhead = false; float maxCurv = 0;
    for (int j = 0; j < 9; j++) {
        const Sample& s = sm[(c.idx + (int)(j * 8 / spacing)) % NS];
        if (s.ramp > 0) rampAhead = true;
        if (s.f & fG) gapAhead = true;
        if (j >= 1 && j <= 6) maxCurv = std::max(maxCurv, fabsf(s.curv));
        if (s.over || (s.f & (fB | fT | fY)) || s.w < 20) lane = 0;
    }
    if (rampAhead || gapAhead) lane = 0;
    float tx = T.x - T.ty * lane, ty = T.y + T.tx * lane;
    float want = atan2f(ty - c.y, tx - c.x), d = angDiff(want, c.a);
    if (d > 0.04f) in.r = true; else if (d < -0.04f) in.l = true;
    static const float corner[3] = { 13.0f, 13.3f, 13.7f };     // easy, normal, hard: how fast CPU cars dare to take a bend
    float vSafe = corner[optDiff] * c.skill / sqrtf(maxCurv + 0.0025f);
    if (sf == S_ICE || sm[k].type == S_ICE) vSafe *= 0.6f;
    if (weather != WX_CLEAR) vSafe *= 0.88f;
    if (c.finished) vSafe = std::min(vSafe, 55.0f);
    if (rampAhead || gapAhead) vSafe = 999;
    c.power = (rampAhead || gapAhead) ? 1.0f : c.skill;   // CPU cars are down on power, except on the run-up to a jump
    in.acc = sp < vSafe && fabsf(d) < 1.3f;
    if (sp > vSafe + 12 || (fabsf(d) > 1.0f && sp > 45)) in.brk = true;
    // drawbridges: stop at the barrier unless there is time to get across
    for (const Drawbridge& db : bridges) {
        int ahead = ((db.k0 - c.idx) % NS + NS) % NS;
        float dist = ahead * spacing;
        if (dist > 75 || dist < 3) continue;
        float toClose = db.open ? db.period - 4.0f - db.t : -1;
        float need = (dist + (db.k1 - db.k0) * spacing + 14) / std::max(sp, 45.0f);
        if (toClose < need) { in.acc = sp < 22 && dist > 17; if (dist < 13 + sp * 0.4f && vfw > 8) in.brk = true; c.stuck = 0; }
    }
    // ferries: wait at the dock, roll on, wait for the far side, roll off
    for (const Ferry& f : ferries) {
        int ahead = ((f.kA - c.idx) % NS + NS) % NS;
        float dist = ahead * spacing;
        if (!c.air && onFerry(f, c.x, c.y)) {
            in.l = in.r = false;
            float al = (c.x - f.x) * f.dx + (c.y - f.y) * f.dy;
            if (f.state == 2) {
                const Sample& T2 = sm[(f.kB + (int)(16 / spacing)) % NS];
                float d2 = angDiff(atan2f(T2.y - c.y, T2.x - c.x), c.a);
                in.l = d2 < -0.04f; in.r = d2 > 0.04f; in.acc = sp < 60; in.brk = false;
            } else if (f.state == 0 && al < 3) { in.acc = sp < 24; in.brk = sp > 34; float d2 = angDiff(atan2f(f.dy, f.dx), c.a); in.l = d2 < -0.05f; in.r = d2 > 0.05f; }
            else { in.acc = vfw < -3; in.brk = vfw > 3; c.stuck = 0; }   // hold still while the raft moves
            return in;
        }
        if (dist < 85 && dist > 0) {
            int nOn = 0;
            for (const Car& o : cars) if (o.on && o.dead <= 0 && onFerry(f, o.x, o.y)) nOn++;
            bool board = f.state == 0 && f.tLeft > (dist + 12) / std::max(sp, 36.0f) + 0.4f && nOn < 4;
            if (board) { if (sp > 52) in.acc = false; }
            else { in.acc = sp < 22 && dist > 15; if (dist < 11 + sp * 0.4f && vfw > 6) in.brk = true; if (dist < 8) in.acc = false; c.stuck = 0; }
        }
    }
    // level crossings
    for (const Crossing& cr : crossings) {
        int ahead = ((cr.k - c.idx) % NS + NS) % NS;
        float dist = ahead * spacing;
        if (dist > 60 || dist < 5) continue;
        if (cr.ramp && sp > 70) continue;
        float tH, tT; trainTimes(trains[cr.train], cr.along, tH, tT);
        float eta = dist / std::max(sp, 30.0f);
        if (eta > tH - 0.45f && eta < tT + 0.3f) { in.acc = false; if (vfw > 12 && dist < 16 + sp * 0.35f) in.brk = true; c.stuck = 0; }
    }
    // stuck handling
    if (raceT > 0 && sp < 7 && !c.air) c.stuck += dt; else if (sp > 15) c.stuck = 0;
    if (c.stuck > 1.2f && c.rev <= 0 && c.stuck < 1.3f) c.rev = 0.8f;
    if (c.rev > 0) { c.rev -= dt; in.acc = false; in.brk = true; std::swap(in.l, in.r); }
    if (c.stuck > 4.5f) { killCar(c, 0.6f); c.stuck = 0; }
    // weapons
    if (c.weapon && raceT > 2 && !c.finished) {
        c.aiFire -= dt;
        if (c.aiFire <= 0) {
            c.aiFire = 0.5f + grng.f();
            bool straight = maxCurv < 0.012f;
            if (c.weapon == WP_TURBO) { if (straight && !rampAhead) in.up = true; }
            else if (c.weapon == WP_MINE) { if (grng.range(3) == 0) in.down = true; }
            else if (c.weapon == WP_MISSILE) { if (c.rank > 0) in.up = true; else if (grng.range(3) == 0) in.down = true; }
            else if (c.weapon == WP_SHOT) {
                for (const Car& o : cars) if (o.on && &o != &c && o.dead <= 0) {
                    float dx = o.x - c.x, dy = o.y - c.y, dd = hypotf(dx, dy);
                    if (dd < 70 && dd > 6) { float da = angDiff(atan2f(dy, dx), c.a); if (fabsf(da) < 0.12f) in.up = true; else if (fabsf(fabsf(da) - PI) < 0.12f) in.down = true; }
                }
            }
        }
    }
    return in;
}

void stepCar(Car& c, Input in, float dt) {
    if (c.dead > 0) { c.dead -= dt; if (c.dead <= 0) respawn(c); return; }
    if (c.invuln > 0) c.invuln -= dt;
    if (c.crashCd > 0) c.crashCd -= dt;
    if (raceT < 0) { in.acc = in.brk = in.l = in.r = in.up = in.down = false; }
    if (in.reset && c.human && raceT > 0) { killCar(c, 0.8f); return; }
    int p = pixAt(c.x, c.y), ly = activeLayer(p, c.z);
    uint8_t s = surf[ly][p];
    const Ferry* fer = nullptr;
    for (const Ferry& f : ferries) if (!c.air && onFerry(f, c.x, c.y)) fer = &f;
    if (fer && (s == S_WATER || s == S_OFF)) s = S_PLANK;      // riding the raft
    float fx = cosf(c.a), fy = sinf(c.a);
    float vf = c.vx * fx + c.vy * fy, vl = -c.vx * fy + c.vy * fx;
    bool ctl = c.spin <= 0;
    float newA = c.a;
    if (c.spin > 0) { c.spin -= dt; newA += 14 * dt; }
    if ((in.up && !c.pUp) || (in.down && !c.pDown)) if (c.weapon && ctl) fireWeapon(c, in.up && !c.pUp);
    c.pUp = in.up; c.pDown = in.down;
    if (!c.air) {
        float acc = 150, drag = 1.4f, grip = 10;
        switch (s) {
        case S_OFF: drag = 4.2f; grip = 7; break;
        case S_DIRT: drag = 1.65f; grip = 6; break;
        case S_ICE: acc = 75; drag = 0.7f; grip = 1.5f; break;
        case S_SHALLOW: drag = 3.4f; grip = 6; break;
        case S_PUDDLE: drag = 1.7f; grip = 1.6f; break;
        case S_WATER: drag = 6; break;
        case S_METAL: grip = 5; break;
        default: break;
        }
        if (weather == WX_RAIN && (s == S_ROAD || s == S_PLANK || s == S_BOOST)) grip *= 0.6f;
        if (weather == WX_SNOW && s != S_ICE) grip *= 0.7f;
        if (fabsf(sm[idxm[ly][p]].bank) > 0.05f && roadLike(s)) grip *= 1.5f;
        if (c.turbo > 0) {
            acc *= 1.7f; c.turbo -= dt;
            if (frameNo & 1) addPart(c.x - fx * 4, c.y - fy * 4 - (c.z - c.gvis), -fx * 30 + (grng.f() - 0.5f) * 20, -fy * 30 + (grng.f() - 0.5f) * 20, 0.25f, FIRE_COLS[grng.range(3)]);
        }
        if (ctl) {
            if (in.acc) vf += acc * c.power * dt;
            if (in.brk) { if (vf > 2) vf -= 260 * dt; else vf -= acc * 0.5f * dt; }
            float st = (in.r ? 1.f : 0.f) - (in.l ? 1.f : 0.f);
            newA += st * 3.7f * clampf(fabsf(vf) / 22, 0, 1) * (vf < 0 ? -1 : 1) * dt;
        } else drag += 2;
        vf -= vf * drag * dt;
        if (vf < -38) vf = -38;
        if (s == S_BOOST && vf > 5 && vf < 175) { vf += 520 * dt; if (c.crashCd <= 0) { snd::boost(); c.crashCd = 0.5f; } }
        float lat0 = vl;
        vl *= expf(-grip * dt);
        c.vx = fx * vf - fy * vl; c.vy = fy * vf + fx * vl;
        // gravity along slopes
        int ix = p % W, iy = p / W;
        if (ix > 0 && ix < W - 1 && iy > HUD_H && iy < H - 1) {
            float gx = ht[ly][p + 1] - ht[ly][p - 1], gy = ht[ly][p + W] - ht[ly][p - W];
            if (fabsf(gx) < 2.5f) c.vx -= 60 * gx * dt;
            if (fabsf(gy) < 2.5f) c.vy -= 60 * gy * dt;
        }
        // skid marks and dust
        float sp = hypotf(c.vx, c.vy);
        bool skid = fabsf(lat0) > 26 || (!ctl && sp > 20) || (in.brk && vf > 40);
        if (skid && ly == 0 && (s == S_ROAD || s == S_BOOST || s == S_PLANK)) {
            for (int side = -1; side <= 1; side += 2) {
                int q = pixAt(c.x - fx * 3.2f - fy * 2.0f * side, c.y - fy * 3.2f + fx * 2.0f * side);
                if (surf[0][q] == s && ((ground[q] >> 8) & 255) > 56) ground[q] = shade(ground[q], 0.8f);
            }
            if (c.human) snd::skidTarget = std::max(snd::skidTarget, 0.12f);
        }
        if (sp > 30 && (frameNo % 3) == 0) {
            if (s == S_DIRT || (s == S_OFF && sandy())) addPart(c.x - fx * 4, c.y - fy * 4, (grng.f() - 0.5f) * 20, (grng.f() - 0.5f) * 20 - 5, 0.5f, grng.range(2) ? RGB(255, 160, 64) : RGB(192, 96, 0));
            else if (s == S_OFF && theme == TH_WINTER) addPart(c.x - fx * 4, c.y - fy * 4, (grng.f() - 0.5f) * 20, (grng.f() - 0.5f) * 20 - 5, 0.4f, C_WHITE);
            else if (s == S_OFF && (frameNo % 6) == 0) addPart(c.x - fx * 4, c.y - fy * 4, (grng.f() - 0.5f) * 14, (grng.f() - 0.5f) * 14, 0.35f, THEMES[theme].offA);
            else if (s == S_SHALLOW || s == S_PUDDLE || (weather == WX_RAIN && roadLike(s) && (frameNo % 6) == 0)) addPart(c.x - fx * 4, c.y - fy * 4, (grng.f() - 0.5f) * 30, -10 - grng.f() * 20, 0.35f, WATER_COLS[grng.range(3)], 80);
        }
    }
    // rotate (pixel exact against walls)
    if (newA != c.a) {
        float nx, ny;
        if (!carBlocked(c, c.x, c.y, newA, &nx, &ny)) c.a = newA;
        else if (carBlocked(c, c.x, c.y, c.a)) c.a = newA;
        else if (!carBlocked(c, c.x + nx * 0.7f, c.y + ny * 0.7f, newA)) { c.x += nx * 0.7f; c.y += ny * 0.7f; c.a = newA; }
    }
    // move (the wind pushes every car sideways a little, a raft carries the cars standing on it)
    {
        float ex = windX * gust * (c.air ? 1.5f : 1.0f) + (fer ? fer->vx : 0), ey = windY * gust * (c.air ? 1.5f : 1.0f) + (fer ? fer->vy : 0);
        if ((ex != 0 || ey != 0) && raceT > 0 && !carBlocked(c, c.x + ex * dt, c.y + ey * dt, c.a)) { c.x += ex * dt; c.y += ey * dt; }
        float nx = 0, ny = 0, tx = c.x + c.vx * dt, ty = c.y + c.vy * dt;
        if (!carBlocked(c, tx, ty, c.a, &nx, &ny)) { c.x = tx; c.y = ty; }
        else {
            float vn = c.vx * nx + c.vy * ny;
            if (vn < 0) {
                c.vx -= 1.35f * vn * nx; c.vy -= 1.35f * vn * ny; c.vx *= 0.97f; c.vy *= 0.97f;
                if (-vn > 28 && c.crashCd <= 0) { snd::crash(clampf(-vn / 90, 0.3f, 1)); c.crashCd = 0.25f; burst(tx - nx * 3, ty - ny * 3, 5, 30, 0.2f, FIRE_COLS, 2); }
            }
            tx = c.x + c.vx * dt; ty = c.y + c.vy * dt;
            if (!carBlocked(c, tx, ty, c.a)) { c.x = tx; c.y = ty; }
            else if (carBlocked(c, c.x, c.y, c.a, &nx, &ny)) {
                for (int i = 0; i < 10; i++) { c.x += nx * 0.5f; c.y += ny * 0.5f; if (!carBlocked(c, c.x, c.y, c.a)) break; }
                c.x = clampf(c.x, 4, W - 4); c.y = clampf(c.y, HUD_H + 4, H - 4);
            }
        }
    }
    // turntables spin the cars standing on them
    p = pixAt(c.x, c.y);
    if (!c.air && ttMask[p] && activeLayer(p, c.z) == 0) {
        const Turntable& tt = turntables[ttMask[p] - 1];
        float da = tt.now * dt, cs = cosf(da), sn = sinf(da), dx = c.x - tt.x, dy = c.y - tt.y;
        float nx2 = tt.x + dx * cs - dy * sn, ny2 = tt.y + dx * sn + dy * cs;
        if (!carBlocked(c, nx2, ny2, c.a + da)) { c.x = nx2; c.y = ny2; c.a += da; float vx = c.vx * cs - c.vy * sn; c.vy = c.vx * sn + c.vy * cs; c.vx = vx; }
    }
    // height
    p = pixAt(c.x, c.y); ly = activeLayer(p, c.z);
    float hg = ht[ly][p], sp = hypotf(c.vx, c.vy), slope = 0;
    if (!c.air) {
        if (hg < c.z - 1.3f) { c.air = true; c.vz = c.slopeVz; if (c.vz > 15) snd::jump(); }
        else {
            c.z = hg;
            if (sp > 5) {
                int pb = pixAt(c.x - c.vx / sp * 3, c.y - c.vy / sp * 3);
                float dh = hg - ht[activeLayer(pb, c.z)][pb];
                if (fabsf(dh) < 2.6f) { slope = dh / 3; c.slopeVz = slope * sp; }
                if (c.vx * cosf(c.a) + c.vy * sinf(c.a) < 0) slope = -slope;
            } else c.slopeVz = 0;
            c.gvis = c.z;
        }
    } else {
        c.vz -= 250 * dt; c.z += c.vz * dt;
        if (c.z <= hg) {
            c.z = hg; c.air = false; c.slopeVz = 0;
            if (c.vz < -30) { snd::land(); if (surf[ly][p] != S_WATER) burst(c.x, c.y, 6, 20, 0.3f, SMOKE_COLS, 2); }
        }
        c.gvis += (hg - c.gvis) * std::min(1.0f, dt * 9); if (c.gvis > c.z) c.gvis = c.z;
        slope = clampf(c.vz / std::max(sp, 30.0f), -0.6f, 0.6f);
    }
    c.pitch += (slope - c.pitch) * std::min(1.0f, dt * 14);
    {
        float lx = -sinf(c.a), lyv = cosf(c.a);
        int pl = pixAt(c.x + lx * 2, c.y + lyv * 2), pr = pixAt(c.x - lx * 2, c.y - lyv * 2);
        float dr = ht[activeLayer(pl, c.z)][pl] - ht[activeLayer(pr, c.z)][pr];
        float roll = (!c.air && fabsf(dr) < 2.6f) ? dr / 4 : 0;
        c.roll += (roll - c.roll) * std::min(1.0f, dt * 14);
    }
    // hazards
    s = surf[ly][p];
    bool realRoad = roadLike(s);
    for (const Ferry& f : ferries) if (!c.air && onFerry(f, c.x, c.y) && (s == S_WATER || s == S_OFF)) s = S_PLANK;
    if (!c.air && ly == 0 && spanMask[p] && bridges[spanMask[p] - 1].lift > 0.3f) s = S_WATER;      // the drawbridge opened under the car
    if (!c.air && s == S_WATER) { burst(c.x, c.y, 26, 45, 0.6f, WATER_COLS, 3, 60); snd::splash(); killCar(c, 1.4f); return; }
    if (c.invuln <= 0 && trainHits(c)) { explosion(c.x, c.y); killCar(c, 1.6f); return; }
    // progress
    if (roadLike(s) || c.air) {
        int i = realRoad ? idxm[ly][p] : nearIdx[p];
        int d = ((i - c.idx) % NS + NS) % NS; if (d > NS / 2) d -= NS;
        if (abs(d) < NS / 7) { c.idx = i; c.lostT = 0; } else { c.lostT += dt; if (c.lostT > 1.2f) { c.idx = i; c.lostT = 0; } }
        int sec = c.idx * NSECT / NS;
        if (sec == (c.sector + 1) % NSECT && !c.finished) {
            c.sector = sec;
            if (sec == 0) {
                c.lap++;
                if (c.lap > raceLaps) { c.finished = true; c.place = ++finishCount; c.ftime = raceT; if (c.human) snd::finish(); }
                else if (c.human && c.lap > 1) snd::lapChime();
            }
        }
    }
}

void collideCars() {
    for (int i = 0; i < 5; i++) for (int j = i + 1; j < 5; j++) {
        Car& a = cars[i], & b = cars[j];
        if (!a.on || !b.on || a.dead > 0 || b.dead > 0 || fabsf(a.z - b.z) > 3.0f) continue;
        float dx = a.x - b.x, dy = a.y - b.y, d = hypotf(dx, dy);
        if (d > 10.5f) continue;
        int ma[96][2], mb[96][2], na = carMask(a.x, a.y, a.a, ma), nb = carMask(b.x, b.y, b.a, mb);
        bool hit = false;
        for (int p = 0; p < na && !hit; p++) for (int q = 0; q < nb; q++) if (ma[p][0] == mb[q][0] && ma[p][1] == mb[q][1]) { hit = true; break; }
        if (!hit) continue;
        if (d < 0.01f) { dx = 1; dy = 0; d = 1; }
        float nx = dx / d, ny = dy / d, rv = (a.vx - b.vx) * nx + (a.vy - b.vy) * ny;
        if (rv < 0) {
            float jimp = -(1.5f) * rv / 2;
            a.vx += jimp * nx; a.vy += jimp * ny; b.vx -= jimp * nx; b.vy -= jimp * ny;
            if (-rv > 25 && a.crashCd <= 0) { snd::crash(clampf(-rv / 100, 0.3f, 0.8f)); a.crashCd = 0.25f; }
        }
        if (!carBlocked(a, a.x + nx * 0.5f, a.y + ny * 0.5f, a.a)) { a.x += nx * 0.5f; a.y += ny * 0.5f; }
        if (!carBlocked(b, b.x - nx * 0.5f, b.y - ny * 0.5f, b.a)) { b.x -= nx * 0.5f; b.y -= ny * 0.5f; }
    }
}

void stepWeapons(float dt) {
    for (size_t i = 0; i < projs.size();) {
        Proj& pr = projs[i];
        bool dead = false;
        pr.life -= dt;
        if (pr.type == WP_MISSILE) {
            Car* best = nullptr; float bd = 1e9f;
            float sp = hypotf(pr.vx, pr.vy), ca = atan2f(pr.vy, pr.vx);
            for (Car& c : cars) if (c.on && c.slot != pr.owner && c.dead <= 0 && !c.finished) {
                float dx = c.x - pr.x, dy = c.y - pr.y, d = hypotf(dx, dy);
                if (fabsf(angDiff(atan2f(dy, dx), ca)) < 1.2f && d < bd) { bd = d; best = &c; }
            }
            if (best) ca += clampf(angDiff(atan2f(best->y - pr.y, best->x - pr.x), ca), -5 * dt, 5 * dt);
            sp = std::min(sp + 200 * dt, 210.0f);
            pr.vx = cosf(ca) * sp; pr.vy = sinf(ca) * sp;
            if (frameNo & 1) addPart(pr.x, pr.y, (grng.f() - 0.5f) * 8, (grng.f() - 0.5f) * 8, 0.3f, grng.range(2) ? C_WHITE : C_GREY);
        }
        pr.x += pr.vx * dt; pr.y += pr.vy * dt;
        if (pr.x < 0 || pr.x >= W || pr.y < HUD_H || pr.y >= H || pr.life <= 0) dead = true;
        else if (pr.type == WP_SHOT) { int p = pixAt(pr.x, pr.y), ly = activeLayer(p, pr.z); if (solid[ly][p] || ht[ly][p] - pr.z > 2.5f) { dead = true; burst(pr.x, pr.y, 4, 20, 0.2f, FIRE_COLS, 2); } }
        if (!dead) for (Car& c : cars) if (c.on && c.slot != pr.owner && c.dead <= 0) {
            float d = hypotf(c.x - pr.x, c.y - pr.y);
            if (d < 5.5f && fabsf(c.z - pr.z) < (pr.type == WP_MISSILE ? 9 : 4)) {
                if (pr.type == WP_MISSILE) { explosion(c.x, c.y); hitCar(c, 1.2f, 0.25f, 45); }
                else { burst(c.x, c.y, 8, 30, 0.25f, FIRE_COLS, 3); snd::crash(0.7f); hitCar(c, 0.6f, 0.55f, 0); }
                dead = true; break;
            }
        }
        if (dead) { if (pr.type == WP_MISSILE && pr.life <= 0) burst(pr.x, pr.y, 10, 30, 0.3f, FIRE_COLS, 4); projs[i] = projs.back(); projs.pop_back(); } else i++;
    }
    for (size_t i = 0; i < mines.size();) {
        Mine& m = mines[i];
        bool boom = false;
        if (m.arm > 0) m.arm -= dt;
        else for (Car& c : cars) if (c.on && c.dead <= 0 && !c.air && fabsf(c.z - m.z) < 3 && hypotf(c.x - m.x, c.y - m.y) < 5.5f) { explosion(m.x, m.y); hitCar(c, 1.2f, 0.25f, 55); boom = true; break; }
        int p = pixAt(m.x, m.y);
        if (surf[activeLayer(p, m.z)][p] == S_WATER || solid[activeLayer(p, m.z)][p]) boom = true;
        if (boom) { mines[i] = mines.back(); mines.pop_back(); } else i++;
    }
    for (ItemBox& it : items) {
        if (it.timer > 0) { it.timer -= dt; if (it.timer <= 0) it.type = 1 + grng.range(WP_COUNT - 1); continue; }
        for (Car& c : cars) if (c.on && c.dead <= 0 && !c.finished && c.weapon == WP_NONE && fabsf(c.z - it.z) < 3 && hypotf(c.x - it.x, c.y - it.y) < 7.0f) {
            c.weapon = it.type; c.ammo = c.weapon == WP_SHOT ? 3 : 1; it.timer = 6;
            if (c.human) snd::pickup();
            burst(it.x, it.y, 8, 25, 0.3f, FIRE_COLS, 4);
            break;
        }
    }
}

void computeRanks() {
    int order[8], n = 0;
    for (int i = 0; i < 5; i++) if (cars[i].on) order[n++] = i;
    auto key = [](const Car& c) { return c.finished ? 1e9f - c.place * 1e6f : (float)(c.lap * NSECT + c.sector) * 100000.0f + c.idx; };
    sortSmall(order, n, [&](int a, int b) { return key(cars[a]) > key(cars[b]); });
    for (int i = 0; i < n; i++) cars[order[i]].rank = i;
}

// ------------------------------------------------------------------ moving parts of a track
void updateMechanics(float dt, float t) {
    gust = clampf(0.65f + 0.35f * sinf(t * 0.9f) + 0.2f * sinf(t * 2.3f), 0.15f, 1.2f);
    for (Turntable& tt : turntables) { tt.now = tt.speed * clampf(sinf(t * 0.7f + tt.x) * 1.6f, -1, 1); tt.ang += tt.now * dt; }
    for (Drawbridge& d : bridges) {
        float ct = fmodf(t + d.phase + 100 * d.period, d.period), down = d.period - 5.5f;
        bool wasOpen = d.open, wasWarn = d.warn;
        d.t = ct; d.open = ct < down + 1.5f; d.warn = ct >= down;
        float u = ct - (down + 1.5f);       // seconds since the barriers closed
        d.lift = u < 0 ? 0 : (u < 0.8f ? u / 0.8f : (u < 3.2f ? 1 : std::max(0.f, 1 - (u - 3.2f) / 0.8f)));
        if (d.open != wasOpen) for (int p : d.barrier) dynSolid[p] = d.open ? 0 : 1;
        if (!attract && d.warn && d.open && (!wasWarn || (int)(ct * 4) != (int)((ct - dt) * 4))) snd::bell();
    }
    for (Ferry& f : ferries) {
        f.tLeft -= dt;
        if (f.tLeft <= 0) { f.state = (f.state + 1) & 3; f.tLeft = (f.state & 1) ? 1.5f : 2.0f; }
        float u = f.state == 0 ? 0 : (f.state == 2 ? 1 : (f.state == 1 ? 1 - f.tLeft / 1.5f : f.tLeft / 1.5f));
        u = u * u * (3 - 2 * u);
        float nx = lerpf(f.ax, f.bx, u), ny = lerpf(f.ay, f.by, u);
        f.vx = (nx - f.x) / dt; f.vy = (ny - f.y) / dt; f.x = nx; f.y = ny;
    }
}
