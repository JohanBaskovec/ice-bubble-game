#include "font.h"
#include "embedded.h"

#include <math.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wshadow"
#endif
#include "stb_truetype.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#define ATLAS_W 2048
#define ATLAS_H 1024
#define OVERSAMPLE 2

/* Plages de caracteres : ASCII, Latin-1, quelques signes typographiques. */
typedef struct Range { int first, count; } Range;
static const Range RANGES[] = {
    { 0x20, 0x7F - 0x20 },     /* ASCII imprimable */
    { 0xA0, 0x100 - 0xA0 },    /* Latin-1 (accents francais, « », etc.) */
    { 0x152, 2 },              /* OE oe */
    { 0x178, 1 },              /* Y trema */
    { 0x2013, 2 },             /* tirets demi-cadratin / cadratin */
    { 0x2018, 4 },             /* guillemets simples/doubles courbes */
    { 0x2026, 1 },             /* points de suspension */
    { 0x20AC, 1 },             /* euro */
    { 0x2212, 1 },             /* signe moins */
};
#define RANGE_COUNT ((int)(sizeof(RANGES) / sizeof(RANGES[0])))

/* Plusieurs tailles de rasterisation : on choisit la plus proche au-dessus. */
static const float BAKE_SIZES[] = { 18.0f, 36.0f, 72.0f };
#define BAKE_COUNT 3

static stbtt_packedchar g_chars[BAKE_COUNT][RANGE_COUNT][256];
static SDL_Texture *g_tex;
static SDL_Renderer *g_r;
static float g_ascent_ratio, g_descent_ratio;   /* par pixel de taille */

Uint32 utf8_next(const char **ps)
{
    const unsigned char *s = (const unsigned char *)*ps;
    Uint32 c = s[0];
    if (c == 0) return 0;
    int extra = 0;
    if (c < 0x80) extra = 0;
    else if ((c & 0xE0) == 0xC0) { c &= 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { c &= 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { c &= 0x07; extra = 3; }
    else { *ps += 1; return '?'; }
    for (int i = 1; i <= extra; i++) {
        if ((s[i] & 0xC0) != 0x80) { *ps += i; return '?'; }
        c = (c << 6) | (s[i] & 0x3F);
    }
    *ps += 1 + extra;
    return c;
}

bool font_init(SDL_Renderer *r)
{
    g_r = r;
    unsigned char *pixels = SDL_calloc(ATLAS_W, ATLAS_H);
    if (!pixels) return false;

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, g_font_data, 0)) {
        SDL_Log("font: police invalide");
        SDL_free(pixels);
        return false;
    }
    int asc, desc, gap;
    stbtt_GetFontVMetrics(&info, &asc, &desc, &gap);
    float unit = stbtt_ScaleForPixelHeight(&info, 1.0f);
    g_ascent_ratio = asc * unit;
    g_descent_ratio = -desc * unit;

    stbtt_pack_context pc;
    if (!stbtt_PackBegin(&pc, pixels, ATLAS_W, ATLAS_H, 0, 1, NULL)) {
        SDL_free(pixels);
        return false;
    }
    bool ok = true;
    for (int b = 0; b < BAKE_COUNT; b++) {
        /* la grande taille n'a pas besoin de sur-echantillonnage */
        int os = BAKE_SIZES[b] >= 64 ? 1 : OVERSAMPLE;
        stbtt_PackSetOversampling(&pc, os, os);
        stbtt_pack_range pr[RANGE_COUNT];
        for (int i = 0; i < RANGE_COUNT; i++) {
            pr[i].font_size = BAKE_SIZES[b];
            pr[i].first_unicode_codepoint_in_range = RANGES[i].first;
            pr[i].array_of_unicode_codepoints = NULL;
            pr[i].num_chars = RANGES[i].count;
            pr[i].chardata_for_range = g_chars[b][i];
        }
        if (!stbtt_PackFontRanges(&pc, g_font_data, 0, pr, RANGE_COUNT)) ok = false;
    }
    stbtt_PackEnd(&pc);
    if (!ok) SDL_Log("font: atlas trop petit, certains glyphes manquent");

    /* atlas alpha -> RGBA blanc */
    Uint32 *rgba = SDL_malloc((size_t)ATLAS_W * ATLAS_H * 4);
    if (!rgba) {
        SDL_free(pixels);
        return false;
    }
    for (int i = 0; i < ATLAS_W * ATLAS_H; i++) {
        Uint8 a = pixels[i];
        Uint8 *px = (Uint8 *)&rgba[i];
        px[0] = px[1] = px[2] = 255;
        px[3] = a;
    }
    SDL_free(pixels);

    g_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, ATLAS_W, ATLAS_H);
    if (!g_tex) {
        SDL_Log("font: %s", SDL_GetError());
        SDL_free(rgba);
        return false;
    }
    SDL_UpdateTexture(g_tex, NULL, rgba, ATLAS_W * 4);
    SDL_SetTextureBlendMode(g_tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(g_tex, SDL_SCALEMODE_LINEAR);
    SDL_free(rgba);
    return true;
}

void font_quit(void)
{
    if (g_tex) SDL_DestroyTexture(g_tex);
    g_tex = NULL;
}

static int pick_bake(float size)
{
    /* taille reelle en pixels ecran = taille logique * echelle de rendu */
    float sx = 1, sy = 1;
    SDL_GetRenderScale(g_r, &sx, &sy);
    int w = 0, h = 0;
    SDL_GetCurrentRenderOutputSize(g_r, &w, &h);
    int lw = 0, lh = 0;
    SDL_RendererLogicalPresentation mode;
    SDL_GetRenderLogicalPresentation(g_r, &lw, &lh, &mode);
    float scale = (lh > 0 && lw > 0) ? fminf((float)w / lw, (float)h / lh) : 1.0f;
    float px = size * scale;
    for (int b = 0; b < BAKE_COUNT; b++)
        if (BAKE_SIZES[b] >= px * 0.9f) return b;
    return BAKE_COUNT - 1;
}

static const stbtt_packedchar *glyph(int bake, Uint32 cp, int *range_out, int *index_out)
{
    for (int i = 0; i < RANGE_COUNT; i++) {
        if ((int)cp >= RANGES[i].first && (int)cp < RANGES[i].first + RANGES[i].count) {
            *range_out = i;
            *index_out = (int)cp - RANGES[i].first;
            return &g_chars[bake][i][*index_out];
        }
    }
    return NULL;
}

float font_measure(const char *text, float size)
{
    int bake = BAKE_COUNT - 1;   /* metrique independante de l'echelle */
    float scale = size / BAKE_SIZES[bake];
    float w = 0;
    const char *s = text;
    Uint32 cp;
    while ((cp = utf8_next(&s)) != 0) {
        int ri, ii;
        const stbtt_packedchar *g = glyph(bake, cp, &ri, &ii);
        if (!g) g = glyph(bake, '?', &ri, &ii);
        if (g) w += g->xadvance * scale;
    }
    return w;
}

void font_draw(const char *text, float x, float y, float size, Color c, TextAlign align)
{
    if (!g_tex || !text || !*text) return;
    float w = font_measure(text, size);
    if (align == ALIGN_CENTER) x -= w / 2;
    else if (align == ALIGN_RIGHT) x -= w;

    int bake = pick_bake(size);
    float scale = size / BAKE_SIZES[bake];
    float ox, oy;
    gfx_get_offset(&ox, &oy);
    c.a *= gfx_get_alpha();

    /* ligne de base : centre vertical entre hauteur des capitales et base */
    (void)g_descent_ratio;
    float baseline = y + size * g_ascent_ratio * 0.36f;

    SDL_Vertex v[4 * 128];
    int idx[6 * 128];
    int nq = 0;
    float pen = 0;
    const char *s = text;
    Uint32 cp;
    while ((cp = utf8_next(&s)) != 0) {
        int ri, ii;
        const stbtt_packedchar *g = glyph(bake, cp, &ri, &ii);
        if (!g) g = glyph(bake, '?', &ri, &ii);
        if (!g) continue;
        if (cp != ' ') {
            stbtt_aligned_quad q;
            float qx = 0, qy = 0;
            stbtt_GetPackedQuad(g_chars[bake][ri], ATLAS_W, ATLAS_H, ii, &qx, &qy, &q, 0);
            float x0 = x + pen + q.x0 * scale + ox, x1 = x + pen + q.x1 * scale + ox;
            float y0 = baseline + q.y0 * scale + oy, y1 = baseline + q.y1 * scale + oy;
            SDL_Vertex *vv = &v[nq * 4];
            float px[4] = { x0, x1, x1, x0 }, py[4] = { y0, y0, y1, y1 };
            float tu[4] = { q.s0, q.s1, q.s1, q.s0 }, tv[4] = { q.t0, q.t0, q.t1, q.t1 };
            for (int k = 0; k < 4; k++) {
                vv[k].position.x = px[k];
                vv[k].position.y = py[k];
                vv[k].color = c;
                vv[k].tex_coord.x = tu[k];
                vv[k].tex_coord.y = tv[k];
            }
            int base = nq * 4;
            int *ix = &idx[nq * 6];
            ix[0] = base; ix[1] = base + 1; ix[2] = base + 2;
            ix[3] = base; ix[4] = base + 2; ix[5] = base + 3;
            nq++;
            if (nq == 128) {
                SDL_RenderGeometry(g_r, g_tex, v, nq * 4, idx, nq * 6);
                nq = 0;
            }
        }
        pen += g->xadvance * scale;
    }
    if (nq) SDL_RenderGeometry(g_r, g_tex, v, nq * 4, idx, nq * 6);
}

void font_draw_outlined(const char *text, float x, float y, float size, Color c, TextAlign align)
{
    Color o = { 5 / 255.0f, 15 / 255.0f, 40 / 255.0f, 0.85f * c.a };
    float d = size / 14.0f;
    if (d < 1.2f) d = 1.2f;
    static const float dirs[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
                                      { 0.7f, 0.7f }, { -0.7f, 0.7f }, { 0.7f, -0.7f }, { -0.7f, -0.7f } };
    for (int i = 0; i < 8; i++) font_draw(text, x + dirs[i][0] * d, y + dirs[i][1] * d, size, o, align);
    font_draw(text, x, y, size, c, align);
}
