/* macOS SDL2 host for the LayerOne 2017 demoboard project.
 * Reuses scene.c/sprites.c/alu.c/color.c/fonts.h from the .X project. */

#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpu.h"
#include "audio.h"
#include "color.h"
#include "scene.h"
#include "sprites.h"

extern const uint8_t  *gpu_get_front_fb(void);
extern const uint16_t *gpu_get_clut(void);

/* The PIC GFX peripheral outputs to a landscape VGA-style display. The
 * framebuffer stores `hres` horizontal samples × `vres` vertical lines,
 * but the display device stretches each sample horizontally by hscale
 * (= vres/hres), so e.g. 160x480 fb -> 480x480 displayed. We mirror that
 * here by sizing the window with the hscale applied. */
#define WINDOW_SCALE 2

static uint32_t rgb565_to_argb8888(uint16_t c)
{
    uint8_t r = (uint8_t)((c >> 11) & 0x1F);
    uint8_t g = (uint8_t)((c >> 5)  & 0x3F);
    uint8_t b = (uint8_t)( c        & 0x1F);
    /* expand 5/6-bit to 8-bit */
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

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Boot the demo with the resolution scene_zero uses. */
    gpu_init();
    gpu_set_res(RES_160x480, DOUBLEBUFFERED, BPP_4);
    gpu_config_clut();
    gpu_config_chr();
    gpu_clear_all_fb();

    /* Initial palette from main.c */
    gpu_clut_set(0,  rgb_2_565(0,   0,   0));
    gpu_clut_set(1,  rgb_2_565(180, 180, 180));
    gpu_clut_set(2,  rgb_2_565(180, 180, 16));
    gpu_clut_set(3,  rgb_2_565(16,  180, 180));
    gpu_clut_set(4,  rgb_2_565(16,  180, 16));
    gpu_clut_set(5,  rgb_2_565(180, 16,  180));
    gpu_clut_set(6,  rgb_2_565(180, 16,  16));
    gpu_clut_set(7,  rgb_2_565(16,  16,  180));
    gpu_clut_set(8,  rgb_2_565(235, 235, 235));
    gpu_clut_set(9,  rgb_2_565(16,  16,  16));
    for (int i = 10; i < 16; i++) gpu_clut_set(i, rgb_2_565(0, 0, 0));

    audio_init();
    scene_init();

    int win_w = (int)(gfx.hres * gfx.hscale) * WINDOW_SCALE;
    int win_h = (int)gfx.vres * WINDOW_SCALE;
    SDL_Window *win = SDL_CreateWindow(
        "L1 Demoboard (mac)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h, SDL_WINDOW_SHOWN);
    if (!win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return 1; }

    SDL_Texture *tex = SDL_CreateTexture(
        ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        (int)gfx.hres, (int)gfx.vres);
    if (!tex) { fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError()); return 1; }

    uint32_t start_ms = SDL_GetTicks();
    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }

        /* Advance demo clock from wall time. */
        uint32_t elapsed_ms = SDL_GetTicks() - start_ms;
        time_sec = elapsed_ms / 1000;

        scene_render_frame();
        gpu_draw_border(0);
        gpu_flip_fb();
        frames++;

        void *pixels = NULL;
        int pitch = 0;
        if (SDL_LockTexture(tex, NULL, &pixels, &pitch) == 0) {
            blit_indexed_to_pixels((uint32_t *)pixels, pitch / 4);
            SDL_UnlockTexture(tex);
        }
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
