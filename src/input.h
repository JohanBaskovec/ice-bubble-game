#ifndef BDG_INPUT_H
#define BDG_INPUT_H

/* Entrees de jeu unifiees : clavier, manettes et commandes tactiles.
 *  - Solo : toutes les sources pilotent le joueur 1.
 *  - Deux joueurs : J1 = fleches + Espace / manette 1 / tactile,
 *                   J2 = WASD (ZQSD en AZERTY) + F / manette 2. */

#include <SDL3/SDL.h>
#include <stdbool.h>

typedef struct PlayerInput {
    bool left, right, up, down, fire;
} PlayerInput;

void input_init(void);
void input_quit(void);
void input_handle_event(const SDL_Event *e);   /* evenement deja converti en coords logiques */
void input_clear(void);

PlayerInput input_player(int index, int player_count);

bool input_touch_visible(void);
void input_draw_touch(void);
int input_gamepad_count(void);

#endif
