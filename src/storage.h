#ifndef BDG_STORAGE_H
#define BDG_STORAGE_H

/*
 * Fichiers : reglages, niveaux de la campagne et niveaux perso.
 *  - Campagne : integree au binaire (levels/<nom>.lvl), ou lue depuis un dossier
 *    passe par --levels-dir (mode developpement : l'editeur ecrit alors
 *    directement dans le depot).
 *  - Niveaux perso : <dossier de preferences SDL>/levels/<nom>.lvl
 *  - Web : le dossier de preferences est persiste dans IndexedDB (IDBFS).
 */

#include "level.h"
#include <stdbool.h>
#include <stddef.h>

#define LEVEL_FILE_MAX 96
#define MAX_LEVEL_ENTRIES 256

typedef enum LevelSource { LEVELSRC_CAMPAIGN, LEVELSRC_USER } LevelSource;

typedef struct LevelEntry {
    LevelSource src;
    char file[LEVEL_FILE_MAX];        /* nom de fichier, sans dossier */
    char name[LEVEL_NAME_MAX];        /* nom affiche (lu dans le fichier) */
    bool valid;
    char error[96];
} LevelEntry;

void storage_init(const char *levels_dir_override);

/* Web : vrai une fois IndexedDB charge. Ailleurs : toujours vrai.
 * storage_poll_ready() renvoie vrai une seule fois, au moment ou ca devient pret. */
bool storage_ready(void);
bool storage_poll_ready(void);

const char *storage_pref_dir(void);
const char *storage_user_levels_dir(void);

char *storage_read_pref(const char *name, size_t *len);   /* a liberer avec SDL_free */
bool storage_write_pref(const char *name, const void *data, size_t len);

int storage_list(LevelSource src, LevelEntry *out, int max);
bool storage_load(LevelSource src, const char *file, Level *out, char *err, size_t cap);
bool storage_save(LevelSource src, const char *file, const Level *lvl, char *err, size_t cap);
bool storage_delete(LevelSource src, const char *file);
bool storage_exists(LevelSource src, const char *file);
bool storage_writable(LevelSource src);

/* Nom de fichier libre pour un nom de niveau : "mon-niveau.lvl", "mon-niveau-2.lvl"... */
void storage_unique_filename(LevelSource src, const char *level_name, char *out, size_t cap);

/* Ouvre le dossier des niveaux perso dans le gestionnaire de fichiers (bureau). */
bool storage_reveal_user_folder(void);

#endif
