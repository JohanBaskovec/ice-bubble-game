#ifndef BDG_APP_H
#define BDG_APP_H

#include <SDL3/SDL.h>
#include <stdbool.h>

typedef enum SceneId {
    SCENE_MENU,
    SCENE_LEVELS,
    SCENE_OPTIONS,
    SCENE_GAME,
    SCENE_EDITOR,
} SceneId;

typedef struct App {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SceneId scene;
    bool quit;
    double time;            /* secondes depuis le lancement */
    bool dev_levels;        /* --levels-dir : la campagne est modifiable */
} App;

extern App g_app;

bool app_init(int argc, char **argv);
void app_quit(void);
void app_event(SDL_Event *e);
void app_iterate(void);

void app_set_scene(SceneId s);
/* Taille logique : 800x540 en jeu, plus grand dans l'editeur. */
void app_set_logical_size(int w, int h);
void app_toggle_fullscreen(void);
void app_apply_settings(void);

/* Points d'entree des scenes (scene_*.c) */
void menu_enter(void);
void menu_frame(float dt);
void levels_enter(void);
void levels_frame(float dt);
void options_enter(void);
void options_frame(float dt);
void editor_enter(void);
void editor_frame(float dt);
void editor_resume_from_test(void);
void editor_open_level(int src, const char *file);
void editor_new_level(void);

#endif
