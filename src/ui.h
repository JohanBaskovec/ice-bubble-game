#ifndef BDG_UI_H
#define BDG_UI_H

/*
 * Mini interface en mode immediat : boutons, champs texte, panneaux.
 * Souris et tactile (via les evenements souris synthetiques de SDL),
 * plus navigation clavier / manette (fleches + Entree, croix + A).
 */

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum BtnStyle {
    BTN_NORMAL,
    BTN_SELECTED,
    BTN_BIG,
    BTN_DANGER,
    BTN_TOOL,       /* bouton d'outil (editeur), petit texte */
    BTN_TOOL_ON,
} BtnStyle;

void ui_init(SDL_Window *w);
void ui_handle_event(const SDL_Event *e);
void ui_begin_frame(void);
void ui_end_frame(void);

/* Navigation clavier/manette entre les boutons (menus). */
void ui_set_nav(bool on);
void ui_reset_focus(void);
void ui_set_enabled(bool on);   /* desactive les widgets suivants (sous une modale) */

bool ui_button(const char *label, float x, float y, float w, float h, BtnStyle style);
bool ui_textfield(char *buf, size_t cap, float x, float y, float w, float h, const char *placeholder);
void ui_panel(float x, float y, float w, float h);
void ui_dim(float x, float y, float w, float h);   /* voile sombre plein ecran */

/* Etat du pointeur pour les zones personnalisees (editeur). */
float ui_mx(void);
float ui_my(void);
bool ui_mouse_down(void);
bool ui_mouse_pressed(void);        /* bouton gauche enfonce cette frame */
bool ui_mouse_released(void);
bool ui_right_pressed(void);
float ui_wheel(void);
bool ui_hover(float x, float y, float w, float h);
bool ui_hot(void);                  /* le pointeur survole un widget cette frame */
bool ui_enter_unfocused(void);      /* Entree / A sans bouton focalise : action par defaut de l'ecran */

bool ui_text_active(void);          /* un champ texte a le focus */
void ui_text_blur(void);
bool ui_back_pressed(void);         /* Echap / retour Android / B manette (consomme l'appui) */
bool ui_key_pressed(SDL_Keycode k); /* touche enfoncee cette frame (hors saisie) */
SDL_Keymod ui_mods(void);

#endif
