#include "audio.h"

#include <SDL3/SDL.h>
#include <math.h>

#define RATE 44100
#define MAX_VOICES 64
#define TWO_PI 6.28318530718f

typedef enum Wave { W_SINE, W_TRIANGLE, W_SQUARE, W_SAW } Wave;
typedef enum Bus { BUS_MUSIC, BUS_SFX } Bus;

typedef struct Voice {
    bool on;
    Wave wave;
    Bus bus;
    int delay;              /* echantillons avant demarrage */
    int len;                /* duree en echantillons */
    int t;
    float f0, f1;           /* frequence de depart / d'arrivee (glissando exponentiel) */
    float gain;
    float phase;
} Voice;

static SDL_AudioStream *g_stream;
static Voice g_voices[MAX_VOICES];
static float g_music_gain = 0.2f, g_music_target = 0.2f;
static float g_sfx_gain = 0.55f;
static const float MASTER = 0.5f;

/* --- sequenceur (thread audio) --- */
static const int LEAD[64] = {
    76, 0, 79, 0, 84, 0, 79, 0, 76, 0, 74, 0, 72, 0, 0, 0,
    74, 0, 79, 0, 83, 0, 79, 0, 74, 0, 72, 0, 71, 0, 0, 0,
    72, 0, 76, 0, 81, 0, 79, 0, 76, 0, 74, 0, 72, 0, 74, 0,
    77, 0, 76, 0, 74, 0, 72, 0, 74, 0, 0, 0, 67, 0, 0, 0,
};
static const int BASS[4] = { 48, 43, 45, 41 };
static int g_step;
static double g_step_clock;     /* echantillons restants avant le prochain pas */
static const double STEP_SAMPLES = RATE * 60.0 / 138.0 / 4.0;

static float mf(int note) { return 440.0f * powf(2.0f, (note - 69) / 12.0f); }

/* doit etre appele avec le flux verrouille (ou depuis le callback) */
static void voice_add(Wave w, Bus bus, float f0, float f1, float dur, float gain, float delay)
{
    int slot = -1;
    for (int i = 0; i < MAX_VOICES; i++)
        if (!g_voices[i].on) { slot = i; break; }
    if (slot < 0) {
        /* vole la voix la plus avancee */
        int best = 0;
        float bestp = -1;
        for (int i = 0; i < MAX_VOICES; i++) {
            float p = (float)g_voices[i].t / (float)(g_voices[i].len + 1);
            if (p > bestp) { bestp = p; best = i; }
        }
        slot = best;
    }
    Voice *v = &g_voices[slot];
    v->on = true;
    v->wave = w;
    v->bus = bus;
    v->delay = (int)(delay * RATE);
    v->len = (int)(dur * RATE) + 1;
    v->t = 0;
    v->f0 = f0;
    v->f1 = f1 > 0 ? f1 : f0;
    v->gain = gain;
    v->phase = 0;
}

static void play_step(int s)
{
    int bar = s >> 4, b = s & 15, root = BASS[bar];
    if (LEAD[s]) {
        voice_add(W_TRIANGLE, BUS_MUSIC, mf(LEAD[s]), 0, 0.28f, 0.22f, 0);
        voice_add(W_SINE, BUS_MUSIC, mf(LEAD[s] + 12), 0, 0.18f, 0.05f, 0);
    }
    if (b % 4 == 0) voice_add(W_SQUARE, BUS_MUSIC, mf(root), 0, 0.3f, 0.06f, 0);
    if (b % 4 == 2) voice_add(W_TRIANGLE, BUS_MUSIC, mf(root + ((b == 6 || b == 14) ? 7 : 12)), 0, 0.18f, 0.12f, 0);
    if (b % 2 == 1) voice_add(W_SQUARE, BUS_MUSIC, 6000, 0, 0.03f, 0.012f, 0);
}

static float osc(Wave w, float ph)
{
    switch (w) {
    case W_SINE: return sinf(ph * TWO_PI);
    case W_TRIANGLE: return 4.0f * fabsf(ph - 0.5f) - 1.0f;
    case W_SQUARE: return ph < 0.5f ? 1.0f : -1.0f;
    default: return 2.0f * ph - 1.0f;
    }
}

static void SDLCALL audio_cb(void *ud, SDL_AudioStream *stream, int additional, int total)
{
    (void)ud;
    (void)total;
    float buf[1024];
    int samples = additional / (int)sizeof(float);
    while (samples > 0) {
        int n = samples > 1024 ? 1024 : samples;
        for (int i = 0; i < n; i++) {
            if (g_music_gain > 0.0005f || g_music_target > 0) {
                g_step_clock -= 1.0;
                if (g_step_clock <= 0) {
                    g_step_clock += STEP_SAMPLES;
                    play_step(g_step);
                    g_step = (g_step + 1) % 64;
                }
            }
            g_music_gain += (g_music_target - g_music_gain) * 0.0005f;
            float music = 0, sfx = 0;
            for (int k = 0; k < MAX_VOICES; k++) {
                Voice *v = &g_voices[k];
                if (!v->on) continue;
                if (v->delay > 0) { v->delay--; continue; }
                float p = (float)v->t / (float)v->len;
                float f = v->f0 == v->f1 ? v->f0 : v->f0 * powf(v->f1 / v->f0, p);
                /* attaque 8 ms puis decroissance exponentielle vers -80 dB */
                float att = v->t / (0.008f * RATE);
                float env = att < 1.0f ? att : expf(-9.2f * p);
                float s = osc(v->wave, v->phase) * env * v->gain;
                v->phase += f / RATE;
                v->phase -= floorf(v->phase);
                if (v->bus == BUS_MUSIC) music += s; else sfx += s;
                if (++v->t >= v->len) v->on = false;
            }
            float out = (music * g_music_gain + sfx * g_sfx_gain) * MASTER;
            /* saturation douce */
            buf[i] = out / (1.0f + fabsf(out));
        }
        SDL_PutAudioStreamData(stream, buf, n * (int)sizeof(float));
        samples -= n;
    }
}

bool audio_init(void)
{
    SDL_AudioSpec spec = { SDL_AUDIO_F32, 1, RATE };
    g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_cb, NULL);
    if (!g_stream) {
        SDL_Log("audio: %s", SDL_GetError());
        return false;
    }
    g_step_clock = 1;
    SDL_ResumeAudioStreamDevice(g_stream);
    return true;
}

void audio_quit(void)
{
    if (g_stream) SDL_DestroyAudioStream(g_stream);
    g_stream = NULL;
}

void audio_pause_device(bool paused)
{
    if (!g_stream) return;
    if (paused) SDL_PauseAudioStreamDevice(g_stream);
    else SDL_ResumeAudioStreamDevice(g_stream);
}

void audio_set_music(bool on)
{
    if (!g_stream) {
        g_music_target = on ? 0.2f : 0;
        return;
    }
    SDL_LockAudioStream(g_stream);
    g_music_target = on ? 0.2f : 0;
    SDL_UnlockAudioStream(g_stream);
}

void audio_set_sfx_volume(float v)
{
    if (g_stream) SDL_LockAudioStream(g_stream);
    g_sfx_gain = 0.55f * (v < 0 ? 0 : (v > 1 ? 1 : v));
    if (g_stream) SDL_UnlockAudioStream(g_stream);
}

void audio_play(Sfx s, int param)
{
    if (!g_stream) return;
    SDL_LockAudioStream(g_stream);
    switch (s) {
    case SFX_SHOOT: voice_add(W_SINE, BUS_SFX, 1300, 2600, 0.12f, 0.18f, 0); break;
    case SFX_PEW: voice_add(W_SQUARE, BUS_SFX, 1800, 2400, 0.06f, 0.05f, 0); break;
    case SFX_POP:
        voice_add(W_TRIANGLE, BUS_SFX, 520.0f + (3 - param) * 170.0f, 180, 0.16f, 0.3f, 0);
        voice_add(W_SINE, BUS_SFX, 2200.0f + (3 - param) * 300.0f, 0, 0.12f, 0.09f, 0.02f);
        break;
    case SFX_POWER: {
        static const int n[4] = { 0, 4, 7, 12 };
        for (int i = 0; i < 4; i++) voice_add(W_TRIANGLE, BUS_SFX, mf(79 + n[i]), 0, 0.15f, 0.2f, i * 0.06f);
        break;
    }
    case SFX_HURT: voice_add(W_SAW, BUS_SFX, 420, 70, 0.45f, 0.18f, 0); break;
    case SFX_SHIELD: voice_add(W_SINE, BUS_SFX, 300, 900, 0.25f, 0.25f, 0); break;
    case SFX_WARN:
        voice_add(W_SQUARE, BUS_SFX, 880, 0, 0.1f, 0.07f, 0);
        voice_add(W_SQUARE, BUS_SFX, 660, 0, 0.1f, 0.07f, 0.14f);
        break;
    case SFX_CLEAR: {
        static const int n[5] = { 72, 76, 79, 84, 88 };
        for (int i = 0; i < 5; i++) voice_add(W_TRIANGLE, BUS_SFX, mf(n[i]), 0, 0.3f, 0.22f, i * 0.09f);
        break;
    }
    case SFX_OVER: {
        static const int n[4] = { 67, 63, 60, 55 };
        for (int i = 0; i < 4; i++) voice_add(W_TRIANGLE, BUS_SFX, mf(n[i]), 0, 0.4f, 0.22f, i * 0.2f);
        break;
    }
    case SFX_FREEZE: voice_add(W_SINE, BUS_SFX, 2400, 600, 0.5f, 0.12f, 0); break;
    case SFX_CLICK: voice_add(W_SINE, BUS_SFX, 900, 1400, 0.05f, 0.12f, 0); break;
    }
    SDL_UnlockAudioStream(g_stream);
}
