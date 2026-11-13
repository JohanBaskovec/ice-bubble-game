#include "settings.h"
#include "storage.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

Settings g_settings = { true, 8, false, TOUCH_AUTO, 0, 1, 1, true };

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void settings_load(void)
{
    size_t len = 0;
    char *text = storage_read_pref("settings.ini", &len);
    if (!text) return;
    char *save = NULL;
    for (char *line = SDL_strtok_r(text, "\n", &save); line; line = SDL_strtok_r(NULL, "\n", &save)) {
        char key[32];
        int v;
        if (SDL_sscanf(line, "%31[a-z_] = %d", key, &v) != 2) continue;
        if (!strcmp(key, "music")) g_settings.music = v != 0;
        else if (!strcmp(key, "sfx_volume")) g_settings.sfx_volume = clampi(v, 0, 10);
        else if (!strcmp(key, "fullscreen")) g_settings.fullscreen = v != 0;
        else if (!strcmp(key, "touch")) g_settings.touch = (TouchMode)clampi(v, 0, 2);
        else if (!strcmp(key, "best_score")) g_settings.best_score = v < 0 ? 0 : v;
        else if (!strcmp(key, "players")) g_settings.players = clampi(v, 1, 2);
        else if (!strcmp(key, "difficulty")) g_settings.difficulty = clampi(v, 0, 3);
        else if (!strcmp(key, "retro")) g_settings.retro = v != 0;
    }
    SDL_free(text);
}

void settings_save(void)
{
    char buf[512];
    int n = SDL_snprintf(buf, sizeof buf,
                         "# Boules de Givre - reglages\n"
                         "music = %d\nsfx_volume = %d\nfullscreen = %d\ntouch = %d\n"
                         "best_score = %d\nplayers = %d\ndifficulty = %d\nretro = %d\n",
                         g_settings.music, g_settings.sfx_volume, g_settings.fullscreen, (int)g_settings.touch,
                         g_settings.best_score, g_settings.players, g_settings.difficulty, g_settings.retro);
    storage_write_pref("settings.ini", buf, (size_t)n);
}
