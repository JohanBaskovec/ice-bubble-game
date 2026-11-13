#ifndef BDG_FONT_H
#define BDG_FONT_H

/* Texte UTF-8 avec la police Fredoka (OFL) rasterisee par stb_truetype. */

#include "gfx.h"
#include <stdbool.h>

typedef enum TextAlign { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT } TextAlign;

bool font_init(SDL_Renderer *r);
void font_quit(void);

/* Largeur du texte a la taille donnee (pixels logiques). */
float font_measure(const char *text, float size);

/* Dessine le texte ; y est le centre vertical de la ligne. */
void font_draw(const char *text, float x, float y, float size, Color c, TextAlign align);

/* Variante avec contour sombre pour la lisibilite sur le decor. */
void font_draw_outlined(const char *text, float x, float y, float size, Color c, TextAlign align);

/* Decode un caractere UTF-8 ; avance *s. Retourne 0 en fin de chaine. */
Uint32 utf8_next(const char **s);

#endif
