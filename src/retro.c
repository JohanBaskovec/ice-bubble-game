#include "retro.h"
#include "config.h"
#include "level.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Palette : 16 rampes x 16 niveaux = 256 couleurs                          */

static const float RAMP_HUE[16] = { 222, 2, 24, 50, 85, 125, 168, 188, 205, 225, 250, 280, 315, 340, 24, 38 };
static const float RAMP_SAT[16] = { 0.12f, 0.78f, 0.88f, 0.92f, 0.70f, 0.62f, 0.55f, 0.72f,
                                    0.76f, 0.70f, 0.55f, 0.55f, 0.62f, 0.78f, 0.45f, 0.55f };
static Color g_pal[256];

static void build_palette(void)
{
    for (int r = 0; r < 16; r++) {
        for (int l = 0; l < 16; l++) {
            float t = l / 15.0f;
            float light = 0.035f + 0.93f * t;
            /* ombres plus froides, lumieres plus chaudes, saturation maximale au milieu */
            float hue = RAMP_HUE[r] + (r == R_GRAY ? 0 : (t - 0.5f) * -16.0f);
            float sat = RAMP_SAT[r] * (1.0f - fabsf(t - 0.5f) * 0.7f);
            g_pal[r * 16 + l] = hsl(hue, sat, light, 1.0f);
        }
    }
}

Color retro_color(int index) { return g_pal[index & 255]; }

/* ------------------------------------------------------------------------ */
/* Canevas indexe                                                           */

#define CLEAR 0xFFFF

typedef struct Canvas {
    int w, h;
    Uint16 *px;
} Canvas;

static Canvas cv_new(int w, int h)
{
    Canvas c = { w, h, SDL_malloc((size_t)w * h * sizeof(Uint16)) };
    if (c.px)
        for (int i = 0; i < w * h; i++) c.px[i] = CLEAR;
    return c;
}

static void cv_free(Canvas *c)
{
    SDL_free(c->px);
    c->px = NULL;
}

static void cv_set(Canvas *c, int x, int y, int col)
{
    if (col < 0 || x < 0 || y < 0 || x >= c->w || y >= c->h) return;
    c->px[y * c->w + x] = (Uint16)col;
}

static int cv_get(const Canvas *c, int x, int y)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return CLEAR;
    return c->px[y * c->w + x];
}

static void cv_rect(Canvas *c, int x, int y, int w, int h, int col)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) cv_set(c, i, j, col);
}

static void cv_line(Canvas *c, int x0, int y0, int x1, int y1, int col)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        cv_set(c, x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* Tramage ordonne 4x4 (Bayer) */
static const int BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
static float bayer(int x, int y) { return (BAYER[y & 3][x & 3] + 0.5f) / 16.0f; }

/* Niveau continu -> niveau entier tramé */
static int dlevel(float v, int x, int y)
{
    int l = (int)floorf(v + bayer(x, y));
    return l < 0 ? 0 : (l > 15 ? 15 : l);
}

/* Degrade entre plusieurs couleurs de palette (t de 0 a 1) */
static int dgrad(const int *stops, int n, float t, int x, int y)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    int i = (int)floorf(t * (n - 1) + bayer(x, y));
    if (i >= n) i = n - 1;
    return stops[i];
}

static void cv_ellipse(Canvas *c, float cx, float cy, float rx, float ry, int col)
{
    for (int y = (int)floorf(cy - ry); y <= (int)ceilf(cy + ry); y++)
        for (int x = (int)floorf(cx - rx); x <= (int)ceilf(cx + rx); x++) {
            float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
            if (dx * dx + dy * dy <= 1) cv_set(c, x, y, col);
        }
}

/* Sphere eclairee en haut a gauche, avec contour optionnel (outline < 0 : aucun). */
static void cv_sphere(Canvas *c, float cx, float cy, float r, int ramp, float lo, float hi, int outline, bool spec,
                      float max_y)
{
    for (int y = (int)floorf(cy - r); y <= (int)ceilf(cy + r); y++) {
        if (y > max_y) break;
        for (int x = (int)floorf(cx - r); x <= (int)ceilf(cx + r); x++) {
            float dx = (x + 0.5f - cx) / r, dy = (y + 0.5f - cy) / r, d2 = dx * dx + dy * dy;
            if (d2 > 1) continue;
            float nz = sqrtf(1 - d2);
            float diff = -0.45f * dx - 0.55f * dy + 0.70f * nz;
            if (diff < 0) diff = 0;
            int col = PAL(ramp, dlevel(lo + (hi - lo) * diff, x, y));
            if (spec && diff > 0.95f) col = PAL(R_GRAY, 15);
            if (outline >= 0 && sqrtf(d2) > 1 - 1.1f / r) col = outline;
            cv_set(c, x, y, col);
        }
    }
}

static SDL_Renderer *g_r;

static SDL_Texture *cv_texture(const Canvas *c)
{
    Uint8 *rgba = SDL_malloc((size_t)c->w * c->h * 4);
    if (!rgba) return NULL;
    for (int i = 0; i < c->w * c->h; i++) {
        Uint16 v = c->px[i];
        Uint8 *p = rgba + i * 4;
        if (v == CLEAR) {
            p[0] = p[1] = p[2] = p[3] = 0;
        } else {
            Color k = g_pal[v & 255];
            p[0] = (Uint8)(k.r * 255 + 0.5f);
            p[1] = (Uint8)(k.g * 255 + 0.5f);
            p[2] = (Uint8)(k.b * 255 + 0.5f);
            p[3] = 255;
        }
    }
    SDL_Texture *t = SDL_CreateTexture(g_r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, c->w, c->h);
    if (t) {
        SDL_UpdateTexture(t, NULL, rgba, c->w * 4);
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    }
    SDL_free(rgba);
    return t;
}

/* Generateur pseudo-aleatoire deterministe pour les decors */
static Uint32 g_seed;
static float rnd(void)
{
    g_seed = g_seed * 1664525u + 1013904223u;
    return (g_seed >> 8) / 16777216.0f;
}

/* ------------------------------------------------------------------------ */
/* Boules                                                                   */

typedef struct Sprite {
    SDL_Texture *tex;
    int w, h;       /* pixels */
    int ax, ay;     /* point d'ancrage (centre de la boule, centre des pieds...) */
} Sprite;

static Sprite g_balls[2][BALL_SIZES];

static const float BALL_RADIUS_UNITS[BALL_SIZES] = { 9, 15, 23, 33 };
static const int BALL_RAMPS[BALL_SIZES] = { R_PINK, R_YELLOW, R_GREEN, R_BLUE };

static Sprite make_ball(int size, bool frozen)
{
    float r = BALL_RADIUS_UNITS[size] / RETRO_SCALE;
    int d = (int)ceilf(2 * r) + 2;
    int cap = (int)roundf(r * 0.6f) + 3;
    Canvas c = cv_new(d, d + cap);
    float cx = d / 2.0f, cy = cap + d / 2.0f;
    int ramp = frozen ? R_CYAN : BALL_RAMPS[size];
    int outline = frozen ? PAL(R_GRAY, 15) : PAL(ramp, 1);

    /* attache doree + anneau */
    int cw = (int)fmaxf(3, roundf(r * 0.55f)), ch = (int)fmaxf(2, roundf(r * 0.3f));
    int capx = (int)roundf(cx - cw / 2.0f), capy = (int)roundf(cy - r) - ch + 1;
    cv_rect(&c, capx, capy, cw, ch, PAL(R_YELLOW, 9));
    cv_rect(&c, capx, capy, 1, ch, PAL(R_YELLOW, 13));
    cv_rect(&c, capx + cw - 1, capy, 1, ch, PAL(R_BROWN, 6));
    float rr = fmaxf(1.0f, r * 0.16f);
    for (int a = 0; a < 16; a++) {
        float ang = a * 6.2832f / 16;
        cv_set(&c, (int)floorf(cx + cosf(ang) * rr), (int)floorf(capy - rr + sinf(ang) * rr), PAL(R_YELLOW, 10));
    }

    cv_sphere(&c, cx, cy, r, ramp, frozen ? 8 : 3, frozen ? 14 : 13, outline, true, 1e9f);

    /* bandeau en pointilles sur la face avant */
    for (int x = 0; x < d; x++) {
        float dx = (x + 0.5f - cx) / r;
        if (fabsf(dx) > 0.82f) continue;
        if (((int)floorf((dx + 1) * 4.5f)) % 2) continue;
        float yc = cy + (0.15f + 0.32f * sqrtf(fmaxf(0, 1 - (dx / 0.9f) * (dx / 0.9f)))) * r;
        int y = (int)floorf(yc);
        cv_set(&c, x, y, PAL(ramp, 14));
        if (r > 10) cv_set(&c, x, y + 1, PAL(ramp, 12));
    }
    Sprite s = { cv_texture(&c), c.w, c.h, (int)cx, (int)cy };
    cv_free(&c);
    return s;
}

/* ------------------------------------------------------------------------ */
/* Bonhommes de neige (20x26 px, ancre : centre x=10, haut du joueur y=2)   */

typedef struct PlayerPal { int hat, band, scarf, gun; } PlayerPal;
static const PlayerPal PLAYER_PALS[2] = { { R_BLUE, R_SKY, R_ORANGE, R_CYAN }, { R_RED, R_PINK, R_GREEN, R_PINK } };
static Sprite g_players[2][3];

static Sprite make_player(int index, int frame)
{
    const PlayerPal *p = &PLAYER_PALS[index];
    Canvas c = cv_new(20, 26);
    int brown = PAL(R_BROWN, 5);
    bool climb = frame > 0;
    int s = frame == 1 ? 2 : -2;

    /* bras (derriere le corps) */
    if (climb) {
        cv_line(&c, 5, 14, 2, 7 + s, brown);
        cv_line(&c, 14, 14, 17, 7 - s, brown);
    } else {
        cv_line(&c, 5, 14, 2, 11, brown);
        cv_line(&c, 15, 14, 17, 11, brown);
    }
    /* corps et tete */
    cv_sphere(&c, 10, 17, 7, R_GRAY, 8.5f, 15, PAL(R_GRAY, 6), false, 1e9f);
    if (!climb)
        for (int i = 0; i < 3; i++) cv_set(&c, 11, 14 + i * 3, PAL(R_GRAY, 1));
    /* canon a givre */
    if (!climb) {
        cv_rect(&c, 15, 6, 3, 7, PAL(p->gun, 9));
        cv_rect(&c, 15, 6, 1, 7, PAL(p->gun, 13));
        cv_set(&c, 16, 5, PAL(R_GRAY, 15));
    }
    cv_sphere(&c, 10, 8, 5, R_GRAY, 8.5f, 15, PAL(R_GRAY, 6), false, 1e9f);
    /* echarpe */
    cv_rect(&c, 5, 11, 11, 1, PAL(p->scarf, 11));
    cv_rect(&c, 5, 12, 11, 1, PAL(p->scarf, 7));
    cv_rect(&c, climb ? 9 : 6, 13, 2, 4, PAL(p->scarf, 9));
    /* visage */
    if (!climb) {
        cv_set(&c, 9, 8, PAL(R_GRAY, 0));
        cv_set(&c, 12, 8, PAL(R_GRAY, 0));
        cv_rect(&c, 11, 9, 4, 1, PAL(R_ORANGE, 10));
        cv_set(&c, 11, 10, PAL(R_ORANGE, 7));
    }
    /* bonnet : demi-sphere, bord, pompon */
    cv_sphere(&c, 10, 6.5f, 5.5f, p->hat, 4, 11, PAL(p->hat, 2), false, 5);
    cv_rect(&c, 4, 5, 12, 2, PAL(p->band, 11));
    cv_rect(&c, 4, 6, 12, 1, PAL(p->band, 8));
    cv_rect(&c, 9, 0, 3, 2, PAL(R_GRAY, 15));
    cv_set(&c, 10, 0, PAL(R_GRAY, 13));

    Sprite sp = { cv_texture(&c), c.w, c.h, 10, 2 };
    cv_free(&c);
    return sp;
}

/* ------------------------------------------------------------------------ */
/* Fruits (pixel art 12x12)                                                 */

const char *const FRUIT_NAMES[FRUIT_COUNT] = { "Cerises", "Fraise", "Pomme", "Poire",
                                               "Orange", "Banane", "Raisin", "Pastèque" };
const int FRUIT_POINTS[FRUIT_COUNT] = { 100, 200, 300, 500, 800, 1000, 1500, 2000 };
const int FRUIT_WEIGHTS[FRUIT_COUNT] = { 20, 16, 14, 10, 8, 6, 4, 2 };

static const char *const FRUIT_ART[FRUIT_COUNT][12] = {
    { "........gG..", ".......gG...", "......g.G...", ".....g...G..", "....g....G..", "..kkk...kkk.",
      ".krwrk.krwrk", ".krrrk.krrrk", ".kRrRk.kRrRk", "..kkk...kkk.", "............", "............" },
    { "...g.gg.g...", "....gGGg....", "..kkrrrrkk..", ".krryrrryrk.", ".krrrrrrrrk.", ".kryrrryrrk.",
      "..krrrrrrk..", "..kRryrRrk..", "...kRrrRk...", "....kRRk....", ".....kk.....", "............" },
    { ".....bgg....", ".....b......", "..kkkkkkkk..", ".krwrrrrrrk.", "krwrrrrrrrrk", "krrrrrrrrrrk",
      "krrrrrrrrrRk", "krrrrrrrrRRk", ".krrrrrrRRk.", ".kRrrrrRRRk.", "..kkRkkRkk..", "............" },
    { "......b.....", ".....kbk....", "....kllk....", "....klwlk...", "...kllllk...", "..kllllllk..",
      ".klwllllllk.", ".kllllllLlk.", ".klllllLLLk.", "..kLllLLLk..", "...kkkkkk...", "............" },
    { "......g.....", ".....gGg....", "...kkkkkk...", "..kowooook..", ".kowoooooOk.", ".koooooooOk.",
      ".koooooooOk.", ".kooooooOOk.", "..kOooOOOk..", "...kkkkkk...", "............", "............" },
    { "..........bk", ".........kyk", "........kyYk", ".......kyyYk", "......kyyYk.", "k...kkyyyYk.",
      "kykkyyyyYk..", ".kyyyyyYYk..", "..kYyyYYk...", "...kkYYk....", ".....kk.....", "............" },
    { ".....bg.....", ".....b.gg...", "...kkbkk....", "..kpwkpwk...", "..kppkppkk..", ".kkkkkkpwk..",
      "kpwkpwkppk..", "kPpkPpkkk...", ".kkkpwkpwk..", "...kPpkPpk..", "....kkkkk...", "............" },
    { "............", "............", "............", "kkkkkkkkkkkk", "krrrkrrkrrrk", "kwrrrrrrrrwk",
      "kgwrrkrrrwgk", ".kgwwrrwwgk.", "..kggwwggk..", "...kkggkk...", ".....kk.....", "............" },
};

static int fruit_color(char ch)
{
    switch (ch) {
    case 'k': return PAL(R_GRAY, 1);
    case 'w': return PAL(R_GRAY, 15);
    case 'r': return PAL(R_RED, 9);
    case 'R': return PAL(R_RED, 5);
    case 'g': return PAL(R_GREEN, 9);
    case 'G': return PAL(R_GREEN, 5);
    case 'y': return PAL(R_YELLOW, 12);
    case 'Y': return PAL(R_YELLOW, 8);
    case 'o': return PAL(R_ORANGE, 10);
    case 'O': return PAL(R_ORANGE, 6);
    case 'b': return PAL(R_BROWN, 5);
    case 'p': return PAL(R_PURPLE, 9);
    case 'P': return PAL(R_PURPLE, 5);
    case 'l': return PAL(R_LIME, 11);
    case 'L': return PAL(R_LIME, 7);
    case 'm': return PAL(R_PINK, 11);
    default: return -1;
    }
}

static SDL_Texture *g_fruits[FRUIT_COUNT];

static SDL_Texture *make_fruit(int f)
{
    Canvas c = cv_new(12, 12);
    for (int y = 0; y < 12; y++) {
        const char *row = FRUIT_ART[f][y];
        for (int x = 0; x < 12 && row[x]; x++) cv_set(&c, x, y, fruit_color(row[x]));
    }
    SDL_Texture *t = cv_texture(&c);
    cv_free(&c);
    return t;
}

/* ------------------------------------------------------------------------ */
/* Motifs : quadrillage des murs/sol, plateformes, echelles                 */

typedef struct WorldStyle {
    int sky[5];
    bool sun_is_moon;
    float sun_x, sun_y, sun_r;
    int sun_col, sun_glow;
    int clouds, cloud_hi, cloud_lo;
    bool stars, aurora;
    int mount_ramp;
    float mount_lo;
    int snow_lit, snow_shade;
    float snowline;
    int hill_lit, hill_shade, pine;
    bool frozen;
    int water[3];
    int glint;
    int ground_hi, ground_lo;
    int bush_ramp;
    float bush_lo, bush_hi;
    bool bush_snow;
    int reed, reed_top;
    bool flowers;
    int border_ramp, plat_ramp;
} WorldStyle;

static const WorldStyle WORLDS[BACKDROP_COUNT] = {
    /* 1 : lac de montagne, en plein jour */
    { { PAL(R_SKY, 6), PAL(R_SKY, 8), PAL(R_SKY, 10), PAL(R_SKY, 12), PAL(R_SKY, 13) },
      false, 325, 48, 11, PAL(R_YELLOW, 14), PAL(R_YELLOW, 12),
      6, PAL(R_GRAY, 15), PAL(R_GRAY, 12),
      false, false,
      R_BLUE, 6,
      PAL(R_GRAY, 15), PAL(R_SKY, 11), 112,
      PAL(R_GREEN, 7), PAL(R_GREEN, 4), PAL(R_GREEN, 2),
      false, { PAL(R_BLUE, 7), PAL(R_SKY, 8), PAL(R_SKY, 10) }, PAL(R_SKY, 14),
      PAL(R_GREEN, 8), PAL(R_GREEN, 6),
      R_GREEN, 3, 10, false,
      PAL(R_LIME, 8), PAL(R_BROWN, 5), true,
      R_GRAY, R_SAND },
    /* 2 : foret enneigee au crepuscule, lac gele */
    { { PAL(R_PURPLE, 4), PAL(R_PURPLE, 6), PAL(R_MAGENTA, 8), PAL(R_PINK, 11), PAL(R_ORANGE, 12) },
      false, 92, 178, 16, PAL(R_ORANGE, 13), PAL(R_ORANGE, 11),
      4, PAL(R_PINK, 12), PAL(R_PURPLE, 8),
      false, false,
      R_PURPLE, 5,
      PAL(R_PINK, 13), PAL(R_PURPLE, 10), 140,
      PAL(R_GRAY, 13), PAL(R_INDIGO, 8), PAL(R_TEAL, 2),
      true, { PAL(R_SKY, 10), PAL(R_SKY, 11), PAL(R_SKY, 12) }, PAL(R_GRAY, 15),
      PAL(R_GRAY, 14), PAL(R_SKY, 11),
      R_TEAL, 2, 7, true,
      PAL(R_BROWN, 6), PAL(R_BROWN, 4), false,
      R_BROWN, R_PURPLE },
    /* 3 : nuit boreale */
    { { PAL(R_INDIGO, 1), PAL(R_INDIGO, 2), PAL(R_BLUE, 3), PAL(R_BLUE, 4), PAL(R_TEAL, 4) },
      true, 300, 46, 10, PAL(R_SAND, 14), PAL(R_SAND, 12),
      0, 0, 0,
      true, true,
      R_INDIGO, 3,
      PAL(R_SKY, 8), PAL(R_INDIGO, 6), 120,
      PAL(R_TEAL, 3), PAL(R_TEAL, 2), PAL(R_TEAL, 1),
      false, { PAL(R_BLUE, 2), PAL(R_BLUE, 3), PAL(R_BLUE, 4) }, PAL(R_SAND, 12),
      PAL(R_INDIGO, 9), PAL(R_SKY, 7),
      R_TEAL, 1, 5, true,
      PAL(R_TEAL, 4), PAL(R_TEAL, 2), false,
      R_INDIGO, R_TEAL },
};

static SDL_Texture *g_border_tile[BACKDROP_COUNT];
static SDL_Texture *g_plat_tile[BACKDROP_COUNT];
static SDL_Texture *g_ladder_tile;
static SDL_Texture *g_backdrops[BACKDROP_COUNT];

/* Carreau 8x8 en relief : le fameux quadrillage */
static void draw_grid_tile(Canvas *c, int ox, int oy, int w, int h, int ramp, int mid)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int col = PAL(ramp, mid - ((x + y) % 4 == 0 ? 1 : 0));
            if (y == 0 || x == 0) col = PAL(ramp, mid + 3);
            if (y == h - 1 || x == w - 1) col = PAL(ramp, mid - 4);
            if (x == w - 1 && y == 0) col = PAL(ramp, mid);
            cv_set(c, ox + x, oy + y, col);
        }
}

static SDL_Texture *make_border_tile(int world)
{
    Canvas c = cv_new(8, 8);
    draw_grid_tile(&c, 0, 0, 8, 8, WORLDS[world].border_ramp, 7);
    SDL_Texture *t = cv_texture(&c);
    cv_free(&c);
    return t;
}

static SDL_Texture *make_plat_tile(int world)
{
    Canvas c = cv_new(8, 7);
    int ramp = WORLDS[world].plat_ramp;
    draw_grid_tile(&c, 0, 0, 8, 7, ramp, 9);
    for (int x = 0; x < 8; x++) cv_set(&c, x, 0, PAL(ramp, 14));   /* arete brillante */
    SDL_Texture *t = cv_texture(&c);
    cv_free(&c);
    return t;
}

static SDL_Texture *make_ladder_tile(void)
{
    Canvas c = cv_new(15, 6);
    for (int y = 0; y < 6; y++) {
        cv_set(&c, 1, y, PAL(R_BROWN, 10));
        cv_set(&c, 2, y, PAL(R_BROWN, 6));
        cv_set(&c, 12, y, PAL(R_BROWN, 10));
        cv_set(&c, 13, y, PAL(R_BROWN, 6));
    }
    for (int x = 3; x < 12; x++) {
        cv_set(&c, x, 2, PAL(R_BROWN, 11));
        cv_set(&c, x, 3, PAL(R_BROWN, 6));
    }
    SDL_Texture *t = cv_texture(&c);
    cv_free(&c);
    return t;
}

/* ------------------------------------------------------------------------ */
/* Decors (400x270)                                                         */

#define HORIZON 186
#define LAKE_END 232
#define GROUND_Y 250       /* FLOOR_Y / 2 */
#define BAND_H 18          /* CEIL_Y / 2 */

/* Ligne de crete par deplacement du point milieu */
static void ridge(float *h, int n, float base, float amp, float rough)
{
    int step = 64;
    for (int x = 0; x < n; x += step) h[x] = base - rnd() * amp;
    h[n - 1] = base - rnd() * amp;
    for (; step > 1; step /= 2) {
        for (int x = step / 2; x < n; x += step) {
            int a = x - step / 2, b = x + step / 2 < n ? x + step / 2 : n - 1;
            h[x] = (h[a] + h[b]) / 2 + (rnd() - 0.5f) * amp * rough * step / 64.0f;
        }
    }
}

static bool is_sky(int v, const WorldStyle *w)
{
    for (int i = 0; i < 5; i++)
        if (v == w->sky[i]) return true;
    return v == CLEAR;
}

static void tile_area(Canvas *c, int x0, int y0, int x1, int y1, int ramp, int mid)
{
    for (int y = y0; y < y1; y += 8)
        for (int x = x0; x < x1; x += 8) {
            int w = x + 8 <= x1 ? 8 : x1 - x, h = y + 8 <= y1 ? 8 : y1 - y;
            draw_grid_tile(c, x, y, w, h, ramp, mid);
        }
}

static void draw_pine(Canvas *c, int x, int base, int height, int col, int snow)
{
    for (int y = 0; y < height; y++) {
        float t = (float)y / height;
        int half = (int)(t * height * 0.32f) + 1;
        /* etages en dents de scie */
        if (y % 5 == 4) half = (int)(half * 0.7f);
        for (int i = -half; i <= half; i++) {
            int col2 = col;
            if (snow >= 0 && (y % 5 == 0 || (i < 0 && y % 5 == 1))) col2 = snow;
            cv_set(c, x + i, base - height + y, col2);
        }
    }
    cv_rect(c, x, base, 1, 2, PAL(R_BROWN, 3));
}

/* Buisson : touffes de feuilles en pixels, eclairees en haut a gauche */
static void cv_bush(Canvas *c, float bx, float by, float br, int ramp, float lo, float hi)
{
    for (int k = 0; k < 5; k++) {
        float ex = bx + (k - 2) * br * 0.45f, ey = by + fabsf(k - 2.0f) * br * 0.18f;
        float r = br * (k == 2 ? 1.0f : 0.7f);
        for (int y = (int)(ey - r); y <= (int)(ey + r); y++) {
            if (y >= GROUND_Y) break;
            for (int x = (int)(ex - r * 1.2f); x <= (int)(ex + r * 1.2f); x++) {
                float dx = (x + 0.5f - ex) / (r * 1.2f), dy = (y + 0.5f - ey) / r;
                float d2 = dx * dx + dy * dy;
                if (d2 > 1) continue;
                float light = 0.5f - 0.5f * (dx * 0.6f + dy * 0.8f);   /* 0..1 */
                float v = lo + (hi - lo) * light + (rnd() - 0.5f) * 3;
                int col = PAL(ramp, dlevel(v, x, y));
                if (d2 > 0.8f && rnd() < 0.5f) col = PAL(ramp, (int)lo > 1 ? (int)lo - 1 : 1);
                cv_set(c, x, y, col);
            }
        }
    }
}

static SDL_Texture *make_backdrop(int world)
{
    const WorldStyle *w = &WORLDS[world];
    Canvas c = cv_new(RETRO_W, RETRO_H);
    g_seed = 0x5EED0000u + (Uint32)world * 7919u;

    /* ciel */
    for (int y = BAND_H; y < HORIZON; y++)
        for (int x = 0; x < RETRO_W; x++)
            cv_set(&c, x, y, dgrad(w->sky, 5, (float)(y - BAND_H) / (HORIZON - BAND_H), x, y));

    if (w->stars)
        for (int i = 0; i < 160; i++) {
            int x = (int)(rnd() * RETRO_W), y = BAND_H + (int)(rnd() * 120);
            cv_set(&c, x, y, rnd() < 0.3f ? PAL(R_GRAY, 15) : PAL(R_GRAY, 11));
        }

    if (w->aurora) {
        for (int x = 0; x < RETRO_W; x++) {
            float yc = 72 + sinf(x / 41.0f) * 15 + sinf(x / 17.0f) * 5;
            float pulse = 0.55f + 0.45f * sinf(x * 0.21f) * sinf(x * 0.05f);
            for (int y = (int)yc - 26; y < (int)yc + 8; y++) {
                float a = 1 - fabsf((float)y - yc) / (y < yc ? 26.0f : 8.0f);
                a *= pulse;
                if (a > bayer(x, y)) cv_set(&c, x, y, a > 0.6f ? PAL(R_GREEN, 11) : PAL(R_TEAL, 8));
            }
        }
    }

    /* soleil ou lune */
    cv_ellipse(&c, w->sun_x, w->sun_y, w->sun_r + 4, w->sun_r + 4, -1);
    for (int y = (int)(w->sun_y - w->sun_r - 6); y < w->sun_y + w->sun_r + 6; y++)
        for (int x = (int)(w->sun_x - w->sun_r - 6); x < w->sun_x + w->sun_r + 6; x++) {
            float d = hypotf(x + 0.5f - w->sun_x, y + 0.5f - w->sun_y);
            if (d < w->sun_r) cv_set(&c, x, y, w->sun_col);
            else if (d < w->sun_r + 5 && (d - w->sun_r) / 5 < bayer(x, y)) cv_set(&c, x, y, w->sun_glow);
        }
    if (w->sun_is_moon) {
        cv_ellipse(&c, w->sun_x - 3, w->sun_y - 2, 2.5f, 2.5f, PAL(R_SAND, 11));
        cv_ellipse(&c, w->sun_x + 3, w->sun_y + 3, 1.5f, 1.5f, PAL(R_SAND, 11));
    }

    /* nuages : grappes d'ellipses, dessous ombre */
    for (int k = 0; k < w->clouds; k++) {
        float cx = 30 + rnd() * 340, cy = 40 + rnd() * 70;
        int parts = 4 + (int)(rnd() * 4);
        for (int i = 0; i < parts; i++) {
            float ex = cx + (rnd() - 0.5f) * 40, ey = cy + (rnd() - 0.5f) * 8;
            float rx = 7 + rnd() * 10, ry = 4 + rnd() * 5;
            for (int y = (int)(ey - ry); y <= (int)(ey + ry); y++)
                for (int x = (int)(ex - rx); x <= (int)(ex + rx); x++) {
                    float dx = (x + 0.5f - ex) / rx, dy = (y + 0.5f - ey) / ry;
                    if (dx * dx + dy * dy > 1) continue;
                    float shade = (y - (cy - 4)) / 8.0f;
                    cv_set(&c, x, y, shade > bayer(x, y) ? w->cloud_lo : w->cloud_hi);
                }
        }
    }

    /* montagnes du fond, avec neige : faces eclairees a gauche, ombre a droite */
    float hb[RETRO_W], hf[RETRO_W], hm[RETRO_W];
    ridge(hb, RETRO_W, 150, 80, 0.62f);
    for (int x = 0; x < RETRO_W; x++) {
        int top = (int)hb[x];
        int a = x - 2 < 0 ? 0 : x - 2, b = x + 2 >= RETRO_W ? RETRO_W - 1 : x + 2;
        float slope = hb[b] - hb[a];            /* < 0 : la pente monte vers la droite */
        bool lit = slope < 0;
        float snow = w->snowline + sinf(x * 0.21f) * 4 + sinf(x * 0.67f) * 2;
        for (int y = top; y < HORIZON; y++) {
            int col;
            if (y < snow && y - top < 34) col = lit ? w->snow_lit : w->snow_shade;
            else {
                float v = w->mount_lo + (lit ? 2.2f : 0) + (float)(y - top) / (HORIZON - top) * 2.5f;
                col = PAL(w->mount_ramp, dlevel(v, x, y));
            }
            cv_set(&c, x, y, col);
        }
    }
    /* chaine plus proche, plus sombre */
    ridge(hm, RETRO_W, 172, 34, 0.55f);
    for (int x = 0; x < RETRO_W; x++) {
        int a = x - 2 < 0 ? 0 : x - 2, b = x + 2 >= RETRO_W ? RETRO_W - 1 : x + 2;
        bool lit = hm[b] - hm[a] < 0;
        for (int y = (int)hm[x]; y < HORIZON; y++) {
            float v = w->mount_lo - 1.5f + (lit ? 1.5f : 0) + (float)(y - hm[x]) / 30.0f * 1.5f;
            cv_set(&c, x, y, PAL(w->mount_ramp, dlevel(v, x, y)));
        }
    }

    /* collines et sapins du rivage */
    ridge(hf, RETRO_W, 178, 16, 0.35f);
    for (int x = 0; x < RETRO_W; x++)
        for (int y = (int)hf[x]; y < HORIZON; y++) {
            int stops[2] = { w->hill_lit, w->hill_shade };
            cv_set(&c, x, y, dgrad(stops, 2, (y - hf[x]) / 7.0f, x, y));
        }
    for (int i = 0; i < 46; i++) {
        int x = (int)(rnd() * RETRO_W);
        int base = (int)hf[x < RETRO_W ? x : RETRO_W - 1] + 4;
        if (base > HORIZON) base = HORIZON;
        draw_pine(&c, x, base, 8 + (int)(rnd() * 9), w->pine, w->frozen ? w->snow_lit : -1);
    }
    cv_rect(&c, 0, HORIZON, RETRO_W, 1, w->hill_shade);

    /* lac : reflet du paysage (tasse verticalement), ondulations, reflets de lumiere */
    for (int y = HORIZON + 1; y < LAKE_END; y++) {
        float t = (float)(y - HORIZON) / (LAKE_END - HORIZON);
        int wave = w->frozen ? 0 : (int)roundf(sinf(y * 1.3f) * 1.5f);
        for (int x = 0; x < RETRO_W; x++) {
            int sx = x + wave;
            int sy = HORIZON - (y - HORIZON) * 2;
            int src = cv_get(&c, sx < 0 ? 0 : (sx >= RETRO_W ? RETRO_W - 1 : sx), sy < BAND_H ? BAND_H : sy);
            int col = dgrad(w->water, 3, t, x, y);
            if (!is_sky(src, w) && src != CLEAR) {
                int ramp = src >> 4, lvl = (src & 15) - 2;
                /* le reflet s'estompe vers le bord du lac (tramage) */
                float keep = w->frozen ? 0.5f : 1.0f - t * 0.8f;
                if (keep > bayer(x, y)) col = PAL(ramp, lvl < 1 ? 1 : lvl);
            }
            cv_set(&c, x, y, col);
        }
    }
    if (w->frozen) {
        for (int k = 0; k < 14; k++) {
            int x = (int)(rnd() * RETRO_W), y = HORIZON + 2 + (int)(rnd() * (LAKE_END - HORIZON - 4));
            for (int s = 0; s < 18; s++) {
                cv_set(&c, x, y, rnd() < 0.5f ? w->glint : PAL(R_INDIGO, 7));
                x += rnd() < 0.5f ? 1 : -1;
                y += rnd() < 0.3f ? (rnd() < 0.5f ? 1 : -1) : 0;
                if (y <= HORIZON || y >= LAKE_END) break;
            }
        }
    } else {
        for (int k = 0; k < 70; k++) {
            int x = (int)(rnd() * RETRO_W), y = HORIZON + 2 + (int)(rnd() * (LAKE_END - HORIZON - 3));
            if (w->sun_is_moon) x = (int)(w->sun_x - 8 + rnd() * 16);
            int len = 2 + (int)(rnd() * 6);
            cv_rect(&c, x, y, len, 1, w->glint);
        }
    }

    /* rive : sol, buissons, roseaux, fleurs, grands sapins sur les cotes */
    for (int y = LAKE_END; y < GROUND_Y; y++)
        for (int x = 0; x < RETRO_W; x++)
            cv_set(&c, x, y, dgrad((int[]){ w->ground_hi, w->ground_lo }, 2, (float)(y - LAKE_END) / 18, x, y));
    for (int x = 0; x < RETRO_W; x++)
        if (rnd() < 0.5f) cv_set(&c, x, LAKE_END - 1, w->ground_hi);

    draw_pine(&c, 16, GROUND_Y - 2, 95, w->pine, w->frozen ? w->snow_lit : -1);
    draw_pine(&c, 386, GROUND_Y - 2, 80, w->pine, w->frozen ? w->snow_lit : -1);

    for (int i = 0; i < 22; i++) {
        float bx = 20 + rnd() * 360, br = 4 + rnd() * 6;
        float by = GROUND_Y - br * 0.6f;
        cv_bush(&c, bx, by, br, w->bush_ramp, w->bush_lo, w->bush_hi);
        if (w->bush_snow) cv_sphere(&c, bx - 1, by - br * 0.45f, br * 0.6f, R_GRAY, 12, 15, -1, false, by - br * 0.2f);
        if (w->flowers)
            for (int f = 0; f < 4; f++) {
                static const int petals[3] = { PAL(R_PINK, 12), PAL(R_YELLOW, 13), PAL(R_GRAY, 15) };
                cv_set(&c, (int)(bx + (rnd() - 0.5f) * br * 1.4f), (int)(by + (rnd() - 0.6f) * br),
                       petals[(int)(rnd() * 3) % 3]);
            }
    }
    for (int i = 0; i < 40; i++) {
        int x = 10 + (int)(rnd() * 380), hgt = 6 + (int)(rnd() * 14);
        int lean = rnd() < 0.5f ? -1 : 1;
        for (int y = 0; y < hgt; y++) cv_set(&c, x + (y > hgt * 0.6f ? lean : 0), GROUND_Y - 1 - y, w->reed);
        if (rnd() < 0.6f) cv_rect(&c, x + lean, GROUND_Y - hgt - 2, 2, 3, w->reed_top);
    }

    /* cadre en quadrillage : bandeau du haut, murs, sol */
    tile_area(&c, 0, 0, RETRO_W, BAND_H, w->border_ramp, 4);
    tile_area(&c, 0, GROUND_Y, RETRO_W, RETRO_H, w->border_ramp, 7);
    tile_area(&c, 0, BAND_H, WALL_W / 2, GROUND_Y, w->border_ramp, 7);
    tile_area(&c, RETRO_W - WALL_W / 2, BAND_H, RETRO_W, GROUND_Y, w->border_ramp, 7);

    SDL_Texture *t = cv_texture(&c);
    cv_free(&c);
    return t;
}

/* ------------------------------------------------------------------------ */

static SDL_Texture *g_target;

bool retro_init(SDL_Renderer *r)
{
    g_r = r;
    build_palette();
    for (int f = 0; f < 2; f++)
        for (int s = 0; s < BALL_SIZES; s++) g_balls[f][s] = make_ball(s, f == 1);
    for (int p = 0; p < 2; p++)
        for (int fr = 0; fr < 3; fr++) g_players[p][fr] = make_player(p, fr);
    for (int f = 0; f < FRUIT_COUNT; f++) g_fruits[f] = make_fruit(f);
    for (int w = 0; w < BACKDROP_COUNT; w++) {
        g_border_tile[w] = make_border_tile(w);
        g_plat_tile[w] = make_plat_tile(w);
        g_backdrops[w] = make_backdrop(w);
    }
    g_ladder_tile = make_ladder_tile();
    g_target = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, RETRO_W, RETRO_H);
    if (g_target) SDL_SetTextureScaleMode(g_target, SDL_SCALEMODE_NEAREST);
    return g_target != NULL;
}

void retro_quit(void)
{
    /* les textures sont liberees avec le renderer */
}

SDL_Texture *retro_target(void) { return g_target; }

static float snap2(float v) { return floorf(v * 0.5f + 0.5f) * 2.0f; }

void retro_blit(SDL_Texture *t, float x, float y, bool flip_x)
{
    if (!t) return;
    float ox, oy, w, h;
    gfx_get_offset(&ox, &oy);
    SDL_GetTextureSize(t, &w, &h);
    SDL_FRect dst = { snap2(x + ox), snap2(y + oy), w * RETRO_SCALE, h * RETRO_SCALE };
    SDL_SetTextureAlphaModFloat(t, gfx_get_alpha());
    SDL_RenderTextureRotated(g_r, t, NULL, &dst, 0, NULL, flip_x ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void retro_tiled(SDL_Texture *t, float x, float y, float w, float h)
{
    if (!t || w <= 0 || h <= 0) return;
    float ox, oy;
    gfx_get_offset(&ox, &oy);
    SDL_FRect dst = { snap2(x + ox), snap2(y + oy), snap2(w), snap2(h) };
    SDL_SetTextureAlphaModFloat(t, gfx_get_alpha());
    SDL_RenderTextureTiled(g_r, t, NULL, RETRO_SCALE, &dst);
}

static int clamp_world(int world) { return world < 1 ? 0 : (world > BACKDROP_COUNT ? BACKDROP_COUNT - 1 : world - 1); }

void retro_draw_backdrop(int world)
{
    SDL_Texture *t = g_backdrops[clamp_world(world)];
    float ox, oy;
    gfx_get_offset(&ox, &oy);
    SDL_FRect dst = { ox, oy, GAME_W, GAME_H };
    SDL_SetTextureAlphaModFloat(t, 1);
    SDL_RenderTexture(g_r, t, NULL, &dst);
}

void retro_draw_platform(int world, float x, float y, float w)
{
    retro_tiled(g_plat_tile[clamp_world(world)], x, y, w, PLAT_H);
}

void retro_draw_ladder(float x, float top, float bottom) { retro_tiled(g_ladder_tile, x, top - 6, LADDER_W, bottom - top + 6); }

void retro_draw_ball(float cx, float cy, int size, bool frozen)
{
    const Sprite *s = &g_balls[frozen ? 1 : 0][size < 0 ? 0 : (size > 3 ? 3 : size)];
    retro_blit(s->tex, cx - s->ax * RETRO_SCALE, cy - s->ay * RETRO_SCALE, false);
}

void retro_draw_player(float cx, float top, int index, int frame, int face)
{
    const Sprite *s = &g_players[index & 1][frame < 0 ? 0 : (frame > 2 ? 2 : frame)];
    /* miroir : l'ancre x=10 reste au centre (largeur 20) */
    retro_blit(s->tex, cx - s->ax * RETRO_SCALE, top - s->ay * RETRO_SCALE, face < 0);
}

void retro_draw_fruit(int fruit, float x, float y)
{
    retro_blit(g_fruits[fruit < 0 ? 0 : fruit % FRUIT_COUNT], x, y, false);
}

