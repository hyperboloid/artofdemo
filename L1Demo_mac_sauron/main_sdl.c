/* macOS SDL2 host for Eye-of-Sauron port to the L1 demoboard.
 *
 * Target hardware budget (PIC24FJ256DA206 GFX peripheral):
 *   - 160x480, single-buffered, 8bpp -> exactly 76,800 bytes (the max)
 *   - 256 CLUT entries available in 8bpp mode
 *   - portrait aspect fits the vertical slit-pupil eye naturally
 *
 * The fire algorithm runs on the GPU's draw framebuffer directly (1 byte
 * per pixel at 8bpp), reachable through gpu_get_draw_fb(). */

#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpu.h"
#include "color.h"

extern const uint8_t  *gpu_get_front_fb(void);
extern const uint16_t *gpu_get_clut(void);

/* Sauron scene API (sauron_scene.c) */
void sauron_init(void);
void sauron_step(void);
void sauron_shutdown(void);

/* Audio API (audio_stub.c) */
void audio_init(void);
void audio_shutdown(void);

/* The PIC GFX peripheral outputs to a landscape VGA-style display. The
 * framebuffer stores `hres` horizontal samples × `vres` vertical lines,
 * but the display device stretches each sample horizontally by hscale
 * (= vres/hres), so 160x480 fb -> 480x480 displayed. The window sizing
 * mirrors that so the eye looks proportionally correct. */
#define WINDOW_SCALE 2

static uint32_t rgb565_to_argb8888(uint16_t c)
{
    uint8_t r = (uint8_t)((c >> 11) & 0x1F);
    uint8_t g = (uint8_t)((c >> 5)  & 0x3F);
    uint8_t b = (uint8_t)( c        & 0x1F);
    uint8_t R = (uint8_t)((r << 3) | (r >> 2));
    uint8_t G = (uint8_t)((g << 2) | (g >> 4));
    uint8_t B = (uint8_t)((b << 3) | (b >> 2));
    return (0xFFu << 24) | (R << 16) | (G << 8) | B;
}

static void blit_indexed_to_pixels(uint32_t *pixels, int pitch_px)
{
    const uint8_t  *fb   = gpu_get_front_fb();
    const uint16_t *clut = gpu_get_clut();
    uint32_t hres = gfx.hres, vres = gfx.vres, bpp = gfx.bpp;
    uint8_t mask = (uint8_t)((1u << bpp) - 1u);

    uint32_t palette[16];
    for (int i = 0; i < 16; i++) palette[i] = rgb565_to_argb8888(clut[i]);

    for (uint32_t y = 0; y < vres; y++) {
        for (uint32_t x = 0; x < hres; x++) {
            uint32_t bit_index = (y * hres + x) * bpp;
            uint32_t byte_index = bit_index >> 3;
            uint32_t bit_off    = bit_index & 7;
            uint8_t idx = (uint8_t)((fb[byte_index] >> bit_off) & mask);
            pixels[y * pitch_px + x] = palette[idx];
        }
    }
}

/* Sauron's original 5-stop fire palette, but expanded across 256 CLUT
 * entries. Same anchors as ta-demo-sauron's main(): black -> dark red ->
 * red -> orange -> yellow -> white. The PIC GFX CLUT takes 565, so the
 * 0..63 VGA component range is shifted to 0..255 then packed via rgb_2_565. */
static void shade_pal(int s, int e, int r1, int g1, int b1, int r2, int g2, int b2)
{
    for (int i = 0; i <= e - s; i++) {
        float k = (float)i / (float)(e - s);
        int r = (int)(r1 + (r2 - r1) * k);
        int g = (int)(g1 + (g2 - g1) * k);
        int b = (int)(b1 + (b2 - b1) * k);
        /* shade_pal anchors are 0..63 (VGA palette), expand to 0..255. */
        uint8_t R = (uint8_t)(r * 255 / 63);
        uint8_t G = (uint8_t)(g * 255 / 63);
        uint8_t B = (uint8_t)(b * 255 / 63);
        gpu_clut_set((uint8_t)(s + i), rgb_2_565(R, G, B));
    }
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    gpu_init();
    /* 160x480 DOUBLEBUFFERED BPP_4 = 76800 bytes (fits exactly).
     * 16-color indexed mode — flicker-free thanks to double buffering. */
    if (!gpu_set_res(RES_160x480, DOUBLEBUFFERED, BPP_4)) {
        fprintf(stderr, "gpu_set_res rejected the configuration\n");
        return 1;
    }
    gpu_config_clut();
    gpu_clear_all_fb();

    /* Sauron's fire palette compressed to 16 entries. Same 5-stop curve
     * (black -> dark red -> red -> orange -> yellow -> white) but with
     * fewer steps. Index N maps to a heat value of N*16..N*16+15. */
    shade_pal(  0,  1,   0,  0,  0,  24,  0,  0);
    shade_pal(  2,  5,  24,  0,  0,  63, 20,  0);
    shade_pal(  6, 10,  63, 20,  0,  63, 50,  0);
    shade_pal( 11, 13,  63, 50,  0,  63, 63, 20);
    shade_pal( 14, 15,  63, 63, 20,  63, 63, 63);

    sauron_init();
    audio_init();

    int win_w = (int)(gfx.hres * gfx.hscale) * WINDOW_SCALE;
    int win_h = (int)gfx.vres * WINDOW_SCALE;
    SDL_Window *win = SDL_CreateWindow(
        "L1 Demoboard — Eye of Sauron",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h, SDL_WINDOW_SHOWN);
    if (!win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return 1; }

    SDL_Texture *tex = SDL_CreateTexture(
        ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        (int)gfx.hres, (int)gfx.vres);
    if (!tex) { fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError()); return 1; }

    /* Optional headless mode: if SAURON_DUMP=path is set, render N frames
     * to settle the fire, then dump the framebuffer as PPM and exit. */
    const char *dump_path = getenv("SAURON_DUMP");
    int dump_frames = 200;

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }

        sauron_step();
        gpu_flip_fb();   /* swap front/back framebuffers (no-op if singlebuffered) */
        frames++;

        void *pixels = NULL;
        int pitch = 0;
        if (SDL_LockTexture(tex, NULL, &pixels, &pitch) == 0) {
            blit_indexed_to_pixels((uint32_t *)pixels, pitch / 4);
            if (dump_path && (int)frames == dump_frames) {
                /* Dump at displayed dimensions so the image isn't squished
                 * (each fb sample replicated hscale times horizontally). */
                FILE *f = fopen(dump_path, "wb");
                if (f) {
                    uint32_t out_w = gfx.hres * gfx.hscale;
                    fprintf(f, "P6\n%u %u\n255\n", out_w, gfx.vres);
                    uint32_t *p = (uint32_t *)pixels;
                    int pitch_px = pitch / 4;
                    for (uint32_t y = 0; y < gfx.vres; y++) {
                        for (uint32_t x = 0; x < gfx.hres; x++) {
                            uint32_t c = p[y * pitch_px + x];
                            uint8_t rgb[3] = {
                                (uint8_t)((c >> 16) & 0xFF),
                                (uint8_t)((c >> 8)  & 0xFF),
                                (uint8_t)( c        & 0xFF),
                            };
                            for (uint32_t s = 0; s < gfx.hscale; s++)
                                fwrite(rgb, 1, 3, f);
                        }
                    }
                    fclose(f);
                }
                SDL_UnlockTexture(tex);
                running = 0;
                continue;
            }
            SDL_UnlockTexture(tex);
        }
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }

    audio_shutdown();
    sauron_shutdown();
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
