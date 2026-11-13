#ifndef BDG_EMBEDDED_H
#define BDG_EMBEDDED_H

#include <stddef.h>

/* Ressources integrees au binaire par cmake/EmbedAssets.cmake. */
typedef struct EmbeddedFile {
    const char *name;            /* nom de fichier, ex. "001-premier-flocon.lvl" */
    const unsigned char *data;   /* contenu (termine par un octet nul en plus) */
    size_t size;                 /* taille sans l'octet nul */
} EmbeddedFile;

extern const unsigned char *const g_font_data;
extern const size_t g_font_size;

extern const EmbeddedFile g_campaign_levels[];
extern const int g_campaign_level_count;

#endif
