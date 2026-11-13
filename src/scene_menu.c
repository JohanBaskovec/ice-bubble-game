#include "app.h"
#include "audio.h"
#include "config.h"
#include "font.h"
#include "game.h"
#include "gfx.h"
#include "input.h"
#include "settings.h"
#include "ui.h"

#include <math.h>

static char g_code[16];
static char g_code_msg[64];
static bool g_code_ok;
static int g_start_level = 1;

void menu_enter(void)
{
    ui_set_nav(true);
    if (g_game.mode != GAMEMODE_DEMO) game_demo_init();
}

static void apply_code(void)
{
    if (!g_code[0]) {
        g_start_level = 1;
        g_code_msg[0] = '\0';
        return;
    }
    int n = game_level_from_code(g_code);
    if (n) {
        g_start_level = n;
        g_code_ok = true;
        SDL_snprintf(g_code_msg, sizeof g_code_msg, "Départ au niveau %d", n);
    } else {
        g_start_level = 1;
        g_code_ok = false;
        SDL_snprintf(g_code_msg, sizeof g_code_msg, "Code inconnu");
    }
}

static void start(void)
{
    apply_code();
    settings_save();
    game_start_campaign(g_start_level, g_settings.players, g_settings.difficulty);
}

void menu_frame(float dt)
{
    game_demo_frame(dt);
    ui_dim(0, 0, GAME_W, GAME_H);
    ui_set_nav(true);

    const float cx = GAME_W / 2.0f;
    float glow = 0.5f + 0.5f * sinf((float)g_app.time * 2);
    font_draw(APP_TITLE, cx, 86 + 4, 62, rgb(0x1F4F9C), ALIGN_CENTER);
    font_draw(APP_TITLE, cx, 86, 62, color_lerp(rgb(0xFFFFFF), rgb(0xBFEAFF), glow), ALIGN_CENTER);
    font_draw("Les boules de Noël se sont rebellées. Tire, divise, givre-les toutes !", cx, 134, 16,
              rgba(0xEAF6FF, 0.85f), ALIGN_CENTER);

    /* joueurs */
    float lx = 250, y = 168;
    font_draw("Joueurs", lx, y + 17, 16, rgba(0xEAF6FF, 0.75f), ALIGN_RIGHT);
    if (ui_button("1 joueur", lx + 14, y, 120, 34, g_settings.players == 1 ? BTN_SELECTED : BTN_NORMAL))
        g_settings.players = 1;
    if (ui_button("2 joueurs", lx + 142, y, 120, 34, g_settings.players == 2 ? BTN_SELECTED : BTN_NORMAL))
        g_settings.players = 2;

    /* difficulte */
    y = 212;
    font_draw("Difficulté", lx, y + 17, 16, rgba(0xEAF6FF, 0.75f), ALIGN_RIGHT);
    for (int i = 0; i < 4; i++)
        if (ui_button(DIFFICULTIES[i].name, lx + 14 + i * 112.0f, y, 104, 34,
                      g_settings.difficulty == i ? BTN_SELECTED : BTN_NORMAL))
            g_settings.difficulty = i;

    /* code */
    y = 256;
    font_draw("Code", lx, y + 17, 16, rgba(0xEAF6FF, 0.75f), ALIGN_RIGHT);
    if (ui_textfield(g_code, sizeof g_code, lx + 14, y, 170, 34, "facultatif")) apply_code();
    if (ui_button("Valider", lx + 192, y, 90, 34, BTN_NORMAL)) apply_code();
    if (g_code_msg[0])
        font_draw(g_code_msg, lx + 292, y + 17, 15, g_code_ok ? rgb(0x8FF0B0) : rgb(0xFF9AA8), ALIGN_LEFT);

    if (ui_button("Jouer", cx - 120, 310, 240, 56, BTN_BIG) || ui_enter_unfocused()) {
        start();
        return;
    }

    float bw = 150, gap = 12;
#if defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_EMSCRIPTEN)
    int nb = 3;   /* pas de bouton Quitter (interdit sur iOS, inutile sur le Web) */
#else
    int nb = 4;
#endif
    float bx = cx - (nb * bw + (nb - 1) * gap) / 2;
    if (ui_button("Mes niveaux", bx, 390, bw, 40, BTN_NORMAL)) app_set_scene(SCENE_LEVELS);
    if (ui_button("Éditeur", bx + (bw + gap), 390, bw, 40, BTN_NORMAL)) app_set_scene(SCENE_EDITOR);
    if (ui_button("Options", bx + 2 * (bw + gap), 390, bw, 40, BTN_NORMAL)) app_set_scene(SCENE_OPTIONS);
    if (nb == 4 && ui_button("Quitter", bx + 3 * (bw + gap), 390, bw, 40, BTN_NORMAL)) g_app.quit = true;

    Color help = rgba(0xEAF6FF, 0.65f);
    if (input_touch_visible()) {
        font_draw("Croix à gauche pour bouger et grimper, flocon à droite pour tirer", cx, 458, 13, help,
                  ALIGN_CENTER);
    } else {
        font_draw("J1 : flèches + Espace  ·  J2 : ZQSD (WASD) + F  ·  manettes acceptées", cx, 452, 13, help,
                  ALIGN_CENTER);
        font_draw("P / Échap : pause  ·  M : musique  ·  F11 : plein écran", cx, 472, 13, help, ALIGN_CENTER);
    }
    char buf[64];
    if (g_settings.best_score) {
        SDL_snprintf(buf, sizeof buf, "Record : %d", g_settings.best_score);
        font_draw(buf, cx, 500, 15, rgb(0xFFE08A), ALIGN_CENTER);
    }
    int pads = input_gamepad_count();
    if (pads) {
        SDL_snprintf(buf, sizeof buf, "%d manette%s connectée%s", pads, pads > 1 ? "s" : "", pads > 1 ? "s" : "");
        font_draw(buf, GAME_W - 12, GAME_H - 14, 12, help, ALIGN_RIGHT);
    }
    font_draw("v" BDG_VERSION, 12, GAME_H - 14, 12, rgba(0xEAF6FF, 0.4f), ALIGN_LEFT);
}
