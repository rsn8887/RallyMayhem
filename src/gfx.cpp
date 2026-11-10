// Rally Mayhem - frame buffer helpers and text
// by rsn8887

#include "game.h"

// ------------------------------------------------------------------ basics
uint32_t fb[N];
int frameNo = 0;

Rng grng(12345);
void rectf(int x, int y, int w, int h, uint32_t c) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) pset(i, j, c); }

// ------------------------------------------------------------------ font (SDL_ttf glyphs cached as bitmaps)
PFont fontS, fontL;

bool loadFont(PFont& f, const std::string& path, int size) {
    TTF_Font* t = TTF_OpenFont(path.c_str(), size);
    if (!t) return false;
    TTF_SetFontHinting(t, TTF_HINTING_NONE);   // the font is drawn on the pixel grid; hinting would only move its lines
    SDL_Color white = { 255, 255, 255, 255 };
    for (int c = 32; c < 127; c++) {
        char s[2] = { (char)c, 0 };
        int w = 0, h = 0;
        TTF_SizeUTF8(t, s, &w, &h);
        f.g[c].adv = w;
        SDL_Surface* sf = TTF_RenderUTF8_Solid(t, s, white);
        if (!sf) continue;
        f.g[c].w = sf->w; f.g[c].h = sf->h; f.g[c].px.assign(sf->w * sf->h, 0);
        for (int y = 0; y < sf->h; y++)
            for (int x = 0; x < sf->w; x++) f.g[c].px[y * sf->w + x] = ((Uint8*)sf->pixels)[y * sf->pitch + x] ? 1 : 0;
        SDL_FreeSurface(sf);
    }
    f.height = size; f.ok = true;
    TTF_CloseFont(t);
    return true;
}
int textW(const PFont& f, const char* s) { int w = 0; for (; *s; s++) w += f.g[(unsigned char)*s & 127].adv; return w; }
void drawTextRaw(const PFont& f, int x, int y, const char* s, uint32_t col) {
    for (; *s; s++) {
        const Glyph& g = f.g[(unsigned char)*s & 127];
        for (int j = 0; j < g.h; j++) for (int i = 0; i < g.w; i++) if (g.px[j * g.w + i]) pset(x + i, y + j, col);
        x += g.adv;
    }
}
void drawText(const PFont& f, int x, int y, const char* s, uint32_t col, bool shadow) {
    if (shadow) drawTextRaw(f, x + 1, y + 1, s, C_BLACK);
    drawTextRaw(f, x, y, s, col);
}
void drawTextC(const PFont& f, int cx, int y, const char* s, uint32_t col) { drawText(f, cx - textW(f, s) / 2, y, s, col); }
