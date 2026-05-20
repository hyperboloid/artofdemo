/*
 *                          The Art Of
 *                      D E M O M A K I N G
 *
 *                     by Alex J. Champandard
 *                          Base Sixteen
 *
 *                http://www.flipcode.com/demomaking
 *
 *  Eye of Sauron variation — procedurally seeds a vertical cat-eye
 *  heat mask with a dark vertical pupil. Flames emanate perpendicular
 *  to the eye's silhouette via a precomputed direction field.
 */

#include <cstdlib>
#include <iostream>
#include <cstring>
#include <cmath>
#include <SDL.h>
#include "vga.h"
#include "timer.h"

VGA *vga;
TIMER *timer;

// static heat mask for the eye silhouette
unsigned char *eyeMask;
// per-pixel inward step (toward eye): the blur reads neighbors at
// (x+inX, y+inY) so heat propagates outward perpendicular to the edge.
signed char *inX;
signed char *inY;
// per-pixel distance from the eye edge (pixels). 0 inside the eye.
// Used to clamp how far flames can travel.
unsigned char *edgeDist;
// region-of-interest bounds for stamping the eye
int eyeMinY, eyeMaxY, eyeMinX, eyeMaxX;
// eye center for spark placement
float eyeCX, eyeCY;
// half-extents (vertical almond: tall)
float halfW, halfH;
// pupil shape (a scaled-down almond, same curve family as the eye)
float pupilHalfW, pupilHalfH, pupilD, pupilR;
// bright iris ring radii (around pupil center)
float irisInner = 9.0f;
float irisOuter = 17.0f;
// current pupil offset (gaze direction) and animated target
float pupilX = 0.0f, pupilY = 0.0f;
float gazeTX = 0.0f, gazeTY = 0.0f;
int gazeHold = 0;

// how far flames can travel from the eye edge (in pixels)
static const int FLAME_REACH = 18;

static inline int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/*
 * create a shade of colours in the palette from s to e
 */
void Shade_Pal( int s, int e, int r1, int g1, int b1, int r2, int g2, int b2 )
{
    int i;
    float k;
    for (i=0; i<=e-s; i++)
    {
        k = (float)i/(float)(e-s);
        vga->SetColour( s+i, (int)(r1+(r2-r1)*k), (int)(g1+(g2-g1)*k), (int)(b1+(b2-b1)*k) );
    }
}

/*
 * Build a vertical cat-eye almond. The almond is the intersection of
 * two circles whose centers lie horizontally to the left and right of
 * the eye center (so the resulting shape is tall, with pointed top
 * and bottom corners).
 *
 * Also build the per-pixel "inward direction" field used by the
 * directional blur: for every pixel outside the eye, store a small
 * integer step (sx, sy) that points back toward the eye center. The
 * blur samples neighbors at (x+sx, y+sy), so heat propagates in the
 * opposite direction — outward, perpendicular to the eye edge.
 */
void BuildEyeMask()
{
    eyeMask = new unsigned char[64000];
    inX = new signed char[64000];
    inY = new signed char[64000];
    edgeDist = new unsigned char[64000];
    memset(eyeMask, 0, 64000);
    memset(inX, 0, 64000);
    memset(inY, 0, 64000);
    memset(edgeDist, 0, 64000);

    eyeCX = 160.0f;
    eyeCY = 100.0f;
    // vertical almond: tall (halfH) and narrow (halfW)
    halfW = 28.0f;
    halfH = 75.0f;

    // Two circles whose intersection forms the almond. Centers are at
    // (cx - d, cy) and (cx + d, cy). Each circle passes through the
    // top/bottom corners (cx, cy +/- halfH) and is tangent to the
    // sides at (cx +/- halfW, cy).
    //   d = (halfH^2 - halfW^2) / (2*halfW)
    //   R = halfW + d
    const float d = (halfH*halfH - halfW*halfW) / (2.0f*halfW);
    const float R = halfW + d;

    // pupil: a scaled-down almond sharing the eye's silhouette.
    // Same two-circle construction, but with smaller half-extents so
    // the pupil's edge follows the same curve as the outer eye.
    // Stamped per-frame at (eyeCX + pupilX, eyeCY + pupilY) so the eye
    // can look around.
    pupilHalfW = 5.0f;
    pupilHalfH = halfH * (pupilHalfW / halfW);
    pupilD = (pupilHalfH*pupilHalfH - pupilHalfW*pupilHalfW) / (2.0f*pupilHalfW);
    pupilR = pupilHalfW + pupilD;

    int minY = 199, maxY = 0, minX = 319, maxX = 0;

    for (int y = 0; y < 200; y++)
    {
        for (int x = 0; x < 320; x++)
        {
            float dx = (float)x - eyeCX;
            float dy = (float)y - eyeCY;

            // distance from left and right circle centers
            float dl = sqrtf((dx + d)*(dx + d) + dy*dy);  // left arc center
            float dr = sqrtf((dx - d)*(dx - d) + dy*dy);  // right arc center

            bool inside = (dl <= R) && (dr <= R);

            if (inside)
            {
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;

                float rad = sqrtf(dx*dx + dy*dy);

                // fade brightness toward the almond's edge so it tapers
                // into flame at the corners. The bright iris ring is
                // NOT baked in here — it is stamped per-frame in Heat()
                // around the moving pupil position.
                float edge = fminf(R - dl, R - dr);
                float edgeFade = fminf(1.0f, edge / 6.0f);

                // body fades toward the pointed corners (eye-center radius)
                float t = (rad - irisOuter) / (halfH - irisOuter);
                if (t < 0) t = 0; if (t > 1) t = 1;
                unsigned char val = (unsigned char)(220.0f - 100.0f*t);
                val = (unsigned char)(val * edgeFade);
                eyeMask[y*320 + x] = val;
                inX[y*320 + x] = 0;
                inY[y*320 + x] = 0;
            }
            else
            {
                // OUTSIDE the eye. Compute outward direction = gradient
                // of (signed) distance from the almond. For the almond
                // formed by intersection of two disks, the outward
                // normal at a point outside is the direction away from
                // whichever arc center is farther — equivalently, the
                // unit vector from the nearest arc center through the
                // point, blended near the corners.
                //
                // Practical approximation: the outward direction is the
                // sum of the two "outside-the-circle" pushes (one per
                // arc). For a point outside circle k with center Ck and
                // radius R, the outward push is (P - Ck)/|P - Ck|
                // scaled by max(0, |P-Ck| - R). This naturally blends
                // into a single direction near the corners and matches
                // the arc normal near the sides.
                float oxL = 0.0f, oyL = 0.0f, oxR = 0.0f, oyR = 0.0f;
                if (dl > R)
                {
                    float w = dl - R;
                    oxL = ((dx + d) / dl) * w;
                    oyL = (dy / dl) * w;
                }
                if (dr > R)
                {
                    float w = dr - R;
                    oxR = ((dx - d) / dr) * w;
                    oyR = (dy / dr) * w;
                }
                float ox = oxL + oxR;
                float oy = oyL + oyR;
                float on = sqrtf(ox*ox + oy*oy);

                // inward direction = -outward, quantized to an integer
                // step in {-1,0,1} for x and y. Round so that the
                // dominant axis always gets a non-zero step.
                int sx = 0, sy = 0;
                if (on > 1e-4f)
                {
                    float ix = -ox / on;
                    float iy = -oy / on;
                    // round half away from zero
                    sx = (ix > 0.5f) ? 1 : (ix < -0.5f ? -1 : 0);
                    sy = (iy > 0.5f) ? 1 : (iy < -0.5f ? -1 : 0);
                    // ensure at least one axis is non-zero
                    if (sx == 0 && sy == 0)
                    {
                        if (fabsf(ix) >= fabsf(iy)) sx = (ix >= 0) ? 1 : -1;
                        else                        sy = (iy >= 0) ? 1 : -1;
                    }
                }
                inX[y*320 + x] = (signed char)sx;
                inY[y*320 + x] = (signed char)sy;

                // signed distance from the almond (intersection of two
                // disks) for an outside point is max(dl - R, dr - R).
                float sd = fmaxf(dl - R, dr - R);
                int isd = (int)(sd + 0.5f);
                if (isd < 0) isd = 0;
                if (isd > 255) isd = 255;
                edgeDist[y*320 + x] = (unsigned char)isd;
            }
        }
    }

    eyeMinY = clampi(minY - 4, 1, 198);
    eyeMaxY = clampi(maxY + 4, 1, 198);
    eyeMinX = clampi(minX - 4, 1, 318);
    eyeMaxX = clampi(maxX + 4, 1, 318);
}

/*
 * Test whether pixel (x,y) is inside the pupil almond at its current
 * offset position. Uses the same two-circle intersection test as the
 * outer eye, scaled down to pupil size.
 */
static inline bool InsidePupil(int x, int y)
{
    float pcx = eyeCX + pupilX;
    float pcy = eyeCY + pupilY;
    float dx = (float)x - pcx;
    float dy = (float)y - pcy;
    float pdl = sqrtf((dx + pupilD)*(dx + pupilD) + dy*dy);
    if (pdl > pupilR) return false;
    float pdr = sqrtf((dx - pupilD)*(dx - pupilD) + dy*dy);
    return pdr <= pupilR;
}

/*
 * Update the gaze: pick a new fixation target occasionally, then ease
 * the pupil toward it. Targets are constrained to a comfortable range
 * inside the eye (so the pupil never escapes the iris).
 */
void UpdateGaze()
{
    if (gazeHold <= 0)
    {
        // pick a new target. Keep most targets near center, but
        // occasionally throw a glance toward a corner.
        float rx = ((rand() & 0xFFFF) / 32767.5f) - 1.0f;  // [-1, 1]
        float ry = ((rand() & 0xFFFF) / 32767.5f) - 1.0f;
        // bias horizontal range narrow (slit-pupil eye is tall),
        // vertical range wider so glances up/down read clearly
        float maxX = halfW - pupilHalfW - 4.0f;
        float maxY = halfH - pupilHalfH - 6.0f;
        gazeTX = rx * maxX * 0.7f;
        gazeTY = ry * maxY * 0.6f;
        // hold the new target for ~60-180 frames (~1-3 seconds at 60fps)
        gazeHold = 60 + (rand() % 120);
    }
    gazeHold--;
    // ease pupil toward target (lerp)
    pupilX += (gazeTX - pupilX) * 0.08f;
    pupilY += (gazeTY - pupilY) * 0.08f;
}

/*
 * Heat injection: stamp the eye, then sprinkle hot sparks in a thin
 * band just outside the eye so flame is fed perpendicular to its edge.
 */
void Heat( unsigned char *dst, int frame )
{
    float flicker = 0.85f + 0.15f * ((rand() & 0xFF) / 255.0f);

    // stamp eye mask
    for (int y = eyeMinY; y <= eyeMaxY; y++)
    {
        int row = y * 320;
        for (int x = 0; x < 320; x++)
        {
            unsigned char m = eyeMask[row + x];
            if (m == 0) continue;
            int v = (int)(m * flicker);
            if (v > 255) v = 255;
            if ((unsigned char)v > dst[row + x]) dst[row + x] = (unsigned char)v;
        }
    }

    // stamp the bright iris ring around the moving pupil center.
    // Limited to pixels actually inside the eye almond so it never
    // bleeds past the silhouette.
    float pcxf = eyeCX + pupilX;
    float pcyf = eyeCY + pupilY;
    int irisR = (int)(irisOuter + 1.0f);
    int iy0 = clampi((int)pcyf - irisR, 0, 199);
    int iy1 = clampi((int)pcyf + irisR, 0, 199);
    int ix0 = clampi((int)pcxf - irisR, 0, 319);
    int ix1 = clampi((int)pcxf + irisR, 0, 319);
    for (int y = iy0; y <= iy1; y++)
    {
        int row = y * 320;
        for (int x = ix0; x <= ix1; x++)
        {
            // must be inside the eye almond
            if (eyeMask[row + x] == 0) continue;
            float ddx = (float)x - pcxf;
            float ddy = (float)y - pcyf;
            float r = sqrtf(ddx*ddx + ddy*ddy);
            if (r > irisOuter) continue;
            unsigned char val;
            if (r >= irisInner)
            {
                val = 255;
            }
            else
            {
                val = (unsigned char)clampi((int)(180 + (r - pupilHalfW) * 12.0f), 0, 255);
            }
            int v = (int)(val * flicker);
            if (v > 255) v = 255;
            if ((unsigned char)v > dst[row + x]) dst[row + x] = (unsigned char)v;
        }
    }

    // stamp pupil (black) on top of the iris at the current gaze offset
    int pcx = (int)pcxf;
    int pcy = (int)pcyf;
    int pr = (int)(pupilHalfW + 2.0f);
    int ph = (int)(pupilHalfH + 2.0f);
    int py0 = clampi(pcy - ph, 0, 199);
    int py1 = clampi(pcy + ph, 0, 199);
    int px0 = clampi(pcx - pr, 0, 319);
    int px1 = clampi(pcx + pr, 0, 319);
    for (int y = py0; y <= py1; y++)
    {
        int row = y * 320;
        for (int x = px0; x <= px1; x++)
        {
            if (InsidePupil(x, y)) dst[row + x] = 0;
        }
    }

    // ring sparks: sample points along the almond's edge by walking the
    // boundary parametrically, then jitter outward a few pixels along
    // the outward normal so flame seeds form a thin shell around the eye.
    int sparkCount = 260 + (rand() % 140);
    const float d = (halfH*halfH - halfW*halfW) / (2.0f*halfW);
    const float R = halfW + d;
    for (int i = 0; i < sparkCount; i++)
    {
        // pick an angle, then pick which arc (left or right) we sit on
        // -- the right arc covers the right half of the almond, drawn
        // from its left-side center at (-d, 0) relative to eye center.
        float t = ((rand() & 0xFFFF) / 65535.0f) * 6.2831853f;
        // restrict t so it covers only the visible portion of the arc
        // (i.e., the arc on the far side of its center)
        float ct = cosf(t), st = sinf(t);
        // right arc: center at (cx - d, cy), pick points with ct > 0 region
        // left arc:  center at (cx + d, cy), pick points with ct < 0 region
        float px, py;
        if (rand() & 1)
        {
            // right side of the almond
            if (ct < 0) ct = -ct;
            px = (eyeCX - d) + R * ct;
            py = eyeCY + R * st;
        }
        else
        {
            if (ct > 0) ct = -ct;
            px = (eyeCX + d) + R * ct;
            py = eyeCY + R * st;
        }
        // jitter outward 0..3 px along the outward normal
        float ndx = px - eyeCX;
        float ndy = py - eyeCY;
        float nn = sqrtf(ndx*ndx + ndy*ndy);
        if (nn > 1e-4f) { ndx /= nn; ndy /= nn; }
        float j = (float)(rand() % 4);
        int ix = (int)(px + ndx * j);
        int iy = (int)(py + ndy * j);
        if (ix < 1 || ix > 318 || iy < 1 || iy > 198) continue;
        // don't seed heat outside the flame band
        if (edgeDist[iy*320 + ix] > FLAME_REACH) continue;
        dst[iy*320 + ix] = 200 + (rand() & 55);
    }

    (void)frame;
}

/*
 * Directional blur: for each pixel outside the eye, look up its
 * "inward" step (sx, sy) and sample three taps along that direction
 * — the pixel itself biased toward the eye plus its two perpendicular
 * neighbors at that same offset. This makes heat travel away from the
 * eye perpendicular to the edge. Each step decrements by one to make
 * the fire lose intensity with distance, as in the classic kernel.
 */
void Blur_Out( unsigned char *src, unsigned char *dst )
{
    // clear the 1-pixel border so we can safely sample +/- 1
    memset(dst, 0, 320);
    for (int y = 1; y < 199; y++)
    {
        int row = y * 320;
        dst[row] = 0;
        for (int x = 1; x < 319; x++)
        {
            int idx = row + x;
            int sx = inX[idx];
            int sy = inY[idx];

            if (sx == 0 && sy == 0)
            {
                // inside the eye: keep pupil pixels black, otherwise
                // do a tiny 4-tap average so the iris doesn't shimmer.
                if (InsidePupil(x, y))
                {
                    dst[idx] = 0;
                    continue;
                }
                int b = (src[idx-1] + src[idx+1] + src[idx-320] + src[idx+320]) >> 2;
                dst[idx] = (unsigned char)b;
                continue;
            }

            // hard cap: kill heat beyond the flame reach
            int dist = edgeDist[idx];
            if (dist > FLAME_REACH)
            {
                dst[idx] = 0;
                continue;
            }

            // perpendicular direction to (sx, sy) for the side taps
            int psx = -sy;
            int psy =  sx;

            int o1 = idx + sy*320 + sx;            // toward eye, on axis
            int o2 = idx + sy*320 + sx + psy*320 + psx;  // toward eye + perp
            int o3 = idx + sy*320 + sx - psy*320 - psx;  // toward eye - perp

            // clamp the sample offsets to valid range
            if (o1 < 0) o1 = 0; else if (o1 >= 64000) o1 = 63999;
            if (o2 < 0) o2 = 0; else if (o2 >= 64000) o2 = 63999;
            if (o3 < 0) o3 = 0; else if (o3 >= 64000) o3 = 63999;

            unsigned int sum = (unsigned int)src[o1]
                             + (unsigned int)src[o2]
                             + (unsigned int)src[o3];
            unsigned int b = sum / 3u;
            // distance-based attenuation: at dist == FLAME_REACH the
            // pixel is forced to 0; closer pixels keep more heat.
            // multiplier = (FLAME_REACH - dist) / FLAME_REACH
            b = (b * (unsigned int)(FLAME_REACH - dist)) / (unsigned int)FLAME_REACH;
            if (b > 0) b--;
            dst[idx] = (unsigned char)b;
        }
        dst[row + 319] = 0;
    }
    memset(dst + 199*320, 0, 320);
}

int main(int argc, char *argv[])
{
    vga = new VGA;

    Shade_Pal( 0,  31,   0, 0, 0,   24,  0, 0 );
    Shade_Pal( 32, 95,  24, 0, 0,   63, 20, 0 );
    Shade_Pal( 96, 175, 63, 20, 0,  63, 50, 0 );
    Shade_Pal( 176, 223, 63, 50, 0, 63, 63, 20 );
    Shade_Pal( 224, 255, 63, 63, 20, 63, 63, 63 );

    BuildEyeMask();

    unsigned char *fire1 = new unsigned char[64000];
    unsigned char *fire2 = new unsigned char[64000];
    unsigned char *tmp;
    memset(fire1, 0, 64000);
    memset(fire2, 0, 64000);

    timer = new TIMER;
    long long startTime = timer->getCount(), frameCount = 0;

    bool running = true;
    SDL_Event event;
    int frame = 0;
    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN)
                running = false;
        }
        if (!running) break;

        UpdateGaze();
        Heat(fire1, frame);
        Blur_Out(fire1, fire2);
        memcpy(vga->page_draw, fire2, 63040);
        vga->Update();

        tmp = fire1; fire1 = fire2; fire2 = tmp;
        frameCount++;
        frame++;
    }

    long long totalTime = timer->getCount() - startTime;
    delete timer;
    float FPS = (float)(frameCount) * 1193180.0f / (float)(totalTime);

    delete vga;
    delete [] eyeMask;
    delete [] inX;
    delete [] inY;
    delete [] edgeDist;
    delete [] fire1;
    delete [] fire2;

    std::cout << "Eye of Sauron — based on The Art of Demomaking by Alex J. Champandard" << std::endl;
    std::cout << FPS << " frames per second." << std::endl;
    return 0;
}
