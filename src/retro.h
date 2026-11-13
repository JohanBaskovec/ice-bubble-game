#ifndef BDG_RETRO_H
#define BDG_RETRO_H

/*
 * Style graphique "retro VGA" : palette fixe de 256 couleurs, pixel art genere
 * en code dans des canevas indexes (avec tramage ordonne), puis converti en
 * textures SDL a l'echelle "nearest". Le jeu est rendu dans une cible de
 * 400x270 pixels (moitie de l'espace logique 800x540) puis agrandi.
 *
 * Palette : 16 rampes de 16 niveaux (sombre -> clair). Index = rampe*16 + niveau.
 */

#include "gfx.h"
#include <stdbool.h>

#define RETRO_W 400
#define RETRO_H 270
#define RETRO_SCALE 2.0f          /* unites logiques par pixel retro */

typedef enum Ramp {
    R_GRAY, R_RED, R_ORANGE, R_YELLOW, R_LIME, R_GREEN, R_TEAL, R_CYAN,
    R_SKY, R_BLUE, R_INDIGO, R_PURPLE, R_MAGENTA, R_PINK, R_BROWN, R_SAND,
} Ramp;

#define PAL(ramp, lvl) ((ramp) * 16 + (lvl))

#define FRUIT_COUNT 8
extern const char *const FRUIT_NAMES[FRUIT_COUNT];
extern const int FRUIT_POINTS[FRUIT_COUNT];
extern const int FRUIT_WEIGHTS[FRUIT_COUNT];

bool retro_init(SDL_Renderer *r);
void retro_quit(void);

/* Couleur d'un index de palette (0..255). */
Color retro_color(int index);

/* Cible de rendu basse resolution (RETRO_W x RETRO_H). */
SDL_Texture *retro_target(void);

/* Dessin de textures en unites logiques (tient compte du decalage et de l'opacite gfx). */
void retro_blit(SDL_Texture *t, float x, float y, bool flip_x);
void retro_tiled(SDL_Texture *t, float x, float y, float w, float h);

/* Elements graphiques */
void retro_draw_backdrop(int world);                       /* world 1..BACKDROP_COUNT */
void retro_draw_platform(int world, float x, float y, float w);
void retro_draw_ladder(float x, float top, float bottom);
void retro_draw_ball(float cx, float cy, int size, bool frozen);
void retro_draw_player(float cx, float top, int index, int frame, int face);  /* frame 0 debout, 1-2 escalade */
void retro_draw_fruit(int fruit, float x, float y);       /* x,y = coin haut-gauche, 24x24 unites */

#endif
