/* SDL2 audio backend. Plays the Sauron 8-bar minor loop using the same
 * synthesis algorithm as the PIC Timer1 ISR in
 * L1Demo_Getting_Started.X/audio.c:
 *   - Each channel has a phase accumulator wrapping at 0x803f.
 *   - Each ISR fire adds (song[idx] - 1) to the accumulator.
 *   - Output sample = sinetable[ (accumulator >> 6) & 0x1FF ].
 *   - The song index advances every "audio tick" (0x7A1 ISR fires).
 *
 * The PIC runs the ISR at FCY/PR1 = 16MHz/256 = 62500 Hz. We run at
 * whatever rate SDL gives us and scale the phase-increment + tick-duration
 * so the audible pitch and tempo match. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL.h>
#include "audio.h"            /* declares sinetable[] */
#include "sauron_music.h"

/* Globals exported via audio.h */
volatile uint32_t time_sec    = 0;
volatile uint32_t time_subsec = 0;
volatile uint8_t  audio_mode  = 0;
volatile unsigned short ch1_val = 0;
volatile unsigned short ch2_val = 0;
volatile unsigned short ch3_val = 0;

static SDL_AudioDeviceID g_dev = 0;

#define PIC_RATE        62500.0     /* Timer1 ISR fires per second on PIC */
#define PIC_TICK_FIRES  1953.0      /* 0x7A1 — ISR fires per song-index advance */

typedef struct {
    float phase;       /* accumulator, wraps at 0x8040 */
    float inc;         /* phase delta per host sample */
} chan_state;

static chan_state ch_melody, ch_mid, ch_bass;

static int    song_idx              = 0;
static double tick_progress         = 0.0;
static double host_samples_per_tick = 0.0;
static double inc_scale             = 0.0;

static float scale_inc(uint16_t v)
{
    if (v <= 1) return 0.0f;
    return (float)((double)(v - 1) * inc_scale);
}

static void load_note(int idx)
{
    ch_melody.inc = scale_inc(sauron_song_melody[idx]);
    ch_mid.inc    = scale_inc(sauron_song_mid[idx]);
    ch_bass.inc   = scale_inc(sauron_song_bass[idx]);
}

static float step_chan(chan_state *c)
{
    c->phase += c->inc;
    if (c->phase >= 0x8040) c->phase -= 0x8040;
    int tbl = ((int)c->phase >> 6) & 0x1FF;
    /* sinetable values span ~0..255 centered near 0x7f. Re-center to [-1, 1]. */
    return ((float)sinetable[tbl] - 127.5f) / 127.5f;
}

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    int16_t *out = (int16_t *)stream;
    int samples = len / (int)sizeof(int16_t);

    for (int i = 0; i < samples; i++) {
        tick_progress += 1.0;
        while (tick_progress >= host_samples_per_tick) {
            tick_progress -= host_samples_per_tick;
            song_idx++;
            if (song_idx >= SAURON_LOOP_TICKS) song_idx = 0;
            load_note(song_idx);
        }

        float m = step_chan(&ch_melody);
        float d = step_chan(&ch_mid);
        float b = step_chan(&ch_bass);

        /* mix: bass dominant for an epic low-end, mid drone behind, melody on top */
        float s = 0.55f * b + 0.20f * d + 0.25f * m;
        if (s >  1.0f) s =  1.0f;
        if (s < -1.0f) s = -1.0f;
        out[i] = (int16_t)(s * 16000.0f);   /* leave headroom */
    }
}

void audio_init(void)
{
    sauron_music_build();

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL audio init: %s\n", SDL_GetError());
        return;
    }

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq     = 44100;
    want.format   = AUDIO_S16SYS;
    want.channels = 1;
    want.samples  = 1024;
    want.callback = audio_callback;

    g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (g_dev == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return;
    }

    double sr = (double)have.freq;
    inc_scale             = PIC_RATE / sr;
    host_samples_per_tick = PIC_TICK_FIRES * (sr / PIC_RATE);

    song_idx = 0;
    tick_progress = 0.0;
    ch_melody.phase = ch_mid.phase = ch_bass.phase = 0.0f;
    load_note(song_idx);

    SDL_PauseAudioDevice(g_dev, 0);
}

void audio_isr(void) { /* unused on host */ }

void audio_shutdown(void)
{
    if (g_dev) {
        SDL_PauseAudioDevice(g_dev, 1);
        SDL_CloseAudioDevice(g_dev);
        g_dev = 0;
    }
}
