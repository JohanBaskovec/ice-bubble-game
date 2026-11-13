#include "input.h"
#include "gfx.h"
#include "settings.h"

#include <math.h>

#define MAX_PADS 4
#define MAX_FINGERS 10

typedef struct Finger {
    bool on;
    SDL_FingerID id;
    float x, y;
} Finger;

static SDL_Gamepad *g_pads[MAX_PADS];
static Finger g_fingers[MAX_FINGERS];
static bool g_touch_seen;

/* zones tactiles (coordonnees logiques du terrain 800x540) */
static const float PAD_X = 105, PAD_Y = 445, PAD_R = 95;
static const float FIRE_X = 712, FIRE_Y = 455, FIRE_R = 70;

void input_init(void)
{
#if defined(SDL_PLATFORM_ANDROID) || defined(SDL_PLATFORM_IOS)
    g_touch_seen = true;
#endif
}

void input_quit(void)
{
    for (int i = 0; i < MAX_PADS; i++)
        if (g_pads[i]) SDL_CloseGamepad(g_pads[i]);
}

int input_gamepad_count(void)
{
    int n = 0;
    for (int i = 0; i < MAX_PADS; i++) n += g_pads[i] != NULL;
    return n;
}

void input_clear(void)
{
    for (int i = 0; i < MAX_FINGERS; i++) g_fingers[i].on = false;
}

static Finger *find_finger(SDL_FingerID id, bool create)
{
    for (int i = 0; i < MAX_FINGERS; i++)
        if (g_fingers[i].on && g_fingers[i].id == id) return &g_fingers[i];
    if (!create) return NULL;
    for (int i = 0; i < MAX_FINGERS; i++)
        if (!g_fingers[i].on) {
            g_fingers[i].on = true;
            g_fingers[i].id = id;
            return &g_fingers[i];
        }
    return NULL;
}

void input_handle_event(const SDL_Event *e)
{
    switch (e->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
        for (int i = 0; i < MAX_PADS; i++)
            if (!g_pads[i]) {
                g_pads[i] = SDL_OpenGamepad(e->gdevice.which);
                break;
            }
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        for (int i = 0; i < MAX_PADS; i++)
            if (g_pads[i] && SDL_GetGamepadID(g_pads[i]) == e->gdevice.which) {
                SDL_CloseGamepad(g_pads[i]);
                g_pads[i] = NULL;
            }
        break;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION: {
        g_touch_seen = true;
        Finger *f = find_finger(e->tfinger.fingerID, e->type == SDL_EVENT_FINGER_DOWN);
        if (f) {
            f->x = e->tfinger.x;
            f->y = e->tfinger.y;
        }
        break;
    }
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED: {
        Finger *f = find_finger(e->tfinger.fingerID, false);
        if (f) f->on = false;
        break;
    }
    default: break;
    }
}

bool input_touch_visible(void)
{
    if (g_settings.touch == TOUCH_ON) return true;
    if (g_settings.touch == TOUCH_OFF) return false;
    return g_touch_seen;
}

static void touch_state(PlayerInput *in)
{
    if (!input_touch_visible()) return;
    for (int i = 0; i < MAX_FINGERS; i++) {
        const Finger *f = &g_fingers[i];
        if (!f->on) continue;
        float dx = f->x - PAD_X, dy = f->y - PAD_Y;
        float d = sqrtf(dx * dx + dy * dy);
        if (d < PAD_R * 1.4f && d > 14) {
            /* croix directionnelle 8 directions */
            float a = atan2f(dy, dx);
            float ca = cosf(a), sa = sinf(a);
            if (ca > 0.38f) in->right = true;
            if (ca < -0.38f) in->left = true;
            if (sa > 0.38f) in->down = true;
            if (sa < -0.38f) in->up = true;
        }
        float fx = f->x - FIRE_X, fy = f->y - FIRE_Y;
        if (fx * fx + fy * fy < FIRE_R * FIRE_R * 1.6f) in->fire = true;
    }
}

static void pad_state(SDL_Gamepad *p, PlayerInput *in)
{
    if (!p) return;
    const Sint16 dead = 12000;
    Sint16 ax = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFTX);
    Sint16 ay = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFTY);
    if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || ax < -dead) in->left = true;
    if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || ax > dead) in->right = true;
    if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_UP) || ay < -dead) in->up = true;
    if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_DOWN) || ay > dead) in->down = true;
    if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_SOUTH) || SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_WEST) ||
        SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16000)
        in->fire = true;
}

/* touches maintenues par le lecteur de scripts (tests) */
static bool g_forced[SDL_SCANCODE_COUNT];

void input_force_key(SDL_Scancode sc, bool down)
{
    if (sc > SDL_SCANCODE_UNKNOWN && sc < SDL_SCANCODE_COUNT) g_forced[sc] = down;
}

static bool key(const bool *k, SDL_Scancode sc) { return k[sc] || g_forced[sc]; }

PlayerInput input_player(int index, int player_count)
{
    PlayerInput in = { 0 };
    const bool *k = SDL_GetKeyboardState(NULL);
    bool solo = player_count < 2;

    if (solo || index == 0) {
        in.left |= key(k, SDL_SCANCODE_LEFT);
        in.right |= key(k, SDL_SCANCODE_RIGHT);
        in.up |= key(k, SDL_SCANCODE_UP);
        in.down |= key(k, SDL_SCANCODE_DOWN);
        in.fire |= key(k, SDL_SCANCODE_SPACE) || key(k, SDL_SCANCODE_RETURN) || key(k, SDL_SCANCODE_RCTRL) ||
                   key(k, SDL_SCANCODE_KP_0);
        touch_state(&in);
    }
    if (solo || index == 1) {
        /* scancodes = position physique : WASD en QWERTY = ZQSD en AZERTY */
        in.left |= key(k, SDL_SCANCODE_A);
        in.right |= key(k, SDL_SCANCODE_D);
        in.up |= key(k, SDL_SCANCODE_W);
        in.down |= key(k, SDL_SCANCODE_S);
        in.fire |= key(k, SDL_SCANCODE_F) || key(k, SDL_SCANCODE_G) || key(k, SDL_SCANCODE_LSHIFT);
    }
    if (solo) {
        for (int i = 0; i < MAX_PADS; i++) pad_state(g_pads[i], &in);
    } else {
        /* la n-ieme manette connectee pilote le n-ieme joueur */
        int seen = 0;
        for (int i = 0; i < MAX_PADS; i++) {
            if (!g_pads[i]) continue;
            if (seen == index) pad_state(g_pads[i], &in);
            seen++;
        }
    }
    return in;
}

void input_draw_touch(void)
{
    if (!input_touch_visible()) return;
    PlayerInput in = { 0 };
    touch_state(&in);
    Color base = rgba(0xFFFFFF, 0.13f), on = rgba(0x9FE0FF, 0.45f), edge = rgba(0xFFFFFF, 0.3f);
    gfx_circle(PAD_X, PAD_Y, PAD_R, rgba(0xFFFFFF, 0.06f));
    gfx_ring(PAD_X, PAD_Y, PAD_R, 2, edge);
    const float off = PAD_R * 0.62f, br = 24;
    struct { float x, y; bool p; int dir; } b[4] = {
        { PAD_X, PAD_Y - off, in.up, 0 },
        { PAD_X + off, PAD_Y, in.right, 1 },
        { PAD_X, PAD_Y + off, in.down, 2 },
        { PAD_X - off, PAD_Y, in.left, 3 },
    };
    for (int i = 0; i < 4; i++) {
        gfx_circle(b[i].x, b[i].y, br, b[i].p ? on : base);
        gfx_arrow(b[i].x, b[i].y, 20, b[i].dir, rgba(0xFFFFFF, 0.8f));
    }
    gfx_circle(FIRE_X, FIRE_Y, FIRE_R * 0.75f, in.fire ? on : rgba(0x7CD0FF, 0.25f));
    gfx_ring(FIRE_X, FIRE_Y, FIRE_R * 0.75f, 2, edge);
    gfx_snowflake(FIRE_X, FIRE_Y, 22, 3, rgba(0xFFFFFF, 0.85f));
}
