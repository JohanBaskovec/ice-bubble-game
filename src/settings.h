#ifndef BDG_SETTINGS_H
#define BDG_SETTINGS_H

#include <stdbool.h>

typedef enum TouchMode { TOUCH_AUTO, TOUCH_ON, TOUCH_OFF } TouchMode;

typedef struct Settings {
    bool music;
    int sfx_volume;         /* 0..10 */
    bool fullscreen;
    TouchMode touch;
    int best_score;
    int players;            /* dernier choix 1/2 */
    int difficulty;         /* 0..3 */
} Settings;

extern Settings g_settings;

void settings_load(void);
void settings_save(void);

#endif
