/*
 * Editeur de niveaux.
 *
 * Ecran logique 1100x640 : le terrain (800x540, memes coordonnees que le jeu)
 * est dessine en (FX, FY), avec une barre d'actions en haut, un panneau
 * d'outils/proprietes a droite et une barre d'etat en bas.
 * Chaque niveau est un fichier .lvl (voir docs/LEVEL_FORMAT.md).
 */
#include "app.h"
#include "config.h"
#include "font.h"
#include "game.h"
#include "gfx.h"
#include "level.h"
#include "settings.h"
#include "storage.h"
#include "ui.h"

#include <math.h>
#include <string.h>

#define ED_W 1100
#define ED_H 640
#define FX 16.0f
#define FY 56.0f
#define PANEL_X 832.0f
#define PANEL_W 252.0f
#define UNDO_MAX 64
#define GRID 10.0f

typedef enum Tool {
    TOOL_SELECT,
    TOOL_PLATFORM,
    TOOL_LADDER,
    TOOL_BALL,
    TOOL_START1,
    TOOL_START2,
    TOOL_LASER,
    TOOL_ERASE,
    TOOL_COUNT
} Tool;

static const char *const TOOL_NAMES[TOOL_COUNT] = { "Sélection (V)", "Plateforme (P)", "Échelle (L)", "Boule (B)",
                                                    "Départ J1 (1)", "Départ J2 (2)", "Laser (R)",
                                                    "Gomme (E)" };
static const char *const TOOL_HINTS[TOOL_COUNT] = {
    "Clic : sélectionner · glisser : déplacer · poignées : redimensionner · Suppr : effacer",
    "Glisser horizontalement pour tracer une plateforme (clic simple : largeur 120)",
    "Clic sur une plateforme : échelle jusqu'à la surface du dessous",
    "Clic : poser une boule (taille et direction dans le panneau)",
    "Clic : position de départ du joueur 1 (posé sur la surface en dessous)",
    "Clic : position de départ du joueur 2 (posé sur la surface en dessous)",
    "Clic : laser vertical (les boules rebondissent dessus, les joueurs le traversent)",
    "Clic : effacer l'objet sous le curseur (clic droit : partout)",
};

typedef enum ObjKind { OBJ_NONE, OBJ_PLATFORM, OBJ_LADDER, OBJ_BALL, OBJ_START, OBJ_LASER } ObjKind;
typedef struct ObjRef { ObjKind kind; int index; } ObjRef;

typedef enum Drag {
    DRAG_NONE,
    DRAG_MOVE,
    DRAG_RESIZE_L,
    DRAG_RESIZE_R,
    DRAG_LADDER_TOP,
    DRAG_LADDER_BOTTOM,
    DRAG_LASER_TOP,
    DRAG_LASER_BOTTOM,
    DRAG_NEW_PLATFORM,
} Drag;

typedef enum Modal { MODAL_NONE, MODAL_OPEN, MODAL_SAVE_AS, MODAL_CONFIRM } Modal;
typedef enum Pending { PENDING_NONE, PENDING_NEW, PENDING_OPEN, PENDING_MENU } Pending;

static struct {
    bool inited;
    Level lvl;
    LevelSource src;
    char file[LEVEL_FILE_MAX];
    bool has_file;
    bool dirty;
    char name_seen[LEVEL_NAME_MAX];

    Tool tool;
    int ball_size, ball_dir;
    bool grid, paths;
    int test_players;

    ObjRef sel;
    Drag drag;
    bool drag_changed;
    float grab_dx, grab_dy;       /* decalage pointeur / objet */
    float anchor_x, anchor_y;     /* nouvelle plateforme */
    float keep;                   /* longueur d'echelle ou bord droit conserve */

    Level undo[UNDO_MAX];
    int undo_n;
    Level redo[UNDO_MAX];
    int redo_n;

    char status[200];
    bool status_err;
    float status_t;

    Modal modal;
    Pending pending;
    char pending_file[LEVEL_FILE_MAX];
    LevelSource pending_src;
    char confirm_msg[160];

    LevelEntry entries[MAX_LEVEL_ENTRIES];
    int entry_n;
    LevelSource open_tab;
    float open_scroll;

    char save_name[LEVEL_NAME_MAX];
    bool save_to_campaign;

    Backdrop bg;
    int bg_hue;
    double t;
} E;

/* ------------------------------------------------------------------------ */
/* Utilitaires                                                              */

static void status(bool err, const char *fmt, ...) SDL_PRINTF_VARARG_FUNC(2);
static void status(bool err, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(E.status, sizeof E.status, fmt, ap);
    va_end(ap);
    E.status_err = err;
    E.status_t = 6;
}

static float snapv(float v)
{
    if (!E.grid || (ui_mods() & SDL_KMOD_SHIFT)) return roundf(v);
    return roundf(v / GRID) * GRID;
}

static float fmx(void) { return ui_mx() - FX; }
static float fmy(void) { return ui_my() - FY; }
static bool in_field(void) { return fmx() >= 0 && fmx() < GAME_W && fmy() >= 0 && fmy() < GAME_H; }

static void checkpoint(void)
{
    if (E.undo_n == UNDO_MAX) {
        memmove(E.undo, E.undo + 1, sizeof(Level) * (UNDO_MAX - 1));
        E.undo_n--;
    }
    E.undo[E.undo_n++] = E.lvl;
    E.redo_n = 0;
    E.dirty = true;
}

static void undo(void)
{
    if (!E.undo_n) { status(false, "Rien à annuler"); return; }
    if (E.redo_n < UNDO_MAX) E.redo[E.redo_n++] = E.lvl;
    E.lvl = E.undo[--E.undo_n];
    E.sel.kind = OBJ_NONE;
    E.dirty = true;
    status(false, "Annulé");
}

static void redo(void)
{
    if (!E.redo_n) { status(false, "Rien à rétablir"); return; }
    if (E.undo_n < UNDO_MAX) E.undo[E.undo_n++] = E.lvl;
    E.lvl = E.redo[--E.redo_n];
    E.sel.kind = OBJ_NONE;
    E.dirty = true;
    status(false, "Rétabli");
}

static void reset_history(void)
{
    E.undo_n = E.redo_n = 0;
    E.sel.kind = OBJ_NONE;
    E.drag = DRAG_NONE;
    E.dirty = false;
    SDL_strlcpy(E.name_seen, E.lvl.name, sizeof E.name_seen);
}

static void ensure_init(void)
{
    if (E.inited) return;
    E.inited = true;
    E.ball_size = 2;
    E.ball_dir = 1;
    E.grid = true;
    E.paths = true;
    E.test_players = 1;
    E.tool = TOOL_SELECT;
    E.bg_hue = -1;
    level_init(&E.lvl);
    reset_history();
}

/* ------------------------------------------------------------------------ */
/* Fichiers                                                                 */

void editor_new_level(void)
{
    ensure_init();
    level_init(&E.lvl);
    E.has_file = false;
    E.file[0] = '\0';
    E.src = LEVELSRC_USER;
    reset_history();
    status(false, "Nouveau niveau");
}

void editor_open_level(int src, const char *file)
{
    ensure_init();
    Level lvl;
    char err[128];
    if (!storage_load((LevelSource)src, file, &lvl, err, sizeof err)) {
        status(true, "Ouverture impossible : %s", err);
        return;
    }
    E.lvl = lvl;
    E.src = (LevelSource)src;
    SDL_strlcpy(E.file, file, sizeof E.file);
    E.has_file = true;
    reset_history();
    if (src == LEVELSRC_CAMPAIGN && !storage_writable(LEVELSRC_CAMPAIGN))
        status(false, "Niveau de campagne ouvert en copie : « Enregistrer » créera un niveau perso");
    else
        status(false, "Ouvert : %s", file);
}

static void open_save_as(void)
{
    E.modal = MODAL_SAVE_AS;
    SDL_strlcpy(E.save_name, E.lvl.name, sizeof E.save_name);
    E.save_to_campaign = E.src == LEVELSRC_CAMPAIGN && storage_writable(LEVELSRC_CAMPAIGN);
    ui_text_blur();
}

static bool save_to(LevelSource src, const char *file)
{
    char err[160];
    level_clamp(&E.lvl);
    if (!storage_save(src, file, &E.lvl, err, sizeof err)) {
        status(true, "Échec de l'enregistrement : %s", err);
        return false;
    }
    E.src = src;
    SDL_strlcpy(E.file, file, sizeof E.file);
    E.has_file = true;
    E.dirty = false;
    char msg[160];
    int problems = level_validate(&E.lvl, msg, sizeof msg);
    if (problems) status(false, "Enregistré (%s) · attention : %s", file, msg);
    else status(false, "Enregistré : %s%s", src == LEVELSRC_USER ? storage_user_levels_dir() : "campagne/", file);
    return true;
}

static void save(void)
{
    if (E.has_file && storage_writable(E.src)) save_to(E.src, E.file);
    else open_save_as();
}

static void test_level(void)
{
    level_clamp(&E.lvl);
    if (E.lvl.ball_count == 0) {
        status(true, "Impossible de tester : il faut au moins une boule");
        return;
    }
    ui_text_blur();
    game_start_test(&E.lvl, E.test_players);
}

void editor_resume_from_test(void)
{
    app_set_scene(SCENE_EDITOR);
    status(false, "Retour du test");
}

static void ask_confirm(Pending p, const char *msg)
{
    E.pending = p;
    SDL_strlcpy(E.confirm_msg, msg, sizeof E.confirm_msg);
    E.modal = MODAL_CONFIRM;
}

static void do_pending(void)
{
    switch (E.pending) {
    case PENDING_NEW: editor_new_level(); break;
    case PENDING_OPEN: editor_open_level((int)E.pending_src, E.pending_file); break;
    case PENDING_MENU: app_set_scene(SCENE_MENU); break;
    default: break;
    }
    E.pending = PENDING_NONE;
}

static void request(Pending p)
{
    if (E.dirty) {
        E.pending = p;
        ask_confirm(p, "Le niveau a des modifications non enregistrées. Les abandonner ?");
    } else {
        E.pending = p;
        do_pending();
    }
}

static void open_dialog(void)
{
    E.modal = MODAL_OPEN;
    E.open_scroll = 0;
    E.entry_n = storage_list(E.open_tab, E.entries, MAX_LEVEL_ENTRIES);
    ui_text_blur();
}

/* ------------------------------------------------------------------------ */
/* Selection / edition                                                      */

static float ball_r(int size) { return BALL_R[size < 0 ? 0 : (size > 3 ? 3 : size)]; }

static ObjRef hit_test(float x, float y)
{
    ObjRef r = { OBJ_NONE, -1 };
    for (int i = E.lvl.ball_count - 1; i >= 0; i--) {
        const LevelBall *b = &E.lvl.balls[i];
        float rr = ball_r(b->size) + 4, dx = x - b->x, dy = y - b->y;
        if (dx * dx + dy * dy < rr * rr) return (ObjRef){ OBJ_BALL, i };
    }
    for (int i = 0; i < 2; i++) {
        const LevelStart *s = &E.lvl.start[i];
        if (x > s->x - 16 && x < s->x + 16 && y > s->y - 48 && y < s->y + 2) return (ObjRef){ OBJ_START, i };
    }
    for (int i = E.lvl.laser_count - 1; i >= 0; i--) {
        const LevelLaser *z = &E.lvl.lasers[i];
        if (fabsf(x - z->x) <= 9 && y >= z->top - 2 && y <= z->bottom + 2) return (ObjRef){ OBJ_LASER, i };
    }
    for (int i = E.lvl.ladder_count - 1; i >= 0; i--) {
        const LevelLadder *l = &E.lvl.ladders[i];
        if (x >= l->x && x <= l->x + LADDER_W && y >= l->top - 8 && y <= l->bottom) return (ObjRef){ OBJ_LADDER, i };
    }
    for (int i = E.lvl.platform_count - 1; i >= 0; i--) {
        const LevelPlatform *p = &E.lvl.platforms[i];
        if (x >= p->x - 2 && x <= p->x + p->w + 2 && y >= p->y - 5 && y <= p->y + PLAT_H + 5)
            return (ObjRef){ OBJ_PLATFORM, i };
    }
    return r;
}

static void delete_obj(ObjRef o)
{
    switch (o.kind) {
    case OBJ_PLATFORM:
        checkpoint();
        memmove(&E.lvl.platforms[o.index], &E.lvl.platforms[o.index + 1],
                sizeof(LevelPlatform) * (size_t)(E.lvl.platform_count - o.index - 1));
        E.lvl.platform_count--;
        break;
    case OBJ_LADDER:
        checkpoint();
        memmove(&E.lvl.ladders[o.index], &E.lvl.ladders[o.index + 1],
                sizeof(LevelLadder) * (size_t)(E.lvl.ladder_count - o.index - 1));
        E.lvl.ladder_count--;
        break;
    case OBJ_LASER:
        checkpoint();
        memmove(&E.lvl.lasers[o.index], &E.lvl.lasers[o.index + 1],
                sizeof(LevelLaser) * (size_t)(E.lvl.laser_count - o.index - 1));
        E.lvl.laser_count--;
        break;
    case OBJ_BALL:
        checkpoint();
        memmove(&E.lvl.balls[o.index], &E.lvl.balls[o.index + 1],
                sizeof(LevelBall) * (size_t)(E.lvl.ball_count - o.index - 1));
        E.lvl.ball_count--;
        break;
    case OBJ_START: status(true, "Les départs des joueurs ne peuvent pas être supprimés (déplace-les)"); return;
    default: return;
    }
    if (E.sel.kind == o.kind) E.sel.kind = OBJ_NONE;
}

static void duplicate_sel(void)
{
    ObjRef s = E.sel;
    if (s.kind == OBJ_PLATFORM && E.lvl.platform_count < LEVEL_MAX_PLATFORMS) {
        checkpoint();
        LevelPlatform p = E.lvl.platforms[s.index];
        p.y -= 40;
        E.lvl.platforms[E.lvl.platform_count] = p;
        E.sel.index = E.lvl.platform_count++;
    } else if (s.kind == OBJ_LADDER && E.lvl.ladder_count < LEVEL_MAX_LADDERS) {
        checkpoint();
        LevelLadder l = E.lvl.ladders[s.index];
        l.x += 40;
        E.lvl.ladders[E.lvl.ladder_count] = l;
        E.sel.index = E.lvl.ladder_count++;
    } else if (s.kind == OBJ_LASER && E.lvl.laser_count < LEVEL_MAX_LASERS) {
        checkpoint();
        LevelLaser z = E.lvl.lasers[s.index];
        z.x += 40;
        E.lvl.lasers[E.lvl.laser_count] = z;
        E.sel.index = E.lvl.laser_count++;
    } else if (s.kind == OBJ_BALL && E.lvl.ball_count < LEVEL_MAX_BALLS) {
        checkpoint();
        LevelBall b = E.lvl.balls[s.index];
        b.x += 60;
        b.dir = -b.dir;
        E.lvl.balls[E.lvl.ball_count] = b;
        E.sel.index = E.lvl.ball_count++;
    } else {
        return;
    }
    level_clamp(&E.lvl);
    status(false, "Dupliqué");
}

static void nudge(float dx, float dy)
{
    ObjRef s = E.sel;
    if (s.kind == OBJ_NONE) return;
    checkpoint();
    switch (s.kind) {
    case OBJ_PLATFORM: E.lvl.platforms[s.index].x += dx; E.lvl.platforms[s.index].y += dy; break;
    case OBJ_LADDER:
        E.lvl.ladders[s.index].x += dx;
        E.lvl.ladders[s.index].top += dy;
        E.lvl.ladders[s.index].bottom += dy;
        break;
    case OBJ_LASER:
        E.lvl.lasers[s.index].x += dx;
        E.lvl.lasers[s.index].top += dy;
        E.lvl.lasers[s.index].bottom += dy;
        break;
    case OBJ_BALL: E.lvl.balls[s.index].x += dx; E.lvl.balls[s.index].y += dy; break;
    case OBJ_START: E.lvl.start[s.index].x += dx; E.lvl.start[s.index].y += dy; break;
    default: break;
    }
    level_clamp(&E.lvl);
}

static void set_start(int k, float x, float y)
{
    E.lvl.start[k].x = x;
    E.lvl.start[k].y = level_surface_below(&E.lvl, x, y);
}

static void place_ladder(float x, float y)
{
    if (E.lvl.ladder_count >= LEVEL_MAX_LADDERS) {
        status(true, "Trop d'échelles (max %d)", LEVEL_MAX_LADDERS);
        return;
    }
    float top = snapv(y), from = y + 1;
    for (int i = 0; i < E.lvl.platform_count; i++) {
        const LevelPlatform *p = &E.lvl.platforms[i];
        if (x >= p->x && x <= p->x + p->w && fabsf(y - p->y) < 22) {
            top = p->y;
            from = p->y + PLAT_H + 1;
            break;
        }
    }
    float bottom = level_surface_below(&E.lvl, x, from);
    if (bottom - top < 30) {
        status(true, "Pas assez de place sous ce point pour une échelle");
        return;
    }
    checkpoint();
    LevelLadder l = { snapv(x - LADDER_W / 2.0f), top, bottom };
    E.lvl.ladders[E.lvl.ladder_count] = l;
    E.sel = (ObjRef){ OBJ_LADDER, E.lvl.ladder_count++ };
    level_clamp(&E.lvl);
}

/* Laser vertical : du dessous de l'obstacle au-dessus (plafond ou plateforme)
 * jusqu'a la surface en dessous. */
static void place_laser(float x, float y)
{
    if (E.lvl.laser_count >= LEVEL_MAX_LASERS) {
        status(true, "Trop de lasers (max %d)", LEVEL_MAX_LASERS);
        return;
    }
    float top = CEIL_Y;
    for (int i = 0; i < E.lvl.platform_count; i++) {
        const LevelPlatform *p = &E.lvl.platforms[i];
        if (x >= p->x && x <= p->x + p->w && p->y + PLAT_H <= y && p->y + PLAT_H > top) top = p->y + PLAT_H;
    }
    float bottom = level_surface_below(&E.lvl, x, y);
    if (bottom - top < 30) {
        status(true, "Pas assez de place pour un laser ici");
        return;
    }
    checkpoint();
    LevelLaser z = { snapv(x), top, bottom };
    E.lvl.lasers[E.lvl.laser_count] = z;
    E.sel = (ObjRef){ OBJ_LASER, E.lvl.laser_count++ };
    level_clamp(&E.lvl);
}

static void place_ball(float x, float y)
{
    if (E.lvl.ball_count >= LEVEL_MAX_BALLS) {
        status(true, "Trop de boules (max %d)", LEVEL_MAX_BALLS);
        return;
    }
    checkpoint();
    LevelBall b = { snapv(x), snapv(y), E.ball_size, E.ball_dir };
    E.lvl.balls[E.lvl.ball_count] = b;
    E.sel = (ObjRef){ OBJ_BALL, E.lvl.ball_count++ };
    level_clamp(&E.lvl);
}

/* Poignees de la selection : retourne le mode de glisser si touchee. */
static Drag handle_at(float x, float y)
{
    if (E.sel.kind == OBJ_PLATFORM) {
        const LevelPlatform *p = &E.lvl.platforms[E.sel.index];
        if (y >= p->y - 6 && y <= p->y + PLAT_H + 6) {
            if (fabsf(x - p->x) < 7) return DRAG_RESIZE_L;
            if (fabsf(x - (p->x + p->w)) < 7) return DRAG_RESIZE_R;
        }
    } else if (E.sel.kind == OBJ_LADDER) {
        const LevelLadder *l = &E.lvl.ladders[E.sel.index];
        if (x >= l->x - 4 && x <= l->x + LADDER_W + 4) {
            if (fabsf(y - l->top) < 7) return DRAG_LADDER_TOP;
            if (fabsf(y - l->bottom) < 7) return DRAG_LADDER_BOTTOM;
        }
    } else if (E.sel.kind == OBJ_LASER) {
        const LevelLaser *z = &E.lvl.lasers[E.sel.index];
        if (fabsf(x - z->x) <= 10) {
            if (fabsf(y - z->top - 4) < 7) return DRAG_LASER_TOP;
            if (fabsf(y - z->bottom + 4) < 7) return DRAG_LASER_BOTTOM;
        }
    }
    return DRAG_NONE;
}

static void begin_move(ObjRef o, float x, float y)
{
    E.sel = o;
    E.drag = DRAG_MOVE;
    E.drag_changed = false;
    switch (o.kind) {
    case OBJ_PLATFORM: E.grab_dx = x - E.lvl.platforms[o.index].x; E.grab_dy = y - E.lvl.platforms[o.index].y; break;
    case OBJ_LADDER:
        E.grab_dx = x - E.lvl.ladders[o.index].x;
        E.grab_dy = y - E.lvl.ladders[o.index].top;
        E.keep = E.lvl.ladders[o.index].bottom - E.lvl.ladders[o.index].top;
        break;
    case OBJ_LASER:
        E.grab_dx = x - E.lvl.lasers[o.index].x;
        E.grab_dy = y - E.lvl.lasers[o.index].top;
        E.keep = E.lvl.lasers[o.index].bottom - E.lvl.lasers[o.index].top;
        break;
    case OBJ_BALL: E.grab_dx = x - E.lvl.balls[o.index].x; E.grab_dy = y - E.lvl.balls[o.index].y; break;
    case OBJ_START: E.grab_dx = x - E.lvl.start[o.index].x; E.grab_dy = 0; break;
    default: E.drag = DRAG_NONE; break;
    }
}

static void field_press(float x, float y)
{
    if (E.tool == TOOL_SELECT || E.tool == TOOL_BALL) {
        Drag h = handle_at(x, y);
        if (h != DRAG_NONE && E.tool == TOOL_SELECT) {
            E.drag = h;
            E.drag_changed = false;
            if (h == DRAG_RESIZE_L) E.keep = E.lvl.platforms[E.sel.index].x + E.lvl.platforms[E.sel.index].w;
            return;
        }
        ObjRef o = hit_test(x, y);
        if (o.kind != OBJ_NONE && (E.tool == TOOL_SELECT || o.kind == OBJ_BALL)) {
            begin_move(o, x, y);
            if (o.kind == OBJ_BALL) {
                E.ball_size = E.lvl.balls[o.index].size;
                E.ball_dir = E.lvl.balls[o.index].dir;
            }
            return;
        }
        if (E.tool == TOOL_BALL) place_ball(x, y);
        else E.sel.kind = OBJ_NONE;
        return;
    }
    switch (E.tool) {
    case TOOL_PLATFORM:
        if (E.lvl.platform_count >= LEVEL_MAX_PLATFORMS) {
            status(true, "Trop de plateformes (max %d)", LEVEL_MAX_PLATFORMS);
            return;
        }
        E.drag = DRAG_NEW_PLATFORM;
        E.anchor_x = snapv(x);
        E.anchor_y = snapv(y);
        break;
    case TOOL_LADDER: place_ladder(x, y); break;
    case TOOL_LASER: place_laser(x, y); break;
    case TOOL_START1:
    case TOOL_START2:
        checkpoint();
        set_start(E.tool == TOOL_START1 ? 0 : 1, snapv(x), y);
        level_clamp(&E.lvl);
        E.sel = (ObjRef){ OBJ_START, E.tool == TOOL_START1 ? 0 : 1 };
        break;
    case TOOL_ERASE: delete_obj(hit_test(x, y)); break;
    default: break;
    }
}

static void field_drag(float x, float y)
{
    if (E.drag == DRAG_NONE || E.drag == DRAG_NEW_PLATFORM) return;
    if (!E.drag_changed) {
        checkpoint();
        E.drag_changed = true;
    }
    ObjRef s = E.sel;
    switch (E.drag) {
    case DRAG_MOVE:
        switch (s.kind) {
        case OBJ_PLATFORM:
            E.lvl.platforms[s.index].x = snapv(x - E.grab_dx);
            E.lvl.platforms[s.index].y = snapv(y - E.grab_dy);
            break;
        case OBJ_LADDER: {
            LevelLadder *l = &E.lvl.ladders[s.index];
            l->x = snapv(x - E.grab_dx);
            l->top = snapv(y - E.grab_dy);
            l->bottom = l->top + E.keep;
            if (l->bottom > FLOOR_Y) { l->bottom = FLOOR_Y; l->top = FLOOR_Y - E.keep; }
            break;
        }
        case OBJ_LASER: {
            LevelLaser *z = &E.lvl.lasers[s.index];
            z->x = snapv(x - E.grab_dx);
            z->top = snapv(y - E.grab_dy);
            z->bottom = z->top + E.keep;
            if (z->bottom > FLOOR_Y) { z->bottom = FLOOR_Y; z->top = FLOOR_Y - E.keep; }
            if (z->top < CEIL_Y) { z->top = CEIL_Y; z->bottom = CEIL_Y + E.keep; }
            break;
        }
        case OBJ_BALL:
            E.lvl.balls[s.index].x = snapv(x - E.grab_dx);
            E.lvl.balls[s.index].y = snapv(y - E.grab_dy);
            break;
        case OBJ_START: set_start(s.index, snapv(x - E.grab_dx), y - 20); break;
        default: break;
        }
        break;
    case DRAG_RESIZE_L: {
        LevelPlatform *p = &E.lvl.platforms[s.index];
        float nx = snapv(x);
        if (nx > E.keep - 16) nx = E.keep - 16;
        p->x = nx;
        p->w = E.keep - nx;
        break;
    }
    case DRAG_RESIZE_R: {
        LevelPlatform *p = &E.lvl.platforms[s.index];
        float w = snapv(x) - p->x;
        p->w = w < 16 ? 16 : w;
        break;
    }
    case DRAG_LADDER_TOP: E.lvl.ladders[s.index].top = snapv(y); break;
    case DRAG_LADDER_BOTTOM: E.lvl.ladders[s.index].bottom = snapv(y); break;
    case DRAG_LASER_TOP: E.lvl.lasers[s.index].top = snapv(y); break;
    case DRAG_LASER_BOTTOM: E.lvl.lasers[s.index].bottom = snapv(y); break;
    default: break;
    }
    level_clamp(&E.lvl);
}

static void new_platform_rect(float x, float *px, float *pw)
{
    float x1 = snapv(x);
    float a = E.anchor_x < x1 ? E.anchor_x : x1, b = E.anchor_x < x1 ? x1 : E.anchor_x;
    if (b - a < 16) {
        a = E.anchor_x - 60;
        b = E.anchor_x + 60;
    }
    *px = a;
    *pw = b - a;
}

static void field_release(float x, float y)
{
    if (E.drag == DRAG_NEW_PLATFORM) {
        float px, pw;
        new_platform_rect(x, &px, &pw);
        checkpoint();
        LevelPlatform p = { px, E.anchor_y, pw };
        E.lvl.platforms[E.lvl.platform_count] = p;
        E.sel = (ObjRef){ OBJ_PLATFORM, E.lvl.platform_count++ };
        level_clamp(&E.lvl);
    }
    E.drag = DRAG_NONE;
}

/* ------------------------------------------------------------------------ */
/* Raccourcis clavier                                                       */

static void shortcuts(void)
{
    if (ui_text_active()) return;
    bool ctrl = (ui_mods() & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
    bool shift = (ui_mods() & SDL_KMOD_SHIFT) != 0;
    if (ctrl) {
        if (ui_key_pressed(SDLK_Z)) { if (shift) redo(); else undo(); }
        if (ui_key_pressed(SDLK_Y)) redo();
        if (ui_key_pressed(SDLK_S)) { if (shift) open_save_as(); else save(); }
        if (ui_key_pressed(SDLK_O)) open_dialog();
        if (ui_key_pressed(SDLK_N)) request(PENDING_NEW);
        if (ui_key_pressed(SDLK_D)) duplicate_sel();
        return;
    }
    static const SDL_Keycode tool_keys[TOOL_COUNT] = { SDLK_V, SDLK_P, SDLK_L, SDLK_B, SDLK_1, SDLK_2, SDLK_R, SDLK_E };
    for (int i = 0; i < TOOL_COUNT; i++)
        if (ui_key_pressed(tool_keys[i])) E.tool = (Tool)i;
    if (ui_key_pressed(SDLK_G)) E.grid = !E.grid;
    if (ui_key_pressed(SDLK_F5) || ui_key_pressed(SDLK_T)) test_level();
    if (ui_key_pressed(SDLK_DELETE) || ui_key_pressed(SDLK_BACKSPACE)) delete_obj(E.sel);
    float step = shift ? GRID : 1;
    if (ui_key_pressed(SDLK_LEFT)) nudge(-step, 0);
    if (ui_key_pressed(SDLK_RIGHT)) nudge(step, 0);
    if (ui_key_pressed(SDLK_UP)) nudge(0, -step);
    if (ui_key_pressed(SDLK_DOWN)) nudge(0, step);
    if (ui_key_pressed(SDLK_X) && E.sel.kind == OBJ_BALL) {
        checkpoint();
        E.lvl.balls[E.sel.index].dir *= -1;
        E.ball_dir = E.lvl.balls[E.sel.index].dir;
    }
}

/* ------------------------------------------------------------------------ */
/* Dessin du terrain                                                        */

static void draw_paths(void)
{
    static const Uint32 cols[4] = { 0xFF7B98, 0xFFD56B, 0x86F5B2, 0x86CCFF };
    for (int i = 0; i < E.lvl.ball_count; i++) {
        const LevelBall *lb = &E.lvl.balls[i];
        Ball b = game_make_ball(lb->x, lb->y, lb->size, lb->dir);
        const float dt = 1.0f / 60, speed = DIFFICULTIES[1].speed;
        for (int s = 0; s < 240; s++) {
            game_ball_step(&E.lvl, &b, dt * speed);
            if (s % 5 == 0) gfx_circle(b.x, b.y, 1.6f, rgba(cols[lb->size], 0.55f * (1 - s / 300.0f)));
        }
    }
}

static void draw_selection(void)
{
    Color hi = rgb(0xFFE08A);
    ObjRef s = E.sel;
    switch (s.kind) {
    case OBJ_PLATFORM: {
        const LevelPlatform *p = &E.lvl.platforms[s.index];
        gfx_rrect_outline(p->x - 3, p->y - 3, p->w + 6, PLAT_H + 6, 5, 2, hi);
        gfx_rect(p->x - 4, p->y + PLAT_H / 2.0f - 6, 8, 12, hi);
        gfx_rect(p->x + p->w - 4, p->y + PLAT_H / 2.0f - 6, 8, 12, hi);
        break;
    }
    case OBJ_LASER: {
        const LevelLaser *z = &E.lvl.lasers[s.index];
        gfx_rrect_outline(z->x - 11, z->top - 3, 22, z->bottom - z->top + 6, 4, 2, hi);
        gfx_rect(z->x - 6, z->top, 12, 8, hi);
        gfx_rect(z->x - 6, z->bottom - 8, 12, 8, hi);
        break;
    }
    case OBJ_LADDER: {
        const LevelLadder *l = &E.lvl.ladders[s.index];
        gfx_rrect_outline(l->x - 3, l->top - 9, LADDER_W + 6, l->bottom - l->top + 12, 4, 2, hi);
        gfx_rect(l->x + LADDER_W / 2.0f - 6, l->top - 4, 12, 8, hi);
        gfx_rect(l->x + LADDER_W / 2.0f - 6, l->bottom - 4, 12, 8, hi);
        break;
    }
    case OBJ_BALL: {
        const LevelBall *b = &E.lvl.balls[s.index];
        gfx_ring(b->x, b->y, ball_r(b->size) + 5, 2, hi);
        break;
    }
    case OBJ_START: {
        const LevelStart *st = &E.lvl.start[s.index];
        gfx_rrect_outline(st->x - 18, st->y - 50, 36, 52, 6, 2, hi);
        break;
    }
    default: break;
    }
}

static void draw_field(void)
{
    if (E.bg_hue != E.lvl.hue) {
        backdrop_build(&E.bg, E.lvl.hue, 1234);
        E.bg_hue = E.lvl.hue;
    }
    E.bg.world = game_world_for(&E.lvl, 0);
    gfx_set_offset(FX, FY);
    SDL_FRect clip = { 0, 0, GAME_W, GAME_H };
    gfx_clip(&clip);
    draw_backdrop(&E.bg, E.t);

    if (E.grid) {
        for (int x = 0; x <= GAME_W; x += 20)
            gfx_rect((float)x, CEIL_Y, 1, FLOOR_Y - CEIL_Y, rgba(0xFFFFFF, x % 100 == 0 ? 0.12f : 0.05f));
        for (int y = 40; y <= FLOOR_Y; y += 20)
            gfx_rect(0, (float)y, GAME_W, 1, rgba(0xFFFFFF, y % 100 == 0 ? 0.12f : 0.05f));
    }
    /* zones non jouables */
    gfx_rect(0, 0, GAME_W, CEIL_Y, rgba(0x040A1E, g_settings.retro ? 0.35f : 0.78f));
    font_draw("bandeau du score (hors jeu)", GAME_W / 2.0f, CEIL_Y / 2.0f, 12, rgba(0xEAF6FF, 0.35f), ALIGN_CENTER);
    gfx_rect(0, FLOOR_Y, GAME_W, 1, rgba(0xFFFFFF, 0.4f));

    for (int i = 0; i < E.lvl.ladder_count; i++)
        draw_ladder(E.lvl.ladders[i].x, E.lvl.ladders[i].top, E.lvl.ladders[i].bottom);
    for (int i = 0; i < E.lvl.platform_count; i++)
        draw_platform(E.lvl.platforms[i].x, E.lvl.platforms[i].y, E.lvl.platforms[i].w);
    for (int i = 0; i < E.lvl.laser_count; i++)
        draw_laser(E.lvl.lasers[i].x, E.lvl.lasers[i].top, E.lvl.lasers[i].bottom, E.t);
    if (E.paths) draw_paths();
    for (int i = 0; i < E.lvl.ball_count; i++) {
        const LevelBall *b = &E.lvl.balls[i];
        draw_ball(b->x, b->y, b->size, false);
        /* fleche de direction */
        float r = ball_r(b->size);
        gfx_arrow(b->x + b->dir * (r + 9), b->y, 12, b->dir > 0 ? 1 : 3, rgba(0xFFFFFF, 0.85f));
    }
    for (int k = 0; k < 2; k++) {
        const LevelStart *s = &E.lvl.start[k];
        draw_player_at(s->x, s->y, k, k ? -1 : 1, k == 1 && E.test_players < 2 ? 0.45f : 0.9f);
        font_draw(k ? "J2" : "J1", s->x, s->y - 58, 12, k ? rgb(0xFF9AAE) : rgb(0x9CC6FF), ALIGN_CENTER);
    }

    if (E.drag == DRAG_NEW_PLATFORM) {
        float px, pw;
        new_platform_rect(fmx(), &px, &pw);
        gfx_set_alpha(0.6f);
        draw_platform(px, E.anchor_y, pw);
        gfx_set_alpha(1);
        char buf[32];
        SDL_snprintf(buf, sizeof buf, "%d px", (int)pw);
        font_draw_outlined(buf, px + pw / 2, E.anchor_y - 14, 12, rgb(0xFFFFFF), ALIGN_CENTER);
    }
    draw_selection();

    /* apercu de l'outil sous le curseur */
    if (E.modal == MODAL_NONE && in_field() && E.drag == DRAG_NONE) {
        float x = fmx(), y = fmy();
        if (E.tool == TOOL_BALL && hit_test(x, y).kind != OBJ_BALL) {
            gfx_set_alpha(0.45f);
            draw_ball(snapv(x), snapv(y), E.ball_size, false);
            gfx_set_alpha(1);
        } else if (E.tool == TOOL_ERASE) {
            ObjRef o = hit_test(x, y);
            if (o.kind != OBJ_NONE && o.kind != OBJ_START) gfx_circle(x, y, 6, rgba(0xFF6B8B, 0.8f));
        } else if (E.tool == TOOL_START1 || E.tool == TOOL_START2) {
            draw_player_at(snapv(x), level_surface_below(&E.lvl, snapv(x), y), E.tool == TOOL_START2,
                           E.tool == TOOL_START2 ? -1 : 1, 0.4f);
        }
    }

    gfx_clip(NULL);
    gfx_set_offset(0, 0);
    gfx_rrect_outline(FX - 2, FY - 2, GAME_W + 4, GAME_H + 4, 4, 2, rgba(0x8CCDFF, 0.4f));
}

/* ------------------------------------------------------------------------ */
/* Panneaux                                                                 */

static void top_bar(void)
{
    float x = FX, y = 10, h = 36;
    if (ui_button("Nouveau", x, y, 92, h, BTN_NORMAL)) request(PENDING_NEW);
    x += 98;
    if (ui_button("Ouvrir…", x, y, 92, h, BTN_NORMAL)) open_dialog();
    x += 98;
    if (ui_button("Enregistrer", x, y, 112, h, BTN_NORMAL)) save();
    x += 118;
    if (ui_button("Enregistrer sous…", x, y, 150, h, BTN_NORMAL)) open_save_as();
    x += 168;
    if (ui_button("Annuler", x, y, 86, h, E.undo_n ? BTN_NORMAL : BTN_TOOL)) undo();
    x += 92;
    if (ui_button("Rétablir", x, y, 86, h, E.redo_n ? BTN_NORMAL : BTN_TOOL)) redo();
    x += 104;
    if (ui_button("Tester (T)", x, y, 120, h, BTN_BIG)) test_level();
    x += 128;
    if (ui_button(E.test_players == 1 ? "1 joueur" : "2 joueurs", x, y, 90, h, BTN_NORMAL))
        E.test_players = 3 - E.test_players;
    if (ui_button("Menu", ED_W - 16 - 90, y, 90, h, BTN_NORMAL)) request(PENDING_MENU);
}

static void header(const char *t, float y)
{
    font_draw(t, PANEL_X, y, 14, rgba(0x9FE0FF, 0.9f), ALIGN_LEFT);
    gfx_rect(PANEL_X, y + 10, PANEL_W, 1, rgba(0x8CCDFF, 0.25f));
}

static void ball_options(float y)
{
    bool editing = E.sel.kind == OBJ_BALL;
    header(editing ? "Boule sélectionnée" : "Nouvelle boule", y);
    y += 22;
    static const char *const sizes[4] = { "1", "2", "3", "4" };
    for (int i = 0; i < 4; i++) {
        if (ui_button(sizes[i], PANEL_X + i * 64.0f, y, 58, 32, E.ball_size == i ? BTN_TOOL_ON : BTN_TOOL)) {
            E.ball_size = i;
            if (editing) {
                checkpoint();
                E.lvl.balls[E.sel.index].size = i;
                level_clamp(&E.lvl);
            }
        }
    }
    y += 38;
    for (int d = 0; d < 2; d++) {
        int dir = d ? 1 : -1;
        if (ui_button(d ? "Vers la droite" : "Vers la gauche", PANEL_X + d * 128.0f, y, 122, 32,
                      E.ball_dir == dir ? BTN_TOOL_ON : BTN_TOOL)) {
            E.ball_dir = dir;
            if (editing) {
                checkpoint();
                E.lvl.balls[E.sel.index].dir = dir;
            }
        }
    }
}

static void selection_info(float y)
{
    char buf[96];
    ObjRef s = E.sel;
    Color c = rgba(0xEAF6FF, 0.85f);
    switch (s.kind) {
    case OBJ_PLATFORM: {
        const LevelPlatform *p = &E.lvl.platforms[s.index];
        header("Plateforme", y);
        SDL_snprintf(buf, sizeof buf, "x %d   y %d   largeur %d", (int)p->x, (int)p->y, (int)p->w);
        break;
    }
    case OBJ_LADDER: {
        const LevelLadder *l = &E.lvl.ladders[s.index];
        header("Échelle", y);
        SDL_snprintf(buf, sizeof buf, "x %d   haut %d   bas %d", (int)l->x, (int)l->top, (int)l->bottom);
        break;
    }
    case OBJ_LASER: {
        const LevelLaser *z = &E.lvl.lasers[s.index];
        header("Laser", y);
        SDL_snprintf(buf, sizeof buf, "x %d   haut %d   bas %d", (int)z->x, (int)z->top, (int)z->bottom);
        break;
    }
    case OBJ_START: {
        const LevelStart *st = &E.lvl.start[s.index];
        header(s.index ? "Départ joueur 2" : "Départ joueur 1", y);
        SDL_snprintf(buf, sizeof buf, "x %d   y %d", (int)st->x, (int)st->y);
        break;
    }
    case OBJ_BALL: {
        const LevelBall *b = &E.lvl.balls[s.index];
        SDL_snprintf(buf, sizeof buf, "x %d   y %d", (int)b->x, (int)b->y);
        font_draw(buf, PANEL_X, y, 13, c, ALIGN_LEFT);
        return;
    }
    default: return;
    }
    font_draw(buf, PANEL_X, y + 26, 13, c, ALIGN_LEFT);
    if (s.kind != OBJ_START) {
        if (ui_button("Dupliquer", PANEL_X, y + 44, 122, 30, BTN_TOOL)) duplicate_sel();
        if (ui_button("Supprimer", PANEL_X + 128, y + 44, 122, 30, BTN_DANGER)) delete_obj(s);
    }
}

static void level_props(float y)
{
    char buf[64];
    header("Niveau", y);
    y += 20;
    ui_textfield(E.lvl.name, sizeof E.lvl.name, PANEL_X, y, PANEL_W, 32, "Nom du niveau");
    y += 40;

    if (g_settings.retro) {
        /* style retro : choix du decor (la teinte ne sert qu'au style moderne) */
        static const char *const NAMES[BACKDROP_COUNT + 1] = { "Auto", "Lac", "Crépuscule", "Nuit" };
        font_draw("Décor", PANEL_X, y + 15, 13, rgba(0xEAF6FF, 0.8f), ALIGN_LEFT);
        if (ui_button("−", PANEL_X + 80, y, 34, 30, BTN_TOOL)) {
            checkpoint();
            E.lvl.backdrop = (E.lvl.backdrop + BACKDROP_COUNT) % (BACKDROP_COUNT + 1);
        }
        font_draw(NAMES[E.lvl.backdrop % (BACKDROP_COUNT + 1)], PANEL_X + 166, y + 15, 13, rgb(0xFFFFFF),
                  ALIGN_CENTER);
        if (ui_button("+", PANEL_X + 218, y, 34, 30, BTN_TOOL)) {
            checkpoint();
            E.lvl.backdrop = (E.lvl.backdrop + 1) % (BACKDROP_COUNT + 1);
        }
    } else {
        font_draw("Teinte", PANEL_X, y + 15, 13, rgba(0xEAF6FF, 0.8f), ALIGN_LEFT);
        if (ui_button("−", PANEL_X + 80, y, 34, 30, BTN_TOOL)) { checkpoint(); E.lvl.hue = (E.lvl.hue + 345) % 360; }
        gfx_rrect(PANEL_X + 120, y + 3, 92, 24, 6, hsl((float)E.lvl.hue + 15, 0.48f, 0.3f, 1.0f));
        SDL_snprintf(buf, sizeof buf, "%d°", E.lvl.hue);
        font_draw(buf, PANEL_X + 166, y + 15, 13, rgb(0xFFFFFF), ALIGN_CENTER);
        if (ui_button("+", PANEL_X + 218, y, 34, 30, BTN_TOOL)) { checkpoint(); E.lvl.hue = (E.lvl.hue + 15) % 360; }
    }
    y += 36;

    font_draw("Renforts", PANEL_X, y + 15, 13, rgba(0xEAF6FF, 0.8f), ALIGN_LEFT);
    /* cycle : defaut -> aucun -> 5 s ... 120 s */
    float r = E.lvl.reinforce;
    if (r == REINFORCE_DEFAULT) SDL_snprintf(buf, sizeof buf, "Selon difficulté");
    else if (r < 0) SDL_snprintf(buf, sizeof buf, "Aucun");
    else SDL_snprintf(buf, sizeof buf, "Toutes les %d s", (int)r);
    if (ui_button("−", PANEL_X + 80, y, 34, 30, BTN_TOOL)) {
        checkpoint();
        if (r == REINFORCE_DEFAULT) E.lvl.reinforce = 120;
        else if (r < 0) E.lvl.reinforce = REINFORCE_DEFAULT;
        else if (r <= 5) E.lvl.reinforce = REINFORCE_OFF;
        else E.lvl.reinforce = r - 5;
    }
    font_draw(buf, PANEL_X + 166, y + 15, 12, rgb(0xFFFFFF), ALIGN_CENTER);
    if (ui_button("+", PANEL_X + 218, y, 34, 30, BTN_TOOL)) {
        checkpoint();
        if (r == REINFORCE_DEFAULT) E.lvl.reinforce = REINFORCE_OFF;
        else if (r < 0) E.lvl.reinforce = 5;
        else if (r >= 120) E.lvl.reinforce = REINFORCE_DEFAULT;
        else E.lvl.reinforce = r + 5;
    }
    y += 40;
    if (ui_button(E.grid ? "Grille : oui (G)" : "Grille : non (G)", PANEL_X, y, 122, 30,
                  E.grid ? BTN_TOOL_ON : BTN_TOOL))
        E.grid = !E.grid;
    if (ui_button(E.paths ? "Trajectoires : oui" : "Trajectoires : non", PANEL_X + 128, y, 124, 30,
                  E.paths ? BTN_TOOL_ON : BTN_TOOL))
        E.paths = !E.paths;
}

static void right_panel(void)
{
    header("Outils", 64);
    for (int i = 0; i < TOOL_COUNT; i++) {
        float x = PANEL_X + (i % 2) * 128.0f, y = 82 + (i / 2) * 38.0f;
        if (ui_button(TOOL_NAMES[i], x, y, 122, 32, E.tool == (Tool)i ? BTN_TOOL_ON : BTN_TOOL)) E.tool = (Tool)i;
    }
    float y = 248;
    if (E.tool == TOOL_BALL || E.sel.kind == OBJ_BALL) {
        ball_options(y);
        y += 100;
        if (E.sel.kind == OBJ_BALL) selection_info(y);
    } else if (E.sel.kind != OBJ_NONE) {
        selection_info(y);
    } else {
        header("Astuce", y);
        font_draw("Clic droit : effacer · Maj : sans grille", PANEL_X, y + 24, 12, rgba(0xEAF6FF, 0.6f), ALIGN_LEFT);
        font_draw("Ctrl+Z / Ctrl+Y : annuler / rétablir", PANEL_X, y + 42, 12, rgba(0xEAF6FF, 0.6f), ALIGN_LEFT);
        font_draw("Ctrl+D : dupliquer · flèches : ajuster", PANEL_X, y + 60, 12, rgba(0xEAF6FF, 0.6f), ALIGN_LEFT);
        font_draw("X : inverser la direction d'une boule", PANEL_X, y + 78, 12, rgba(0xEAF6FF, 0.6f), ALIGN_LEFT);
    }
    level_props(424);
}

static void status_bar(float dt)
{
    float y = 604;
    gfx_rrect(FX, y, ED_W - 2 * FX, 30, 8, rgba(0x040A20, 0.75f));
    char buf[220];
    if (E.status_t > 0) {
        E.status_t -= dt;
        font_draw(E.status, FX + 10, y + 15, 13, E.status_err ? rgb(0xFF9AA8) : rgb(0x8FF0B0), ALIGN_LEFT);
    } else {
        font_draw(TOOL_HINTS[E.tool], FX + 10, y + 15, 13, rgba(0xEAF6FF, 0.7f), ALIGN_LEFT);
    }
    const char *src = E.src == LEVELSRC_CAMPAIGN ? "campagne" : "perso";
    SDL_snprintf(buf, sizeof buf, "%s%s%s · boules %d/%d · plateformes %d · échelles %d · lasers %d",
                 E.dirty ? "* " : "", E.has_file ? E.file : "(non enregistré)", E.has_file ? "" : "", E.lvl.ball_count,
                 LEVEL_MAX_BALLS, E.lvl.platform_count, E.lvl.ladder_count, E.lvl.laser_count);
    (void)src;
    float right = ED_W - FX - 10;
    if (in_field()) {
        char pos[32];
        SDL_snprintf(pos, sizeof pos, "x %d  y %d", (int)fmx(), (int)fmy());
        font_draw(pos, right, y + 15, 13, rgba(0xEAF6FF, 0.8f), ALIGN_RIGHT);
        right -= 100;
    }
    font_draw(buf, right, y + 15, 12, rgba(0xEAF6FF, 0.6f), ALIGN_RIGHT);
}

/* ------------------------------------------------------------------------ */
/* Fenetres modales                                                         */

static void modal_open(void)
{
    const float w = 640, h = 470, x = (ED_W - w) / 2, y = (ED_H - h) / 2;
    ui_dim(0, 0, ED_W, ED_H);
    ui_panel(x, y, w, h);
    font_draw_outlined("Ouvrir un niveau", x + w / 2, y + 30, 24, rgb(0xFFFFFF), ALIGN_CENTER);
    if (ui_button("Mes niveaux", x + w / 2 - 170, y + 54, 166, 32,
                  E.open_tab == LEVELSRC_USER ? BTN_SELECTED : BTN_NORMAL)) {
        E.open_tab = LEVELSRC_USER;
        open_dialog();
    }
    if (ui_button("Campagne", x + w / 2 + 4, y + 54, 166, 32,
                  E.open_tab == LEVELSRC_CAMPAIGN ? BTN_SELECTED : BTN_NORMAL)) {
        E.open_tab = LEVELSRC_CAMPAIGN;
        open_dialog();
    }
    const float lx = x + 20, ly = y + 98, lw = w - 40, lh = h - 160, row = 40;
    if (ui_hover(lx, ly, lw, lh)) E.open_scroll -= ui_wheel() * row;
    float max_scroll = E.entry_n * row - lh;
    if (E.open_scroll > max_scroll) E.open_scroll = max_scroll;
    if (E.open_scroll < 0) E.open_scroll = 0;
    SDL_FRect clip = { lx, ly, lw, lh };
    gfx_clip(&clip);
    if (E.entry_n == 0) font_draw("Aucun niveau", x + w / 2, ly + 30, 16, rgba(0xEAF6FF, 0.6f), ALIGN_CENTER);
    int chosen = -1;
    for (int i = 0; i < E.entry_n; i++) {
        float ry = ly + i * row - E.open_scroll;
        if (ry + row < ly || ry > ly + lh) continue;
        const LevelEntry *e = &E.entries[i];
        char label[160];
        SDL_snprintf(label, sizeof label, "%s   —   %s", e->name, e->file);
        if (ui_button(label, lx, ry + 2, lw, row - 4, e->valid ? BTN_NORMAL : BTN_DANGER) && ry >= ly - 1 &&
            ry + row <= ly + lh + 1)
            chosen = i;
    }
    gfx_clip(NULL);
    if (chosen >= 0) {
        if (!E.entries[chosen].valid) {
            status(true, "%s : %s", E.entries[chosen].file, E.entries[chosen].error);
        } else {
            E.modal = MODAL_NONE;
            E.pending_src = E.entries[chosen].src;
            SDL_strlcpy(E.pending_file, E.entries[chosen].file, sizeof E.pending_file);
            request(PENDING_OPEN);
        }
    }
    if (ui_button("Annuler", x + w / 2 - 80, y + h - 50, 160, 36, BTN_NORMAL) || ui_back_pressed())
        E.modal = MODAL_NONE;
}

static void modal_save_as(void)
{
    const float w = 560, h = 300, x = (ED_W - w) / 2, y = (ED_H - h) / 2;
    ui_dim(0, 0, ED_W, ED_H);
    ui_panel(x, y, w, h);
    font_draw_outlined("Enregistrer sous", x + w / 2, y + 32, 24, rgb(0xFFFFFF), ALIGN_CENTER);
    font_draw("Nom du niveau", x + 30, y + 74, 14, rgba(0xEAF6FF, 0.8f), ALIGN_LEFT);
    bool submit = ui_textfield(E.save_name, sizeof E.save_name, x + 30, y + 90, w - 60, 38, "Mon niveau");

    LevelSource dest = E.save_to_campaign ? LEVELSRC_CAMPAIGN : LEVELSRC_USER;
    char slug[64], file[LEVEL_FILE_MAX];
    level_slugify(E.save_name[0] ? E.save_name : "niveau", slug, sizeof slug);
    SDL_snprintf(file, sizeof file, "%s.lvl", slug);
    bool exists = storage_exists(dest, file);
    bool same = E.has_file && E.src == dest && !SDL_strcmp(E.file, file);
    char info[200];
    SDL_snprintf(info, sizeof info, "Fichier : %s%s", file,
                 exists && !same ? "  (existe déjà : il sera remplacé)" : "");
    font_draw(info, x + 30, y + 146, 13, exists && !same ? rgb(0xFFE08A) : rgba(0xEAF6FF, 0.7f), ALIGN_LEFT);

    if (storage_writable(LEVELSRC_CAMPAIGN)) {
        if (ui_button(E.save_to_campaign ? "Destination : campagne (dépôt)" : "Destination : mes niveaux", x + 30,
                      y + 170, 300, 32, BTN_TOOL))
            E.save_to_campaign = !E.save_to_campaign;
    }

    if (ui_button("Enregistrer", x + w - 30 - 340, y + h - 60, 160, 40, BTN_BIG) || submit) {
        if (!E.save_name[0]) {
            status(true, "Donne un nom au niveau");
        } else {
            SDL_strlcpy(E.lvl.name, E.save_name, sizeof E.lvl.name);
            SDL_strlcpy(E.name_seen, E.lvl.name, sizeof E.name_seen);
            if (save_to(dest, file)) E.modal = MODAL_NONE;
        }
    }
    if (ui_button("Annuler", x + w - 30 - 160, y + h - 60, 160, 40, BTN_NORMAL) || ui_back_pressed())
        E.modal = MODAL_NONE;
}

static void modal_confirm(void)
{
    const float w = 560, h = 190, x = (ED_W - w) / 2, y = (ED_H - h) / 2;
    ui_dim(0, 0, ED_W, ED_H);
    ui_panel(x, y, w, h);
    font_draw(E.confirm_msg, x + w / 2, y + 60, 16, rgb(0xFFFFFF), ALIGN_CENTER);
    if (ui_button("Abandonner", x + w / 2 - 250, y + h - 64, 160, 40, BTN_DANGER)) {
        E.modal = MODAL_NONE;
        E.dirty = false;
        do_pending();
    }
    if (ui_button("Enregistrer", x + w / 2 - 80, y + h - 64, 160, 40, BTN_BIG)) {
        E.modal = MODAL_NONE;
        save();
        if (!E.dirty) do_pending();
    }
    if (ui_button("Annuler", x + w / 2 + 90, y + h - 64, 160, 40, BTN_NORMAL) || ui_back_pressed()) {
        E.modal = MODAL_NONE;
        E.pending = PENDING_NONE;
    }
}

/* ------------------------------------------------------------------------ */

void editor_enter(void)
{
    ensure_init();
    ui_set_nav(false);
    E.drag = DRAG_NONE;
}

void editor_frame(float dt)
{
    E.t += dt;
    ui_set_nav(false);

    /* le nom modifie dans le champ texte compte comme une modification */
    if (SDL_strcmp(E.name_seen, E.lvl.name) != 0) {
        SDL_strlcpy(E.name_seen, E.lvl.name, sizeof E.name_seen);
        E.dirty = true;
    }

    if (E.modal == MODAL_NONE) {
        bool over_field = in_field() && !(fmy() < 0);
        if (ui_mouse_pressed() && over_field) field_press(fmx(), fmy());
        if (ui_right_pressed() && over_field) delete_obj(hit_test(fmx(), fmy()));
        if (ui_mouse_down() && E.drag != DRAG_NONE) field_drag(fmx(), fmy());
        if (ui_mouse_released() && E.drag != DRAG_NONE) field_release(fmx(), fmy());
        shortcuts();
        if (ui_back_pressed()) {
            if (E.sel.kind != OBJ_NONE) E.sel.kind = OBJ_NONE;
            else request(PENDING_MENU);
        }
    }
    if (g_app.scene != SCENE_EDITOR) return;   /* on vient de quitter */

    gfx_rect_vgrad(0, 0, ED_W, ED_H, rgb(0x0A1430), rgb(0x040817));
    draw_field();
    ui_set_enabled(E.modal == MODAL_NONE);
    top_bar();
    right_panel();
    status_bar(dt);
    ui_set_enabled(true);

    switch (E.modal) {
    case MODAL_OPEN: modal_open(); break;
    case MODAL_SAVE_AS: modal_save_as(); break;
    case MODAL_CONFIRM: modal_confirm(); break;
    default: break;
    }
}
