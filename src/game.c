#include "game.h"
#include "retro.h"
#include "app.h"
#include "audio.h"
#include "config.h"
#include "font.h"
#include "gfx.h"
#include "input.h"
#include "settings.h"
#include "storage.h"
#include "ui.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

Game g_game;

/* ------------------------------------------------------------------------ */
/* Constantes de jeu (reprises du prototype HTML)                           */

static const float GRAVITY = 520.0f;      /* gravite des boules */
static const float BALL_VX = 100.0f;
const float BALL_R[4] = { 9, 15, 23, 33 };
/* hauteur de rebond : toutes les tailles rebondissent aussi haut que la geante */
static const float BALL_H[4] = { 300, 300, 300, 300 };
static const int BALL_POINTS[4] = { 200, 150, 100, 50 };
static float BALL_V[4];                                 /* sqrt(2 g h) */

const Difficulty DIFFICULTIES[4] = {
    { "Débutant", 0.72f, 5, 36 },
    { "Normal", 0.9f, 3, 28 },
    { "Difficile", 1.05f, 3, 22 },
    { "Expert", 1.22f, 2, 16 },
};

static const Uint32 BALL_COL[4][2] = {
    { 0xFF7B98, 0xA50D36 }, { 0xFFD56B, 0xB86E00 }, { 0x86F5B2, 0x0F7D42 }, { 0x86CCFF, 0x1A55AA } };

static const char *const CODE_WORDS[] = { "SAPIN", "FLOCON", "IGLOO", "GIVRE", "BANQUISE",
                                          "AURORE", "BLIZZARD", "ETOILE", "LUGE", "TRAINEAU" };
#define CODE_WORD_N ((int)(sizeof(CODE_WORDS) / sizeof(CODE_WORDS[0])))

/* Campagne chargee au lancement d'une partie */
static LevelEntry g_campaign[MAX_LEVEL_ENTRIES];
static int g_campaign_n;

/* ------------------------------------------------------------------------ */
/* Utilitaires                                                              */

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float frand(float a, float b) { return a + (float)SDL_randf() * (b - a); }

static bool circ_rect(float cx, float cy, float r, float x, float y, float w, float h)
{
    float px = clampf(cx, x, x + w), py = clampf(cy, y, y + h);
    float dx = cx - px, dy = cy - py;
    return dx * dx + dy * dy < r * r;
}

static void init_constants(void)
{
    for (int i = 0; i < 4; i++) BALL_V[i] = sqrtf(2 * GRAVITY * BALL_H[i]);
}

const char *game_code_for_level(int n)
{
    static char buf[24];
    int k = (n - 1) / 5 - 1;
    if (k >= 0 && k < CODE_WORD_N) return CODE_WORDS[k];
    SDL_snprintf(buf, sizeof buf, "NEIGE%d", n);
    return buf;
}

int game_level_from_code(const char *code)
{
    char c[32];
    int n = 0;
    for (const char *p = code; *p && n < 31; p++)
        if (*p != ' ') c[n++] = (char)SDL_toupper((unsigned char)*p);
    c[n] = '\0';
    if (!n) return 0;
    for (int i = 0; i < CODE_WORD_N; i++)
        if (!SDL_strcmp(c, CODE_WORDS[i])) return (i + 1) * 5 + 1;
    if (!SDL_strncmp(c, "NEIGE", 5)) {
        int v = SDL_atoi(c + 5);
        if (v > 5 && (v - 1) % 5 == 0) return v;
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Creation d'entites                                                       */

static void add_ball(Game *g, float x, float y, int size, int dir)
{
    if (g->ball_n >= MAX_BALLS) return;
    Ball *b = &g->balls[g->ball_n++];
    b->x = x;
    b->y = y;
    b->size = size;
    b->vx = dir * BALL_VX;
    b->vy = 0;
}

Ball game_make_ball(float x, float y, int size, int dir)
{
    Ball b = { x, y, dir * BALL_VX, 0, size };
    return b;
}

static void burst(Game *g, float x, float y, Uint32 col, int n)
{
    Color c = rgb(col);
    for (int i = 0; i < n && g->part_n < MAX_PARTICLES; i++) {
        Particle *p = &g->parts[g->part_n++];
        float a = frand(0, 6.283f), s = frand(60, 260);
        p->x = x;
        p->y = y;
        p->vx = cosf(a) * s;
        p->vy = sinf(a) * s - 60;
        p->life = frand(0.4f, 0.9f);
        p->max = 0.9f;
        p->size = frand(2, 5);
        p->rot = frand(0, 6);
        p->r = c.r;
        p->g = c.g;
        p->b = c.b;
    }
}

static void reset_player_pos(Game *g, Player *p)
{
    const LevelStart *s = &g->level.start[p->index];
    p->x = clampf(s->x - p->w / 2, WALL_W, GAME_W - WALL_W - p->w);
    p->y = s->y - p->h;
    p->vy = 0;
    p->climb = false;
    p->ladder = -1;
    p->ground = true;
}

static void make_player(Game *g, int i)
{
    Player *p = &g->players[i];
    memset(p, 0, sizeof *p);
    p->index = i;
    p->w = 28;
    p->h = 44;
    p->face = i ? -1 : 1;
    p->lives = DIFFICULTIES[g->diff_index].lives;
    p->alive = true;
    p->ladder = -1;
}

static void init_flakes(Game *g)
{
    for (int i = 0; i < MAX_FLAKES; i++) {
        Flake *f = &g->flakes[i];
        f->x = frand(0, GAME_W);
        f->y = frand(0, GAME_H);
        f->r = frand(0.6f, 2.4f);
        f->speed = frand(15, 40);
        f->phase = frand(0, 6);
    }
}

/* ------------------------------------------------------------------------ */
/* Niveaux                                                                  */

static void setup_level(Game *g, const Level *lvl, int seed)
{
    g->level = *lvl;
    g->ball_n = g->beam_n = g->bullet_n = g->pup_n = g->part_n = g->popup_n = 0;
    for (int i = 0; i < lvl->ball_count; i++)
        add_ball(g, lvl->balls[i].x, lvl->balls[i].y, lvl->balls[i].size, lvl->balls[i].dir);
    /* tours supplementaires de la campagne : boules en renfort */
    for (int i = 0; i < g->loop_n; i++)
        add_ball(g, frand(80, 720), 110, g->loop_n + 1 > 3 ? 3 : g->loop_n + 1, SDL_rand(2) ? 1 : -1);

    const Difficulty *d = &DIFFICULTIES[g->diff_index];
    if (lvl->reinforce == REINFORCE_DEFAULT) g->spawn_interval = d->spawn;
    else if (lvl->reinforce < 0) g->spawn_interval = -1;
    else g->spawn_interval = lvl->reinforce;

    g->level_t = 0;
    g->freeze_t = 0;
    g->spawn_t = g->spawn_interval;
    g->warned = false;
    g->next_spawn_x = frand(60, 740);
    g->ready_t = 1.8f;
    g->state = GS_READY;
    g->over_delay = 0;
    for (int i = 0; i < g->players_n; i++) {
        Player *p = &g->players[i];
        if (p->alive) {
            reset_player_pos(g, p);
            p->rapid = p->dbl = p->stick = false;
            p->inv = 0;
        }
    }
    backdrop_build(&g->bg, lvl->hue + g->loop_n * 25, seed);
    g->bg.world = game_world_for(lvl, g->level_no);
}

int game_world_for(const Level *lvl, int level_no)
{
    if (lvl->backdrop >= 1 && lvl->backdrop <= BACKDROP_COUNT) return lvl->backdrop;
    if (level_no > 0) return (level_no - 1) / 10 % BACKDROP_COUNT + 1;
    return 1;
}

static bool load_campaign_level(Game *g, int n)
{
    if (g_campaign_n == 0) return false;
    int idx = (n - 1) % g_campaign_n;
    g->loop_n = (n - 1) / g_campaign_n;
    g->level_no = n;
    Level lvl;
    char err[128];
    if (!g_campaign[idx].valid || !storage_load(LEVELSRC_CAMPAIGN, g_campaign[idx].file, &lvl, err, sizeof err)) {
        SDL_Log("niveau %s invalide : %s", g_campaign[idx].file, err);
        level_init(&lvl);
        lvl.ball_count = 1;
        lvl.balls[0] = (LevelBall){ 400, 150, 2, 1 };
    }
    SDL_snprintf(g->title, sizeof g->title, "%s", lvl.name);
    setup_level(g, &lvl, n * 7919 + 3);
    return true;
}

static void start_common(Game *g, GameMode mode, int players, int difficulty)
{
    init_constants();
    g->mode = mode;
    g->players_n = players < 1 ? 1 : (players > 2 ? 2 : players);
    g->diff_index = difficulty < 0 ? 0 : (difficulty > 3 ? 3 : difficulty);
    g->loop_n = 0;
    g->new_record = false;
    g->won = false;
    for (int i = 0; i < g->players_n; i++) make_player(g, i);
    init_flakes(g);
}

void game_start_campaign(int start_level, int players, int difficulty)
{
    Game *g = &g_game;
    g_campaign_n = storage_list(LEVELSRC_CAMPAIGN, g_campaign, MAX_LEVEL_ENTRIES);
    start_common(g, GAMEMODE_CAMPAIGN, players, difficulty);
    if (!load_campaign_level(g, start_level < 1 ? 1 : start_level)) {
        SDL_Log("aucun niveau de campagne");
        app_set_scene(SCENE_MENU);
        return;
    }
    app_set_scene(SCENE_GAME);
}

void game_start_single(const Level *lvl, int players, int difficulty)
{
    Game *g = &g_game;
    start_common(g, GAMEMODE_SINGLE, players, difficulty);
    g->level_no = 0;
    SDL_snprintf(g->title, sizeof g->title, "%s", lvl->name);
    setup_level(g, lvl, (int)SDL_GetTicks());
    app_set_scene(SCENE_GAME);
}

void game_start_test(const Level *lvl, int players)
{
    Game *g = &g_game;
    start_common(g, GAMEMODE_TEST, players, 1);
    g->level_no = 0;
    SDL_snprintf(g->title, sizeof g->title, "Test : %s", lvl->name);
    setup_level(g, lvl, 1234);
    g->ready_t = 0.8f;
    app_set_scene(SCENE_GAME);
}

void game_pause(void)
{
    Game *g = &g_game;
    if (g->state == GS_PLAY || g->state == GS_READY) {
        g->state_before_pause = g->state;
        g->state = GS_PAUSE;
        ui_reset_focus();
    }
}

static void resume(Game *g) { g->state = g->state_before_pause; }

/* ------------------------------------------------------------------------ */
/* Joueurs                                                                  */

static float limit_above(const Game *g, float x, float y)
{
    float lim = CEIL_Y;
    for (int i = 0; i < g->level.platform_count; i++) {
        const LevelPlatform *pl = &g->level.platforms[i];
        if (x >= pl->x && x <= pl->x + pl->w && pl->y + PLAT_H <= y - 1) lim = fmaxf(lim, pl->y + PLAT_H);
    }
    return lim;
}

static int ladder_at(const Game *g, const Player *p)
{
    float cx = p->x + p->w / 2, f = p->y + p->h;
    for (int i = 0; i < g->level.ladder_count; i++) {
        const LevelLadder *l = &g->level.ladders[i];
        if (cx > l->x - 5 && cx < l->x + LADDER_W + 5 && f >= l->top - 3 && f <= l->bottom + 3) return i;
    }
    return -1;
}

static int beams_of(const Game *g, int owner)
{
    int n = 0;
    for (int i = 0; i < g->beam_n; i++) n += g->beams[i].owner == owner;
    return n;
}

static void update_player(Game *g, Player *p, float dt)
{
    if (!p->alive) return;
    PlayerInput in = input_player(p->index, g->players_n);
    p->inv = fmaxf(0, p->inv - dt);
    p->cool -= dt;

    int lad = ladder_at(g, p);
    if (!p->climb && lad >= 0 && p->ground) {
        const LevelLadder *l = &g->level.ladders[lad];
        float f = p->y + p->h;
        if ((in.up && f > l->top + 1) || (in.down && f < l->bottom - 1)) {
            p->climb = true;
            p->ladder = lad;
            p->x = l->x + LADDER_W / 2.0f - p->w / 2;
        }
    }
    p->moving = false;
    if (p->climb) {
        const LevelLadder *l = &g->level.ladders[p->ladder];
        p->vy = 0;
        if (in.up) { p->y -= 150 * dt; p->walk_t += dt; }
        if (in.down) { p->y += 150 * dt; p->walk_t += dt; }
        if (p->y + p->h < l->top) p->y = l->top - p->h;
        if (p->y + p->h > l->bottom) p->y = l->bottom - p->h;
        float f = p->y + p->h;
        bool at_end = fabsf(f - l->top) < 1 || fabsf(f - l->bottom) < 1;
        if (at_end && (in.left || in.right)) {
            p->climb = false;
            p->ground = true;
        }
    }
    if (!p->climb) {
        int dx = (in.right ? 1 : 0) - (in.left ? 1 : 0);
        p->x = clampf(p->x + dx * 175 * dt, WALL_W, GAME_W - WALL_W - p->w);
        if (dx) {
            p->face = dx;
            p->moving = true;
            p->walk_t += dt;
        }
        p->vy += 1400 * dt;
        float prev = p->y + p->h;
        p->y += p->vy * dt;
        float nf = p->y + p->h;
        float land = -1;
        if (nf >= FLOOR_Y) land = FLOOR_Y;
        for (int i = 0; i < g->level.platform_count; i++) {
            const LevelPlatform *pl = &g->level.platforms[i];
            if (p->x + p->w > pl->x + 4 && p->x < pl->x + pl->w - 4 && prev <= pl->y + 0.5f && nf >= pl->y &&
                (land < 0 || pl->y < land))
                land = pl->y;
        }
        if (land >= 0) {
            p->y = land - p->h;
            p->vy = 0;
            p->ground = true;
        } else {
            p->ground = false;
        }
    }
    bool pressed = in.fire && !p->fire_held;
    p->fire_held = in.fire;
    if (g->state == GS_PLAY && p->rapid) {
        /* tir rapide : rafale tant que le bouton est maintenu */
        if (in.fire && p->cool <= 0 && g->bullet_n < MAX_BULLETS) {
            float x = p->x + p->w / 2, base = p->y + p->h;
            g->bullets[g->bullet_n++] = (Bullet){ x, p->y + 4, limit_above(g, x, base), p->index };
            p->cool = 0.1f;
            audio_play(SFX_PEW, 0);
        }
    } else if (g->state == GS_PLAY && pressed) {
        /* colonne : un tir par appui, sans attendre. Au maximum, la derniere colonne
         * tiree est remplacee (les precedentes restent). */
        float x = p->x + p->w / 2, base = p->y + p->h;
        int max = p->dbl ? 2 : 1;
        if (beams_of(g, p->index) >= max) {
            int last = -1;
            for (int i = 0; i < g->beam_n; i++)
                if (g->beams[i].owner == p->index && (last < 0 || g->beams[i].seq > g->beams[last].seq)) last = i;
            if (last >= 0) g->beams[last] = g->beams[--g->beam_n];
        }
        if (g->beam_n < MAX_BEAMS) {
            g->beams[g->beam_n++] = (Beam){ x, base, base - 6, limit_above(g, x, base), p->index,
                                            p->stick ? 3.0f : 0, ++g->beam_seq };
            audio_play(SFX_SHOOT, 0);
        }
    }
}

static bool any_alive(const Game *g)
{
    for (int i = 0; i < g->players_n; i++)
        if (g->players[i].alive) return true;
    return false;
}

static void hurt(Game *g, Player *p)
{
    if (p->shield) {
        p->shield = false;
        p->inv = 1.5f;
        audio_play(SFX_SHIELD, 0);
        burst(g, p->x + 14, p->y + 22, 0x9FE8FF, 18);
        return;
    }
    p->lives--;
    audio_play(SFX_HURT, 0);
    burst(g, p->x + 14, p->y + 22, 0xFFFFFF, 30);
    if (p->lives > 0) {
        reset_player_pos(g, p);
        p->inv = 2.5f;
    } else {
        p->alive = false;
    }
    for (int i = g->beam_n - 1; i >= 0; i--)
        if (g->beams[i].owner == p->index) g->beams[i] = g->beams[--g->beam_n];
    if (!any_alive(g)) g->over_delay = 0.9f;
}

/* ------------------------------------------------------------------------ */
/* Boules                                                                   */

static void ball_vs_rect(Ball *b, float r, float x, float y, float w, float h)
{
    float cx = clampf(b->x, x, x + w), cy = clampf(b->y, y, y + h);
    float dx = b->x - cx, dy = b->y - cy;
    if (dx * dx + dy * dy >= r * r) return;
    if (b->x >= x && b->x <= x + w) {
        if (b->y < y + h / 2.0f) { b->y = y - r; b->vy = -BALL_V[b->size]; }
        else { b->y = y + h + r; b->vy = fabsf(b->vy); }
    } else if (b->y >= y && b->y <= y + h) {
        if (b->x < x) { b->x = x - r; b->vx = -fabsf(b->vx); }
        else { b->x = x + w + r; b->vx = fabsf(b->vx); }
    } else {
        float d = sqrtf(dx * dx + dy * dy);
        if (d < 0.0001f) d = 1;
        float nx = dx / d, ny = dy / d;
        b->x = cx + nx * r;
        b->y = cy + ny * r;
        float dot = b->vx * nx + b->vy * ny;
        if (dot < 0) { b->vx -= 2 * dot * nx; b->vy -= 2 * dot * ny; }
        if (ny < -0.4f) b->vy = -BALL_V[b->size];
        b->vx = (b->vx < 0 ? -1 : 1) * BALL_VX;
    }
}

void game_ball_step(const Level *lvl, Ball *b, float dt)
{
    if (BALL_V[0] == 0) init_constants();
    float r = BALL_R[b->size];
    b->vy += GRAVITY * dt;
    b->x += b->vx * dt;
    b->y += b->vy * dt;
    if (b->x - r < WALL_W) { b->x = WALL_W + r; b->vx = fabsf(b->vx); }
    if (b->x + r > GAME_W - WALL_W) { b->x = GAME_W - WALL_W - r; b->vx = -fabsf(b->vx); }
    if (b->y - r < CEIL_Y) { b->y = CEIL_Y + r; b->vy = fabsf(b->vy); }
    if (b->y + r > FLOOR_Y) { b->y = FLOOR_Y - r; b->vy = -BALL_V[b->size]; }
    for (int j = 0; j < lvl->platform_count; j++) {
        const LevelPlatform *pl = &lvl->platforms[j];
        ball_vs_rect(b, r, pl->x, pl->y, pl->w, PLAT_H);
    }
    /* lasers : les boules rebondissent dessus comme sur un mur fin */
    for (int j = 0; j < lvl->laser_count; j++) {
        const LevelLaser *z = &lvl->lasers[j];
        ball_vs_rect(b, r, z->x - LASER_W / 2.0f, z->top, LASER_W, z->bottom - z->top);
    }
}

static void update_balls(Game *g, float dt)
{
    for (int i = 0; i < g->ball_n; i++) game_ball_step(&g->level, &g->balls[i], dt);
}

static void drop_pup(Game *g, float x, float y)
{
    static const int weights[PUP_FRUIT] = { 3, 3, 2, 2, 1, 3 };
    if (g->pup_n >= MAX_PUPS) return;
    float t = frand(0, 14);
    PupKind k = PUP_RAPID;
    for (int i = 0; i < PUP_FRUIT; i++) {
        if (t < weights[i]) { k = (PupKind)i; break; }
        t -= weights[i];
    }
    g->pups[g->pup_n++] = (Pup){ x - 11, y - 11, 0, k, 0 };
}

static void drop_fruit(Game *g, float x, float y)
{
    if (g->pup_n >= MAX_PUPS) return;
    int total = 0;
    for (int i = 0; i < FRUIT_COUNT; i++) total += FRUIT_WEIGHTS[i];
    int t = SDL_rand(total), f = 0;
    while (f < FRUIT_COUNT - 1 && t >= FRUIT_WEIGHTS[f]) t -= FRUIT_WEIGHTS[f++];
    g->pups[g->pup_n++] = (Pup){ x - 11, y - 11, 0, PUP_FRUIT, f };
}

static void add_popup(Game *g, float x, float y, int value)
{
    if (g->popup_n >= MAX_POPUPS) g->popups[0] = g->popups[--g->popup_n];
    g->popups[g->popup_n++] = (Popup){ x, y, 1.2f, value };
}

static void pop_ball(Game *g, int i, int owner)
{
    Ball b = g->balls[i];
    g->balls[i] = g->balls[--g->ball_n];
    audio_play(SFX_POP, b.size);
    if (owner >= 0) g->players[owner].score += BALL_POINTS[b.size];
    burst(g, b.x, b.y, BALL_COL[b.size][0], 10 + b.size * 5);
    if (b.size > 0) {
        for (int d = -1; d <= 1; d += 2) {
            add_ball(g, b.x + d * 4, b.y, b.size - 1, d);
            g->balls[g->ball_n - 1].vy = -fmaxf(230, BALL_V[b.size - 1] * 0.7f);
        }
    }
    float roll = SDL_randf();
    if (roll < 0.16f) drop_pup(g, b.x, b.y);
    else if (roll < 0.30f) drop_fruit(g, b.x, b.y);
}

static void apply_pup(Game *g, Player *p, PupKind k)
{
    audio_play(SFX_POWER, 0);
    switch (k) {
    case PUP_RAPID: p->rapid = true; p->dbl = p->stick = false; break;
    case PUP_DOUBLE: p->dbl = true; p->rapid = false; break;
    case PUP_FREEZE: g->freeze_t = 4.5f; audio_play(SFX_FREEZE, 0); break;
    case PUP_SHIELD: p->shield = true; break;
    case PUP_LIFE: p->lives++; break;
    case PUP_STICK: p->stick = true; p->rapid = false; break;
    case PUP_FRUIT: break;
    default: break;
    }
}

/* ------------------------------------------------------------------------ */
/* Mise a jour                                                              */

static void update_flakes(Game *g, float dt)
{
    for (int i = 0; i < MAX_FLAKES; i++) {
        Flake *f = &g->flakes[i];
        f->y += f->speed * dt;
        f->phase += dt;
        f->x += sinf(f->phase) * 8 * dt;
        if (f->y > GAME_H) { f->y = -4; f->x = frand(0, GAME_W); }
    }
}

static void update_parts(Game *g, float dt)
{
    for (int i = g->part_n - 1; i >= 0; i--) {
        Particle *q = &g->parts[i];
        q->life -= dt;
        if (q->life <= 0) { g->parts[i] = g->parts[--g->part_n]; continue; }
        q->vy += 500 * dt;
        q->x += q->vx * dt;
        q->y += q->vy * dt;
        q->rot += dt * 8;
    }
}

static void finish_game_over(Game *g)
{
    g->state = GS_OVER;
    g->end_t = 0;
    audio_play(SFX_OVER, 0);
    ui_reset_focus();
    int total = 0;
    for (int i = 0; i < g->players_n; i++) total += g->players[i].score;
    if (g->mode == GAMEMODE_CAMPAIGN && total > g_settings.best_score) {
        g_settings.best_score = total;
        g->new_record = true;
        settings_save();
    }
    if (g->mode != GAMEMODE_CAMPAIGN) {
        g->state = GS_DONE;
        g->won = false;
    }
}

static void update_play(Game *g, float dt)
{
    if (g->state == GS_READY) {
        g->ready_t -= dt;
        if (g->ready_t <= 0) { g->ready_t = 0; g->state = GS_PLAY; }
    }
    for (int i = 0; i < g->players_n; i++) update_player(g, &g->players[i], dt);

    if (g->state == GS_PLAY) {
        g->level_t += dt;
        if (g->freeze_t > 0) g->freeze_t -= dt;
        float ts = g->freeze_t > 0 ? 0
                                   : DIFFICULTIES[g->diff_index].speed * (1 + 0.1f * g->loop_n) *
                                         (1 + fminf(0.5f, g->level_t / 110));
        update_balls(g, dt * ts);
        if (g->freeze_t <= 0 && g->spawn_interval > 0) {
            g->spawn_t -= dt;
            if (g->spawn_t < 2.5f && !g->warned) { g->warned = true; audio_play(SFX_WARN, 0); }
            if (g->spawn_t <= 0) {
                int s = 1 + (int)(g->level_t / 60);
                if (s > 2) s = 2;
                add_ball(g, g->next_spawn_x, CEIL_Y + BALL_R[s] + 2, s, SDL_rand(2) ? 1 : -1);
                g->spawn_t = g->spawn_interval;
                g->warned = false;
                g->next_spawn_x = frand(60, 740);
            }
        }
    }

    /* rayons de givre */
    for (int j = g->beam_n - 1; j >= 0; j--) {
        Beam *bm = &g->beams[j];
        bool gone = false;
        if (bm->top > bm->stop) {
            bm->top -= 680 * dt;
            if (bm->top <= bm->stop) {
                /* la colonne touche le haut : elle disparait, sauf avec le batonnet de glace */
                bm->top = bm->stop;
                burst(g, bm->x, bm->stop + 4, 0xC9F2FF, 5);
                if (bm->hold <= 0) gone = true;
            }
        } else {
            bm->hold -= dt;
            if (bm->hold <= 0) {
                burst(g, bm->x, (bm->top + bm->base) / 2, 0xC9F2FF, 8);
                gone = true;
            }
        }
        if (!gone) {
            for (int i = g->ball_n - 1; i >= 0; i--) {
                Ball *b = &g->balls[i];
                if (circ_rect(b->x, b->y, BALL_R[b->size], bm->x - 3, bm->top, 6, bm->base - bm->top)) {
                    pop_ball(g, i, bm->owner);
                    gone = true;
                    break;
                }
            }
        }
        if (gone) g->beams[j] = g->beams[--g->beam_n];
    }
    for (int j = g->bullet_n - 1; j >= 0; j--) {
        Bullet *bl = &g->bullets[j];
        bl->y -= 760 * dt;
        bool gone = bl->y <= bl->stop;
        if (!gone) {
            for (int i = g->ball_n - 1; i >= 0; i--) {
                Ball *b = &g->balls[i];
                float r = BALL_R[b->size] + 3, dx = b->x - bl->x, dy = b->y - bl->y;
                if (dx * dx + dy * dy < r * r) {
                    pop_ball(g, i, bl->owner);
                    gone = true;
                    break;
                }
            }
        }
        if (gone) g->bullets[j] = g->bullets[--g->bullet_n];
    }

    /* bonus */
    for (int j = g->pup_n - 1; j >= 0; j--) {
        Pup *u = &g->pups[j];
        float prev = u->y + 22;
        u->vy += 600 * dt;
        u->y += u->vy * dt;
        float nf = u->y + 22, land = nf >= FLOOR_Y ? FLOOR_Y : -1;
        for (int i = 0; i < g->level.platform_count; i++) {
            const LevelPlatform *pl = &g->level.platforms[i];
            if (u->x + 22 > pl->x && u->x < pl->x + pl->w && prev <= pl->y + 0.5f && nf >= pl->y &&
                (land < 0 || pl->y < land))
                land = pl->y;
        }
        if (land >= 0) { u->y = land - 22; u->vy = 0; }
        for (int i = 0; i < g->players_n; i++) {
            Player *p = &g->players[i];
            if (p->alive && u->x < p->x + p->w && u->x + 22 > p->x && u->y < p->y + p->h && u->y + 22 > p->y) {
                if (u->kind == PUP_FRUIT) {
                    int pts = FRUIT_POINTS[u->fruit];
                    p->score += pts;
                    add_popup(g, u->x + 11, u->y, pts);
                    audio_play(SFX_POWER, 0);
                } else {
                    apply_pup(g, p, u->kind);
                }
                burst(g, u->x + 11, u->y + 11, 0xFFF7A8, 12);
                g->pups[j] = g->pups[--g->pup_n];
                break;
            }
        }
    }
    for (int j = g->popup_n - 1; j >= 0; j--) {
        Popup *q = &g->popups[j];
        q->life -= dt;
        q->y -= 40 * dt;
        if (q->life <= 0) g->popups[j] = g->popups[--g->popup_n];
    }
    update_parts(g, dt);

    if (g->state == GS_PLAY) {
        for (int i = 0; i < g->players_n; i++) {
            Player *p = &g->players[i];
            if (!p->alive || p->inv > 0) continue;
            for (int k = 0; k < g->ball_n; k++) {
                Ball *b = &g->balls[k];
                if (circ_rect(b->x, b->y, BALL_R[b->size] * 0.9f, p->x + 7, p->y + 6, p->w - 14, p->h - 8)) {
                    hurt(g, p);
                    break;
                }
            }
        }
        if (g->over_delay > 0) {
            g->over_delay -= dt;
            if (g->over_delay <= 0) finish_game_over(g);
        } else if (g->ball_n == 0 && any_alive(g)) {
            g->state = GS_CLEAR;
            g->clear_t = 3.2f;
            g->clear_bonus = (int)fmaxf(0, roundf((60 - g->level_t) * 20));
            for (int i = 0; i < g->players_n; i++)
                if (g->players[i].alive) g->players[i].score += g->clear_bonus;
            g->beam_n = g->bullet_n = 0;
            audio_play(SFX_CLEAR, 0);
        }
    }
}

static void update_clear(Game *g, float dt)
{
    g->clear_t -= dt;
    update_parts(g, dt);
    if (g->clear_t > 0) return;
    if (g->mode == GAMEMODE_CAMPAIGN) {
        load_campaign_level(g, g->level_no + 1);
    } else {
        g->state = GS_DONE;
        g->won = true;
        g->end_t = 0;
        ui_reset_focus();
    }
}

/* ------------------------------------------------------------------------ */
/* Demo (fond des menus)                                                    */

void game_demo_init(void)
{
    Game *g = &g_game;
    init_constants();
    memset(g, 0, sizeof *g);
    g->mode = GAMEMODE_DEMO;
    level_init(&g->level);
    add_ball(g, 150, 140, 3, 1);
    add_ball(g, 560, 200, 2, -1);
    add_ball(g, 380, 120, 1, 1);
    init_flakes(g);
    backdrop_build(&g->bg, 215, 42);
    g->bg.world = 1;
}

void game_demo_frame(float dt)
{
    Game *g = &g_game;
    if (g->mode != GAMEMODE_DEMO) game_demo_init();
    g->clock += dt;
    update_flakes(g, dt);
    update_balls(g, dt * 0.8f);
    update_parts(g, dt);
    draw_game(g);
}

/* ------------------------------------------------------------------------ */
/* Scene de jeu                                                             */

static void leave_game(Game *g)
{
    if (g->mode == GAMEMODE_TEST) {
        editor_resume_from_test();
    } else if (g->mode == GAMEMODE_SINGLE) {
        app_set_scene(SCENE_LEVELS);
    } else {
        app_set_scene(SCENE_MENU);
    }
}

static void restart(Game *g)
{
    if (g->mode == GAMEMODE_CAMPAIGN) {
        start_common(g, GAMEMODE_CAMPAIGN, g->players_n, g->diff_index);
        load_campaign_level(g, 1);
    } else {
        Level lvl = g->level;
        GameMode m = g->mode;
        start_common(g, m, g->players_n, g->diff_index);
        setup_level(g, &lvl, 1234);
    }
}

static void overlay_pause(Game *g)
{
    ui_dim(0, 0, GAME_W, GAME_H);
    font_draw_outlined("Pause", GAME_W / 2.0f, 150, 44, rgb(0xFFFFFF), ALIGN_CENTER);
    font_draw(g->title, GAME_W / 2.0f, 195, 18, rgba(0xEAF6FF, 0.8f), ALIGN_CENTER);
    float x = GAME_W / 2.0f - 130;
    if (ui_button("Reprendre", x, 230, 260, 54, BTN_BIG) || ui_enter_unfocused()) resume(g);
    if (ui_button("Recommencer", x, 300, 260, 42, BTN_NORMAL)) restart(g);
    const char *quit = g->mode == GAMEMODE_TEST ? "Retour à l'éditeur" : "Quitter la partie";
    if (ui_button(quit, x, 352, 260, 42, BTN_NORMAL)) leave_game(g);
    if (ui_back_pressed()) resume(g);
}

static void overlay_end(Game *g)
{
    ui_dim(0, 0, GAME_W, GAME_H);
    int total = 0;
    for (int i = 0; i < g->players_n; i++) total += g->players[i].score;
    char line[128];
    float y = 140;
    if (g->state == GS_OVER) {
        font_draw_outlined("Fin de partie", GAME_W / 2.0f, y, 44, rgb(0xFFFFFF), ALIGN_CENTER);
        y += 52;
        if (g->players_n > 1) {
            SDL_snprintf(line, sizeof line, "J1 : %d  ·  J2 : %d", g->players[0].score, g->players[1].score);
            font_draw(line, GAME_W / 2.0f, y, 18, rgb(0xEAF6FF), ALIGN_CENTER);
            y += 28;
        }
        SDL_snprintf(line, sizeof line, "Score : %d  ·  niveau %d", total, g->level_no);
        font_draw(line, GAME_W / 2.0f, y, 20, rgb(0xEAF6FF), ALIGN_CENTER);
        y += 30;
        if (g->new_record) font_draw("Nouveau record !", GAME_W / 2.0f, y, 22, rgb(0x8FF0B0), ALIGN_CENTER);
        else {
            SDL_snprintf(line, sizeof line, "Record : %d", g_settings.best_score);
            font_draw(line, GAME_W / 2.0f, y, 18, rgba(0xEAF6FF, 0.8f), ALIGN_CENTER);
        }
    } else {
        font_draw_outlined(g->won ? "Niveau réussi !" : "Perdu !", GAME_W / 2.0f, y, 44,
                           g->won ? rgb(0x8FF0B0) : rgb(0xFF9AA8), ALIGN_CENTER);
        y += 52;
        font_draw(g->title, GAME_W / 2.0f, y, 20, rgb(0xEAF6FF), ALIGN_CENTER);
        y += 30;
        SDL_snprintf(line, sizeof line, "Score : %d  ·  temps %.1f s", total, g->level_t);
        font_draw(line, GAME_W / 2.0f, y, 18, rgba(0xEAF6FF, 0.85f), ALIGN_CENTER);
    }
    float x = GAME_W / 2.0f - 130;
    if (ui_button(g->state == GS_OVER ? "Rejouer" : "Recommencer", x, 280, 260, 54, BTN_BIG) ||
        (ui_enter_unfocused() && g->end_t > 1.0f))   /* evite un rejeu involontaire en tirant */
        restart(g);
    const char *back = g->mode == GAMEMODE_TEST ? "Retour à l'éditeur"
                       : g->mode == GAMEMODE_SINGLE ? "Mes niveaux" : "Menu";
    if (ui_button(back, x, 350, 260, 42, BTN_NORMAL) || ui_back_pressed()) leave_game(g);
}

void game_frame(float dt)
{
    Game *g = &g_game;
    g->clock += dt;
    if (g->state != GS_PAUSE) update_flakes(g, dt);

    switch (g->state) {
    case GS_READY:
    case GS_PLAY:
        update_play(g, dt);
        if (ui_back_pressed() || ui_key_pressed(SDLK_P)) game_pause();
        break;
    case GS_CLEAR:
        update_clear(g, dt);
        if (g->clear_t < 2.3f && (ui_key_pressed(SDLK_SPACE) || ui_key_pressed(SDLK_RETURN) || ui_mouse_pressed()))
            g->clear_t = 0;
        break;
    case GS_OVER:
    case GS_DONE:
        g->end_t += dt;
        update_parts(g, dt);
        update_balls(g, dt * 0.5f);
        break;
    case GS_PAUSE:
        break;
    }

    draw_game(g);

    bool playing = g->state == GS_PLAY || g->state == GS_READY;
    ui_set_nav(!playing);
    if (playing) {
        input_draw_touch();
        /* bouton pause (souris / tactile) */
        if (ui_button("II", GAME_W - 40, 4, 32, 28, BTN_TOOL)) game_pause();
    } else if (g->state == GS_PAUSE) {
        overlay_pause(g);
    } else if (g->state == GS_OVER || g->state == GS_DONE) {
        overlay_end(g);
    }
}
