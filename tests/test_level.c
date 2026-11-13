/* Tests du format de niveau : lecture, ecriture, aller-retour, erreurs,
 * et validation de tous les niveaux de la campagne (levels/<nom>.lvl). */
#include "level.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "%s:%d: echec : %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

static const char *SAMPLE =
    "# commentaire\n"
    "format 1\n"
    "name \"Le \\\"grand\\\" pont\"\n"
    "hue 230\n"
    "reinforcements 20\n"
    "start 1 300 500\n"
    "start 2 500 340\n"
    "platform 300 340 200\r\n"
    "ladder 385 340 500\n"
    "ball 150 150 3 right   # en fin de ligne\n"
    "ball 650 150 2 left\n"
    "future_key 1 2 3\n";

static void test_parse(void)
{
    Level l;
    char err[128];
    CHECK(level_parse(&l, SAMPLE, strlen(SAMPLE), err, sizeof err));
    CHECK(strcmp(l.name, "Le \"grand\" pont") == 0);
    CHECK(l.hue == 230);
    CHECK(fabsf(l.reinforce - 20) < 0.001f);
    CHECK(l.start[1].x == 500 && l.start[1].y == 340);
    CHECK(l.platform_count == 1 && l.platforms[0].w == 200);
    CHECK(l.ladder_count == 1 && l.ladders[0].bottom == 500);
    CHECK(l.ball_count == 2);
    CHECK(l.balls[0].size == 2 && l.balls[0].dir == 1);
    CHECK(l.balls[1].size == 1 && l.balls[1].dir == -1);
}

static void test_roundtrip(void)
{
    Level a, b;
    char err[128];
    CHECK(level_parse(&a, SAMPLE, strlen(SAMPLE), err, sizeof err));
    char buf[4096];
    size_t n = level_serialize(&a, buf, sizeof buf);
    CHECK(n > 0 && n < sizeof buf);
    CHECK(level_serialize(&a, NULL, 0) == n);
    CHECK(level_parse(&b, buf, n, err, sizeof err));
    CHECK(memcmp(&a, &b, sizeof a) == 0);

    /* valeurs speciales des renforts */
    a.reinforce = REINFORCE_OFF;
    n = level_serialize(&a, buf, sizeof buf);
    CHECK(strstr(buf, "reinforcements off") != NULL);
    CHECK(level_parse(&b, buf, n, err, sizeof err) && b.reinforce == REINFORCE_OFF);
}

static void expect_error(const char *text, const char *needle)
{
    Level l;
    char err[160] = "";
    bool ok = level_parse(&l, text, strlen(text), err, sizeof err);
    CHECK(!ok);
    if (!ok && needle && !strstr(err, needle)) {
        fprintf(stderr, "message inattendu : '%s' (attendu : '%s')\n", err, needle);
        g_failures++;
    }
}

static void test_errors(void)
{
    expect_error("name \"x\"\n", "format");
    expect_error("format 99\n", "trop recente");
    expect_error("format 1\nball 1 2 9 left\n", "ligne 2");
    expect_error("format 1\nball 1 2 1 haut\n", "direction");
    expect_error("format 1\nplatform 1 2\n", "platform");
    expect_error("format 1\nname \"pas ferme\n", "guillemet");
    expect_error("format 1\nreinforcements 1\n", "renforts");
}

static void test_clamp_and_validate(void)
{
    Level l;
    level_init(&l);
    char msg[160];
    CHECK(level_validate(&l, msg, sizeof msg) > 0);   /* aucune boule */
    l.balls[l.ball_count++] = (LevelBall){ -50, 9999, 7, 0 };
    level_clamp(&l);
    CHECK(l.balls[0].size == 3 && l.balls[0].dir == 1);
    CHECK(l.balls[0].x >= 33 && l.balls[0].y <= 500 - 33);
}

static void test_slug(void)
{
    char s[64];
    level_slugify("Le Belvédère !", s, sizeof s);
    CHECK(strcmp(s, "le-belvedere") == 0);
    level_slugify("  ***  ", s, sizeof s);
    CHECK(strcmp(s, "niveau") == 0);
    level_slugify("Ça glisse à Noël", s, sizeof s);
    CHECK(strcmp(s, "ca-glisse-a-noel") == 0);
}

static void test_campaign(void)
{
    DIR *d = opendir(BDG_LEVELS_DIR);
    CHECK(d != NULL);
    if (!d) return;
    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        size_t n = strlen(ent->d_name);
        if (n < 5 || strcmp(ent->d_name + n - 4, ".lvl") != 0) continue;
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", BDG_LEVELS_DIR, ent->d_name);
        FILE *f = fopen(path, "rb");
        CHECK(f != NULL);
        if (!f) continue;
        char buf[16384];
        size_t len = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[len] = '\0';
        Level l;
        char err[160];
        if (!level_parse(&l, buf, len, err, sizeof err)) {
            fprintf(stderr, "%s : %s\n", ent->d_name, err);
            g_failures++;
            continue;
        }
        char msg[160];
        if (level_validate(&l, msg, sizeof msg)) {
            fprintf(stderr, "%s : %s\n", ent->d_name, msg);
            g_failures++;
        }
        count++;
    }
    closedir(d);
    CHECK(count > 0);
    printf("campagne : %d niveaux valides\n", count);
}

int main(void)
{
    test_parse();
    test_roundtrip();
    test_errors();
    test_clamp_and_validate();
    test_slug();
    test_campaign();
    if (g_failures) {
        fprintf(stderr, "%d echec(s)\n", g_failures);
        return 1;
    }
    printf("tous les tests passent\n");
    return 0;
}
