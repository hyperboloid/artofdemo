/*
 * L1 Demo macOS Viewer
 * Renders the scene.c demoscene graphics using SDL2 on macOS.
 * This emulates the PIC24F GPU API (rcc_color, rcc_rec, etc.)
 * and CLUT palette system so the scenes display natively.
 */

#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// GPU emulation types (mirrors gpu.h)
// ---------------------------------------------------------------------------

typedef enum {
    RES_320x240,
    RES_80x480,
    RES_160x480,
    RES_320x480,
    RES_480x480,
    RES_640x480
} resolution;

typedef enum {
    SINGLEBUFFERED = 1,
    DOUBLEBUFFERED = 2
} framebuffers;

typedef enum {
    BPP_1 = 1,
    BPP_2 = 2,
    BPP_4 = 4,
    BPP_8 = 8,
    BPP_16 = 16
} colordepth;

struct GFXConfig {
    uint16_t frame_buffers;
    uint16_t clock_div;
    uint32_t hres;
    uint32_t vres;
    uint32_t bpp;
    uint32_t hfp, hpw, hbp;
    uint32_t vfp, vpw, vbp;
    uint32_t hscale;
    uint32_t buffer_size;
    uint32_t fb_offset;
};

// ---------------------------------------------------------------------------
// Scene types (mirrors scene.h)
// ---------------------------------------------------------------------------

#define TOTAL_NUM_SCENES 2

struct Scene {
    uint16_t scene_id;
    uint32_t start_time;
    uint32_t stop_time;
    uint8_t  music_track_id;
    resolution  res;
    framebuffers fb_num;
    colordepth color_depth;
};

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

static struct GFXConfig gfx;
static struct Scene scene[TOTAL_NUM_SCENES];
static void (*scene_func)(void);

static uint32_t frames = 0;
static volatile uint32_t time_sec = 0;

// CLUT: 16 entries stored as RGB888 for easy SDL rendering
static uint8_t clut_r[256], clut_g[256], clut_b[256];

// Current RCC drawing color (CLUT index)
static unsigned int current_color = 0;

// SDL globals
static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture  *texture  = NULL;

// Pixel buffer (native resolution)
static uint32_t *pixels = NULL;
static uint32_t  pixel_w = 0, pixel_h = 0;

// Window scale factor
#define WINDOW_SCALE 2

// ---------------------------------------------------------------------------
// Color helper (mirrors color.c)
// ---------------------------------------------------------------------------

static uint16_t rgb_2_565(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t red   = r >> 3;
    uint8_t green = g >> 2;
    uint8_t blue  = b >> 3;
    return (uint16_t)((red << 11) | (green << 5) | blue);
}

// Store CLUT entry as full RGB for rendering
static void gpu_clut_set(uint8_t index, uint16_t color565)
{
    // Convert RGB565 back to RGB888 for display
    uint8_t r5 = (color565 >> 11) & 0x1F;
    uint8_t g6 = (color565 >> 5)  & 0x3F;
    uint8_t b5 =  color565        & 0x1F;
    clut_r[index] = (r5 << 3) | (r5 >> 2);
    clut_g[index] = (g6 << 2) | (g6 >> 4);
    clut_b[index] = (b5 << 3) | (b5 >> 2);
}

// ---------------------------------------------------------------------------
// GPU emulation
// ---------------------------------------------------------------------------

static uint8_t gpu_set_res(resolution res, framebuffers fb, colordepth bpp)
{
    switch (res) {
        case RES_320x240:
            gfx.hres = 320; gfx.vres = 240; break;
        case RES_80x480:
            gfx.hres = 80;  gfx.vres = 480; break;
        case RES_160x480:
            gfx.hres = 160; gfx.vres = 480; break;
        case RES_320x480:
            gfx.hres = 320; gfx.vres = 480; break;
        case RES_480x480:
            gfx.hres = 480; gfx.vres = 480; break;
        case RES_640x480:
            gfx.hres = 640; gfx.vres = 480; break;
    }
    gfx.frame_buffers = fb;
    gfx.bpp = bpp;
    gfx.hscale = gfx.vres / gfx.hres;

    // Reallocate pixel buffer if resolution changed
    if (pixel_w != gfx.hres || pixel_h != gfx.vres) {
        pixel_w = gfx.hres;
        pixel_h = gfx.vres;
        free(pixels);
        pixels = calloc(pixel_w * pixel_h, sizeof(uint32_t));

        // Recreate texture
        if (texture) SDL_DestroyTexture(texture);
        texture = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            pixel_w, pixel_h);
    }
    return 1;
}

static void gpu_clear_fb(void)
{
    memset(pixels, 0, pixel_w * pixel_h * sizeof(uint32_t));
}

static void gpu_clear_all_fb(void)
{
    gpu_clear_fb();
}

static void rcc_color(unsigned int color)
{
    current_color = color;
}

static void rcc_rec(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint8_t r = clut_r[current_color];
    uint8_t g = clut_g[current_color];
    uint8_t b = clut_b[current_color];
    uint32_t argb = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;

    for (uint16_t dy = y; dy < y + h && dy < pixel_h; dy++) {
        for (uint16_t dx = x; dx < x + w && dx < pixel_w; dx++) {
            pixels[dy * pixel_w + dx] = argb;
        }
    }
}

static void rcc_pixel(unsigned long ax, unsigned long ay)
{
    if (ax < pixel_w && ay < pixel_h) {
        uint8_t r = clut_r[current_color];
        uint8_t g = clut_g[current_color];
        uint8_t b = clut_b[current_color];
        uint32_t argb = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        // Draw with hscale like original
        for (uint32_t sy = 0; sy < gfx.hscale && (ay + sy) < pixel_h; sy++) {
            pixels[(ay + sy) * pixel_w + ax] = argb;
        }
    }
}

__attribute__((unused))
static void rcc_line(float x1, float y1, float x2, float y2, uint8_t color)
{
    double hl = fabs(x2 - x1), vl = fabs(y2 - y1);
    double length = (hl > vl) ? hl : vl;
    float deltax = (x2 - x1) / (float)length;
    float deltay = (y2 - y1) / (float)length;
    for (int i = 0; i < (int)length; i++) {
        unsigned long x = (int)(x1 += deltax);
        unsigned long y = (int)(y1 += deltay);
        if (x < gfx.hres && y < gfx.vres) {
            rcc_color(color);
            rcc_pixel(x, y);
        }
    }
}

static void gpu_draw_border(uint16_t c)
{
    rcc_color(c);
    rcc_rec(0, 0, 1, gfx.vres);
    rcc_rec(gfx.hres - 1, 0, 1, gfx.vres);
    rcc_rec(0, 0, gfx.hres, gfx.hscale);
    rcc_rec(0, gfx.vres - gfx.hscale, gfx.hres - 1, gfx.hscale);
}

// ---------------------------------------------------------------------------
// Scene functions (mirrors scene.c exactly)
// ---------------------------------------------------------------------------

static void scene_zero(void)
{
    rcc_color(1);
    rcc_rec(1, 0, gfx.hres / 7, gfx.vres - 1);

    rcc_color(2);
    rcc_rec(1 + ((gfx.hres / 7) * 1), 0, gfx.hres / 7, gfx.vres - 1);

    rcc_color(3);
    rcc_rec(1 + ((gfx.hres / 7) * 2), 0, gfx.hres / 7, gfx.vres - 1);

    rcc_color(4);
    rcc_rec(1 + ((gfx.hres / 7) * 3), 0, gfx.hres / 7, gfx.vres - 1);

    rcc_color(5);
    rcc_rec(1 + ((gfx.hres / 7) * 4), 0, gfx.hres / 7, gfx.vres - 1);

    rcc_color(6);
    rcc_rec(1 + ((gfx.hres / 7) * 5), 0, gfx.hres / 7, gfx.vres - 1);

    rcc_color(7);
    rcc_rec(1 + ((gfx.hres / 7) * 6), 0, gfx.hres / 7, gfx.vres - 1);
}

static void scene_one(void)
{
    gpu_clear_fb();
    rcc_color(1);
    rcc_rec(0, 0, gfx.hres - 1, gfx.vres - 1);
}

// ---------------------------------------------------------------------------
// Scene manager (mirrors scene.c)
// ---------------------------------------------------------------------------

static void scene_init(void)
{
    scene[0].scene_id = 0;
    scene[0].start_time = 0;
    scene[0].stop_time = 10;
    scene[0].music_track_id = 0;
    scene[0].res = RES_160x480;
    scene[0].fb_num = SINGLEBUFFERED;
    scene[0].color_depth = BPP_4;

    scene[1].scene_id = 1;
    scene[1].start_time = 10;
    scene[1].stop_time = 2000;
    scene[1].music_track_id = 0;
    scene[1].res = RES_160x480;
    scene[1].fb_num = DOUBLEBUFFERED;
    scene[1].color_depth = BPP_4;

    scene_func = &scene_zero;
    gpu_set_res(scene[0].res, scene[0].fb_num, scene[0].color_depth);
}

static void scene_render_frame(void)
{
    static uint16_t scene_index = 0;

    if (time_sec >= scene[scene_index].stop_time) {
        if (scene_index < TOTAL_NUM_SCENES) {
            scene_index++;
        }

        switch (scene_index) {
            case 0:  scene_func = &scene_zero; break;
            case 1:  scene_func = &scene_one;  break;
            default: scene_func = &scene_zero; break;
        }

        gpu_set_res(scene[scene_index].res, scene[scene_index].fb_num, scene[scene_index].color_depth);
    }

    scene_func();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // Initial resolution for window sizing (160x480 scaled)
    int win_w = 160 * WINDOW_SCALE;
    int win_h = 480 * WINDOW_SCALE;

    window = SDL_CreateWindow(
        "L1 Demo Viewer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Load CLUT (same palette as main.c)
    memset(clut_r, 0, sizeof(clut_r));
    memset(clut_g, 0, sizeof(clut_g));
    memset(clut_b, 0, sizeof(clut_b));

    gpu_clut_set(0,  rgb_2_565(0, 0, 0));            // Black
    gpu_clut_set(1,  rgb_2_565(180, 180, 180));       // Light Grey
    gpu_clut_set(2,  rgb_2_565(180, 180, 16));        // Yellow
    gpu_clut_set(3,  rgb_2_565(16, 180, 180));        // Cyan
    gpu_clut_set(4,  rgb_2_565(16, 180, 16));         // Green
    gpu_clut_set(5,  rgb_2_565(180, 16, 180));        // Magenta
    gpu_clut_set(6,  rgb_2_565(180, 16, 16));         // Red
    gpu_clut_set(7,  rgb_2_565(16, 16, 180));         // Blue
    gpu_clut_set(8,  rgb_2_565(235, 235, 235));       // White
    gpu_clut_set(9,  rgb_2_565(16, 16, 16));          // Light Black
    gpu_clut_set(10, rgb_2_565(0, 0, 0));             // Black
    gpu_clut_set(11, rgb_2_565(0, 0, 0));             // Black
    gpu_clut_set(12, rgb_2_565(0, 0, 0));             // Black
    gpu_clut_set(13, rgb_2_565(0, 0, 0));             // Black
    gpu_clut_set(14, rgb_2_565(0, 0, 0));             // Black
    gpu_clut_set(15, rgb_2_565(0, 0, 0));             // Black

    // Initialize scene manager
    scene_init();
    gpu_clear_all_fb();

    // Timing
    Uint32 start_ticks = SDL_GetTicks();
    bool running = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE ||
                    event.key.keysym.sym == SDLK_q) {
                    running = false;
                }
            }
        }

        // Update time (seconds since start)
        time_sec = (SDL_GetTicks() - start_ticks) / 1000;

        // Render scene
        scene_render_frame();
        gpu_draw_border(0);

        // Upload pixel buffer to texture
        SDL_UpdateTexture(texture, NULL, pixels, pixel_w * sizeof(uint32_t));

        // Render texture scaled to window
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        frames++;
    }

    // Cleanup
    free(pixels);
    if (texture)  SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window)   SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
