#include "ui.h"
#include "audio.h"
#include "font.h"
#include "gfx.h"

#include <string.h>

#define MAX_KEYS 16

static SDL_Window *g_win;

static float g_mx = -1000, g_my = -1000;
static bool g_down, g_pressed, g_released, g_rpressed;
static float g_wheel;

static int g_id;              /* compteur de widgets de la frame */
static int g_active = -1;     /* widget sur lequel le bouton a ete enfonce */
static bool g_hot;

static bool g_nav;
static int g_focus;           /* index du bouton focalise (navigation) */
static int g_focus_count, g_last_focus_count;
static int g_nav_delta;
static bool g_nav_activate;
static bool g_show_focus;
static bool g_enter_unfocused;  /* Entree/A sans focus visible : action par defaut */

static char *g_text_buf;      /* champ texte actif */
static char *g_text_submit;    /* champ valide par Entree cette frame */

static bool g_back;
static bool g_enabled = true;  /* faux : widgets affiches mais inactifs (sous une modale) */
static SDL_Keycode g_keys[MAX_KEYS];
static int g_key_count;
static SDL_Keymod g_mods;          /* modificateurs des touches enfoncees cette frame */

void ui_init(SDL_Window *w) { g_win = w; }

void ui_set_nav(bool on) { g_nav = on; }
void ui_set_enabled(bool on) { g_enabled = on; }
void ui_reset_focus(void) { g_focus = 0; }

static void text_backspace(void)
{
    size_t n = strlen(g_text_buf);
    if (n == 0) return;
    n--;
    while (n > 0 && ((unsigned char)g_text_buf[n] & 0xC0) == 0x80) n--;
    g_text_buf[n] = '\0';
}

static size_t g_text_cap;

void ui_text_blur(void)
{
    if (g_text_buf) SDL_StopTextInput(g_win);
    g_text_buf = NULL;
}

bool ui_text_active(void) { return g_text_buf != NULL; }

void ui_handle_event(const SDL_Event *e)
{
    switch (e->type) {
    case SDL_EVENT_MOUSE_MOTION:
        g_mx = e->motion.x;
        g_my = e->motion.y;
        if (e->motion.which != SDL_TOUCH_MOUSEID) g_show_focus = false;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        g_mx = e->button.x;
        g_my = e->button.y;
        if (e->button.button == SDL_BUTTON_LEFT) {
            g_down = true;
            g_pressed = true;
        } else if (e->button.button == SDL_BUTTON_RIGHT) {
            g_rpressed = true;
        }
        g_show_focus = false;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        g_mx = e->button.x;
        g_my = e->button.y;
        if (e->button.button == SDL_BUTTON_LEFT) {
            g_down = false;
            g_released = true;
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL: g_wheel += e->wheel.y; break;
    case SDL_EVENT_TEXT_INPUT:
        if (g_text_buf) {
            size_t n = strlen(g_text_buf), add = strlen(e->text.text);
            if (n + add < g_text_cap) memcpy(g_text_buf + n, e->text.text, add + 1);
        }
        break;
    case SDL_EVENT_KEY_DOWN: {
        SDL_Keycode k = e->key.key;
        g_mods |= e->key.mod;
        if (g_text_buf) {
            if (k == SDLK_BACKSPACE) text_backspace();
            else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { g_text_submit = g_text_buf; ui_text_blur(); }
            else if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) ui_text_blur();
            break;
        }
        if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) g_back = true;
        if (g_nav && !e->key.repeat && (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE)) {
            if (g_show_focus) g_nav_activate = true;
            else g_enter_unfocused = true;
        }
        if (g_nav) {
            if (k == SDLK_UP || k == SDLK_LEFT) { g_nav_delta--; g_show_focus = true; }
            if (k == SDLK_DOWN || k == SDLK_RIGHT || k == SDLK_TAB) { g_nav_delta++; g_show_focus = true; }
        }
        if (g_key_count < MAX_KEYS) g_keys[g_key_count++] = k;
        break;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        switch (e->gbutton.button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: g_nav_delta--; g_show_focus = true; break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: g_nav_delta++; g_show_focus = true; break;
        case SDL_GAMEPAD_BUTTON_SOUTH:
            if (g_show_focus) g_nav_activate = true;
            else g_enter_unfocused = true;
            break;
        case SDL_GAMEPAD_BUTTON_EAST:
        case SDL_GAMEPAD_BUTTON_BACK: g_back = true; break;
        default: break;
        }
        break;
    default: break;
    }
}

void ui_begin_frame(void)
{
    g_id = 0;
    g_enabled = true;
    g_hot = false;
    g_focus_count = 0;
    if (g_last_focus_count > 0) {
        g_focus = (g_focus + g_nav_delta) % g_last_focus_count;
        if (g_focus < 0) g_focus += g_last_focus_count;
    }
    g_nav_delta = 0;
}

void ui_end_frame(void)
{
    g_last_focus_count = g_focus_count;
    if (g_focus >= g_focus_count && g_focus_count > 0) g_focus = g_focus_count - 1;
    if (g_released) g_active = -1;
    g_pressed = g_released = g_rpressed = false;
    g_wheel = 0;
    g_nav_activate = false;
    g_enter_unfocused = false;
    g_back = false;
    g_key_count = 0;
    g_mods = 0;
    g_text_submit = NULL;
}

float ui_mx(void) { return g_mx; }
float ui_my(void) { return g_my; }
bool ui_mouse_down(void) { return g_down; }
bool ui_mouse_pressed(void) { return g_pressed; }
bool ui_mouse_released(void) { return g_released; }
bool ui_right_pressed(void) { return g_rpressed; }
float ui_wheel(void) { return g_wheel; }
bool ui_hot(void) { return g_hot; }
bool ui_enter_unfocused(void) { return g_enter_unfocused; }
bool ui_back_pressed(void)
{
    /* consomme l'evenement : un seul ecran reagit a un appui */
    bool b = g_back;
    g_back = false;
    return b;
}
SDL_Keymod ui_mods(void) { return g_mods | SDL_GetModState(); }

bool ui_key_pressed(SDL_Keycode k)
{
    if (g_text_buf) return false;
    for (int i = 0; i < g_key_count; i++)
        if (g_keys[i] == k) return true;
    return false;
}

bool ui_hover(float x, float y, float w, float h)
{
    float ox, oy;
    gfx_get_offset(&ox, &oy);
    float mx = g_mx - ox, my = g_my - oy;
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

/* ------------------------------------------------------------------------ */

/* Style retro (actif avec la police pixel) : rectangles biseautes */
static bool retro_ui(void) { return font_pixel(); }

static void bevel(float x, float y, float w, float h, Color face, Color light, Color dark, bool sunken)
{
    gfx_rect(x, y, w, h, face);
    Color a = sunken ? dark : light, b = sunken ? light : dark;
    gfx_rect(x, y, w, 3, a);
    gfx_rect(x, y, 3, h, a);
    gfx_rect(x, y + h - 3, w, 3, b);
    gfx_rect(x + w - 3, y, 3, h, b);
    gfx_rect(x - 2, y - 2, w + 4, 2, rgb(0x050814));
    gfx_rect(x - 2, y + h, w + 4, 2, rgb(0x050814));
    gfx_rect(x - 2, y, 2, h, rgb(0x050814));
    gfx_rect(x + w, y, 2, h, rgb(0x050814));
}

void ui_panel(float x, float y, float w, float h)
{
    if (retro_ui()) {
        bevel(x, y, w, h, rgba(0x101830, 0.92f), rgb(0x6A7CA8), rgb(0x1A2238), false);
        return;
    }
    gfx_rrect(x, y, w, h, 14, rgba(0x040A20, 0.82f));
    gfx_rrect_outline(x, y, w, h, 14, 2, rgba(0x8CCDFF, 0.35f));
}

void ui_dim(float x, float y, float w, float h) { gfx_rect(x, y, w, h, rgba(0x040A20, 0.7f)); }

bool ui_button(const char *label, float x, float y, float w, float h, BtnStyle style)
{
    int id = g_id++;
    bool hover = g_enabled && ui_hover(x, y, w, h);
    int focus_index = -1;
    if (g_nav) focus_index = g_focus_count++;
    if (hover) {
        g_hot = true;
        if (g_nav && !g_show_focus) g_focus = focus_index;
    }
    if (hover && g_pressed) g_active = id;
    bool clicked = hover && g_released && g_active == id;
    bool focused = g_nav && g_show_focus && focus_index == g_focus;
    if (focused && g_nav_activate) clicked = true;
    bool held = g_down && g_active == id && hover;

    float r = style == BTN_BIG ? 12 : 10;
    float dy = held ? 2 : 0;
    float size = h * 0.48f;
    if (size > 26) size = 26;
    Color text = rgb(0xDFF3FF);

    if (retro_ui()) {
        if (style == BTN_BIG) size = h * 0.5f;
    } else switch (style) {
    case BTN_BIG:
        size = h * 0.5f;
        gfx_rrect(x, y + 4, w, h, r, rgb(0x1F5C99));
        gfx_rrect_vgrad(x, y + dy, w, h, r, rgb(0x9FE0FF), rgb(0x4AA3E6));
        gfx_rrect_outline(x, y + dy, w, h, r, 2, rgb(0xDFF5FF));
        text = rgb(0x06203F);
        break;
    case BTN_SELECTED:
    case BTN_TOOL_ON:
        gfx_rrect(x, y + dy, w, h, r, rgb(0x7CD0FF));
        gfx_rrect_outline(x, y + dy, w, h, r, 2, rgb(0xBFEAFF));
        text = rgb(0x082044);
        break;
    case BTN_DANGER:
        gfx_rrect(x, y + dy, w, h, r, rgba(0xFF6B8B, hover ? 0.45f : 0.25f));
        gfx_rrect_outline(x, y + dy, w, h, r, 2, rgba(0xFF9AAE, 0.6f));
        break;
    default:
        gfx_rrect(x, y + dy, w, h, r, rgba(0x78BEFF, hover ? 0.25f : 0.12f));
        gfx_rrect_outline(x, y + dy, w, h, r, 2, rgba(0x8CCDFF, 0.35f));
        break;
    }
    if (retro_ui()) {
        /* repeint par-dessus : boutons biseautes facon annees 90 */
        static const Uint32 FACES[][3] = {
            { 0x3A4A78, 0x7F94C8, 0x1A2140 },    /* normal */
            { 0x2FA8D8, 0xA8ECFF, 0x0F4F78 },    /* selectionne */
            { 0xE8A020, 0xFFE08A, 0x7A4A08 },    /* grand */
            { 0xA82848, 0xFF8AA0, 0x501020 },    /* danger */
        };
        int k = 0;
        if (style == BTN_SELECTED || style == BTN_TOOL_ON) k = 1;
        else if (style == BTN_BIG) k = 2;
        else if (style == BTN_DANGER) k = 3;
        Color face = rgb(FACES[k][0]);
        if (hover && !held) face = color_lerp(face, rgb(0xFFFFFF), 0.15f);
        bevel(x, y, w, h, face, rgb(FACES[k][1]), rgb(FACES[k][2]), held);
        text = k == 0 || k == 3 ? rgb(0xFFFFFF) : rgb(0x101828);
        if (focused) {
            Color f = rgb(0xFFFFFF);
            gfx_rect(x - 6, y - 6, w + 12, 2, f);
            gfx_rect(x - 6, y + h + 4, w + 12, 2, f);
            gfx_rect(x - 6, y - 6, 2, h + 12, f);
            gfx_rect(x + w + 4, y - 6, 2, h + 12, f);
        }
        dy = held ? 2 : 0;
        float tw = font_measure(label, size);
        if (tw > w - 12 && tw > 0) size *= (w - 12) / tw;
        font_draw(label, x + w / 2 + dy, y + h / 2 + dy, size, text, ALIGN_CENTER);
        if (clicked) audio_play(SFX_CLICK, 0);
        return clicked;
    }
    if (style == BTN_TOOL || style == BTN_TOOL_ON) size = h * 0.42f > 16 ? 16 : h * 0.42f;
    if (focused) gfx_rrect_outline(x - 4, y - 4 + dy, w + 8, h + 8, r + 4, 2.5f, rgb(0xFFFFFF));

    /* reduit le texte s'il deborde */
    float tw = font_measure(label, size);
    if (tw > w - 12 && tw > 0) size *= (w - 12) / tw;
    font_draw(label, x + w / 2, y + h / 2 + dy, size, text, ALIGN_CENTER);

    if (clicked) audio_play(SFX_CLICK, 0);
    return clicked;
}

bool ui_textfield(char *buf, size_t cap, float x, float y, float w, float h, const char *placeholder)
{
    g_id++;
    bool hover = g_enabled && ui_hover(x, y, w, h);
    if (hover) g_hot = true;
    bool focused = g_text_buf == buf;
    if (g_pressed) {
        if (hover && !focused) {
            g_text_buf = buf;
            g_text_cap = cap;
            SDL_Rect area = { (int)x, (int)y, (int)w, (int)h };
            SDL_SetTextInputArea(g_win, &area, 0);
            SDL_StartTextInput(g_win);
            focused = true;
        } else if (!hover && focused) {
            ui_text_blur();
            focused = false;
        }
    }
    if (retro_ui()) {
        bevel(x, y, w, h, rgba(0x000000, 0.55f), rgb(0x1A2238), focused ? rgb(0x9FE0FF) : rgb(0x6A7CA8), false);
    } else {
        gfx_rrect(x, y, w, h, 10, rgba(0x000000, 0.35f));
        gfx_rrect_outline(x, y, w, h, 10, 2, focused ? rgb(0x9FE0FF) : rgba(0x8CCDFF, 0.35f));
    }
    float size = h * 0.5f;
    if (size > 20) size = 20;
    if (buf[0]) {
        font_draw(buf, x + 10, y + h / 2, size, rgb(0xFFFFFF), ALIGN_LEFT);
    } else if (placeholder && !focused) {
        font_draw(placeholder, x + 10, y + h / 2, size, rgba(0xFFFFFF, 0.4f), ALIGN_LEFT);
    }
    if (focused && (SDL_GetTicks() / 500) % 2 == 0) {
        float cx = x + 11 + font_measure(buf, size);
        gfx_rect(cx, y + h * 0.22f, 2, h * 0.56f, rgb(0x9FE0FF));
    }
    return g_text_submit == buf;
}
