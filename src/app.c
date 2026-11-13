#include "app.h"
#include "audio.h"
#include "config.h"
#include "font.h"
#include "retro.h"
#include "game.h"
#include "gfx.h"
#include "input.h"
#include "settings.h"
#include "storage.h"
#include "ui.h"

App g_app;

static Uint64 g_last_ns;
static bool g_start_in_editor;
static int g_start_play;              /* --play N : lance la campagne au niveau N */
static const char *g_shot_path;       /* --screenshot : capture puis quitte (tests / CI) */
static int g_shot_frames = 90;
static int g_frame;

/* --script : rejoue des entrees (coordonnees logiques) pour les tests.
 * Une commande par ligne : "<image> down|up|move|rdown x y", "<image> key NOM [ctrl|shift]",
 * "<image> text CHAINE", "<image> shot fichier.bmp", "<image> quit",
 * "<image> hold NOM" / "<image> release NOM" : maintenir / relacher une touche (ex. Space). */
static char *g_script;
static char *g_script_pos;
static char g_script_shot[256];      /* capture demandee, faite apres le dessin */

static void script_load(const char *path)
{
    size_t len;
    g_script = SDL_LoadFile(path, &len);
    g_script_pos = g_script;
    if (!g_script) SDL_Log("script illisible : %s", path);
}

static void script_step(void)
{
    while (g_script_pos && *g_script_pos) {
        char line[256];
        char *nl = SDL_strchr(g_script_pos, '\n');
        size_t n = nl ? (size_t)(nl - g_script_pos) : SDL_strlen(g_script_pos);
        if (n >= sizeof line) n = sizeof line - 1;
        SDL_memcpy(line, g_script_pos, n);
        line[n] = '\0';
        int frame = 0;
        char cmd[16] = "", a[128] = "", b[32] = "";
        int fields = SDL_sscanf(line, "%d %15s %127s %31s", &frame, cmd, a, b);
        if (fields >= 2 && line[0] != '#' && frame > g_frame) return;   /* pas encore */
        g_script_pos = nl ? nl + 1 : g_script_pos + n;
        if (fields < 2 || line[0] == '#') continue;

        SDL_Event e;
        SDL_zero(e);
        if (!SDL_strcmp(cmd, "down") || !SDL_strcmp(cmd, "up") || !SDL_strcmp(cmd, "rdown")) {
            e.type = cmd[0] == 'u' ? SDL_EVENT_MOUSE_BUTTON_UP : SDL_EVENT_MOUSE_BUTTON_DOWN;
            e.button.button = cmd[0] == 'r' ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
            e.button.x = (float)SDL_atof(a);
            e.button.y = (float)SDL_atof(b);
        } else if (!SDL_strcmp(cmd, "move")) {
            e.type = SDL_EVENT_MOUSE_MOTION;
            e.motion.x = (float)SDL_atof(a);
            e.motion.y = (float)SDL_atof(b);
        } else if (!SDL_strcmp(cmd, "key")) {
            e.type = SDL_EVENT_KEY_DOWN;
            e.key.key = SDL_GetKeyFromName(a);
            e.key.down = true;
            if (!SDL_strcmp(b, "ctrl")) e.key.mod = SDL_KMOD_LCTRL;
            if (!SDL_strcmp(b, "shift")) e.key.mod = SDL_KMOD_LSHIFT;
        } else if (!SDL_strcmp(cmd, "text")) {
            const char *t = SDL_strstr(line, "text ") + 5;
            e.type = SDL_EVENT_TEXT_INPUT;
            e.text.text = t;
            ui_handle_event(&e);
            continue;
        } else if (!SDL_strcmp(cmd, "hold") || !SDL_strcmp(cmd, "release")) {
            input_force_key(SDL_GetScancodeFromName(a), cmd[0] == 'h');
            continue;
        } else if (!SDL_strcmp(cmd, "shot")) {
            SDL_strlcpy(g_script_shot, a, sizeof g_script_shot);
            continue;
        } else if (!SDL_strcmp(cmd, "quit")) {
            g_app.quit = true;
            continue;
        } else {
            continue;
        }
        input_handle_event(&e);
        ui_handle_event(&e);
        if (e.type == SDL_EVENT_KEY_DOWN) {
            /* relache aussitot pour l'etat des modificateurs */
            e.type = SDL_EVENT_KEY_UP;
            e.key.down = false;
            e.key.mod = 0;
            ui_handle_event(&e);
        }
    }
}

#if defined(SDL_PLATFORM_ANDROID) || defined(SDL_PLATFORM_IOS)
#define BDG_MOBILE 1
#else
#define BDG_MOBILE 0
#endif

static void usage(void)
{
    SDL_Log("Options :\n"
            "  --levels-dir <dossier>  lit/ecrit la campagne dans ce dossier (ex. ./levels)\n"
            "  --editor                ouvre directement l'editeur\n"
            "  --fullscreen            plein ecran\n"
            "  --play <n>              lance directement la campagne au niveau n\n"
            "  --screenshot <f.bmp>    enregistre une capture apres --frames images puis quitte\n"
            "  --frames <n>            nombre d'images avant la capture (defaut 90)");
}

bool app_init(int argc, char **argv)
{
    const char *levels_dir = NULL;
    bool force_fullscreen = false;
    for (int i = 1; i < argc; i++) {
        if (!SDL_strcmp(argv[i], "--levels-dir") && i + 1 < argc) levels_dir = argv[++i];
        else if (!SDL_strcmp(argv[i], "--editor")) g_start_in_editor = true;
        else if (!SDL_strcmp(argv[i], "--fullscreen")) force_fullscreen = true;
        else if (!SDL_strcmp(argv[i], "--play") && i + 1 < argc) g_start_play = SDL_atoi(argv[++i]);
        else if (!SDL_strcmp(argv[i], "--screenshot") && i + 1 < argc) g_shot_path = argv[++i];
        else if (!SDL_strcmp(argv[i], "--frames") && i + 1 < argc) g_shot_frames = SDL_atoi(argv[++i]);
        else if (!SDL_strcmp(argv[i], "--script") && i + 1 < argc) script_load(argv[++i]);
        else if (!SDL_strcmp(argv[i], "--help") || !SDL_strcmp(argv[i], "-h")) {
            usage();
            return false;
        }
    }

    SDL_SetAppMetadata(APP_TITLE, BDG_VERSION, BDG_APP_ID);
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init : %s", SDL_GetError());
        return false;
    }

    storage_init(levels_dir);
    g_app.dev_levels = levels_dir != NULL;
    settings_load();
    if (force_fullscreen) g_settings.fullscreen = true;

    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (BDG_MOBILE) flags |= SDL_WINDOW_FULLSCREEN;
#ifdef __EMSCRIPTEN__
    flags |= SDL_WINDOW_FILL_DOCUMENT;   /* le canvas suit la taille de la page */
#endif
    if (!SDL_CreateWindowAndRenderer(APP_TITLE, 1200, 810, flags, &g_app.window, &g_app.renderer)) {
        SDL_Log("fenetre : %s", SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(g_app.window, 400, 270);
    SDL_SetRenderVSync(g_app.renderer, 1);
    SDL_SetRenderDrawBlendMode(g_app.renderer, SDL_BLENDMODE_BLEND);
    app_set_logical_size(GAME_W, GAME_H);

    gfx_init(g_app.renderer);
    if (!font_init(g_app.renderer)) return false;
    if (!retro_init(g_app.renderer)) SDL_Log("style retro indisponible : %s", SDL_GetError());
    audio_init();
    input_init();
    ui_init(g_app.window);
    app_apply_settings();

    game_demo_init();
    if (g_start_in_editor) app_set_scene(SCENE_EDITOR);
    else if (g_start_play > 0) game_start_campaign(g_start_play, g_settings.players, g_settings.difficulty);
    else app_set_scene(SCENE_MENU);
    g_last_ns = SDL_GetTicksNS();
    return true;
}

void app_quit(void)
{
    settings_save();
    SDL_free(g_script);
    audio_quit();
    input_quit();
    font_quit();
    /* SDL detruit la fenetre et le renderer lui-meme apres SDL_AppQuit */
}

void app_set_logical_size(int w, int h)
{
    SDL_SetRenderLogicalPresentation(g_app.renderer, w, h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

void app_set_scene(SceneId s)
{
    ui_text_blur();
    ui_reset_focus();
    g_app.scene = s;
    if (s == SCENE_EDITOR) app_set_logical_size(1100, 640);
    else app_set_logical_size(GAME_W, GAME_H);
    switch (s) {
    case SCENE_MENU: menu_enter(); break;
    case SCENE_LEVELS: levels_enter(); break;
    case SCENE_OPTIONS: options_enter(); break;
    case SCENE_EDITOR: editor_enter(); break;
    case SCENE_GAME: break;
    }
}

void app_toggle_fullscreen(void)
{
    g_settings.fullscreen = !g_settings.fullscreen;
    SDL_SetWindowFullscreen(g_app.window, g_settings.fullscreen);
    settings_save();
}

void app_apply_settings(void)
{
    audio_set_music(g_settings.music);
    audio_set_sfx_volume(g_settings.sfx_volume / 10.0f);
    if (!BDG_MOBILE) SDL_SetWindowFullscreen(g_app.window, g_settings.fullscreen);
}

void app_event(SDL_Event *e)
{
    switch (e->type) {
    case SDL_EVENT_QUIT:
        g_app.quit = true;
        return;
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
    case SDL_EVENT_DID_ENTER_BACKGROUND:
        if (g_app.scene == SCENE_GAME) game_pause();
        settings_save();
        audio_pause_device(true);
        input_clear();
        return;
    case SDL_EVENT_DID_ENTER_FOREGROUND:
        audio_pause_device(false);
        return;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (g_app.scene == SCENE_GAME) game_pause();
        input_clear();
        return;
    default: break;
    }

    SDL_ConvertEventToRenderCoordinates(g_app.renderer, e);

    if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat && !ui_text_active()) {
        if (e->key.key == SDLK_F11 || (e->key.key == SDLK_RETURN && (e->key.mod & SDL_KMOD_ALT))) {
            app_toggle_fullscreen();
            return;
        }
        if (e->key.key == SDLK_M && g_app.scene != SCENE_EDITOR) {
            g_settings.music = !g_settings.music;
            audio_set_music(g_settings.music);
            settings_save();
        }
    }
    if (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && e->gbutton.button == SDL_GAMEPAD_BUTTON_START &&
        g_app.scene == SCENE_GAME)
        game_pause();

    input_handle_event(e);
    ui_handle_event(e);
}

void app_iterate(void)
{
    Uint64 now = SDL_GetTicksNS();
    float dt = (float)((now - g_last_ns) / 1e9);
    g_last_ns = now;
    if (dt > 1.0f / 30) dt = 1.0f / 30;
    if (dt < 0) dt = 0;
    g_app.time += dt;

    /* Web : les reglages ne sont lisibles qu'apres le chargement d'IndexedDB */
    if (storage_poll_ready()) {
#ifdef __EMSCRIPTEN__
        settings_load();
        app_apply_settings();
#endif
    }

    SDL_SetRenderDrawColor(g_app.renderer, 4, 8, 23, 255);
    SDL_RenderClear(g_app.renderer);

    if (g_script) script_step();
    ui_begin_frame();
    /* police pixel en style retro, sauf dans l'editeur (outil dense en texte) */
    font_set_pixel(g_settings.retro && g_app.scene != SCENE_EDITOR);
    switch (g_app.scene) {
    case SCENE_MENU: menu_frame(dt); break;
    case SCENE_LEVELS: levels_frame(dt); break;
    case SCENE_OPTIONS: options_frame(dt); break;
    case SCENE_GAME: game_frame(dt); break;
    case SCENE_EDITOR: editor_frame(dt); break;
    }
    ui_end_frame();

    g_frame++;
    if (g_script_shot[0]) {
        SDL_Surface *shot = SDL_RenderReadPixels(g_app.renderer, NULL);
        if (shot) SDL_SaveBMP(shot, g_script_shot);
        SDL_DestroySurface(shot);
        g_script_shot[0] = '\0';
    }
    if (g_shot_path && g_frame == g_shot_frames) {
        SDL_Surface *shot = SDL_RenderReadPixels(g_app.renderer, NULL);
        if (shot && SDL_SaveBMP(shot, g_shot_path)) SDL_Log("capture : %s", g_shot_path);
        else SDL_Log("capture impossible : %s", SDL_GetError());
        SDL_DestroySurface(shot);
        g_app.quit = true;
    }

    SDL_RenderPresent(g_app.renderer);
}
