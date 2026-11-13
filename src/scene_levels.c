#include "app.h"
#include "config.h"
#include "font.h"
#include "game.h"
#include "gfx.h"
#include "settings.h"
#include "storage.h"
#include "ui.h"

static LevelEntry g_entries[MAX_LEVEL_ENTRIES];
static int g_count;
static LevelSource g_tab = LEVELSRC_USER;
static float g_scroll;
static int g_confirm_delete = -1;
static char g_msg[160];
static bool g_loaded;

static void refresh(void)
{
    g_count = storage_list(g_tab, g_entries, MAX_LEVEL_ENTRIES);
    g_confirm_delete = -1;
    g_loaded = storage_ready() || g_tab == LEVELSRC_CAMPAIGN;
}

void levels_enter(void)
{
    ui_set_nav(false);
    g_scroll = 0;
    g_msg[0] = '\0';
    refresh();
}

static void play(const LevelEntry *e)
{
    Level lvl;
    char err[128];
    if (!storage_load(e->src, e->file, &lvl, err, sizeof err)) {
        SDL_snprintf(g_msg, sizeof g_msg, "Impossible de charger %s : %s", e->file, err);
        return;
    }
    game_start_single(&lvl, g_settings.players, g_settings.difficulty);
}

void levels_frame(float dt)
{
    game_demo_frame(dt);
    ui_dim(0, 0, GAME_W, GAME_H);
    ui_set_nav(false);
    if (!g_loaded && storage_ready()) refresh();

    const float cx = GAME_W / 2.0f;
    font_draw_outlined("Niveaux", cx, 40, 34, rgb(0xFFFFFF), ALIGN_CENTER);

    if (ui_button("Mes niveaux", cx - 200, 72, 196, 34, g_tab == LEVELSRC_USER ? BTN_SELECTED : BTN_NORMAL)) {
        g_tab = LEVELSRC_USER;
        g_scroll = 0;
        refresh();
    }
    if (ui_button("Campagne", cx + 4, 72, 196, 34, g_tab == LEVELSRC_CAMPAIGN ? BTN_SELECTED : BTN_NORMAL)) {
        g_tab = LEVELSRC_CAMPAIGN;
        g_scroll = 0;
        refresh();
    }

    /* liste */
    const float lx = 60, ly = 118, lw = GAME_W - 120, lh = 330, row = 48;
    ui_panel(lx - 10, ly - 6, lw + 20, lh + 12);
    float content = g_count * row;
    if (ui_hover(lx, ly, lw, lh)) g_scroll -= ui_wheel() * row;
    float max_scroll = content > lh ? content - lh : 0;
    if (g_scroll > max_scroll) g_scroll = max_scroll;
    if (g_scroll < 0) g_scroll = 0;

    SDL_FRect clip = { lx - 6, ly, lw + 12, lh };
    gfx_clip(&clip);
    if (!g_loaded) {
        font_draw("Chargement…", cx, ly + 40, 18, rgba(0xEAF6FF, 0.7f), ALIGN_CENTER);
    } else if (g_count == 0) {
        font_draw(g_tab == LEVELSRC_USER ? "Aucun niveau perso pour l'instant." : "Aucun niveau.", cx, ly + 40, 18,
                  rgba(0xEAF6FF, 0.7f), ALIGN_CENTER);
        if (g_tab == LEVELSRC_USER)
            font_draw("Crée ton premier niveau avec l'éditeur !", cx, ly + 70, 15, rgba(0xEAF6FF, 0.6f),
                      ALIGN_CENTER);
    }
    int action = -1, action_kind = 0;
    for (int i = 0; i < g_count; i++) {
        float y = ly + i * row - g_scroll;
        if (y + row < ly || y > ly + lh) continue;
        const LevelEntry *e = &g_entries[i];
        if (i % 2 == 0) gfx_rrect(lx, y + 2, lw, row - 4, 8, rgba(0x78BEFF, 0.06f));
        char label[128];
        if (g_tab == LEVELSRC_CAMPAIGN) SDL_snprintf(label, sizeof label, "%d. %s", i + 1, e->name);
        else SDL_snprintf(label, sizeof label, "%s", e->name);
        font_draw(label, lx + 12, y + 17, 17, e->valid ? rgb(0xFFFFFF) : rgb(0xFF9AA8), ALIGN_LEFT);
        font_draw(e->valid ? e->file : e->error, lx + 12, y + 35, 11, rgba(0xEAF6FF, 0.5f), ALIGN_LEFT);

        /* les boutons ne reagissent que dans la zone visible */
        bool visible = y >= ly - 1 && y + row <= ly + lh + 1;
        float bx = lx + lw - 8;
        if (g_tab == LEVELSRC_USER) {
            bx -= 96;
            BtnStyle st = g_confirm_delete == i ? BTN_DANGER : BTN_NORMAL;
            if (ui_button(g_confirm_delete == i ? "Confirmer ?" : "Supprimer", bx, y + 8, 92, 32, st) && visible) {
                action = i;
                action_kind = 3;
            }
        }
        bx -= 84;
        if (ui_button("Éditer", bx, y + 8, 80, 32, BTN_NORMAL) && visible) {
            action = i;
            action_kind = 2;
        }
        bx -= 84;
        if (e->valid && ui_button("Jouer", bx, y + 8, 80, 32, BTN_SELECTED) && visible) {
            action = i;
            action_kind = 1;
        }
    }
    gfx_clip(NULL);

    if (action >= 0) {
        LevelEntry *e = &g_entries[action];
        if (action_kind == 1) {
            play(e);
            return;
        } else if (action_kind == 2) {
            editor_open_level((int)e->src, e->file);
            app_set_scene(SCENE_EDITOR);
            return;
        } else if (action_kind == 3) {
            if (g_confirm_delete == action) {
                if (storage_delete(e->src, e->file)) SDL_snprintf(g_msg, sizeof g_msg, "« %s » supprimé.", e->name);
                else SDL_snprintf(g_msg, sizeof g_msg, "Suppression impossible : %s", SDL_GetError());
                refresh();
            } else {
                g_confirm_delete = action;
            }
        }
    }

    /* barre du bas */
    float by = 466;
    if (ui_button("Retour", 50, by, 140, 40, BTN_NORMAL) || ui_back_pressed()) {
        app_set_scene(SCENE_MENU);
        return;
    }
    if (ui_button("Nouveau niveau", cx - 100, by, 200, 40, BTN_BIG)) {
        editor_new_level();
        app_set_scene(SCENE_EDITOR);
        return;
    }
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_MACOS) || defined(SDL_PLATFORM_LINUX)
    if (g_tab == LEVELSRC_USER && ui_button("Ouvrir le dossier", GAME_W - 210, by, 160, 40, BTN_NORMAL))
        storage_reveal_user_folder();
#endif
    if (g_msg[0]) font_draw(g_msg, cx, 522, 13, rgb(0xFFE08A), ALIGN_CENTER);
    else if (g_tab == LEVELSRC_USER && storage_ready())
        font_draw(storage_user_levels_dir(), cx, 522, 11, rgba(0xEAF6FF, 0.45f), ALIGN_CENTER);
    else if (g_tab == LEVELSRC_CAMPAIGN)
        font_draw(g_app.dev_levels ? "Mode développeur : la campagne est modifiable (--levels-dir)"
                                   : "Les niveaux de la campagne s'ouvrent en copie dans l'éditeur",
                  cx, 522, 12, rgba(0xEAF6FF, 0.55f), ALIGN_CENTER);
}
