// Rally Mayhem - input devices, race flow, HUD, title and results screens
// by rsn8887

#include "game.h"

// ------------------------------------------------------------------ input devices
static const int NDEV = 10; // 0,1 = keyboard halves, 2.. = game controllers
struct Device { bool present = false; SDL_GameController* gc = nullptr; SDL_JoystickID id = -1; Input cur, prev; };
static Device devs[NDEV];

static void pollDevices() {
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    devs[0].present = devs[1].present = true;
    for (int i = 0; i < NDEV; i++) devs[i].prev = devs[i].cur;
    Input a, b;
    a.l = k[SDL_SCANCODE_LEFT]; a.r = k[SDL_SCANCODE_RIGHT]; a.up = k[SDL_SCANCODE_UP]; a.down = k[SDL_SCANCODE_DOWN];
    a.acc = k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_RCTRL]; a.brk = k[SDL_SCANCODE_RSHIFT]; a.reset = k[SDL_SCANCODE_BACKSPACE];
    a.start = k[SDL_SCANCODE_RETURN]; a.back = k[SDL_SCANCODE_ESCAPE] || k[SDL_SCANCODE_AC_BACK]; // AC_BACK = Android's Back button
    b.l = k[SDL_SCANCODE_A]; b.r = k[SDL_SCANCODE_D]; b.up = k[SDL_SCANCODE_W]; b.down = k[SDL_SCANCODE_S];
    b.acc = k[SDL_SCANCODE_LCTRL]; b.brk = k[SDL_SCANCODE_LSHIFT]; b.reset = k[SDL_SCANCODE_Q];
    devs[0].cur = a; devs[1].cur = b;
    for (int i = 2; i < NDEV; i++) {
        Device& d = devs[i];
        if (!d.present || !d.gc) { d.cur = Input(); continue; }
        Input in;
        int ax = SDL_GameControllerGetAxis(d.gc, SDL_CONTROLLER_AXIS_LEFTX), ay = SDL_GameControllerGetAxis(d.gc, SDL_CONTROLLER_AXIS_LEFTY);
        in.l = SDL_GameControllerGetButton(d.gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || ax < -12000;
        in.r = SDL_GameControllerGetButton(d.gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || ax > 12000;
        in.up = SDL_GameControllerGetButton(d.gc, SDL_CONTROLLER_BUTTON_DPAD_UP) || ay < -22000;
        in.down = SDL_GameControllerGetButton(d.gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ay > 22000;
        in.acc = SDL_GameControllerGetButton(d.gc, SDL_CONTROLLER_BUTTON_A) || SDL_GameControllerGetAxis(d.gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 8000;
        in.brk = SDL_GameControllerGetButton(d.gc, SDL_CONTROLLER_BUTTON_B) || SDL_GameControllerGetButton(d.gc, SDL_CONTROLLER_BUTTON_X) || SDL_GameControllerGetAxis(d.gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 8000;
        in.reset = SDL_GameControllerGetButton(d.gc, SDL_CONTROLLER_BUTTON_Y);
        in.start = SDL_GameControllerGetButton(d.gc, SDL_CONTROLLER_BUTTON_START);
        in.back = SDL_GameControllerGetButton(d.gc, SDL_CONTROLLER_BUTTON_BACK);
#ifdef __SWITCH__
        // a single Joy-Con has only one of +/-, so either one is Start on Switch
        in.start = in.start || in.back;
        in.back = false;
#endif
        d.cur = in;
    }
}
void addController(int joyIndex) {
    if (!SDL_IsGameController(joyIndex)) return;
    SDL_GameController* gc = SDL_GameControllerOpen(joyIndex);
    if (!gc) return;
    SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gc));
    for (int i = 2; i < NDEV; i++) if (devs[i].present && devs[i].id == id) return;
    for (int i = 2; i < NDEV; i++) if (!devs[i].present) { devs[i].present = true; devs[i].gc = gc; devs[i].id = id; devs[i].cur = devs[i].prev = Input(); return; }
    SDL_GameControllerClose(gc);
}
void removeController(SDL_JoystickID id) {
    for (int i = 2; i < NDEV; i++) if (devs[i].present && devs[i].id == id) { SDL_GameControllerClose(devs[i].gc); devs[i].gc = nullptr; devs[i].present = false; devs[i].cur = Input(); }
}
#define EDGE(d, f) (devs[d].cur.f && !devs[d].prev.f)
static bool anyEdgeStart() { for (int i = 0; i < NDEV; i++) if (devs[i].present && EDGE(i, start)) return true; return false; }
static bool anyEdgeBrake() { for (int i = 0; i < NDEV; i++) if (devs[i].present && EDGE(i, brk)) return true; return false; }
static bool anyEdgeBack() { for (int i = 0; i < NDEV; i++) if (devs[i].present && EDGE(i, back)) return true; return false; }

// ------------------------------------------------------------------ game flow
enum { ST_TITLE, ST_RACE, ST_RESULTS, ST_FINAL };
int state = ST_TITLE, menuSel = 0, optTrack = 0, optLaps = 5, curTrack = 0;
bool optItems = true, paused = false, quit = false;
int optCpu = -1;        // CPU cars: -1 = fill every free slot, 0 = none, otherwise that many
int optDiff = 1;        // CPU level: 0 easy, 1 normal, 2 hard
int joined[5] = { -1, -1, -1, -1, -1 }; // device per player slot
static int totalPts[5] = { 0, 0, 0, 0, 0 };
static float stateT = 0, endT = -1;
static int resultOrder[5], resultN = 0, lastBeep = 0;
static const int POINTS[5] = { 10, 6, 4, 2, 1 };

static int humanCount() { int n = 0; for (int i = 0; i < 5; i++) if (joined[i] >= 0) n++; return n; }
// how many CPU cars race, given the menu setting and the free slots
static int cpuCount() { int free = 5 - humanCount(); return optCpu < 0 ? free : std::min(optCpu, free); }
// is player slot i taken by a CPU car? (the first free slots get them)
static bool cpuSlot(int i) {
    if (joined[i] >= 0) return false;
    int before = 0;
    for (int k = 0; k < i; k++) if (joined[k] < 0) before++;
    return before < cpuCount();
}

void startRace(int track, bool isAttract) {
    attract = isAttract; curTrack = track;
    buildLevel(track);
    if (!optItems) items.clear();
    parts.clear(); projs.clear(); mines.clear(); leaves.clear(); drops.clear(); streaks.clear();
    raceT = -3.6f; finishCount = 0; endT = -1; lastBeep = 0; paused = false; raceLaps = isAttract ? 99 : optLaps;
    int grid = 0;
    for (int i = 0; i < 5; i++) {
        Car& c = cars[i];
        int pts = c.points;
        c = Car(); c.points = pts; c.slot = i;
        c.human = !isAttract && joined[i] >= 0; c.dev = c.human ? joined[i] : -1;
        c.on = c.human || isAttract || cpuSlot(i);
        if (!c.on) continue;
        int g = (grid + track) % 5; grid++;
        startPose(g, c.x, c.y, c.a);
        c.idx = pixAt(c.x, c.y) >= 0 ? nearIdx[pixAt(c.x, c.y)] : 0;
        c.z = c.gvis = sm[c.idx].h; c.sector = NSECT - 1; c.lap = 0;
        c.power = 1; static const float base[3] = { 0.76f, 0.80f, 0.86f };      // CPU engine power by level (a human car has 1.0)
        c.skill = c.human ? 1.0f : base[optDiff] + 0.025f * ((i * 3 + track) % 5);
        c.lane = (i - 2) * 0.5f;
    }
    snd::setMusic(isAttract); snd::mute = isAttract;
    state = isAttract ? ST_TITLE : ST_RACE; stateT = 0;
}

void finishRace() {
    computeRanks();
    resultN = 0;
    for (int r = 0; r < 5; r++) for (int i = 0; i < 5; i++) if (cars[i].on && cars[i].rank == r) resultOrder[resultN++] = i;
    for (int r = 0; r < resultN; r++) { cars[resultOrder[r]].points += POINTS[r]; totalPts[resultOrder[r]] = cars[resultOrder[r]].points; }
    state = ST_RESULTS; stateT = 0; snd::setMusic(true); snd::mute = true;
    for (int k = 0; k < 5; k++) snd::eng[k].tv = 0;
    snd::skidTarget = 0; snd::rainTarget = 0;
}

static void simulate(float dt) {
    raceT += dt;
    trainUpdate(raceT + 3.6f);
    updateMechanics(dt, raceT + 3.6f);
    computeRanks();
    for (Car& c : cars) {
        if (!c.on) continue;
        Input in;
        if (c.human && !c.finished) in = devs[c.dev].cur; else in = aiInput(c, dt);
        stepCar(c, in, dt);
    }
    collideCars();
    stepWeapons(dt);
}

void updateRace(float frameDt) {
    snd::skidTarget = 0;
    const float dt = 1.0f / 120;
    for (int i = 0; i < 2; i++) simulate(dt);
    stepAmbient(frameDt);
    for (int i = 0; i < 5; i++) {
        const Car& c = cars[i];
        float sp = hypotf(c.vx, c.vy);
        snd::eng[i].tf = 48 + sp * 1.25f + (c.turbo > 0 ? 40 : 0) + i * 3;
        snd::eng[i].tv = (c.on && c.dead <= 0) ? (attract ? 0.0f : (c.human ? 0.07f : 0.022f)) : 0;
    }
    if (attract) return;
    int b = raceT < 0 ? (int)ceilf(-raceT) : 0;
    if (b != lastBeep) { if (b >= 1 && b <= 3) snd::beep(false); else if (b == 0 && lastBeep == 1) snd::beep(true); lastBeep = b; }
    // end of race: all humans done, or a while after the winner
    bool humansDone = true, anyHuman = false, allDone = true;
    for (const Car& c : cars) if (c.on) { if (c.human) { anyHuman = true; if (!c.finished) humansDone = false; } if (!c.finished) allDone = false; }
    if (finishCount > 0 && endT < 0) endT = raceT + 25;
    if (allDone || (anyHuman && humansDone && endT > 0 && raceT > endT - 22) || (endT > 0 && raceT > endT)) {
        if (stateT == 0) stateT = 0.001f;
    }
    if (stateT > 0) { stateT += frameDt; if (stateT > 2.5f) finishRace(); }
}

static void dimScreen(float f) { for (int p = 0; p < N; p++) fb[p] = shade(fb[p], f); }
void backdrop(float dim) { drawWorld(); if (dim < 1) dimScreen(dim); }
static void drawMiniCar(int x, int y, int slot) {
    uint32_t c = CAR_COL[slot];
    rectf(x, y + 1, 7, 4, c); rectf(x + 2, y + 2, 3, 2, C_WHITE); pset(x + 4, y + 2, RGB(0, 0, 96)); pset(x + 4, y + 3, RGB(0, 0, 96));
    pset(x + 1, y, C_BLACK); pset(x + 5, y, C_BLACK); pset(x + 1, y + 5, C_BLACK); pset(x + 5, y + 5, C_BLACK);
}

void drawHud() {
    rectf(0, 0, W, HUD_H, C_BLACK);
    char buf[64];
    for (int i = 0; i < 5; i++) {
        const Car& c = cars[i];
        if (!c.on) continue;
        int x = i * 64 + 1;
        drawMiniCar(x, 1, i);
        if (c.finished) snprintf(buf, sizeof(buf), "%d.DONE", c.place);
        else snprintf(buf, sizeof(buf), "%d L%d/%d", c.rank + 1, std::max(1, std::min(c.lap, raceLaps)), raceLaps);
        drawTextRaw(fontS, x + 9, 1, buf, c.human ? C_WHITE : C_GREY);
        if (c.weapon) { drawIcon(x + 54, 1, c.weapon); if (c.weapon == WP_SHOT) for (int k = 0; k < c.ammo; k++) pset(x + 62, 2 + k * 2, C_YELLOW); }
    }
    if (raceT < 0) {
        int n = (int)ceilf(-raceT);
        rectf(W / 2 - 22, 60, 44, 16, C_BLACK);
        for (int k = 0; k < 3; k++) { uint32_t col = (3 - n) >= k && n <= 3 ? C_RED : RGB(64, 0, 0); rectf(W / 2 - 18 + k * 13, 63, 10, 10, col); }
        drawTextC(fontS, W / 2, 80, def->name, C_WHITE);
    } else if (raceT < 0.9f) {
        rectf(W / 2 - 22, 60, 44, 16, C_BLACK);
        for (int k = 0; k < 3; k++) rectf(W / 2 - 18 + k * 13, 63, 10, 10, C_GREEN);
        drawTextC(fontL, W / 2, 82, "GO!", C_GREEN);
    }
    if (raceT < 2.5f) for (const Car& c : cars) if (c.on && c.human) {
        snprintf(buf, sizeof(buf), "P%d", c.slot + 1);
        drawTextC(fontS, (int)c.x, (int)c.y - 16, buf, CAR_COL[c.slot]);
        pset((int)c.x, (int)c.y - 7, C_WHITE); pset((int)c.x - 1, (int)c.y - 8, C_WHITE); pset((int)c.x + 1, (int)c.y - 8, C_WHITE);
    }
    if (endT > 0 && stateT == 0 && raceT > endT - 10) { snprintf(buf, sizeof(buf), "%d", (int)ceilf(endT - raceT)); drawTextC(fontL, W / 2, 20, buf, C_YELLOW); }
    if (stateT > 0) drawTextC(fontL, W / 2, 100, "RACE OVER", C_YELLOW);
    if (paused) { rectf(W / 2 - 130, 82, 260, 48, C_BLACK); drawTextC(fontL, W / 2, 90, "PAUSED", C_WHITE); drawTextC(fontS, W / 2, 116, DESKTOP ? "START/ENTER: RESUME   " BRAKE_LABEL "/ESC: QUIT RACE" : START_LABEL ": RESUME   " BRAKE_LABEL ": QUIT RACE", C_YELLOW); }
}

static const char* devName(int d) { return d == 0 ? "KEYS 1" : d == 1 ? "KEYS 2" : "PAD"; }

static void updateTitle(float frameDt) {
    updateRace(frameDt);
    stateT += frameDt;
    bool change = false;
    for (int d = 0; d < NDEV; d++) {
        if (!devs[d].present) continue;
        int slot = -1; for (int i = 0; i < 5; i++) if (joined[i] == d) slot = i;
        if (EDGE(d, acc) && slot < 0) { for (int i = 0; i < 5; i++) if (joined[i] < 0) { joined[i] = d; snd::join(); break; } }
        if (EDGE(d, brk) && slot >= 0) { joined[slot] = -1; snd::menu(); }
        if (EDGE(d, up)) { menuSel = (menuSel + 4) % 5; snd::menu(); }
        if (EDGE(d, down)) { menuSel = (menuSel + 1) % 5; snd::menu(); }
        int dir = (EDGE(d, r) ? 1 : 0) - (EDGE(d, l) ? 1 : 0);
        if (dir) {
            snd::menu();
            if (menuSel == 0) { optTrack = (optTrack + dir + (int)TRACKS.size()) % (int)TRACKS.size(); change = true; }
            if (menuSel == 1) optLaps = std::max(1, std::min(9, optLaps + dir));
            if (menuSel == 2) {     // ALL, 1, 2, ... (as many as there are free slots), OFF
                int free = 5 - humanCount(), n = cpuCount();
                int pos = optCpu < 0 ? 0 : (n == 0 ? free + 1 : n);
                pos = (pos + dir + free + 2) % (free + 2);
                optCpu = pos == 0 ? -1 : (pos == free + 1 ? 0 : pos);
            }
            if (menuSel == 3) optDiff = (optDiff + dir + 3) % 3;
            if (menuSel == 4) { optItems = !optItems; if (optItems) buildItems(); else { items.clear(); for (Car& c : cars) c.weapon = WP_NONE; } }
        }
    }
    for (int i = 0; i < 5; i++) if (joined[i] >= 2 && !devs[joined[i]].present) joined[i] = -1;
    if (change) startRace(optTrack, true);
    if (humanCount() == 0) for (int d = 0; d < NDEV; d++) if (devs[d].present && EDGE(d, start)) { joined[0] = d; break; }
    if (anyEdgeStart() && humanCount() > 0) {
        for (int i = 0; i < 5; i++) { cars[i].points = 0; totalPts[i] = 0; }
        startRace(optTrack, false);
    }
}

static void drawTitle() {
    backdrop(0.5f);
    char buf[80];
    const char* title = GAME_TITLE;
    rectf(W / 2 - 80, 10, 160, 34, C_BLACK); rectf(40, 87, 240, 60, C_BLACK); rectf(20, 150, 280, 13, C_BLACK); rectf(20, 168, 280, 68, C_BLACK);
    int tw = textW(fontL, title), tx = W / 2 - tw / 2;
    drawTextRaw(fontL, tx + 2, 16, title, C_BLACK); drawTextRaw(fontL, tx + 1, 15, title, C_RED); drawTextRaw(fontL, tx, 14, title, C_YELLOW);
    drawTextC(fontS, W / 2, 34, "BY RSN8887", C_PASTELCYAN);
    for (int i = 0; i < 5; i++) {
        int x = 8 + i * 62;
        rectf(x, 50, 56, 30, C_BLACK); rectf(x, 50, 56, 1, CAR_COL[i]); rectf(x, 79, 56, 1, CAR_COL[i]);
        snprintf(buf, sizeof(buf), "P%d", i + 1);
        drawTextRaw(fontS, x + 4, 54, buf, CAR_COL[i]); drawMiniCar(x + 44, 54, i);
        if (joined[i] >= 0) { drawTextRaw(fontS, x + 4, 63, "READY", C_GREEN); drawTextRaw(fontS, x + 4, 71, devName(joined[i]), C_GREY); }
        else if ((frameNo >> 5) & 1) drawTextRaw(fontS, x + 4, 63, "PRESS", C_GREY), drawTextRaw(fontS, x + 4, 71, GAS_LABEL, C_GREY);
        else drawTextRaw(fontS, x + 4, 67, cpuSlot(i) ? "CPU" : "---", C_GREY);
    }
    const char* names[5] = { "TRACK", "LAPS", "CPU CARS", "CPU LEVEL", "BONUS ITEMS" };
    static const char* levels[3] = { "EASY", "NORMAL", "HARD" };
    for (int i = 0; i < 5; i++) {
        int y = 91 + i * 11;
        uint32_t col = menuSel == i ? C_YELLOW : C_WHITE;
        if (i == 0) snprintf(buf, sizeof(buf), "< %d %s >", optTrack + 1, TRACKS[optTrack].name);
        else if (i == 1) snprintf(buf, sizeof(buf), "< %d >", optLaps);
        else if (i == 2) { if (optCpu < 0) snprintf(buf, sizeof(buf), "< ALL >"); else if (cpuCount() == 0) snprintf(buf, sizeof(buf), "< OFF >"); else snprintf(buf, sizeof(buf), "< %d >", cpuCount()); }
        else if (i == 3) snprintf(buf, sizeof(buf), "< %s >", levels[optDiff]);
        else snprintf(buf, sizeof(buf), "< %s >", optItems ? "ON" : "OFF");
        drawText(fontS, 60, y, names[i], col); drawText(fontS, 136, y, buf, col);
        if (menuSel == i) drawText(fontS, 48, y, ">", C_YELLOW);
    }
    if (humanCount() > 0) { if ((frameNo >> 4) & 1) drawTextC(fontS, W / 2, 153, DESKTOP ? "PRESS START OR ENTER TO RACE" : "PRESS " START_LABEL " TO RACE", C_GREEN); }
    else drawTextC(fontS, W / 2, 153, "PRESS " GAS_LABEL " TO JOIN - UP TO 5 PLAYERS", C_WHITE);
    drawTextC(fontS, W / 2, 172, PAD_HELP, C_PASTELCYAN);
    drawTextC(fontS, W / 2, 182, optItems ? "UP/DOWN: FIRE WEAPON AHEAD/BEHIND" : "TURN ON BONUS ITEMS FOR WEAPONS", C_PASTELCYAN);
#if defined(__SWITCH__)
    drawTextC(fontS, W / 2, 200, "SINGLE JOY-CONS: HOLD SIDEWAYS", C_GREY);
    drawTextC(fontS, W / 2, 224, "V" GAME_VERSION, C_GREY);
#elif defined(__vita__)
    drawTextC(fontS, W / 2, 200, "PS TV: UP TO 4 CONTROLLERS", C_GREY);
    drawTextC(fontS, W / 2, 224, "SELECT QUIT   V" GAME_VERSION, C_GREY);
#elif defined(__ANDROID__)
    drawTextC(fontS, W / 2, 200, "CONNECT GAMEPADS FOR MORE PLAYERS", C_GREY);
    drawTextC(fontS, W / 2, 224, "BACK QUIT   V" GAME_VERSION, C_GREY);
#else
    drawTextC(fontS, W / 2, 196, "KEYS 1: ARROWS  SPACE GAS  RSHIFT BRAKE", C_GREY);
    drawTextC(fontS, W / 2, 206, "KEYS 2: WASD  LCTRL GAS  LSHIFT BRAKE", C_GREY);
    drawTextC(fontS, W / 2, 224, "F11 FULLSCREEN   ESC QUIT   V" GAME_VERSION, C_GREY);
#endif
}

static void drawResults(bool final) {
    backdrop(0.5f);
    char buf[80];
    rectf(50, 58, 220, 100, C_BLACK); rectf(40, 196, 240, 30, C_BLACK);
    drawTextC(fontL, W / 2, 20, final ? "CHAMPIONSHIP" : "RESULTS", C_YELLOW);
    if (!final) { snprintf(buf, sizeof(buf), "TRACK %d - %s", curTrack + 1, def->name); drawTextC(fontS, W / 2, 42, buf, C_WHITE); }
    int order[8], n = 0;
    if (final) { for (int i = 0; i < 5; i++) if (cars[i].on) order[n++] = i; sortSmall(order, n, [](int a, int b) { return cars[a].points > cars[b].points; }); }
    else { n = resultN; for (int i = 0; i < n; i++) order[i] = resultOrder[i]; }
    drawText(fontS, 60, 62, "POS  CAR           RACE  TOTAL", C_GREY);
    for (int r = 0; r < n; r++) {
        const Car& c = cars[order[r]];
        int y = 76 + r * 16;
        snprintf(buf, sizeof(buf), "%d.", r + 1); drawText(fontS, 60, y, buf, C_WHITE);
        drawMiniCar(88, y, c.slot);
        snprintf(buf, sizeof(buf), "%s %s", CAR_NAME[c.slot], c.human ? "" : "CPU"); if (c.human) snprintf(buf, sizeof(buf), "%s P%d", CAR_NAME[c.slot], c.slot + 1);
        drawText(fontS, 100, y, buf, CAR_COL[c.slot]);
        if (!final) { snprintf(buf, sizeof(buf), "+%d", POINTS[r]); drawText(fontS, 174, y, buf, C_WHITE); }
        snprintf(buf, sizeof(buf), "%3d", c.points); drawText(fontS, 216, y, buf, C_YELLOW);
    }
    if (final && n > 0) { snprintf(buf, sizeof(buf), "%s WINS THE CUP!", CAR_NAME[order[0]]); if ((frameNo >> 4) & 1) drawTextC(fontS, W / 2, 170, buf, CAR_COL[order[0]]); }
    if (stateT > 1.0f) drawTextC(fontS, W / 2, 200, final ? "PRESS " START_LABEL " FOR TITLE SCREEN" : "PRESS " START_LABEL " FOR NEXT TRACK", C_GREEN);
    drawTextC(fontS, W / 2, 214, DESKTOP ? BRAKE_LABEL "/ESC: TITLE SCREEN" : BRAKE_LABEL ": TITLE SCREEN", C_GREY);
}

static void toTitle() { for (int i = 0; i < 5; i++) cars[i].points = 0; startRace(optTrack, true); }

void frame(float frameDt) {
    frameNo++;
    pollDevices();
    switch (state) {
    case ST_TITLE:
        updateTitle(frameDt);
        if (state == ST_TITLE) { if (anyEdgeBack()) quit = true; if (stateT > 40) { startRace((curTrack + 1) % (int)TRACKS.size(), true); } drawTitle(); }
        else { backdrop(1); drawHud(); }
        break;
    case ST_RACE:
        if (anyEdgeStart()) { paused = !paused; snd::menu(); }
        else if (anyEdgeBack() || (paused && anyEdgeBrake())) { if (paused) { toTitle(); break; } paused = true; }
        if (!paused) updateRace(frameDt); else { for (int k = 0; k < 5; k++) snd::eng[k].tv = 0; snd::skidTarget = 0; }
        if (state == ST_RACE) { backdrop(1); drawHud(); } else drawResults(false);
        break;
    case ST_RESULTS:
    case ST_FINAL:
        stateT += frameDt; stepAmbient(frameDt);
        drawResults(state == ST_FINAL);
        if (anyEdgeBack() || (stateT > 1.0f && anyEdgeBrake())) { toTitle(); break; }
        if (stateT > 1.0f && anyEdgeStart()) {
            if (state == ST_FINAL) toTitle();
            else if (curTrack + 1 >= (int)TRACKS.size()) { state = ST_FINAL; stateT = 0; }
            else { optTrack = curTrack + 1; startRace(curTrack + 1, false); }
        }
        break;
    }
}
