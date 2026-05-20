/* Eye of Sauron retargeted from 320x200 to the L1 demoboard's 160x480
 * portrait 8bpp framebuffer. Same algorithm as ta-demo-sauron/sauron.cpp,
 * but at the smaller resolution and written in C instead of C++.
 *
 * The almond is built taller (halfH=140) and slightly narrower (halfW=22)
 * to use the portrait canvas. Flame reach is scaled down to match.
 *
 * The fire heat field lives in two byte arrays (current + previous frame).
 * Each frame we Heat() then Blur_Out() into the GPU framebuffer (which is
 * already byte-per-pixel at 8bpp), then swap the two halves. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gpu.h"

#define HRES 160
#define VRES 480
#define FB_SIZE (HRES * VRES)

extern uint8_t *gpu_get_draw_fb(void);

/* Geometry — VERTICAL slit-pupil almond on a LANDSCAPE display.
 *
 * The PIC displays the 160-wide fb stretched 3x horizontally, so the
 * effective display surface is 480x480. We author in fb coords, where a
 * fb-pixel displays 3x wider than tall. To get a tall vertical eye on
 * screen we want narrow fb-x and tall fb-y, and to leave breathing room
 * on the sides so fire can radiate horizontally across the landscape.
 *
 * Target displayed eye: ~110 wide x 320 tall.
 * In fb coords: halfW_fb=18 -> 54 displayed (narrow), halfH_fb=160 (tall).
 *
 * Arc centers offset HORIZONTALLY in fb (the original sauron construction),
 * so the almond points up and down. */
static const float EYE_CX = HRES / 2.0f;       /* 80 (fb-x) */
static const float EYE_CY = VRES / 2.0f;       /* 240 (fb-y) */
static const float HALF_W = 18.0f;             /* narrow slit (fb-x) -> ~54 displayed */
static const float HALF_H = 160.0f;            /* tall almond (fb-y) */

static const float IRIS_INNER = 8.0f;
static const float IRIS_OUTER = 16.0f;

/* How far flames can travel from the eye edge (pixels). */
static const int FLAME_REACH = 16;

static uint8_t *eye_mask = NULL;     /* baked-in body brightness */
static int8_t  *in_x = NULL;         /* per-pixel inward step */
static int8_t  *in_y = NULL;
static uint8_t *edge_dist = NULL;    /* distance from almond edge (0 inside) */
static uint8_t *fire1 = NULL;
static uint8_t *fire2 = NULL;

static int eye_min_y, eye_max_y;
static int eye_min_x, eye_max_x;

/* Pupil — same construction (smaller almond sharing the outer curve). */
static float pupil_half_w, pupil_half_h, pupil_d, pupil_r;
static float pupil_x = 0.0f, pupil_y = 0.0f;
static float gaze_tx = 0.0f, gaze_ty = 0.0f;
static int   gaze_hold = 0;

static int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int inside_pupil(int x, int y)
{
    float pcx = EYE_CX + pupil_x;
    float pcy = EYE_CY + pupil_y;
    float dx = (float)x - pcx;
    float dy = (float)y - pcy;
    /* Vertical slit almond: arc centers offset horizontally by pupil_d. */
    float pdl = sqrtf((dx + pupil_d) * (dx + pupil_d) + dy * dy);
    if (pdl > pupil_r) return 0;
    float pdr = sqrtf((dx - pupil_d) * (dx - pupil_d) + dy * dy);
    return pdr <= pupil_r;
}

static void build_eye_mask(void)
{
    /* Vertical-slit almond: intersection of two circles of radius R whose
     * centers sit d to the left and right of the eye center on the
     * horizontal axis. HALF_H is the long (vertical) axis. */
    const float d = (HALF_H * HALF_H - HALF_W * HALF_W) / (2.0f * HALF_W);
    const float R = HALF_W + d;

    /* pupil scaled to share the outer curve */
    pupil_half_w = 3.5f;
    pupil_half_h = HALF_H * (pupil_half_w / HALF_W);
    pupil_d = (pupil_half_h * pupil_half_h - pupil_half_w * pupil_half_w)
              / (2.0f * pupil_half_w);
    pupil_r = pupil_half_w + pupil_d;

    int min_y = VRES - 1, max_y = 0, min_x = HRES - 1, max_x = 0;

    for (int y = 0; y < VRES; y++) {
        for (int x = 0; x < HRES; x++) {
            float dx = (float)x - EYE_CX;
            float dy = (float)y - EYE_CY;
            /* Arc centers offset horizontally: left at (cx-d, cy), right at (cx+d, cy). */
            float dl = sqrtf((dx + d) * (dx + d) + dy * dy);   /* dist from left arc center */
            float dr = sqrtf((dx - d) * (dx - d) + dy * dy);   /* dist from right arc center */
            int inside = (dl <= R) && (dr <= R);
            int idx = y * HRES + x;

            if (inside) {
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;

                float rad = sqrtf(dx * dx + dy * dy);
                float edge = fminf(R - dl, R - dr);
                float edge_fade = fminf(1.0f, edge / 5.0f);
                /* Use the long axis (HALF_H) to taper toward the corners. */
                float t = (rad - IRIS_OUTER) / (HALF_H - IRIS_OUTER);
                if (t < 0) t = 0;
                if (t > 1) t = 1;
                int v = (int)(220.0f - 100.0f * t);
                v = (int)(v * edge_fade);
                if (v < 0) v = 0;
                if (v > 255) v = 255;
                eye_mask[idx] = (uint8_t)v;
                in_x[idx] = 0;
                in_y[idx] = 0;
            } else {
                /* outward push from each arc center we're outside of */
                float oxL = 0.0f, oyL = 0.0f, oxR = 0.0f, oyR = 0.0f;
                if (dl > R) {
                    float w = dl - R;
                    oxL = ((dx + d) / dl) * w;
                    oyL = (dy / dl) * w;
                }
                if (dr > R) {
                    float w = dr - R;
                    oxR = ((dx - d) / dr) * w;
                    oyR = (dy / dr) * w;
                }
                float ox = oxL + oxR;
                float oy = oyL + oyR;
                float on = sqrtf(ox * ox + oy * oy);

                int sx = 0, sy = 0;
                if (on > 1e-4f) {
                    float ix = -ox / on;
                    float iy = -oy / on;
                    sx = (ix > 0.5f) ? 1 : (ix < -0.5f ? -1 : 0);
                    sy = (iy > 0.5f) ? 1 : (iy < -0.5f ? -1 : 0);
                    if (sx == 0 && sy == 0) {
                        if (fabsf(ix) >= fabsf(iy)) sx = (ix >= 0) ? 1 : -1;
                        else                        sy = (iy >= 0) ? 1 : -1;
                    }
                }
                in_x[idx] = (int8_t)sx;
                in_y[idx] = (int8_t)sy;

                float sd = fmaxf(dl - R, dr - R);
                int isd = (int)(sd + 0.5f);
                if (isd < 0) isd = 0;
                if (isd > 255) isd = 255;
                edge_dist[idx] = (uint8_t)isd;
            }
        }
    }

    eye_min_y = clampi(min_y - 4, 1, VRES - 2);
    eye_max_y = clampi(max_y + 4, 1, VRES - 2);
    eye_min_x = clampi(min_x - 4, 1, HRES - 2);
    eye_max_x = clampi(max_x + 4, 1, HRES - 2);
}

static void update_gaze(void)
{
    if (gaze_hold <= 0) {
        float rx = ((rand() & 0xFFFF) / 32767.5f) - 1.0f;
        float ry = ((rand() & 0xFFFF) / 32767.5f) - 1.0f;
        /* Vertical slit: more travel along the long (vertical) axis. */
        float max_x = HALF_W - pupil_half_w - 4.0f;
        float max_y = HALF_H - pupil_half_h - 6.0f;
        gaze_tx = rx * max_x * 0.7f;
        gaze_ty = ry * max_y * 0.6f;
        gaze_hold = 60 + (rand() % 120);
    }
    gaze_hold--;
    pupil_x += (gaze_tx - pupil_x) * 0.08f;
    pupil_y += (gaze_ty - pupil_y) * 0.08f;
}

static void heat(uint8_t *dst)
{
    float flicker = 0.85f + 0.15f * ((rand() & 0xFF) / 255.0f);

    /* stamp eye body */
    for (int y = eye_min_y; y <= eye_max_y; y++) {
        int row = y * HRES;
        for (int x = 0; x < HRES; x++) {
            uint8_t m = eye_mask[row + x];
            if (m == 0) continue;
            int v = (int)(m * flicker);
            if (v > 255) v = 255;
            if ((uint8_t)v > dst[row + x]) dst[row + x] = (uint8_t)v;
        }
    }

    /* iris ring around the moving pupil */
    float pcxf = EYE_CX + pupil_x;
    float pcyf = EYE_CY + pupil_y;
    int iris_r = (int)(IRIS_OUTER + 1.0f);
    int iy0 = clampi((int)pcyf - iris_r, 0, VRES - 1);
    int iy1 = clampi((int)pcyf + iris_r, 0, VRES - 1);
    int ix0 = clampi((int)pcxf - iris_r, 0, HRES - 1);
    int ix1 = clampi((int)pcxf + iris_r, 0, HRES - 1);
    for (int y = iy0; y <= iy1; y++) {
        int row = y * HRES;
        for (int x = ix0; x <= ix1; x++) {
            if (eye_mask[row + x] == 0) continue;
            float ddx = (float)x - pcxf;
            float ddy = (float)y - pcyf;
            float r = sqrtf(ddx * ddx + ddy * ddy);
            if (r > IRIS_OUTER) continue;
            uint8_t val;
            if (r >= IRIS_INNER) {
                val = 255;
            } else {
                int t = (int)(180 + (r - pupil_half_w) * 12.0f);
                if (t < 0) t = 0; if (t > 255) t = 255;
                val = (uint8_t)t;
            }
            int v = (int)(val * flicker);
            if (v > 255) v = 255;
            if ((uint8_t)v > dst[row + x]) dst[row + x] = (uint8_t)v;
        }
    }

    /* black pupil on top */
    int pcx = (int)pcxf;
    int pcy = (int)pcyf;
    int pr = (int)(pupil_half_w + 2.0f);
    int ph = (int)(pupil_half_h + 2.0f);
    int py0 = clampi(pcy - ph, 0, VRES - 1);
    int py1 = clampi(pcy + ph, 0, VRES - 1);
    int px0 = clampi(pcx - pr, 0, HRES - 1);
    int px1 = clampi(pcx + pr, 0, HRES - 1);
    for (int y = py0; y <= py1; y++) {
        int row = y * HRES;
        for (int x = px0; x <= px1; x++) {
            if (inside_pupil(x, y)) dst[row + x] = 0;
        }
    }

    /* ring sparks along the silhouette. Arc centers are left and right
     * of the eye for the vertical almond, so sample each arc on its "far"
     * side (left arc's right half, right arc's left half). */
    int spark_count = 220 + (rand() % 120);
    const float d = (HALF_H * HALF_H - HALF_W * HALF_W) / (2.0f * HALF_W);
    const float R = HALF_W + d;
    for (int i = 0; i < spark_count; i++) {
        float t = ((rand() & 0xFFFF) / 65535.0f) * 6.2831853f;
        float ct = cosf(t), st = sinf(t);
        float px, py;
        if (rand() & 1) {
            /* right arc (center at cx-d), draw its right half (ct > 0) */
            if (ct < 0) ct = -ct;
            px = (EYE_CX - d) + R * ct;
            py = EYE_CY + R * st;
        } else {
            /* left arc (center at cx+d), draw its left half (ct < 0) */
            if (ct > 0) ct = -ct;
            px = (EYE_CX + d) + R * ct;
            py = EYE_CY + R * st;
        }
        float ndx = px - EYE_CX;
        float ndy = py - EYE_CY;
        float nn = sqrtf(ndx * ndx + ndy * ndy);
        if (nn > 1e-4f) { ndx /= nn; ndy /= nn; }
        float j = (float)(rand() % 3);
        int ix = (int)(px + ndx * j);
        int iy = (int)(py + ndy * j);
        if (ix < 1 || ix > HRES - 2 || iy < 1 || iy > VRES - 2) continue;
        if (edge_dist[iy * HRES + ix] > FLAME_REACH) continue;
        dst[iy * HRES + ix] = (uint8_t)(200 + (rand() & 55));
    }
}

static void blur_out(const uint8_t *src, uint8_t *dst)
{
    memset(dst, 0, HRES);
    for (int y = 1; y < VRES - 1; y++) {
        int row = y * HRES;
        dst[row] = 0;
        for (int x = 1; x < HRES - 1; x++) {
            int idx = row + x;
            int sx = in_x[idx];
            int sy = in_y[idx];

            if (sx == 0 && sy == 0) {
                if (inside_pupil(x, y)) { dst[idx] = 0; continue; }
                int b = (src[idx - 1] + src[idx + 1]
                       + src[idx - HRES] + src[idx + HRES]) >> 2;
                dst[idx] = (uint8_t)b;
                continue;
            }

            int dist = edge_dist[idx];
            if (dist > FLAME_REACH) { dst[idx] = 0; continue; }

            int psx = -sy;
            int psy =  sx;

            int o1 = idx + sy * HRES + sx;
            int o2 = idx + sy * HRES + sx + psy * HRES + psx;
            int o3 = idx + sy * HRES + sx - psy * HRES - psx;

            if (o1 < 0) o1 = 0; else if (o1 >= FB_SIZE) o1 = FB_SIZE - 1;
            if (o2 < 0) o2 = 0; else if (o2 >= FB_SIZE) o2 = FB_SIZE - 1;
            if (o3 < 0) o3 = 0; else if (o3 >= FB_SIZE) o3 = FB_SIZE - 1;

            unsigned sum = (unsigned)src[o1] + (unsigned)src[o2] + (unsigned)src[o3];
            unsigned b = sum / 3u;
            b = (b * (unsigned)(FLAME_REACH - dist)) / (unsigned)FLAME_REACH;
            if (b > 0) b--;
            dst[idx] = (uint8_t)b;
        }
        dst[row + HRES - 1] = 0;
    }
    memset(dst + (VRES - 1) * HRES, 0, HRES);
}

void sauron_init(void)
{
    eye_mask  = (uint8_t *)calloc(FB_SIZE, 1);
    in_x      = (int8_t  *)calloc(FB_SIZE, 1);
    in_y      = (int8_t  *)calloc(FB_SIZE, 1);
    edge_dist = (uint8_t *)calloc(FB_SIZE, 1);
    fire1     = (uint8_t *)calloc(FB_SIZE, 1);
    fire2     = (uint8_t *)calloc(FB_SIZE, 1);
    build_eye_mask();
}

void sauron_step(void)
{
    update_gaze();
    heat(fire1);
    blur_out(fire1, fire2);

    /* Pack the 0..255 heat field into the GPU's 4bpp framebuffer (16
     * colors, 2 pixels per byte). Quantize to 4 bits with the low nibble
     * holding the even-x pixel and the high nibble the odd-x pixel — this
     * matches the bit-packing order of fb_set_pixel() in gpu_sdl.c. */
    uint8_t *dst = gpu_get_draw_fb();
    int n = (HRES * VRES) / 2;
    for (int i = 0; i < n; i++) {
        uint8_t a = fire2[i * 2]     >> 4;       /* even-x pixel */
        uint8_t b = fire2[i * 2 + 1] >> 4;       /* odd-x pixel  */
        dst[i] = (uint8_t)((b << 4) | a);
    }

    /* swap working buffers */
    uint8_t *tmp = fire1; fire1 = fire2; fire2 = tmp;
}

void sauron_shutdown(void)
{
    free(eye_mask);
    free(in_x);
    free(in_y);
    free(edge_dist);
    free(fire1);
    free(fire2);
}
