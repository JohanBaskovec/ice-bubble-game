#ifndef BDG_GFX_H
#define BDG_GFX_H

/*
 * Primitives de dessin vectoriel au-dessus de SDL_RenderGeometry.
 * Tout est dessine en coordonnees logiques ; SDL met a l'echelle
 * (SDL_SetRenderLogicalPresentation), donc le rendu reste net en HiDPI.
 */

#include <SDL3/SDL.h>
#include <stdbool.h>

typedef SDL_FColor Color;

void gfx_init(SDL_Renderer *r);
SDL_Renderer *gfx_renderer(void);

/* Couleurs */
Color rgb(Uint32 hex);                    /* 0xRRGGBB */
Color rgba(Uint32 hex, float a);
Color hsl(float h, float s, float l, float a);   /* h en degres, s/l en 0..1 */
Color color_lerp(Color a, Color b, float t);
Color color_alpha(Color c, float a);

/* Etat global : translation et opacite multiplicatrice */
void gfx_set_offset(float x, float y);
void gfx_get_offset(float *x, float *y);
void gfx_set_alpha(float a);
float gfx_get_alpha(void);

/* Formes pleines */
void gfx_rect(float x, float y, float w, float h, Color c);
void gfx_rect_vgrad(float x, float y, float w, float h, Color top, Color bottom);
void gfx_rect_hgrad(float x, float y, float w, float h, Color left, Color right);
void gfx_rrect(float x, float y, float w, float h, float r, Color c);
void gfx_rrect_vgrad(float x, float y, float w, float h, float r, Color top, Color bottom);
void gfx_circle(float x, float y, float r, Color c);
void gfx_ellipse(float x, float y, float rx, float ry, Color c);
void gfx_arc_fill(float x, float y, float r, float a0, float a1, Color c);   /* secteur */
void gfx_tri(float x1, float y1, float x2, float y2, float x3, float y3, Color c);
void gfx_quad(const SDL_FPoint p[4], Color c);
void gfx_quad_rot(float cx, float cy, float w, float h, float angle, Color c);
void gfx_poly(const SDL_FPoint *pts, int n, Color c);   /* polygone convexe */

/* Disque avec degrade radial : centre lumineux decale (hx, hy). */
void gfx_ball(float x, float y, float r, float hx, float hy, Color inner, Color mid, Color outer);

/* Traits */
void gfx_line(float x1, float y1, float x2, float y2, float thick, Color c);
void gfx_ring(float x, float y, float r, float thick, Color c);
void gfx_rrect_outline(float x, float y, float w, float h, float r, float thick, Color c);
void gfx_polyline(const SDL_FPoint *pts, int n, float thick, Color c);

/* Icones dessinees (la police ne contient pas ces symboles) */
void gfx_heart(float cx, float cy, float size, Color c);
void gfx_snowflake(float cx, float cy, float size, float thick, Color c);
void gfx_arrow(float cx, float cy, float size, int dir, Color c);   /* 0 haut,1 droite,2 bas,3 gauche */
void gfx_shield_icon(float cx, float cy, float size, Color c);

/* Rectangle de decoupe en coordonnees logiques (NULL = aucun). */
void gfx_clip(const SDL_FRect *r);

#endif
