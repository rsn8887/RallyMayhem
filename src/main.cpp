// Rally Mayhem - program entry, window and main loop, developer tools
// by rsn8887

#include "game.h"

// ------------------------------------------------------------------ developer tools
static void saveBMP(const char* path) {
    FILE* f = fopen(path, "wb"); if (!f) return;
    uint8_t hd[54] = { 'B', 'M' };
    uint32_t sz = 54 + N * 3, off = 54, ih = 40, w = W, h = H, img = N * 3; uint16_t pl = 1, bpp = 24;
    memcpy(hd + 2, &sz, 4); memcpy(hd + 10, &off, 4); memcpy(hd + 14, &ih, 4); memcpy(hd + 18, &w, 4); memcpy(hd + 22, &h, 4);
    memcpy(hd + 26, &pl, 2); memcpy(hd + 28, &bpp, 2); memcpy(hd + 34, &img, 4);
    fwrite(hd, 1, 54, f);
    for (int y = H - 1; y >= 0; y--) for (int x = 0; x < W; x++) { uint32_t c = fb[y * W + x]; uint8_t b[3] = { (uint8_t)c, (uint8_t)(c >> 8), (uint8_t)(c >> 16) }; fwrite(b, 1, 3, f); }
    fclose(f);
}
// --selftest: CPU cars race every track; prints how many laps they manage in 100 simulated seconds.
static int selfTest(const char* shotDir) {
    int bad = 0;
    char path[512];
    for (int t = 0; t < (int)TRACKS.size(); t++) {
        optLaps = 99; optCpu = -1; optItems = true;
        for (int i = 0; i < 5; i++) joined[i] = -1;
        startRace(t, true); attract = false; raceLaps = 99;
        int deaths = 0; bool wasDead[5] = { false, false, false, false, false };
        for (int f = 0; f < 60 * 100; f++) {
            frameNo++;
            updateRace(1.0f / 60);
            for (int i = 0; i < 5; i++) { bool d = cars[i].dead > 0; if (d && !wasDead[i]) deaths++; wasDead[i] = d; }
            if (shotDir && f == 60 * 9) { backdrop(1); drawHud(); snprintf(path, sizeof(path), "%s/track%02d.bmp", shotDir, t + 1); saveBMP(path); }
        }
        int mn = 99, mx = 0;
        for (int i = 0; i < 5; i++) { mn = std::min(mn, cars[i].lap); mx = std::max(mx, cars[i].lap); }
        printf("%2d %-16s len %5.0f  laps %d..%d  respawns %d %s\n", t + 1, TRACKS[t].name, trackLen, mn - 1, mx - 1, deaths, mn < 3 ? " <-- CHECK" : "");
        if (mn < 3) bad++;
    }
    if (shotDir) {
        joined[0] = 0; optLaps = 5; startRace(0, true); for (int f = 0; f < 200; f++) frame(1.0f / 60);
        snprintf(path, sizeof(path), "%s/title.bmp", shotDir); saveBMP(path);
        startRace(2, false); for (int f = 0; f < 100; f++) frame(1.0f / 60);
        snprintf(path, sizeof(path), "%s/start.bmp", shotDir); saveBMP(path);
        finishRace(); for (int f = 0; f < 30; f++) frame(1.0f / 60);
        snprintf(path, sizeof(path), "%s/results.bmp", shotDir); saveBMP(path);
    }
    return bad;
}

// ------------------------------------------------------------------ main
// Command line options (desktop):
//   --fullscreen, -f   start in fullscreen (F11 toggles it)
//   --noitems          start with bonus items switched off
//   --level N          CPU level: 0 easy, 1 normal, 2 hard
//   --selftest         CPU cars race every track without a window and report their laps
//   --shots DIR        like --selftest, and also saves a BMP picture of every track into DIR
int main(int argc, char** argv) {
    bool fullscreen = false, test = false;
    const char* shotDir = nullptr;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--fullscreen") || !strcmp(argv[i], "-f")) fullscreen = true;
        else if (!strcmp(argv[i], "--selftest")) test = true;
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) { test = true; shotDir = argv[++i]; }
        else if (!strcmp(argv[i], "--noitems")) optItems = false;
        else if (!strcmp(argv[i], "--level") && i + 1 < argc) optDiff = std::max(0, std::min(2, atoi(argv[++i])));
    }
#ifdef __SWITCH__
    romfsInit();
#endif
#ifdef __ANDROID__
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");     // Back = Esc (pause / quit from title)
#endif
    if (SDL_Init(test ? 0 : (SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER)) != 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    if (TTF_Init() != 0) { fprintf(stderr, "TTF_Init: %s\n", TTF_GetError()); return 1; }
    {
#if defined(__SWITCH__)
        const std::string tries[] = { "romfs:/font.ttf" };                 // assets/ is packed into the .nro
#elif defined(__vita__)
        const std::string tries[] = { "app0:/assets/font.ttf" };           // packed into the .vpk
#elif defined(__ANDROID__)
        const std::string tries[] = { "font.ttf" };                        // read from the APK's assets
#else
        char* bp = SDL_GetBasePath();
        std::string base = bp ? bp : "";
        if (bp) SDL_free(bp);
        const std::string tries[] = { base + "assets/font.ttf", base + "font.ttf", base + "../Resources/assets/font.ttf", "assets/font.ttf", "font.ttf" };
#endif
        for (const std::string& p : tries) if (loadFont(fontS, p, 8) && loadFont(fontL, p, 16)) break;
        if (!fontL.ok) { fprintf(stderr, "Could not load assets/font.ttf: %s\n", TTF_GetError()); return 1; }
    }
    if (test) return selfTest(shotDir);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
#if defined(__SWITCH__)
    SDL_Window* win = SDL_CreateWindow(GAME_TITLE, 0, 0, 1280, 720, 0);   // always fullscreen 720p
#elif defined(__vita__)
    SDL_Window* win = SDL_CreateWindow(GAME_TITLE, 0, 0, 960, 544, 0);    // Vita screen
#elif defined(__ANDROID__)
    SDL_Window* win = SDL_CreateWindow(GAME_TITLE, 0, 0, W * 2, H * 2, SDL_WINDOW_FULLSCREEN);  // whole screen
#else
    SDL_Window* win = SDL_CreateWindow(GAME_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W * 3, H * 3,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
#endif
    if (!win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
#if defined(__vita__)
    // use SDL's native GXM renderer (precompiled shaders) rather than OpenGL ES via vitaGL,
    // which would need the runtime shader compiler (libshacccg.suprx) on the console
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "VITA gxm");
#endif
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(ren, W, H);
#if defined(__vita__) || defined(SHARP_BILINEAR)
    // fill the Vita's 544 px height with the 4:3 image (725x544, black bars left/right)
    SDL_RenderSetIntegerScale(ren, SDL_FALSE);
#else
    SDL_RenderSetIntegerScale(ren, SDL_TRUE);    // crisp pixels: 3x = 960x720 on Switch
#endif
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H);
#if defined(__vita__) || defined(SHARP_BILINEAR)
    // "sharp bilinear": the frame is first scaled an exact 2x with nearest-neighbour into
    // a 640x480 render target (crisp square pixels), and only that is smoothed (bilinear)
    // for the last 1.13x up to 725x544, so just the pixel edges are slightly soft.
    SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);
    SDL_Texture* tex2x = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W * 2, H * 2);
    if (tex2x) SDL_SetTextureScaleMode(tex2x, SDL_ScaleModeLinear);
    else SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);   // no render targets: plain bilinear
#endif
    SDL_ShowCursor(SDL_DISABLE);
    snd::init();
    for (int i = 0; i < SDL_NumJoysticks(); i++) addController(i);
    grng = Rng(SDL_GetTicks() * 2654435761u + 7);
    startRace(0, true);

    Uint32 next = SDL_GetTicks();
    while (!quit) {
#ifdef __SWITCH__
        if (!appletMainLoop()) break;      // HOME menu -> close
#endif
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
            else if (e.type == SDL_CONTROLLERDEVICEADDED) addController(e.cdevice.which);
            else if (e.type == SDL_CONTROLLERDEVICEREMOVED) removeController(e.cdevice.which);
            else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                if (e.key.keysym.sym == SDLK_F11) { fullscreen = !fullscreen; SDL_SetWindowFullscreen(win, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0); }
            }
        }
        frame(1.0f / 60);
        SDL_UpdateTexture(tex, nullptr, fb, W * 4);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
#if defined(__vita__) || defined(SHARP_BILINEAR)
        if (tex2x) {
            SDL_SetRenderTarget(ren, tex2x);
            SDL_RenderCopy(ren, tex, nullptr, nullptr);      // exact 2x, nearest
            SDL_SetRenderTarget(ren, nullptr);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex2x, nullptr, nullptr);    // remaining 1.13x, bilinear
        } else
#endif
        {
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, nullptr, nullptr);
        }
        SDL_RenderPresent(ren);
        // hold 60 fps even without vsync (or on 120 Hz displays)
        next += 1000 / 60 + ((frameNo % 3) ? 1 : 0);
        Uint32 now = SDL_GetTicks();
        if (now < next) SDL_Delay(next - now); else if (now > next + 100) next = now;
    }
    if (snd::dev) SDL_CloseAudioDevice(snd::dev);
#if defined(__vita__) || defined(SHARP_BILINEAR)
    if (tex2x) SDL_DestroyTexture(tex2x);
#endif
    SDL_DestroyTexture(tex); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win);
    TTF_Quit(); SDL_Quit();
#ifdef __SWITCH__
    romfsExit();
#endif
    return 0;
}
