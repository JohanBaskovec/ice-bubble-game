#include "level.h"
#include "config.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void set_err(char *err, size_t cap, int line, const char *fmt, ...)
{
    if (!err || cap == 0) return;
    int n = 0;
    if (line > 0) n = snprintf(err, cap, "ligne %d : ", line);
    if (n < 0 || (size_t)n >= cap) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err + n, cap - (size_t)n, fmt, ap);
    va_end(ap);
}

void level_init(Level *lvl)
{
    memset(lvl, 0, sizeof(*lvl));
    snprintf(lvl->name, sizeof(lvl->name), "Nouveau niveau");
    lvl->hue = 215;
    lvl->reinforce = REINFORCE_DEFAULT;
    lvl->start[0].x = 336; lvl->start[0].y = FLOOR_Y;
    lvl->start[1].x = 464; lvl->start[1].y = FLOOR_Y;
}

/* ------------------------------------------------------------------------ */
/* Lecture                                                                  */
/* ------------------------------------------------------------------------ */

#define MAX_TOKENS 8
#define LINE_MAX_LEN 512

typedef struct Tokens {
    int count;
    char *tok[MAX_TOKENS];
} Tokens;

/* Decoupe une ligne en mots ; gere "chaines entre guillemets" et les
 * commentaires (#). Modifie la ligne en place. */
static bool tokenize(char *line, Tokens *t)
{
    t->count = 0;
    char *p = line;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p || *p == '#') break;
        if (t->count >= MAX_TOKENS) return false;
        if (*p == '"') {
            p++;
            char *dst = p;
            t->tok[t->count++] = dst;
            while (*p && *p != '"') {
                if (*p == '\\' && (p[1] == '"' || p[1] == '\\')) p++;
                *dst++ = *p++;
            }
            if (*p != '"') return false;      /* guillemet non ferme */
            p++;
            *dst = '\0';
        } else {
            t->tok[t->count++] = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) *p++ = '\0';
        }
    }
    return true;
}

static bool parse_num(const char *s, float *out)
{
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s || *end != '\0' || !isfinite(v)) return false;
    *out = (float)v;
    return true;
}

static bool parse_int(const char *s, int *out)
{
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return false;
    *out = (int)v;
    return true;
}

bool level_parse(Level *out, const char *text, size_t len, char *err, size_t err_cap)
{
    Level lvl;
    level_init(&lvl);
    lvl.name[0] = '\0';
    if (err && err_cap) err[0] = '\0';

    bool saw_format = false;
    int line_no = 0;
    size_t pos = 0;

    /* BOM UTF-8 eventuel */
    if (len >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF)
        pos = 3;

    while (pos < len) {
        char line[LINE_MAX_LEN];
        size_t n = 0;
        line_no++;
        while (pos < len && text[pos] != '\n') {
            if (text[pos] == '\0') { pos = len; break; }
            if (n + 1 < sizeof(line)) line[n++] = text[pos];
            pos++;
        }
        if (pos < len) pos++;                 /* saute '\n' */
        if (n > 0 && line[n - 1] == '\r') n--;
        line[n] = '\0';

        Tokens t;
        if (!tokenize(line, &t)) {
            set_err(err, err_cap, line_no, "syntaxe invalide (guillemet manquant ou trop de valeurs)");
            return false;
        }
        if (t.count == 0) continue;

        const char *key = t.tok[0];
        int argc = t.count - 1;
        char **argv = t.tok + 1;

        if (strcmp(key, "format") == 0) {
            int v;
            if (argc != 1 || !parse_int(argv[0], &v)) {
                set_err(err, err_cap, line_no, "format attendu : format <version>");
                return false;
            }
            if (v > LEVEL_FORMAT_VERSION) {
                set_err(err, err_cap, line_no, "version de format %d trop recente (max %d)", v,
                        LEVEL_FORMAT_VERSION);
                return false;
            }
            saw_format = true;
        } else if (strcmp(key, "name") == 0) {
            if (argc != 1) {
                set_err(err, err_cap, line_no, "format attendu : name \"Nom du niveau\"");
                return false;
            }
            snprintf(lvl.name, sizeof(lvl.name), "%s", argv[0]);
        } else if (strcmp(key, "hue") == 0) {
            int v;
            if (argc != 1 || !parse_int(argv[0], &v)) {
                set_err(err, err_cap, line_no, "format attendu : hue <0-359>");
                return false;
            }
            lvl.hue = ((v % 360) + 360) % 360;
        } else if (strcmp(key, "reinforcements") == 0) {
            float v;
            if (argc != 1) {
                set_err(err, err_cap, line_no, "format attendu : reinforcements default|off|<secondes>");
                return false;
            }
            if (strcmp(argv[0], "default") == 0) lvl.reinforce = REINFORCE_DEFAULT;
            else if (strcmp(argv[0], "off") == 0) lvl.reinforce = REINFORCE_OFF;
            else if (parse_num(argv[0], &v) && v >= 3.0f && v <= 600.0f) lvl.reinforce = v;
            else {
                set_err(err, err_cap, line_no, "renforts : 'default', 'off' ou 3 a 600 secondes");
                return false;
            }
        } else if (strcmp(key, "start") == 0) {
            int who;
            float x, y;
            if (argc != 3 || !parse_int(argv[0], &who) || who < 1 || who > 2 ||
                !parse_num(argv[1], &x) || !parse_num(argv[2], &y)) {
                set_err(err, err_cap, line_no, "format attendu : start <1|2> <x> <y>");
                return false;
            }
            lvl.start[who - 1].x = x;
            lvl.start[who - 1].y = y;
        } else if (strcmp(key, "platform") == 0) {
            LevelPlatform p;
            if (argc != 3 || !parse_num(argv[0], &p.x) || !parse_num(argv[1], &p.y) ||
                !parse_num(argv[2], &p.w)) {
                set_err(err, err_cap, line_no, "format attendu : platform <x> <y> <largeur>");
                return false;
            }
            if (lvl.platform_count >= LEVEL_MAX_PLATFORMS) {
                set_err(err, err_cap, line_no, "trop de plateformes (max %d)", LEVEL_MAX_PLATFORMS);
                return false;
            }
            lvl.platforms[lvl.platform_count++] = p;
        } else if (strcmp(key, "ladder") == 0) {
            LevelLadder l;
            if (argc != 3 || !parse_num(argv[0], &l.x) || !parse_num(argv[1], &l.top) ||
                !parse_num(argv[2], &l.bottom)) {
                set_err(err, err_cap, line_no, "format attendu : ladder <x> <haut> <bas>");
                return false;
            }
            if (lvl.ladder_count >= LEVEL_MAX_LADDERS) {
                set_err(err, err_cap, line_no, "trop d'echelles (max %d)", LEVEL_MAX_LADDERS);
                return false;
            }
            lvl.ladders[lvl.ladder_count++] = l;
        } else if (strcmp(key, "ball") == 0) {
            LevelBall b;
            int size;
            if (argc != 4 || !parse_num(argv[0], &b.x) || !parse_num(argv[1], &b.y) ||
                !parse_int(argv[2], &size) || size < 1 || size > BALL_SIZES) {
                set_err(err, err_cap, line_no, "format attendu : ball <x> <y> <taille 1-4> <left|right>");
                return false;
            }
            if (strcmp(argv[3], "left") == 0) b.dir = -1;
            else if (strcmp(argv[3], "right") == 0) b.dir = 1;
            else {
                set_err(err, err_cap, line_no, "direction de boule : 'left' ou 'right'");
                return false;
            }
            b.size = size - 1;
            if (lvl.ball_count >= LEVEL_MAX_BALLS) {
                set_err(err, err_cap, line_no, "trop de boules (max %d)", LEVEL_MAX_BALLS);
                return false;
            }
            lvl.balls[lvl.ball_count++] = b;
        }
        /* cle inconnue : ignoree (fichier d'une version future) */
    }

    if (!saw_format) {
        set_err(err, err_cap, 0, "ligne 'format 1' manquante : ce n'est pas un niveau valide");
        return false;
    }
    if (lvl.name[0] == '\0') snprintf(lvl.name, sizeof(lvl.name), "Sans titre");

    level_clamp(&lvl);
    *out = lvl;
    return true;
}

/* ------------------------------------------------------------------------ */
/* Ecriture                                                                 */
/* ------------------------------------------------------------------------ */

typedef struct Writer {
    char *buf;
    size_t cap, len;
} Writer;

static void wr(Writer *w, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *dst = w->len < w->cap ? w->buf + w->len : NULL;
    size_t room = w->len < w->cap ? w->cap - w->len : 0;
    int n = vsnprintf(dst, room, fmt, ap);
    va_end(ap);
    if (n > 0) w->len += (size_t)n;
}

/* Nombre court : entier si possible, sinon une decimale. */
static const char *num(char *tmp, size_t cap, float v)
{
    float r = roundf(v);
    if (fabsf(v - r) < 0.05f) snprintf(tmp, cap, "%d", (int)r);
    else snprintf(tmp, cap, "%.1f", v);
    return tmp;
}

size_t level_serialize(const Level *lvl, char *buf, size_t cap)
{
    Writer w = { buf, cap, 0 };
    char a[24], b[24], c[24];

    wr(&w, "# Boules de Givre - fichier de niveau (voir docs/LEVEL_FORMAT.md)\n");
    wr(&w, "format %d\n", LEVEL_FORMAT_VERSION);

    wr(&w, "name \"");
    for (const char *p = lvl->name; *p; p++) {
        if (*p == '"' || *p == '\\') wr(&w, "\\%c", *p);
        else if (*p != '\n' && *p != '\r') wr(&w, "%c", *p);
    }
    wr(&w, "\"\n");

    wr(&w, "hue %d\n", lvl->hue);
    if (lvl->reinforce == REINFORCE_DEFAULT) wr(&w, "reinforcements default\n");
    else if (lvl->reinforce < 0) wr(&w, "reinforcements off\n");
    else wr(&w, "reinforcements %s\n", num(a, sizeof a, lvl->reinforce));

    wr(&w, "\n# joueurs : start <1|2> <x centre> <y pieds>\n");
    for (int i = 0; i < 2; i++)
        wr(&w, "start %d %s %s\n", i + 1, num(a, sizeof a, lvl->start[i].x),
           num(b, sizeof b, lvl->start[i].y));

    if (lvl->platform_count) wr(&w, "\n# platform <x> <y> <largeur>\n");
    for (int i = 0; i < lvl->platform_count; i++) {
        const LevelPlatform *p = &lvl->platforms[i];
        wr(&w, "platform %s %s %s\n", num(a, sizeof a, p->x), num(b, sizeof b, p->y),
           num(c, sizeof c, p->w));
    }

    if (lvl->ladder_count) wr(&w, "\n# ladder <x gauche> <haut> <bas>\n");
    for (int i = 0; i < lvl->ladder_count; i++) {
        const LevelLadder *l = &lvl->ladders[i];
        wr(&w, "ladder %s %s %s\n", num(a, sizeof a, l->x), num(b, sizeof b, l->top),
           num(c, sizeof c, l->bottom));
    }

    if (lvl->ball_count) wr(&w, "\n# ball <x> <y> <taille 1-4> <left|right>\n");
    for (int i = 0; i < lvl->ball_count; i++) {
        const LevelBall *bl = &lvl->balls[i];
        wr(&w, "ball %s %s %d %s\n", num(a, sizeof a, bl->x), num(b, sizeof b, bl->y),
           bl->size + 1, bl->dir < 0 ? "left" : "right");
    }

    if (cap > 0) buf[w.len < cap ? w.len : cap - 1] = '\0';
    return w.len;
}

/* ------------------------------------------------------------------------ */
/* Validation, bornes, utilitaires                                          */
/* ------------------------------------------------------------------------ */

static const float BALL_RADIUS[BALL_SIZES] = { 9, 15, 23, 33 };

void level_clamp(Level *lvl)
{
    lvl->hue = ((lvl->hue % 360) + 360) % 360;
    for (int i = 0; i < 2; i++) {
        lvl->start[i].x = clampf(lvl->start[i].x, 14, GAME_W - 14);
        lvl->start[i].y = clampf(lvl->start[i].y, CEIL_Y + 50, FLOOR_Y);
    }
    for (int i = 0; i < lvl->platform_count; i++) {
        LevelPlatform *p = &lvl->platforms[i];
        p->w = clampf(p->w, 16, GAME_W);
        p->x = clampf(p->x, 0, GAME_W - p->w);
        p->y = clampf(p->y, CEIL_Y + 20, FLOOR_Y - PLAT_H - 20);
    }
    for (int i = 0; i < lvl->ladder_count; i++) {
        LevelLadder *l = &lvl->ladders[i];
        l->x = clampf(l->x, 0, GAME_W - LADDER_W);
        l->bottom = clampf(l->bottom, CEIL_Y + 40, FLOOR_Y);
        l->top = clampf(l->top, CEIL_Y + 20, l->bottom - 20);
    }
    for (int i = 0; i < lvl->ball_count; i++) {
        LevelBall *b = &lvl->balls[i];
        if (b->size < 0) b->size = 0;
        if (b->size >= BALL_SIZES) b->size = BALL_SIZES - 1;
        b->dir = b->dir < 0 ? -1 : 1;
        float r = BALL_RADIUS[b->size];
        b->x = clampf(b->x, r, GAME_W - r);
        b->y = clampf(b->y, CEIL_Y + r, FLOOR_Y - r);
    }
}

int level_validate(const Level *lvl, char *msg, size_t msg_cap)
{
    int problems = 0;
    char first[160] = "";
#define PROBLEM(...)                                                   \
    do {                                                               \
        if (problems++ == 0) snprintf(first, sizeof first, __VA_ARGS__); \
    } while (0)

    if (lvl->ball_count == 0) PROBLEM("Il faut au moins une boule.");

    for (int i = 0; i < lvl->ball_count; i++) {
        const LevelBall *b = &lvl->balls[i];
        float r = BALL_RADIUS[b->size];
        for (int j = 0; j < lvl->platform_count; j++) {
            const LevelPlatform *p = &lvl->platforms[j];
            float cx = clampf(b->x, p->x, p->x + p->w), cy = clampf(b->y, p->y, p->y + PLAT_H);
            float dx = b->x - cx, dy = b->y - cy;
            if (dx * dx + dy * dy < r * r) {
                PROBLEM("La boule %d touche une plateforme.", i + 1);
                break;
            }
        }
        for (int k = 0; k < 2; k++) {
            const LevelStart *s = &lvl->start[k];
            if (fabsf(b->x - s->x) < r + 60 && b->y > s->y - 44 - r - 60 && b->y < s->y + r)
                PROBLEM("La boule %d est trop proche du depart du joueur %d.", i + 1, k + 1);
        }
    }

    for (int i = 0; i < lvl->ladder_count; i++) {
        const LevelLadder *l = &lvl->ladders[i];
        if (l->bottom - l->top < 30) PROBLEM("L'echelle %d est trop courte.", i + 1);
    }

    if (msg && msg_cap) snprintf(msg, msg_cap, "%s", first);
    return problems;
#undef PROBLEM
}

void level_slugify(const char *name, char *out, size_t cap)
{
    if (cap == 0) return;
    size_t n = 0;
    bool dash = false;
    for (const unsigned char *p = (const unsigned char *)name; *p && n + 1 < cap; p++) {
        unsigned char c = *p;
        /* translitteration minimale des accents francais (UTF-8 sur 2 octets) */
        if (c == 0xC3 && p[1]) {
            unsigned char d = p[1];
            const char *map = NULL;
            if ((d >= 0xA0 && d <= 0xA5) || (d >= 0x80 && d <= 0x85)) map = "a";
            else if (d == 0xA7 || d == 0x87) map = "c";
            else if ((d >= 0xA8 && d <= 0xAB) || (d >= 0x88 && d <= 0x8B)) map = "e";
            else if ((d >= 0xAC && d <= 0xAF) || (d >= 0x8C && d <= 0x8F)) map = "i";
            else if ((d >= 0xB2 && d <= 0xB6) || (d >= 0x92 && d <= 0x96)) map = "o";
            else if ((d >= 0xB9 && d <= 0xBC) || (d >= 0x99 && d <= 0x9C)) map = "u";
            p++;
            if (map) { out[n++] = map[0]; dash = false; }
            continue;
        }
        if (c >= 0x80) continue;
        if (isalnum(c)) {
            out[n++] = (char)tolower(c);
            dash = false;
        } else if (!dash && n > 0) {
            out[n++] = '-';
            dash = true;
        }
    }
    while (n > 0 && out[n - 1] == '-') n--;
    out[n] = '\0';
    if (n == 0) snprintf(out, cap, "niveau");
}

float level_surface_below(const Level *lvl, float x, float y_from)
{
    float best = FLOOR_Y;
    for (int i = 0; i < lvl->platform_count; i++) {
        const LevelPlatform *p = &lvl->platforms[i];
        if (x >= p->x && x <= p->x + p->w && p->y >= y_from && p->y < best) best = p->y;
    }
    return best;
}
