#ifndef BDG_LEVEL_H
#define BDG_LEVEL_H

/*
 * Description d'un niveau et format de fichier texte ".lvl".
 * Module en C standard pur (pas de SDL) pour etre testable facilement.
 * Specification complete : docs/LEVEL_FORMAT.md
 */

#include <stdbool.h>
#include <stddef.h>

#define LEVEL_FORMAT_VERSION 1

#define LEVEL_MAX_PLATFORMS 32
#define LEVEL_MAX_LADDERS   32
#define LEVEL_MAX_BALLS     24
#define LEVEL_MAX_LASERS    8
#define LEVEL_NAME_MAX      48

#define BALL_SIZES 4        /* tailles internes 0..3 (fichier : 1..4) */
#define BACKDROP_COUNT 3    /* decors du style retro (1..3) ; 0 = automatique */

/* Renforts : boules qui tombent du plafond a intervalle regulier. */
#define REINFORCE_DEFAULT 0.0f   /* intervalle selon la difficulte */
#define REINFORCE_OFF    -1.0f   /* aucun renfort */

typedef struct LevelPlatform {
    float x, y, w;          /* coin haut-gauche, largeur ; hauteur = PLAT_H */
} LevelPlatform;

typedef struct LevelLadder {
    float x;                /* bord gauche ; largeur = LADDER_W */
    float top, bottom;      /* y du haut (sommet praticable) et du bas */
} LevelLadder;

/* Laser : barriere verticale. Les boules rebondissent dessus, les joueurs
 * (et leurs tirs) la traversent. */
typedef struct LevelLaser {
    float x;                /* centre ; epaisseur de collision = LASER_W */
    float top, bottom;
} LevelLaser;

typedef struct LevelBall {
    float x, y;             /* centre */
    int size;               /* 0 = petite .. 3 = geante */
    int dir;                /* -1 gauche, +1 droite */
} LevelBall;

typedef struct LevelStart {
    float x;                /* centre horizontal du joueur */
    float y;                /* y des pieds */
} LevelStart;

typedef struct Level {
    char name[LEVEL_NAME_MAX];
    int hue;                        /* teinte du decor (style moderne), 0..359 */
    int backdrop;                   /* decor du style retro : 0 = auto, 1..BACKDROP_COUNT */
    float reinforce;                /* REINFORCE_DEFAULT, REINFORCE_OFF ou secondes */
    LevelStart start[2];

    int platform_count;
    LevelPlatform platforms[LEVEL_MAX_PLATFORMS];
    int ladder_count;
    LevelLadder ladders[LEVEL_MAX_LADDERS];
    int ball_count;
    LevelBall balls[LEVEL_MAX_BALLS];
    int laser_count;
    LevelLaser lasers[LEVEL_MAX_LASERS];
} Level;

/* Niveau vide avec valeurs par defaut. */
void level_init(Level *lvl);

/* Analyse un texte .lvl. Retourne false en cas d'erreur bloquante et ecrit
 * un message (avec numero de ligne) dans err. Les cles inconnues sont
 * ignorees pour la compatibilite ascendante. */
bool level_parse(Level *out, const char *text, size_t len, char *err, size_t err_cap);

/* Serialise le niveau. Retourne la longueur ecrite (hors '\0'), ou la
 * taille necessaire si buf est trop petit (comme snprintf). */
size_t level_serialize(const Level *lvl, char *buf, size_t cap);

/* Verifie la jouabilite. Retourne le nombre de problemes ; ecrit le premier
 * dans msg (peut etre NULL). */
int level_validate(const Level *lvl, char *msg, size_t msg_cap);

/* Ramene toutes les valeurs dans des bornes sures (apres edition). */
void level_clamp(Level *lvl);

/* Convertit un nom libre en nom de fichier : "Le Pont !" -> "le-pont". */
void level_slugify(const char *name, char *out, size_t cap);

/* y de la premiere surface (plateforme ou sol) au niveau de x, a partir de
 * y_from vers le bas. Utilise par l'editeur et le placement des joueurs. */
float level_surface_below(const Level *lvl, float x, float y_from);

#endif
