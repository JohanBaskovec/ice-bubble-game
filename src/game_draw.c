#include "config.h"
#include "font.h"
#include "game.h"
#include "gfx.h"
#include "retro.h"
#include "settings.h"

#include <math.h>
#include <stdio.h>

#define PI_F 3.14159265f

/* Decor retro courant (fixe par draw_backdrop, utilise par les plateformes) */
static int g_world = 1;

static bool retro(void) { return g_settings.retro; }

/* ------------------------------------------------------------------------ */
/* Decor                                                                    */

static Uint32 mulberry(Uint32 *s)
{
    Uint32 t = (*s += 0x6D2B79F5u);
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return t ^ (t >> 14);
}
static float rnd01(Uint32 *s) { return (float)(mulberry(s) / 4294967296.0); }

void backdrop_build(Backdrop *bg, int hue, int seed)
{
    Uint32 s = (Uint32)seed;
    bg->hue = hue;
    for (int i = 0; i < MAX_STARS; i++) {
        bg->stars[i][0] = rnd01(&s) * GAME_W;
        bg->stars[i][1] = CEIL_Y + rnd01(&s) * 260;
        bg->stars[i][2] = rnd01(&s) * 1.6f + 0.5f;
        bg->stars[i][3] = rnd01(&s) * 0.7f + 0.2f;
    }
    for (int k = 0; k < 3; k++) {
        bg->aurora[k][0] = 90 + k * 28 + rnd01(&s) * 30;
        bg->aurora[k][1] = rnd01(&s);
    }
    bg->moon_x = 100 + rnd01(&s) * 600;
    const float amp[2] = { 170, 110 };
    for (int li = 0; li < 2; li++) {
        int n = 0;
        float x = 0, y = FLOOR_Y - amp[li] * rnd01(&s);
        while (x < GAME_W + 60 && n < 32) {
            bg->mountain[li][n][0] = x;
            bg->mountain[li][n][1] = y;
            n++;
            x += 40 + rnd01(&s) * 70;
            y = FLOOR_Y - 40 - rnd01(&s) * amp[li] - (li ? 0 : 60);
        }
        bg->mountain_n[li] = n;
    }
}

void draw_backdrop(const Backdrop *bg, double t)
{
    g_world = bg->world < 1 ? 1 : bg->world;
    if (retro()) {
        retro_draw_backdrop(g_world);
        return;
    }
    float h = (float)bg->hue;
    Color c0 = hsl(h, 0.55f, 0.09f, 1.0f), c1 = hsl(h + 15, 0.48f, 0.20f, 1.0f), c2 = hsl(h + 30, 0.42f, 0.34f, 1.0f);
    gfx_rect_vgrad(0, 0, GAME_W, FLOOR_Y * 0.6f, c0, c1);
    gfx_rect_vgrad(0, FLOOR_Y * 0.6f, GAME_W, FLOOR_Y * 0.4f + 2, c1, c2);

    for (int i = 0; i < MAX_STARS; i++) {
        float tw = 0.75f + 0.25f * sinf((float)t * 1.7f + i * 1.3f);
        gfx_rect(bg->stars[i][0], bg->stars[i][1], bg->stars[i][2], bg->stars[i][2],
                 rgba(0xFFFFFF, bg->stars[i][3] * tw));
    }

    /* aurores boreales : bandes ondulantes */
    for (int k = 0; k < 3; k++) {
        Color c = hsl(140.0f + k * 45, 0.85f, 0.62f, 0.09f);
        float y0 = bg->aurora[k][0], ph = bg->aurora[k][1] + (float)t * 0.05f * (k + 1);
        float px = 0, py = y0 + sinf(k * 2 + ph) * 22;
        for (int x = 10; x <= GAME_W; x += 10) {
            float y = y0 + sinf(x / 90.0f + k * 2 + ph) * 22;
            SDL_FPoint q[4] = { { px, py - 13 }, { (float)x, y - 13 }, { (float)x, y + 13 }, { px, py + 13 } };
            gfx_quad(q, c);
            px = (float)x;
            py = y;
        }
    }

    /* lune */
    for (int i = 4; i >= 1; i--) gfx_circle(bg->moon_x, 95, 22 + i * 6.0f, rgba(0xFFF8C0, 0.035f));
    gfx_circle(bg->moon_x, 95, 22, rgb(0xF3F7E0));
    gfx_circle(bg->moon_x - 7, 90, 5, rgba(0xB4B496, 0.35f));
    gfx_circle(bg->moon_x + 6, 102, 3.5f, rgba(0xB4B496, 0.35f));

    /* montagnes (2 plans) */
    Color mc[2] = { hsl(h + 10, 0.35f, 0.26f, 1.0f), hsl(h + 20, 0.30f, 0.17f, 1.0f) };
    for (int li = 0; li < 2; li++) {
        int n = bg->mountain_n[li];
        for (int i = 0; i + 1 < n; i++) {
            float x0 = bg->mountain[li][i][0], y0 = bg->mountain[li][i][1];
            float x1 = bg->mountain[li][i + 1][0], y1 = bg->mountain[li][i + 1][1];
            SDL_FPoint q[4] = { { x0, y0 }, { x1, y1 }, { x1, FLOOR_Y + 2 }, { x0, FLOOR_Y + 2 } };
            gfx_quad(q, mc[li]);
        }
        /* dernier point jusqu'au bord droit */
        if (n > 0 && bg->mountain[li][n - 1][0] < GAME_W) {
            float x0 = bg->mountain[li][n - 1][0], y0 = bg->mountain[li][n - 1][1];
            SDL_FPoint q[4] = { { x0, y0 }, { GAME_W, y0 }, { GAME_W, FLOOR_Y + 2 }, { x0, FLOOR_Y + 2 } };
            gfx_quad(q, mc[li]);
        }
    }

    /* sol enneige */
    gfx_rect_vgrad(0, FLOOR_Y + 2, GAME_W, GAME_H - FLOOR_Y - 2, rgb(0xF4FBFF), rgb(0x8FC0E4));
    for (int x = 0; x <= GAME_W; x += 20) gfx_ellipse(x + 10.0f, FLOOR_Y + 3, 11, 4, rgb(0xF4FBFF));

    /* murs lateraux en glace */
    gfx_rect_hgrad(0, CEIL_Y, WALL_W, FLOOR_Y - CEIL_Y + 2, rgba(0xBFE9FF, 0.75f), rgba(0x5FA8DC, 0.55f));
    gfx_rect_hgrad(GAME_W - WALL_W, CEIL_Y, WALL_W, FLOOR_Y - CEIL_Y + 2, rgba(0x5FA8DC, 0.55f),
                   rgba(0xBFE9FF, 0.75f));
}

void draw_flakes(const Game *g)
{
    if (retro() && g_world == 1) return;   /* pas de neige sur le lac en ete */
    for (int i = 0; i < MAX_FLAKES; i++) {
        const Flake *f = &g->flakes[i];
        gfx_circle(f->x, f->y, f->r, rgba(0xFFFFFF, 0.75f));
    }
}

/* ------------------------------------------------------------------------ */
/* Elements de niveau                                                       */

void draw_platform(float x, float y, float w)
{
    if (retro()) {
        retro_draw_platform(g_world, x, y, w);
        return;
    }
    gfx_rrect_vgrad(x, y, w, PLAT_H, 4, rgb(0xD8F3FF), rgb(0x4F9BD0));
    gfx_rrect_outline(x, y, w, PLAT_H, 4, 1, rgba(0x143C6E, 0.5f));
    /* neige sur le dessus */
    gfx_rect(x, y, w, 4, rgb(0xFFFFFF));
    for (float bx = x + 6; bx < x + w - 2; bx += 12) {
        float r = 5.5f;
        if (bx + r > x + w) r = x + w - bx;
        if (r > 1) gfx_ellipse(bx, y + 1, r + 1, 3.5f, rgb(0xFFFFFF));
    }
    /* stalactites */
    for (float sx = x + 8; sx < x + w - 6; sx += 19) {
        float l = 4 + ((int)(sx * 7) % 5) * 1.6f;
        gfx_tri(sx - 3, y + PLAT_H, sx + 3, y + PLAT_H, sx, y + PLAT_H + l, rgba(0xD2F0FF, 0.9f));
    }
}

void draw_ladder(float x, float top, float bottom)
{
    if (retro()) {
        retro_draw_ladder(x, top, bottom);
        return;
    }
    float w = LADDER_W;
    for (int pass = 0; pass < 2; pass++) {
        Color c = pass ? rgb(0xA9DCF5) : rgb(0x2B4F73);
        float t = pass ? 3 : 5;
        gfx_line(x + 4, top - 6, x + 4, bottom, t, c);
        gfx_line(x + w - 4, top - 6, x + w - 4, bottom, t, c);
    }
    for (float y = top + 6; y < bottom; y += 12) {
        gfx_line(x + 4, y + 1, x + w - 4, y + 1, 3, rgb(0x2B4F73));
        gfx_line(x + 4, y, x + w - 4, y, 3, rgb(0xD6F1FF));
    }
}

void draw_laser(float x, float top, float bottom, double t)
{
    /* rayon vert qui scintille entre deux emetteurs metalliques */
    float flick = 0.75f + 0.25f * sinf((float)t * 31.0f) * sinf((float)t * 7.0f + x);
    float y0 = top + 6, h = bottom - top - 12;
    if (h > 0) {
        gfx_rect(x - 5, y0, 10, h, rgba(0x30FF60, 0.16f * flick));
        gfx_rect(x - 3, y0, 6, h, rgba(0x50FF70, 0.45f * flick));
        gfx_rect(x - 1, y0, 2, h, rgba(0xE8FFE8, flick));
        /* etincelles qui montent le long du rayon */
        for (int i = 0; i < 4; i++) {
            float p = fmodf((float)t * 90.0f + i * h / 4, h);
            gfx_rect(x - 2, y0 + h - p, 4, 4, rgba(0xC8FFC8, 0.8f * flick));
        }
    }
    for (int k = 0; k < 2; k++) {
        float ey = k ? bottom - 8 : top;
        gfx_rect(x - 8, ey, 16, 8, rgb(0x56606E));
        gfx_rect(x - 8, ey, 16, 2, rgb(0xA8B4C4));
        gfx_rect(x - 8, ey + 6, 16, 2, rgb(0x2A3038));
        gfx_rect(x - 3, k ? ey - 1 : ey + 6, 6, 3, rgba(0x80FF90, flick));
    }
}

static const Uint32 BALL_COLORS[4][2] = {
    { 0xFF7B98, 0xA50D36 }, { 0xFFD56B, 0xB86E00 }, { 0x86F5B2, 0x0F7D42 }, { 0x86CCFF, 0x1A55AA } };

void draw_ball(float x, float y, int size, bool frozen)
{
    if (retro()) {
        retro_draw_ball(x, y, size, frozen);
        return;
    }
    float r = BALL_R[size];
    Color gold = rgb(0xD9B24A);
    /* attache de boule de Noel */
    gfx_rect(x - r * 0.26f, y - r - r * 0.2f, r * 0.52f, r * 0.28f, gold);
    gfx_ring(x, y - r - r * 0.3f, r * 0.13f, fmaxf(1.5f, r * 0.08f), gold);
    gfx_ball(x, y, r, x - r * 0.35f, y - r * 0.4f, rgb(0xFFFFFF), rgb(BALL_COLORS[size][0]),
             rgb(BALL_COLORS[size][1]));
    /* bandeau decoratif en pointilles (face avant) */
    float cy = y + r * 0.15f, rx = r * 0.9f, ry = r * 0.32f;
    const int dashes = 5;
    for (int i = 0; i < dashes; i++) {
        float a0 = PI_F * (i + 0.2f) / dashes, a1 = PI_F * (i + 0.75f) / dashes;
        gfx_line(x + cosf(a0) * rx, cy + sinf(a0) * ry, x + cosf(a1) * rx, cy + sinf(a1) * ry, r * 0.13f,
                 rgba(0xFFFFFF, 0.6f));
    }
    if (frozen) {
        gfx_circle(x, y, r, rgba(0xBEF0FF, 0.45f));
        gfx_ring(x, y, r, 2, rgb(0xFFFFFF));
    }
}

/* ------------------------------------------------------------------------ */
/* Bonhomme de neige                                                        */

typedef struct Palette { Uint32 hat, band, scarf, gun; } Palette;
static const Palette PALETTES[2] = { { 0x2C6BE0, 0x86B8FF, 0xFF8A2A, 0x9BE8FF },
                                     { 0xD93A55, 0xFF8FA2, 0x36B86A, 0xFFD0F0 } };

static void draw_snowman(float cx, float top, int index, int f, bool climb, float walk_t, bool shield,
                         double t, float bob)
{
    if (retro()) {
        int frame = climb ? (sinf(walk_t * 10) > 0 ? 1 : 2) : 0;
        retro_draw_player(cx, top + roundf(bob / 2) * 2, index, frame, f);
        if (shield) {
            float a = 0.55f + sinf((float)t * 6.7f) * 0.2f;
            gfx_ring(cx, top + 22, 29, 2, rgba(0x96E6FF, a));
        }
        return;
    }
    const Palette *pal = &PALETTES[index & 1];
    float ox, oy;
    gfx_get_offset(&ox, &oy);
    gfx_set_offset(ox + cx, oy + top + bob);

    gfx_ellipse(0, 44 - bob, 13, 3, rgba(0x001432, 0.25f));
    Color arm = rgb(0x6B4423);
    if (climb) {
        float s = sinf(walk_t * 10) * 4;
        gfx_line(-10, 25, -15, 10 + s, 2.5f, arm);
        gfx_line(10, 25, 15, 10 - s, 2.5f, arm);
    } else {
        gfx_line(-f * 10.0f, 27, -f * 17.0f, 21, 2.5f, arm);
        gfx_line(f * 9.0f, 27, f * 14.0f, 22, 2.5f, arm);
    }
    gfx_ball(0, 30, 14, -4, 24, rgb(0xFFFFFF), rgb(0xF0F7FC), rgb(0xBCD8EE));
    for (int i = 0; i < 3; i++) gfx_circle(f * 1.5f, 25.0f + i * 6, 1.6f, rgb(0x1D2433));
    if (!climb) {
        gfx_rrect(f * 12.0f - 3, 9, 6, 14, 2, rgb(pal->gun));
        gfx_tri(f * 12.0f - 3, 9, f * 12.0f, 4, f * 12.0f + 3, 9, rgb(0xFFFFFF));
    }
    gfx_ball(0, 12, 10, -3, 8, rgb(0xFFFFFF), rgb(0xF0F7FC), rgb(0xC4DCF0));

    float flap = sinf(walk_t * 12 + index) * 2;
    gfx_rrect(-11, 18, 22, 5, 2, rgb(pal->scarf));
    float ang = -f * 0.15f + flap * 0.04f;
    gfx_quad_rot(-f * 7.0f - sinf(ang) * 5.5f, 21 + cosf(ang) * 5.5f, 5, 11, ang, rgb(pal->scarf));

    if (!climb) {
        gfx_circle(-3.0f + f * 2, 12, 1.7f, rgb(0x1D2433));
        gfx_circle(3.0f + f * 2, 12, 1.7f, rgb(0x1D2433));
        gfx_tri(f * 2.0f, 14, f * 11.0f, 15.5f, f * 2.0f, 17, rgb(0xFF8A1E));
    }
    gfx_arc_fill(0, 8, 10.5f, PI_F, 2 * PI_F, rgb(pal->hat));
    gfx_rrect(-11.5f, 5.5f, 23, 4.5f, 2, rgb(pal->band));
    gfx_circle(0, -3, 3.6f, rgb(0xFFFFFF));

    if (shield) {
        float a = 0.55f + sinf((float)t * 6.7f) * 0.2f;
        gfx_circle(0, 22, 29, rgba(0x96E6FF, 0.08f));
        gfx_ring(0, 22, 29, 2.5f, rgba(0x96E6FF, a));
    }
    gfx_set_offset(ox, oy);
}

void draw_player_at(float cx, float feet_y, int index, int face, float alpha)
{
    float a = gfx_get_alpha();
    gfx_set_alpha(a * alpha);
    draw_snowman(cx, feet_y - 44, index, face, false, 0, false, 0, 0);
    gfx_set_alpha(a);
}

static void draw_player(const Game *g, const Player *p)
{
    if (!p->alive) return;
    float bob = (p->moving && p->ground) ? -fabsf(sinf(p->walk_t * 14)) * 2 : 0;
    float a = gfx_get_alpha();
    if (p->inv > 0 && ((int)(p->inv * 14)) % 2 == 0) gfx_set_alpha(a * 0.35f);
    draw_snowman(p->x + p->w / 2, p->y, p->index, p->face, p->climb, p->walk_t, p->shield, g->clock, bob);
    gfx_set_alpha(a);
}

/* ------------------------------------------------------------------------ */
/* Projectiles, bonus, particules                                           */

static void draw_beam(const Beam *bm)
{
    /* clignote juste avant de disparaitre */
    if (bm->hold > 0 && bm->hold < 0.6f && bm->top <= bm->stop && ((int)(bm->hold * 12)) % 2) return;
    float h = bm->base - bm->top - 8;
    if (h > 0) {
        gfx_rect_hgrad(bm->x - 4, bm->top + 8, 4, h, rgba(0x78D2FF, 0.7f), rgb(0xFFFFFF));
        gfx_rect_hgrad(bm->x, bm->top + 8, 4, h, rgb(0xFFFFFF), rgba(0x78D2FF, 0.7f));
        /* dents de givre */
        for (float y = bm->top + 14; y < bm->base; y += 16) {
            gfx_tri(bm->x - 4, y, bm->x - 7, y + 4, bm->x - 4, y + 8, rgba(0xC9F2FF, 0.8f));
            gfx_tri(bm->x + 4, y + 8, bm->x + 7, y + 12, bm->x + 4, y + 16, rgba(0xC9F2FF, 0.8f));
        }
    }
    gfx_tri(bm->x - 6, bm->top + 10, bm->x, bm->top, bm->x + 6, bm->top + 10, rgb(0xFFFFFF));
    gfx_circle(bm->x, bm->top + 4, 2, rgba(0xFFFFFF, 0.9f));
    if (bm->hold > 0 && bm->top <= bm->stop) {
        /* colonne accrochee : bloc de glace au sommet */
        gfx_rect(bm->x - 8, bm->top, 16, 8, rgb(0xE6F8FF));
        gfx_rect(bm->x - 8, bm->top + 6, 16, 2, rgb(0x7CC8F0));
    }
}

static void pup_icon(PupKind k, float cx, float cy, float s)
{
    Color ink = rgb(0x0B1F3D);
    switch (k) {
    case PUP_RAPID: font_draw("R", cx, cy, s * 0.6f, ink, ALIGN_CENTER); break;
    case PUP_DOUBLE: font_draw("×2", cx, cy, s * 0.55f, ink, ALIGN_CENTER); break;
    case PUP_FREEZE: gfx_snowflake(cx, cy, s * 0.32f, 1.6f, ink); break;
    case PUP_SHIELD: gfx_shield_icon(cx, cy, s * 0.55f, ink); break;
    case PUP_LIFE: gfx_heart(cx, cy, s * 0.6f, rgb(0xC0103A)); break;
    case PUP_STICK:
        /* gros batonnet de glace */
        gfx_rect(cx - s * 0.05f, cy + s * 0.12f, s * 0.1f, s * 0.3f, rgb(0x8A5A2B));
        gfx_rrect(cx - s * 0.2f, cy - s * 0.38f, s * 0.4f, s * 0.54f, s * 0.12f, ink);
        gfx_rrect(cx - s * 0.14f, cy - s * 0.32f, s * 0.28f, s * 0.42f, s * 0.08f, rgb(0xFFFFFF));
        break;
    default: break;
    }
}

static const Uint32 PUP_COLORS[PUP_KINDS] = { 0xFFB347, 0x7CD0FF, 0xB9F3FF, 0x9B8CFF, 0xFF6B8B, 0x8CF0C8, 0xFFF7A8 };

static void draw_pup(const Pup *u)
{
    if (u->kind == PUP_FRUIT) {
        if (!retro()) gfx_circle(u->x + 11, u->y + 11, 15, rgba(0xFFFFFF, 0.25f));
        retro_draw_fruit(u->fruit, u->x - 1, u->y - 2);
        return;
    }
    if (retro()) {
        /* caisse biseautee */
        gfx_rect(u->x, u->y, 22, 22, rgb(PUP_COLORS[u->kind]));
        gfx_rect(u->x, u->y, 22, 2, rgb(0xFFFFFF));
        gfx_rect(u->x, u->y, 2, 22, rgb(0xFFFFFF));
        gfx_rect(u->x, u->y + 20, 22, 2, rgb(0x203050));
        gfx_rect(u->x + 20, u->y, 2, 22, rgb(0x203050));
        pup_icon(u->kind, u->x + 11, u->y + 11, 22);
        return;
    }
    gfx_rrect(u->x, u->y, 22, 22, 6, rgb(PUP_COLORS[u->kind]));
    gfx_rrect_outline(u->x, u->y, 22, 22, 6, 2, rgb(0xFFFFFF));
    pup_icon(u->kind, u->x + 11, u->y + 11, 22);
}

/* ------------------------------------------------------------------------ */
/* HUD                                                                      */

typedef struct HudIcon { PupKind k; float f; } HudIcon;

static void draw_popups(const Game *g)
{
    char buf[16];
    for (int i = 0; i < g->popup_n; i++) {
        const Popup *q = &g->popups[i];
        float a = q->life > 0.4f ? 1 : q->life / 0.4f;
        SDL_snprintf(buf, sizeof buf, "%d", q->value);
        font_draw_outlined(buf, q->x, q->y, 16, rgba(0xFFE860, a), ALIGN_CENTER);
    }
}

static void draw_hud(const Game *g)
{
    char buf[96];
    if (!retro()) {
        gfx_rect(0, 0, GAME_W, CEIL_Y, rgba(0x040A1E, 0.78f));
        gfx_rect(0, CEIL_Y - 1, GAME_W, 1, rgba(0x96D2FF, 0.35f));
    }

    for (int i = 0; i < g->players_n; i++) {
        const Player *p = &g->players[i];
        bool right = i == 1;
        float x = right ? GAME_W - 46 : 12;
        Color col = p->alive ? (i ? rgb(0xFF9AAE) : rgb(0x9CC6FF)) : rgb(0x666677);
        SDL_snprintf(buf, sizeof buf, "J%d  %06d", i + 1, p->score);
        font_draw(buf, x, 18, 16, col, right ? ALIGN_RIGHT : ALIGN_LEFT);
        float lw = font_measure(buf, 16);
        float hx = right ? x - lw - 16 : x + lw + 16;
        int hearts = p->lives < 0 ? 0 : (p->lives > 6 ? 6 : p->lives);
        for (int k = 0; k < hearts; k++) gfx_heart(hx + (right ? -k : k) * 15.0f, 18, 13, rgb(0xFF6B8B));
        if (p->lives > 6) font_draw("+", hx + (right ? -6 : 6) * 15.0f, 18, 14, rgb(0xFF6B8B), ALIGN_CENTER);
        float ix = hx + (right ? -1 : 1) * (hearts * 15.0f + 6);

        HudIcon icons[4];
        int n = 0;
        if (p->rapid) icons[n++] = (HudIcon){ PUP_RAPID, 1 };
        if (p->dbl) icons[n++] = (HudIcon){ PUP_DOUBLE, 1 };
        if (p->stick) icons[n++] = (HudIcon){ PUP_STICK, 1 };
        if (p->shield) icons[n++] = (HudIcon){ PUP_SHIELD, 1 };
        for (int k = 0; k < n; k++) {
            float bx = right ? ix - 18 : ix;
            gfx_rrect(bx, 8, 18, 18, 4, rgb(PUP_COLORS[icons[k].k]));
            gfx_rect(bx, 8, 18, 18 * (1 - icons[k].f), rgba(0x000000, 0.35f));
            pup_icon(icons[k].k, bx + 9, 17, 18);
            ix += right ? -22 : 22;
        }
    }
    if (g->players_n == 1) {
        int best = g_settings.best_score;
        if (g->mode == GAMEMODE_CAMPAIGN && g->players[0].score > best) best = g->players[0].score;
        SDL_snprintf(buf, sizeof buf, "RECORD %06d", best);
        if (g->mode == GAMEMODE_CAMPAIGN) font_draw(buf, GAME_W - 50, 18, 14, rgb(0xC8D8EA), ALIGN_RIGHT);
    }

    if (g->mode == GAMEMODE_CAMPAIGN) SDL_snprintf(buf, sizeof buf, "NIVEAU %d", g->level_no);
    else SDL_snprintf(buf, sizeof buf, "%s", g->level.name);
    if (retro()) {
        font_draw_outlined(buf, GAME_W / 2.0f, 18, 15, rgb(0xFFFFFF), ALIGN_CENTER);
    } else {
        font_draw(buf, GAME_W / 2.0f, 13, 15, rgb(0xFFFFFF), ALIGN_CENTER);
        font_draw(DIFFICULTIES[g->diff_index].name, GAME_W / 2.0f, 28, 10, rgb(0x8FB6D8), ALIGN_CENTER);
    }

    if (g->spawn_interval > 0) {
        float bw = 180, bx = GAME_W / 2.0f - bw / 2, by = GAME_H - 20;
        float f = g->spawn_t / g->spawn_interval;
        f = f < 0 ? 0 : (f > 1 ? 1 : f);
        gfx_rrect(bx, by, bw, 8, 4, rgba(0x14325A, 0.35f));
        if (f > 0.02f) gfx_rrect(bx, by, bw * f, 8, 4, f < 0.25f ? rgb(0xFF6B8B) : rgb(0x5FB4EE));
        if (!retro()) font_draw("renfort de boules", GAME_W / 2.0f, by - 8, 10, rgb(0x2A5C8A), ALIGN_CENTER);
    }
    if (g->freeze_t > 0) {
        SDL_snprintf(buf, sizeof buf, "GEL ! %.1f s", g->freeze_t);
        font_draw_outlined(buf, GAME_W / 2.0f, CEIL_Y + 18, 16, rgb(0xCFF6FF), ALIGN_CENTER);
    }
}

/* ------------------------------------------------------------------------ */

static void draw_game_layers(const Game *g);

void draw_game(const Game *g)
{
    SDL_Texture *target = retro() ? retro_target() : NULL;
    if (!target) {
        draw_game_layers(g);
        return;
    }
    /* style retro : tout est dessine dans une cible 400x270, puis agrandi sans lissage */
    SDL_Renderer *r = gfx_renderer();
    SDL_Texture *prev = SDL_GetRenderTarget(r);
    float ox, oy;
    gfx_get_offset(&ox, &oy);
    float alpha = gfx_get_alpha();
    SDL_SetRenderTarget(r, target);
    SDL_SetRenderScale(r, 1.0f / RETRO_SCALE, 1.0f / RETRO_SCALE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    gfx_set_offset(0, 0);
    gfx_set_alpha(1);
    font_set_pixel_target(true);

    draw_game_layers(g);

    font_set_pixel_target(false);
    SDL_SetRenderTarget(r, prev);
    gfx_set_offset(ox, oy);
    gfx_set_alpha(alpha);
    SDL_FRect dst = { ox, oy, GAME_W, GAME_H };
    SDL_SetTextureAlphaModFloat(target, alpha);
    SDL_RenderTexture(r, target, NULL, &dst);
}

static void draw_game_layers(const Game *g)
{
    draw_backdrop(&g->bg, g->clock);
    draw_flakes(g);
    for (int i = 0; i < g->level.ladder_count; i++)
        draw_ladder(g->level.ladders[i].x, g->level.ladders[i].top, g->level.ladders[i].bottom);
    for (int i = 0; i < g->level.platform_count; i++)
        draw_platform(g->level.platforms[i].x, g->level.platforms[i].y, g->level.platforms[i].w);
    for (int i = 0; i < g->level.laser_count; i++)
        draw_laser(g->level.lasers[i].x, g->level.lasers[i].top, g->level.lasers[i].bottom, g->clock);
    for (int i = 0; i < g->pup_n; i++) draw_pup(&g->pups[i]);
    for (int i = 0; i < g->beam_n; i++) draw_beam(&g->beams[i]);
    for (int i = 0; i < g->bullet_n; i++) {
        const Bullet *b = &g->bullets[i];
        SDL_FPoint d[4] = { { b->x, b->y - 7 }, { b->x + 3, b->y }, { b->x, b->y + 5 }, { b->x - 3, b->y } };
        gfx_quad(d, rgb(0xE9FBFF));
    }
    bool frozen = g->freeze_t > 0 && g->state == GS_PLAY;
    for (int i = 0; i < g->ball_n; i++) draw_ball(g->balls[i].x, g->balls[i].y, g->balls[i].size, frozen);
    if (g->mode != GAMEMODE_DEMO)
        for (int i = 0; i < g->players_n; i++) draw_player(g, &g->players[i]);
    for (int i = 0; i < g->part_n; i++) {
        const Particle *q = &g->parts[i];
        float a = q->life / q->max;
        Color c = { q->r, q->g, q->b, a < 0 ? 0 : (a > 1 ? 1 : a) };
        gfx_quad_rot(q->x, q->y, q->size, q->size * 1.6f, q->rot, c);
    }
    if (g->mode == GAMEMODE_DEMO) return;
    draw_popups(g);

    if (g->state == GS_PLAY && g->spawn_interval > 0 && g->spawn_t < 2.5f && g->freeze_t <= 0 &&
        ((int)(g->spawn_t * 6)) % 2 == 0)
        gfx_tri(g->next_spawn_x - 9, CEIL_Y + 4, g->next_spawn_x + 9, CEIL_Y + 4, g->next_spawn_x, CEIL_Y + 18,
                rgb(0xFF6B8B));

    draw_hud(g);

    char buf[96];
    if (g->state == GS_READY) {
        if (g->mode == GAMEMODE_CAMPAIGN) SDL_snprintf(buf, sizeof buf, "NIVEAU %d", g->level_no);
        else SDL_snprintf(buf, sizeof buf, "%s", g->level.name);
        font_draw_outlined(buf, GAME_W / 2.0f, GAME_H / 2.0f - 30, 46, rgb(0xFFFFFF), ALIGN_CENTER);
        if (g->mode == GAMEMODE_CAMPAIGN)
            font_draw_outlined(g->level.name, GAME_W / 2.0f, GAME_H / 2.0f + 12, 20, rgb(0xCFE8FF), ALIGN_CENTER);
        font_draw_outlined(g->ready_t > 0.5f ? "PRÊT ?" : "GO !", GAME_W / 2.0f, GAME_H / 2.0f + 50, 30,
                           rgb(0x9FE0FF), ALIGN_CENTER);
    }
    if (g->state == GS_CLEAR) {
        font_draw_outlined("BRAVO !", GAME_W / 2.0f, GAME_H / 2.0f - 50, 50, rgb(0xFFFFFF), ALIGN_CENTER);
        SDL_snprintf(buf, sizeof buf, "Bonus temps +%d", g->clear_bonus);
        font_draw_outlined(buf, GAME_W / 2.0f, GAME_H / 2.0f, 22, rgb(0xFFE08A), ALIGN_CENTER);
        if (g->mode == GAMEMODE_CAMPAIGN && g->level_no % 5 == 0) {
            SDL_snprintf(buf, sizeof buf, "Code pour reprendre au niveau %d :  %s", g->level_no + 1,
                         game_code_for_level(g->level_no + 1));
            font_draw_outlined(buf, GAME_W / 2.0f, GAME_H / 2.0f + 40, 20, rgb(0x9FFFC4), ALIGN_CENTER);
        }
    }
}
