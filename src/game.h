#ifndef BDG_GAME_H
#define BDG_GAME_H

/* Simulation et rendu d'une partie (scene de jeu). */

#include "level.h"
#include <stdbool.h>

#define MAX_BALLS     160
#define MAX_BEAMS     8
#define MAX_BULLETS   48
#define MAX_PUPS      12
#define MAX_PARTICLES 600
#define MAX_FLAKES    90
#define MAX_STARS     80
#define MAX_POPUPS    16

typedef enum GameMode {
    GAMEMODE_CAMPAIGN,   /* enchainement des niveaux integres */
    GAMEMODE_SINGLE,     /* un niveau perso */
    GAMEMODE_TEST,       /* test depuis l'editeur */
    GAMEMODE_DEMO,       /* fond anime des menus */
} GameMode;

typedef enum GameState {
    GS_READY,
    GS_PLAY,
    GS_CLEAR,
    GS_OVER,
    GS_PAUSE,
    GS_DONE,             /* niveau unique termine (perso / test) */
} GameState;

/* PUP_FRUIT : bonus de points (fruit), sans effet sur le joueur */
/* PUP_STICK : gros batonnet de glace, les colonnes restent accrochees au plafond */
typedef enum PupKind {
    PUP_RAPID, PUP_DOUBLE, PUP_FREEZE, PUP_SHIELD, PUP_LIFE, PUP_STICK, PUP_FRUIT, PUP_KINDS
} PupKind;

typedef struct Player {
    int index;
    float x, y, w, h;       /* x,y = coin haut-gauche */
    float vy;
    bool climb, ground, alive, moving, shield;
    int ladder;             /* echelle en cours d'escalade */
    int face;
    int lives, score;
    float inv, cool, walk_t;
    bool rapid, dbl, stick;   /* bonus actifs jusqu'a la fin du niveau */
    bool fire_held;           /* tir de l'image precedente (colonne = un tir par appui) */
} Player;

typedef struct Ball { float x, y, vx, vy; int size; } Ball;
/* Colonne de givre : monte de base vers stop ; hold > 0 = reste accrochee hold secondes */
typedef struct Beam { float x, base, top, stop; int owner; float hold; int seq; } Beam;
typedef struct Bullet { float x, y, stop; int owner; } Bullet;
typedef struct Pup { float x, y, vy; PupKind kind; int fruit; } Pup;   /* reste jusqu'a la fin du niveau */
typedef struct Popup { float x, y, life; int value; } Popup;   /* "+500" qui monte */
typedef struct Particle { float x, y, vx, vy, life, max, size, rot; float r, g, b; } Particle;
typedef struct Flake { float x, y, r, speed, phase; } Flake;

typedef struct Backdrop {
    int hue;
    int world;                      /* decor du style retro, 1..BACKDROP_COUNT */
    float moon_x;
    float stars[MAX_STARS][4];      /* x, y, taille, alpha */
    float aurora[3][2];             /* y0, phase */
    int mountain_n[2];
    float mountain[2][32][2];
} Backdrop;

typedef struct Difficulty {
    const char *name;
    float speed;
    int lives;
    float spawn;
} Difficulty;

extern const Difficulty DIFFICULTIES[4];
extern const float BALL_R[4];

typedef struct Game {
    GameMode mode;
    GameState state, state_before_pause;
    int players_n;
    int diff_index;
    int level_no;           /* numero affiche (campagne) */
    int loop_n;             /* tours de campagne deja faits */
    char title[64];
    Level level;

    Player players[2];
    int ball_n, beam_n, bullet_n, pup_n, part_n, popup_n;
    int beam_seq;           /* ordre de tir des colonnes */
    Ball balls[MAX_BALLS];
    Beam beams[MAX_BEAMS];
    Bullet bullets[MAX_BULLETS];
    Pup pups[MAX_PUPS];
    Popup popups[MAX_POPUPS];
    Particle parts[MAX_PARTICLES];
    Flake flakes[MAX_FLAKES];
    Backdrop bg;

    float level_t, ready_t, clear_t, freeze_t, spawn_t, spawn_interval, next_spawn_x;
    float over_delay;
    float end_t;            /* temps passe sur l'ecran de fin */
    bool warned;
    int clear_bonus;
    bool new_record;
    bool won;               /* GS_DONE : niveau reussi ? */
    double clock;
} Game;

extern Game g_game;

/* Lancement */
void game_start_campaign(int start_level, int players, int difficulty);
void game_start_single(const Level *lvl, int players, int difficulty);
void game_start_test(const Level *lvl, int players);
void game_demo_init(void);
void game_demo_frame(float dt);     /* fond des menus : mise a jour + dessin */

/* Scene de jeu */
void game_frame(float dt);
void game_pause(void);

/* Physique d'une boule (partagee avec l'apercu de trajectoire de l'editeur) */
void game_ball_step(const Level *lvl, Ball *b, float dt);
Ball game_make_ball(float x, float y, int size, int dir);

/* Codes de reprise de la campagne */
const char *game_code_for_level(int n);
int game_level_from_code(const char *code);

/* Rendu (game_draw.c) ; utilisable par l'editeur avec un decalage gfx */
void draw_backdrop(const Backdrop *bg, double t);
void draw_flakes(const Game *g);
void draw_platform(float x, float y, float w);
void draw_ladder(float x, float top, float bottom);
void draw_laser(float x, float top, float bottom, double t);
void draw_ball(float x, float y, int size, bool frozen);
void draw_player_at(float cx, float feet_y, int index, int face, float alpha);
void draw_game(const Game *g);
void backdrop_build(Backdrop *bg, int hue, int seed);

/* Decor retro d'un niveau : champ backdrop, sinon un decor par tranche de 10
 * niveaux de campagne (level_no > 0), sinon le premier. */
int game_world_for(const Level *lvl, int level_no);

#endif
