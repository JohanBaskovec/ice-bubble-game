#include "storage.h"
#include "config.h"
#include "embedded.h"

#include <SDL3/SDL.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static char g_pref[1024];
static char g_user_levels[1100];
static char g_override[1024];
static bool g_ready;
static bool g_ready_reported;

static void finish_init(void)
{
    char *pref = SDL_GetPrefPath(APP_ORG, APP_NAME);
    if (pref) {
        SDL_strlcpy(g_pref, pref, sizeof g_pref);
        SDL_free(pref);
    } else {
        SDL_Log("storage: pas de dossier de preferences (%s)", SDL_GetError());
        SDL_strlcpy(g_pref, "./", sizeof g_pref);
    }
    SDL_snprintf(g_user_levels, sizeof g_user_levels, "%slevels/", g_pref);
    SDL_CreateDirectory(g_user_levels);
    g_ready = true;
}

void storage_init(const char *levels_dir_override)
{
    if (levels_dir_override && *levels_dir_override) {
        size_t n = SDL_strlcpy(g_override, levels_dir_override, sizeof g_override - 1);
        if (n > 0 && g_override[n - 1] != '/' && g_override[n - 1] != '\\') SDL_strlcat(g_override, "/", sizeof g_override);
        SDL_Log("storage: campagne lue et ecrite dans %s", g_override);
    }
#ifdef __EMSCRIPTEN__
    /* IDBFS : on charge IndexedDB avant de toucher au dossier de preferences */
    EM_ASM({
        Module.bdgFsReady = 0;
        try { FS.mkdir('/libsdl'); } catch (e) {}
        FS.mount(IDBFS, {}, '/libsdl');
        FS.syncfs(true, function(err) {
            if (err) console.warn('IDBFS', err);
            Module.bdgFsReady = 1;
        });
    });
#else
    finish_init();
#endif
}

bool storage_ready(void)
{
#ifdef __EMSCRIPTEN__
    if (!g_ready && EM_ASM_INT({ return Module.bdgFsReady | 0; })) finish_init();
#endif
    return g_ready;
}

bool storage_poll_ready(void)
{
    if (!g_ready_reported && storage_ready()) {
        g_ready_reported = true;
        return true;
    }
    return false;
}

static void persist(void)
{
#ifdef __EMSCRIPTEN__
    EM_ASM({
        FS.syncfs(false, function(err) { if (err) console.warn('IDBFS', err); });
    });
#endif
}

const char *storage_pref_dir(void) { return g_pref; }
const char *storage_user_levels_dir(void) { return g_user_levels; }

/* ------------------------------------------------------------------------ */

static bool write_atomic(const char *path, const void *data, size_t len)
{
    char tmp[1200];
    SDL_snprintf(tmp, sizeof tmp, "%s.tmp", path);
    if (!SDL_SaveFile(tmp, data, len)) {
        SDL_Log("storage: ecriture impossible %s : %s", tmp, SDL_GetError());
        return false;
    }
    if (!SDL_RenamePath(tmp, path)) {
        /* Windows refuse parfois d'ecraser : on supprime puis on renomme */
        SDL_RemovePath(path);
        if (!SDL_RenamePath(tmp, path)) {
            SDL_Log("storage: renommage impossible %s : %s", path, SDL_GetError());
            return false;
        }
    }
    persist();
    return true;
}

char *storage_read_pref(const char *name, size_t *len)
{
    if (!storage_ready()) return NULL;
    char path[1200];
    SDL_snprintf(path, sizeof path, "%s%s", g_pref, name);
    return SDL_LoadFile(path, len);
}

bool storage_write_pref(const char *name, const void *data, size_t len)
{
    if (!storage_ready()) return false;
    char path[1200];
    SDL_snprintf(path, sizeof path, "%s%s", g_pref, name);
    return write_atomic(path, data, len);
}

/* ------------------------------------------------------------------------ */

static const char *dir_for(LevelSource src)
{
    if (src == LEVELSRC_USER) return g_user_levels;
    return g_override[0] ? g_override : NULL;     /* NULL = campagne integree */
}

static bool valid_filename(const char *file)
{
    size_t n = SDL_strlen(file);
    if (n < 5 || n >= LEVEL_FILE_MAX) return false;
    if (SDL_strcmp(file + n - 4, ".lvl") != 0) return false;
    for (const char *p = file; *p; p++)
        if (*p == '/' || *p == '\\' || *p == ':' || (p[0] == '.' && p[1] == '.')) return false;
    return true;
}

static int cmp_entries(const void *a, const void *b)
{
    return SDL_strcmp(((const LevelEntry *)a)->file, ((const LevelEntry *)b)->file);
}

static void fill_entry(LevelEntry *e, LevelSource src, const char *file, const char *text, size_t len)
{
    SDL_zerop(e);
    e->src = src;
    SDL_strlcpy(e->file, file, sizeof e->file);
    Level lvl;
    if (text && level_parse(&lvl, text, len, e->error, sizeof e->error)) {
        e->valid = true;
        SDL_strlcpy(e->name, lvl.name, sizeof e->name);
    } else {
        e->valid = false;
        SDL_strlcpy(e->name, file, sizeof e->name);
        if (!text) SDL_strlcpy(e->error, "lecture impossible", sizeof e->error);
    }
}

int storage_list(LevelSource src, LevelEntry *out, int max)
{
    int n = 0;
    const char *dir = dir_for(src);
    if (!dir) {
        for (int i = 0; i < g_campaign_level_count && n < max; i++, n++)
            fill_entry(&out[n], src, g_campaign_levels[i].name, (const char *)g_campaign_levels[i].data,
                       g_campaign_levels[i].size);
        return n;
    }
    if (!storage_ready()) return 0;
    int count = 0;
    char **files = SDL_GlobDirectory(dir, "*.lvl", 0, &count);
    if (!files) return 0;
    for (int i = 0; i < count && n < max; i++) {
        if (!valid_filename(files[i])) continue;
        char path[1200];
        SDL_snprintf(path, sizeof path, "%s%s", dir, files[i]);
        size_t len = 0;
        char *text = SDL_LoadFile(path, &len);
        fill_entry(&out[n++], src, files[i], text, len);
        SDL_free(text);
    }
    SDL_free(files);
    SDL_qsort(out, (size_t)n, sizeof *out, cmp_entries);
    return n;
}

bool storage_load(LevelSource src, const char *file, Level *out, char *err, size_t cap)
{
    const char *dir = dir_for(src);
    if (!dir) {
        for (int i = 0; i < g_campaign_level_count; i++)
            if (!SDL_strcmp(g_campaign_levels[i].name, file))
                return level_parse(out, (const char *)g_campaign_levels[i].data, g_campaign_levels[i].size, err,
                                   cap);
        SDL_snprintf(err, cap, "niveau introuvable : %s", file);
        return false;
    }
    if (!valid_filename(file)) {
        SDL_snprintf(err, cap, "nom de fichier invalide");
        return false;
    }
    char path[1200];
    SDL_snprintf(path, sizeof path, "%s%s", dir, file);
    size_t len = 0;
    char *text = SDL_LoadFile(path, &len);
    if (!text) {
        SDL_snprintf(err, cap, "lecture impossible : %s", SDL_GetError());
        return false;
    }
    bool ok = level_parse(out, text, len, err, cap);
    SDL_free(text);
    return ok;
}

bool storage_writable(LevelSource src) { return dir_for(src) != NULL && storage_ready(); }

bool storage_save(LevelSource src, const char *file, const Level *lvl, char *err, size_t cap)
{
    const char *dir = dir_for(src);
    if (!dir || !storage_ready()) {
        SDL_snprintf(err, cap, "la campagne integree est en lecture seule");
        return false;
    }
    if (!valid_filename(file)) {
        SDL_snprintf(err, cap, "nom de fichier invalide : %s", file);
        return false;
    }
    size_t need = level_serialize(lvl, NULL, 0) + 1;
    char *buf = SDL_malloc(need);
    if (!buf) {
        SDL_snprintf(err, cap, "memoire insuffisante");
        return false;
    }
    size_t len = level_serialize(lvl, buf, need);
    char path[1200];
    SDL_snprintf(path, sizeof path, "%s%s", dir, file);
    bool ok = write_atomic(path, buf, len);
    SDL_free(buf);
    if (!ok) SDL_snprintf(err, cap, "ecriture impossible : %s", SDL_GetError());
    return ok;
}

bool storage_delete(LevelSource src, const char *file)
{
    const char *dir = dir_for(src);
    if (!dir || !valid_filename(file)) return false;
    char path[1200];
    SDL_snprintf(path, sizeof path, "%s%s", dir, file);
    bool ok = SDL_RemovePath(path);
    persist();
    return ok;
}

bool storage_exists(LevelSource src, const char *file)
{
    const char *dir = dir_for(src);
    if (!dir) {
        for (int i = 0; i < g_campaign_level_count; i++)
            if (!SDL_strcmp(g_campaign_levels[i].name, file)) return true;
        return false;
    }
    char path[1200];
    SDL_snprintf(path, sizeof path, "%s%s", dir, file);
    return SDL_GetPathInfo(path, NULL);
}

void storage_unique_filename(LevelSource src, const char *level_name, char *out, size_t cap)
{
    char slug[64];
    level_slugify(level_name, slug, sizeof slug);
    SDL_snprintf(out, cap, "%s.lvl", slug);
    for (int i = 2; i < 1000 && storage_exists(src, out); i++) SDL_snprintf(out, cap, "%s-%d.lvl", slug, i);
}

bool storage_reveal_user_folder(void)
{
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_MACOS) || defined(SDL_PLATFORM_LINUX)
    char url[1200];
    SDL_snprintf(url, sizeof url, "file://%s", g_user_levels);
    return SDL_OpenURL(url);
#else
    return false;
#endif
}
