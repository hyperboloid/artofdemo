/* SDL2-backed reimplementation of gpu.c. Maintains an indexed framebuffer
 * + 16-entry CLUT, matching the PIC24FJ256DA206 GFX peripheral behavior the
 * original gpu.c programs. The host main loop reads gpu_get_front() each
 * frame to blit to the SDL texture. */

#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pic_compat.h"
#include "gpu.h"
#include "alu.h"

volatile struct GFXConfig gfx;
volatile uint32_t frames = 0;

__eds__ uint8_t GFXDisplayBuffer[GFX_MAX_BUFFER_SIZE];

static uint16_t clut[256] = {0};
static uint8_t  *current_draw_fb = GFXDisplayBuffer;
static uint8_t  *current_disp_fb = GFXDisplayBuffer;

/* Character rendering state, mimicking the PIC GPU's char unit. */
static uint16_t chr_fg = 0xFFFF;
static uint16_t chr_bg = 0x0000;
static uint16_t chr_x  = 0;
static uint16_t chr_y  = 0;

/* Current rectangle-copy color. */
static uint8_t  rcc_cur_color = 0;

/* Snake the pixel index into the framebuffer respecting bpp packing. */
static void fb_set_pixel(uint8_t *fb, uint32_t x, uint32_t y, uint8_t color)
{
    if (x >= gfx.hres || y >= gfx.vres) return;
    uint32_t bpp = gfx.bpp;
    uint32_t bit_index = (y * gfx.hres + x) * bpp;
    uint32_t byte_index = bit_index >> 3;
    uint32_t bit_off    = bit_index & 7;
    uint8_t mask = (uint8_t)((1u << bpp) - 1u);
    uint8_t v = (uint8_t)(color & mask);
    fb[byte_index] = (uint8_t)((fb[byte_index] & ~(mask << bit_off)) | (v << bit_off));
}

void gpu_init(void)
{
    memset(GFXDisplayBuffer, 0, sizeof(GFXDisplayBuffer));
}

uint8_t gpu_set_res(resolution res, framebuffers fb, colordepth bpp)
{
    switch (res) {
        case RES_320x240: gfx.hres = 320; gfx.vres = 240; break;
        case RES_80x480:  gfx.hres = 80;  gfx.vres = 480; break;
        case RES_160x480: gfx.hres = 160; gfx.vres = 480; break;
        case RES_320x480: gfx.hres = 320; gfx.vres = 480; break;
        case RES_480x480: gfx.hres = 480; gfx.vres = 480; break;
        case RES_640x480: gfx.hres = 640; gfx.vres = 480; break;
    }
    gfx.frame_buffers = fb;
    gfx.bpp = bpp;
    gfx.hscale = gfx.vres / gfx.hres;

    if (fb == DOUBLEBUFFERED) {
        gfx.buffer_size = 2 * ((gfx.hres * gfx.vres) / (8 / gfx.bpp));
        gfx.fb_offset = gfx.buffer_size / 2;
    } else {
        gfx.buffer_size = ((gfx.hres * gfx.vres) / (8 / gfx.bpp));
        gfx.fb_offset = 0;
    }
    if (gfx.buffer_size > GFX_MAX_BUFFER_SIZE) return 0;

    current_draw_fb = GFXDisplayBuffer;
    current_disp_fb = GFXDisplayBuffer;
    return 1;
}

void gpu_config(void) { /* no hardware to configure on host */ }
void gpu_set_fb(__eds__ uint8_t *buf) { current_disp_fb = buf; }

void gpu_flip_fb(void)
{
    static uint8_t fb_index = 1;
    if (gfx.frame_buffers != DOUBLEBUFFERED) return;
    if (fb_index) {
        current_disp_fb = &GFXDisplayBuffer[0];
        current_draw_fb = &GFXDisplayBuffer[gfx.fb_offset];
    } else {
        current_disp_fb = &GFXDisplayBuffer[gfx.fb_offset];
        current_draw_fb = &GFXDisplayBuffer[0];
    }
    fb_index = !fb_index;
}

void gpu_clear_fb(void)
{
    memset(current_draw_fb, 0, (gfx.hres * gfx.vres) / (8 / gfx.bpp));
}

void gpu_clear_all_fb(void)
{
    memset(GFXDisplayBuffer, 0, sizeof(GFXDisplayBuffer));
}

void gpu_draw_border(uint16_t c)
{
    rcc_color(c);
    rcc_rec(0, 0, 1, gfx.vres);
    rcc_rec(gfx.hres - 1, 0, 1, gfx.vres);
    rcc_rec(0, 0, gfx.hres, gfx.hscale);
    rcc_rec(0, gfx.vres - gfx.hscale, gfx.hres - 1, gfx.hscale);
}

void gpu_config_clut(void) { /* CLUT is always "on" in software */ }

void gpu_clut_set(uint8_t index, uint16_t color)
{
    clut[index] = color;
}

void gpu_config_chr(void)
{
    chr_fg = 0xFFFF;
    chr_bg = 0x0000;
}

/* TODO: decode the PIC font table (variable-width glyphs with a header).
 * For now, gpu_chr_print is a no-op so scenes that don't use text render
 * correctly. Scenes that depend on text will need this filled in. */
void gpu_chr_print(char *c, uint16_t x, uint16_t y, uint8_t transparent)
{
    (void)c; (void)x; (void)y; (void)transparent;
    (void)chr_x; (void)chr_y; (void)chr_fg; (void)chr_bg;
}

void gpu_chr_fg_color(unsigned int color) { chr_fg = (uint16_t)color; }
void gpu_chr_bg_color(unsigned int color) { chr_bg = (uint16_t)color; }

void rcc_set_fb_dest(__eds__ uint8_t *buf) { current_draw_fb = buf; }
void rcc_color(unsigned int color)         { rcc_cur_color = (uint8_t)color; }

void rcc_rec(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    for (uint32_t yy = y; yy < (uint32_t)y + h && yy < gfx.vres; yy++) {
        for (uint32_t xx = x; xx < (uint32_t)x + w && xx < gfx.hres; xx++) {
            fb_set_pixel(current_draw_fb, xx, yy, rcc_cur_color);
        }
    }
}

void rcc_pixel(unsigned long ax, unsigned long ay)
{
    fb_set_pixel(current_draw_fb, (uint32_t)ax, (uint32_t)ay, rcc_cur_color);
}

void rcc_line(float x1, float y1, float x2, float y2, uint8_t color)
{
    unsigned int i;
    double hl = fabs(x2 - x1), vl = fabs(y2 - y1);
    double length = (hl > vl) ? hl : vl;
    if (length < 1) return;
    float dx = (x2 - x1) / (float)length;
    float dy = (y2 - y1) / (float)length;
    for (i = 0; i < (unsigned int)length; i++) {
        unsigned long x = (unsigned long)(x1 += dx);
        unsigned long y = (unsigned long)(y1 += dy);
        if (x < gfx.hres && y < gfx.vres) {
            rcc_color(color);
            rcc_pixel(x, y);
        }
    }
}

/* Host-facing accessors used by main_sdl.c */
const uint8_t *gpu_get_front_fb(void) { return current_disp_fb; }
const uint16_t *gpu_get_clut(void)    { return clut; }

/* Direct access to the current draw framebuffer for full-frame effects.
 * Only meaningful at 8bpp (1 byte per pixel). The size is hres*vres bytes. */
uint8_t *gpu_get_draw_fb(void) { return current_draw_fb; }
