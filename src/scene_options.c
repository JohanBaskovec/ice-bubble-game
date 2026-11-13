#include "app.h"
#include "audio.h"
#include "config.h"
#include "font.h"
#include "game.h"
#include "gfx.h"
#include "settings.h"
#include "storage.h"
#include "ui.h"

void options_enter(void) { ui_set_nav(true); }

void options_frame(float dt)
{
    game_demo_frame(dt);
    ui_dim(0, 0, GAME_W, GAME_H);
    ui_set_nav(true);
    const float cx = GAME_W / 2.0f;
    font_draw_outlined("Options", cx, 70, 44, rgb(0xFFFFFF), ALIGN_CENTER);

    float lx = 300, x = 320, y = 118;
    Color lbl = rgba(0xEAF6FF, 0.8f);
    char buf[64];
    bool changed = false;

    font_draw("Style graphique", lx, y + 18, 18, lbl, ALIGN_RIGHT);
    if (ui_button("Rétro", x, y, 120, 36, g_settings.retro ? BTN_SELECTED : BTN_NORMAL) && !g_settings.retro) {
        g_settings.retro = true;
        changed = true;
    }
    if (ui_button("Moderne", x + 128, y, 120, 36, g_settings.retro ? BTN_NORMAL : BTN_SELECTED) && g_settings.retro) {
        g_settings.retro = false;
        changed = true;
    }

    y += 48;
    font_draw("Musique", lx, y + 18, 18, lbl, ALIGN_RIGHT);
    if (ui_button(g_settings.music ? "Activée" : "Coupée", x, y, 160, 36,
                  g_settings.music ? BTN_SELECTED : BTN_NORMAL)) {
        g_settings.music = !g_settings.music;
        changed = true;
    }

    y += 48;
    font_draw("Effets sonores", lx, y + 18, 18, lbl, ALIGN_RIGHT);
    if (ui_button("−", x, y, 44, 36, BTN_NORMAL) && g_settings.sfx_volume > 0) {
        g_settings.sfx_volume--;
        changed = true;
    }
    SDL_snprintf(buf, sizeof buf, "%d / 10", g_settings.sfx_volume);
    font_draw(buf, x + 80, y + 18, 18, rgb(0xFFFFFF), ALIGN_CENTER);
    if (ui_button("+", x + 116, y, 44, 36, BTN_NORMAL) && g_settings.sfx_volume < 10) {
        g_settings.sfx_volume++;
        changed = true;
    }

#if !defined(SDL_PLATFORM_ANDROID) && !defined(SDL_PLATFORM_IOS)
    y += 48;
    font_draw("Plein écran", lx, y + 18, 18, lbl, ALIGN_RIGHT);
    if (ui_button(g_settings.fullscreen ? "Oui" : "Non", x, y, 160, 36,
                  g_settings.fullscreen ? BTN_SELECTED : BTN_NORMAL))
        app_toggle_fullscreen();
#endif

    y += 48;
    font_draw("Commandes tactiles", lx, y + 18, 18, lbl, ALIGN_RIGHT);
    static const char *const touch_names[3] = { "Auto", "Toujours", "Jamais" };
    for (int i = 0; i < 3; i++)
        if (ui_button(touch_names[i], x + i * 104.0f, y, 96, 36,
                      (int)g_settings.touch == i ? BTN_SELECTED : BTN_NORMAL)) {
            g_settings.touch = (TouchMode)i;
            changed = true;
        }

    y += 48;
    font_draw("Record", lx, y + 18, 18, lbl, ALIGN_RIGHT);
    SDL_snprintf(buf, sizeof buf, "%d", g_settings.best_score);
    font_draw(buf, x, y + 18, 18, rgb(0xFFE08A), ALIGN_LEFT);
    if (g_settings.best_score && ui_button("Effacer", x + 120, y, 110, 36, BTN_DANGER)) {
        g_settings.best_score = 0;
        changed = true;
    }

    if (changed) {
        app_apply_settings();
        settings_save();
    }

    if (ui_button("Retour", cx - 100, 430, 200, 46, BTN_BIG) || ui_back_pressed()) app_set_scene(SCENE_MENU);

    Color small = rgba(0xEAF6FF, 0.5f);
    font_draw(APP_TITLE " v" BDG_VERSION " · polices Fredoka (OFL) et font8x8 (domaine public) · SDL3 (zlib)", cx, 500, 12,
              small, ALIGN_CENTER);
    if (storage_ready()) {
        SDL_snprintf(buf, sizeof buf, "%s", storage_pref_dir());
        font_draw(buf, cx, 520, 11, small, ALIGN_CENTER);
    }
}
