#ifndef BDG_CONFIG_H
#define BDG_CONFIG_H

/* Dimensions logiques du terrain de jeu (toutes les coordonnees du jeu
 * et des fichiers de niveau sont exprimees dans cet espace). */
#define GAME_W   800
#define GAME_H   540
#define CEIL_Y   36     /* bas du bandeau HUD : plafond des boules */
#define FLOOR_Y  500    /* haut du sol enneige */
#define PLAT_H   14     /* epaisseur des plateformes */
#define LADDER_W 30     /* largeur des echelles */
#define LASER_W 6           /* epaisseur de collision d'un laser */
#define WALL_W   8      /* murs lateraux : les boules et joueurs rebondissent dessus */

#define APP_TITLE "Boules de Givre"
#define APP_ORG   "johanbaskovec"
#define APP_NAME  "boules-de-givre"

#ifndef BDG_VERSION
#define BDG_VERSION "dev"
#endif

#endif
