#ifndef BDG_AUDIO_H
#define BDG_AUDIO_H

/* Synthetiseur logiciel : musique sequencee et effets generes a la volee.
 * Aucun fichier son : tout est calcule dans le callback audio de SDL. */

#include <stdbool.h>

typedef enum Sfx {
    SFX_SHOOT,
    SFX_PEW,
    SFX_POP,        /* param = taille de la boule 0..3 */
    SFX_POWER,
    SFX_HURT,
    SFX_SHIELD,
    SFX_WARN,
    SFX_CLEAR,
    SFX_OVER,
    SFX_FREEZE,
    SFX_CLICK,
} Sfx;

bool audio_init(void);
void audio_quit(void);
void audio_play(Sfx s, int param);
void audio_set_music(bool on);
void audio_set_sfx_volume(float v);     /* 0..1 */
void audio_pause_device(bool paused);   /* passage en arriere-plan */

#endif
