/*
 * Point d'entree : modele "main callbacks" de SDL3.
 * Le meme code tourne sur bureau, Web (Emscripten), Android et iOS :
 * SDL appelle SDL_AppIterate a chaque image au lieu d'une boucle while.
 */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "app.h"

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)appstate;
    return app_init(argc, argv) ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    (void)appstate;
    app_event(event);
    return g_app.quit ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    (void)appstate;
    app_iterate();
    return g_app.quit ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)appstate;
    (void)result;
    app_quit();
}
