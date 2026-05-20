/*
 *                          The Art Of
 *                      D E M O M A K I N G
 *
 *                     by Alex J. Champandard
 *                          Base Sixteen
 *
 *                http://www.flipcode.com/demomaking
 *
 *  OpenAI logo — three interlocking ouroboros snakes arranged in the
 *  six-petal blossom of the OpenAI mark.
 *
 *  Each snake's body follows a 2-lobe limaçon
 *      r(t) = R0 + A*cos(2t),   t in [0, 2*PI]
 *  centered at the screen midpoint and rotated by 0, 120, or 240
 *  degrees. The three loops cross near the center, producing the
 *  six-lobed hexagonal silhouette of the OpenAI mark.
 *
 *  Over/under at crossings is resolved by sorting every stamp from
 *  every snake by a common depth z(t) and drawing back-to-front.
 *  z is phase-shifted by 120 degrees per snake so the three snakes
 *  weave together in a Borromean fashion.
 */

#include <cstdlib>
#include <iostream>
#include <cstring>
#include <cstdio>
#include <string>
#include <cmath>
#include <algorithm>
#include <vector>
#include <SDL.h>
#include "vga.h"
#include "timer.h"

VGA *vga;
TIMER *timer;

static const float PI = 3.14159265358979f;
static const float TWO_PI = 6.28318530717958f;

// Screen / logo geometry.
static const float CX = 160.0f;
static const float CY = 100.0f;
// Per-snake loop: a 2-lobe peanut with lobe tips at r = R0+A and a
// pinch near r = R0-A. Choose A close to R0 so each loop pinches
// nearly to a point at the center (matching the OpenAI logo where all
// three loops meet near the middle).
static const float R0 = 40.0f;
static const float A_LOBE = 32.0f;

// Body thickness profile
static const float BODY_THICK_HEAD = 7.5f;
static const float BODY_THICK_TAIL = 1.2f;

static inline int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline float clampf(float v, float lo, float hi)
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
    for (int i = 0; i <= e - s; i++)
    {
        float k = (float)i / (float)(e - s);
        vga->SetColour( s+i,
            (int)(r1 + (r2 - r1) * k),
            (int)(g1 + (g2 - g1) * k),
            (int)(b1 + (b2 - b1) * k) );
    }
}

/*
 * Evaluate one snake's centerline at parameter t in [0, 2*PI],
 * given the snake's rotation angle rot (radians).
 */
struct CurvePt {
    float x, y;
    float tangent;
    float z;
};

// Arc-length-to-t lookup table for the canonical (unrotated) loop.
// Built once at startup so the snake's head moves at constant screen
// speed regardless of where it is on the loop (pinches are tight and
// would otherwise cause the head to crawl through them slowly).
static const int   ARC_LUT_SIZE = 4096;
static float       arcLut[ARC_LUT_SIZE + 1]; // t-value at each arc-length step
static float       sLut[ARC_LUT_SIZE + 1];   // arc-length at each t step (forward)
static float       totalArcLength = 0.0f;

// Per-snake rotation, set once at the start of Render() and read by
// stampBody() to transform pixel coords back into the canonical
// (unrotated) snake frame for stationary-scale lookup.
static float       snakeRot[3] = {0.0f, 0.0f, 0.0f};

static void buildArcLut()
{
    // Numerically integrate ds/dt over [0, 2*PI], then invert.
    const int N = 32768;
    std::vector<float> tArr(N + 1), sArr(N + 1);
    float prevX = R0 + A_LOBE, prevY = 0.0f;
    sArr[0] = 0.0f;
    tArr[0] = 0.0f;
    for (int i = 1; i <= N; i++)
    {
        float t = TWO_PI * (float)i / (float)N;
        float r = R0 + A_LOBE * cosf(2.0f * t);
        float x = r * cosf(t);
        float y = r * sinf(t);
        float dx = x - prevX;
        float dy = y - prevY;
        sArr[i] = sArr[i - 1] + sqrtf(dx*dx + dy*dy);
        tArr[i] = t;
        prevX = x; prevY = y;
    }
    totalArcLength = sArr[N];

    // Invert: for each uniform arc-length sample s, find the
    // corresponding t by linear interpolation between tabulated points.
    int j = 0;
    for (int i = 0; i <= ARC_LUT_SIZE; i++)
    {
        float s = totalArcLength * (float)i / (float)ARC_LUT_SIZE;
        while (j < N && sArr[j + 1] < s) j++;
        float ds = sArr[j + 1] - sArr[j];
        float f = (ds > 1e-6f) ? (s - sArr[j]) / ds : 0.0f;
        arcLut[i] = tArr[j] + f * (tArr[j + 1] - tArr[j]);
    }

    // Forward LUT: arc-length at uniform t samples (linear-interp from
    // the dense tArr/sArr). Used by per-pixel scale shading so the
    // lattice is anchored to snake-local arc length and stays
    // stationary on screen as the snake "moves".
    for (int i = 0; i <= ARC_LUT_SIZE; i++)
    {
        float t = TWO_PI * (float)i / (float)ARC_LUT_SIZE;
        float idx = t / TWO_PI * (float)N;
        int j2 = (int)idx;
        if (j2 < 0) j2 = 0;
        if (j2 >= N) j2 = N - 1;
        float f = idx - (float)j2;
        sLut[i] = sArr[j2] + f * (sArr[j2 + 1] - sArr[j2]);
    }
}

// Forward: t in [0, 2*PI) → arc-length from t=0.
static inline float tToS(float t)
{
    t -= floorf(t / TWO_PI) * TWO_PI;
    float idx = t / TWO_PI * (float)ARC_LUT_SIZE;
    int i0 = (int)idx;
    if (i0 < 0) i0 = 0;
    if (i0 >= ARC_LUT_SIZE) i0 = ARC_LUT_SIZE - 1;
    float frac = idx - (float)i0;
    return sLut[i0] + frac * (sLut[i0 + 1] - sLut[i0]);
}

// Convert an arc-length-fraction phase in [0, 1) (wrapping) to a curve
// parameter t in [0, 2*PI).
static inline float phaseToT(float phase)
{
    phase -= floorf(phase);
    float idx = phase * (float)ARC_LUT_SIZE;
    int i0 = (int)idx;
    if (i0 < 0) i0 = 0;
    if (i0 >= ARC_LUT_SIZE) i0 = ARC_LUT_SIZE - 1;
    float frac = idx - (float)i0;
    return arcLut[i0] + frac * (arcLut[i0 + 1] - arcLut[i0]);
}

static inline CurvePt evalSnake(float t, float rot, float zPhase)
{
    float r  = R0 + A_LOBE * cosf(2.0f * t);
    float dr = -2.0f * A_LOBE * sinf(2.0f * t);
    float c = cosf(t), s = sinf(t);
    float lx = r * c;
    float ly = r * s;
    float ldx = dr * c - r * s;
    float ldy = dr * s + r * c;
    float cr = cosf(rot), sr = sinf(rot);
    CurvePt p;
    p.x = CX + lx * cr - ly * sr;
    p.y = CY + lx * sr + ly * cr;
    float gdx = ldx * cr - ldy * sr;
    float gdy = ldx * sr + ldy * cr;
    p.tangent = atan2f(gdy, gdx);
    // Depth oscillates twice per loop (once per lobe), phase-shifted
    // per snake so the three snakes interlock Borromean-style.
    p.z = sinf(2.0f * t + zPhase);
    return p;
}

/*
 * Stamp a body segment with per-stamp tangent frame for shading.
 */
static void stampBody(unsigned char *dst, unsigned char *headMask,
                      int snakeIdx, float cx, float cy, float halfT,
                      float taperT, float tangent, float uOffset)
{
    (void)tangent; (void)uOffset;
    if (halfT < 0.5f) return;
    int x0 = (int)floorf(cx - halfT) - 1;
    int x1 = (int)ceilf (cx + halfT) + 1;
    int y0 = (int)floorf(cy - halfT) - 1;
    int y1 = (int)ceilf (cy + halfT) + 1;
    if (x0 < 0) x0 = 0; if (x1 > 319) x1 = 319;
    if (y0 < 0) y0 = 0; if (y1 > 199) y1 = 199;

    float h2 = halfT * halfT;

    const float SCALE_U = 5.0f;
    const float SCALE_V = 4.0f;

    // Inverse rotation: pixel screen-space → canonical (unrotated)
    // snake-local frame. Once in that frame we read polar (theta, r)
    // and use them as stationary lattice coordinates. Same trick as
    // the original ouroboros, generalised from "ring polar coords" to
    // "per-snake local polar coords".
    float rot = snakeRot[snakeIdx];
    float cR = cosf(-rot), sR = sinf(-rot);

    for (int y = y0; y <= y1; y++)
    {
        float dy = (float)y - cy;
        int row = y * 320;
        // pixel global offset from screen center for the local-frame
        // inverse-rotation. (cy is the stamp's center, not the screen
        // center — recompute relative to (CX, CY) below.)
        for (int x = x0; x <= x1; x++)
        {
            float dx = (float)x - cx;
            float d2 = dx*dx + dy*dy;
            if (d2 > h2) continue;

            // Snake-local frame: pixel position relative to (CX, CY),
            // un-rotated by the snake's rotation. The loop in that
            // frame is the canonical r(t) = R0 + A*cos(2t) shape that
            // doesn't move, so coordinates here are screen-stationary.
            float gx = (float)x - CX;
            float gy = (float)y - CY;
            float lx = gx * cR - gy * sR;
            float ly = gx * sR + gy * cR;

            float thetaPx = atan2f(ly, lx);
            float rPx = sqrtf(lx*lx + ly*ly);
            float rCenter = R0 + A_LOBE * cosf(2.0f * thetaPx);
            float vPix = rPx - rCenter;   // signed distance from centerline (stationary)
            float uPix = tToS(thetaPx);   // arc length along snake at this theta (stationary)

            // Cross-section parameter uses the LOCAL stamp's halfT so
            // shading still tapers correctly even though the lattice
            // is stationary.
            float tt = vPix / halfT;
            tt = clampf(tt, -1.0f, 1.0f);
            float across = sqrtf(1.0f - tt*tt);

            float vCell = vPix / SCALE_V;
            int vRow = (int)floorf(vCell + 1000.0f) - 1000;
            float vFrac = vCell - (float)vRow;
            float uShift = (vRow & 1) ? 0.5f : 0.0f;
            float uCell = uPix / SCALE_U + uShift;
            int uIdx = (int)floorf(uCell + 1000.0f) - 1000;
            float uFrac = uCell - (float)uIdx;

            float du = uFrac - 0.5f;
            float dv = vFrac - 0.5f;
            float diamond = fabsf(du) + fabsf(dv);

            float crown = 1.0f - diamond * 2.0f;
            if (crown < 0) crown = 0;
            float edgeDark = (diamond > 0.42f) ? 1.0f : 0.0f;

            float lit = 0.5f + 0.5f * tt;

            float shade = 0.30f + 0.50f * lit;
            shade *= 0.55f + 0.45f * across;
            shade += 0.30f * crown * across;
            shade -= 0.22f * edgeDark;
            shade *= 0.55f + 0.45f * taperT;
            shade = clampf(shade, 0.0f, 1.0f);

            int colour = 16 + (int)(shade * 207.0f);
            colour = clampi(colour, 16, 223);
            if (headMask[row + x] == (unsigned char)snakeIdx) continue;
            dst[row + x] = (unsigned char)colour;
        }
    }
}

static inline float headProfile(float u)
{
    if (u < 0.0f || u > 1.0f) return 0.0f;
    if (u < 0.30f)
    {
        float t = u / 0.30f;
        return 1.0f + 0.05f * t * t;
    }
    else if (u < 0.85f)
    {
        float t = (u - 0.30f) / 0.55f;
        float s = t * t * (3.0f - 2.0f * t);
        return 1.05f * (1.0f - s) + 0.55f * s;
    }
    else
    {
        float t = (u - 0.85f) / 0.15f;
        return 0.55f * (1.0f - t) + 0.18f * t;
    }
}

static void drawHead(unsigned char *dst, unsigned char *headMask,
                     int snakeIdx, float cx, float cy, float radius,
                     float tangent, bool drawMouthCutout, float jawOpen)
{
    const float headLen = radius * 2.0f;
    const float backAlong = -radius * 0.7f;
    const float biteLine  = -radius * 0.05f;

    int bx0 = (int)floorf(cx - headLen) - 2;
    int bx1 = (int)ceilf (cx + headLen) + 2;
    int by0 = (int)floorf(cy - headLen) - 2;
    int by1 = (int)ceilf (cy + headLen) + 2;
    if (bx0 < 0) bx0 = 0; if (bx1 > 319) bx1 = 319;
    if (by0 < 0) by0 = 0; if (by1 > 199) by1 = 199;

    const float hingeAlong = backAlong + radius * 0.55f;

    float ct = cosf(tangent), st = sinf(tangent);

    for (int y = by0; y <= by1; y++)
    {
        float dyR = (float)y - cy;
        int row = y * 320;
        for (int x = bx0; x <= bx1; x++)
        {
            float dxR = (float)x - cx;
            float along  =  dxR * ct + dyR * st;
            float across = -dxR * st + dyR * ct;

            float u = (along - backAlong) / headLen;

            bool inUpper = false;
            if (u >= 0.0f && u <= 1.0f && across >= biteLine)
            {
                float halfW = radius * 0.69f * headProfile(u);
                float top = biteLine + halfW;
                if (across <= top) inUpper = true;
            }

            bool inLower = false;
            if (u >= 0.0f && u <= 1.0f && across < biteLine)
            {
                float halfW = radius * 0.59f * headProfile(u);
                float bot = biteLine - halfW;
                if (across >= bot) inLower = true;
            }

            bool inMouth = false;
            bool onMouthBorder = false;
            if ((inUpper || inLower) && drawMouthCutout && jawOpen > 0.05f
                && along > hingeAlong && across < biteLine)
            {
                float frontExtent = (backAlong + headLen * 0.95f) - hingeAlong;
                float wedgeProg = (along - hingeAlong) / frontExtent;
                wedgeProg = clampf(wedgeProg, 0.0f, 1.0f);
                float wedgeDepth = wedgeProg * jawOpen * radius * 0.55f;
                float depthIntoMouth = biteLine - across;
                if (depthIntoMouth < wedgeDepth)
                {
                    inMouth = true;
                    inLower = false;
                    if (depthIntoMouth < 1.2f ||
                        (wedgeDepth - depthIntoMouth) < 1.2f)
                    {
                        onMouthBorder = true;
                    }
                }
            }

            if (!inUpper && !inLower && !inMouth) continue;

            int colour;

            float eyeD2 = 1e9f, hlD2 = 1e9f, browVal = 1e9f, nostrilD2 = 1e9f;
            bool onLip = false;
            if (inUpper)
            {
                float eyeAlong  = along  - radius * 0.15f;
                float eyeAcross = across - radius * 0.40f;
                eyeD2 = eyeAlong*eyeAlong + eyeAcross*eyeAcross;
                float hlAlong  = eyeAlong  + 1.0f;
                float hlAcross = eyeAcross - 1.0f;
                hlD2 = hlAlong*hlAlong + hlAcross*hlAcross;

                float browAlong = along - radius * 0.18f;
                float browCenterAcross = 0.62f * radius - browAlong * 0.04f;
                float browAcrossOffset = across - browCenterAcross;
                browVal = (browAlong * browAlong) / ((radius * 0.30f) * (radius * 0.30f))
                        + (browAcrossOffset * browAcrossOffset) / (1.8f * 1.8f);

                float nostrilAlong  = along  - radius * 0.90f;
                float nostrilAcross = across - radius * 0.20f;
                nostrilD2 = nostrilAlong*nostrilAlong + nostrilAcross*nostrilAcross;

                onLip = fabsf(across - biteLine) < 1.0f && u > 0.05f && u < 0.95f;
            }

            if (onMouthBorder)
            {
                colour = 2;
            }
            else if (inMouth)
            {
                colour = 8;
            }
            else if (hlD2 < 1.0f)
            {
                colour = 250;
            }
            else if (eyeD2 < 4.0f)
            {
                colour = 4;
            }
            else if (eyeD2 < 9.0f)
            {
                colour = 24;
            }
            else if (browVal < 1.0f)
            {
                colour = 28;
            }
            else if (nostrilD2 < 1.5f)
            {
                colour = 18;
            }
            else if (onLip)
            {
                colour = 22;
            }
            else
            {
                const float SCALE_U = 5.0f;
                const float SCALE_V = 4.0f;
                float uPix = along;
                float vPix = across;
                float vCell = vPix / SCALE_V;
                int vRow = (int)floorf(vCell + 1000.0f) - 1000;
                float vFrac = vCell - (float)vRow;
                float uShift = (vRow & 1) ? 0.5f : 0.0f;
                float uCell = uPix / SCALE_U + uShift;
                float uFrac = uCell - floorf(uCell);
                float diamond = fabsf(uFrac - 0.5f) + fabsf(vFrac - 0.5f);
                float crown = 1.0f - diamond * 2.0f;
                if (crown < 0) crown = 0;
                float edgeDark = (diamond > 0.42f) ? 1.0f : 0.0f;

                float tt = vPix / BODY_THICK_HEAD;
                tt = clampf(tt, -1.0f, 1.0f);
                float roundness = sqrtf(1.0f - tt*tt);

                float lit = 0.5f + 0.5f * tt;
                float shade = 0.30f + 0.50f * lit;
                shade *= 0.55f + 0.45f * roundness;
                shade += 0.30f * crown * roundness;
                shade -= 0.22f * edgeDark;
                shade = clampf(shade, 0.0f, 1.0f);
                int base = 16 + (int)(shade * 207.0f);
                colour = clampi(base, 16, 223);
            }

            dst[row + x] = (unsigned char)colour;
            headMask[row + x] = (unsigned char)snakeIdx;
        }
    }
}

struct Stamp {
    float cx, cy;
    float halfT;
    float taperT;
    float tangent;
    float uOffset;
    float z;
    int   snake;
    int   order;
};

struct HeadInfo {
    float cx, cy;
    float tangent;
    float z;
    int   snake;
};

static void buildSnake(std::vector<Stamp>& stamps, HeadInfo& outHead,
                       int snakeIdx, float rot, float zPhase, float headPhase)
{
    const int STEPS = 700;
    // bite expressed as a fraction of the loop's arc length, so the
    // tail gap is the same visible length regardless of where on the
    // curve we are.
    const float bitePhase = 0.025f;
    float tHead = phaseToT(headPhase);

    CurvePt headPt = evalSnake(tHead, rot, zPhase);
    outHead.cx = headPt.x;
    outHead.cy = headPt.y;
    outHead.tangent = headPt.tangent + PI;
    outHead.z = headPt.z;
    outHead.snake = snakeIdx;

    float arcLen = 0.0f;
    CurvePt prev = headPt;
    for (int i = 0; i <= STEPS; i++)
    {
        float u01 = (float)i / (float)STEPS;
        // Step uniformly in arc-length so body stamps are evenly
        // spaced along the snake (no clustering at the pinches).
        float ph = headPhase + (1.0f - bitePhase) * u01;
        float t = phaseToT(ph);
        CurvePt p = evalSnake(t, rot, zPhase);
        float dx = p.x - prev.x;
        float dy = p.y - prev.y;
        arcLen += sqrtf(dx*dx + dy*dy);
        prev = p;

        float taper;
        if (u01 < 0.75f)
        {
            taper = 1.0f;
        }
        else
        {
            float uu = (u01 - 0.75f) / 0.25f;
            float s = uu * uu * (3.0f - 2.0f * uu);
            taper = 1.0f - s * 0.65f;
        }
        float halfT = BODY_THICK_TAIL +
                      (BODY_THICK_HEAD - BODY_THICK_TAIL) * taper;

        Stamp s;
        s.cx = p.x; s.cy = p.y;
        s.halfT = halfT;
        s.taperT = taper;
        s.tangent = p.tangent;
        s.uOffset = arcLen;
        s.z = p.z;
        s.snake = snakeIdx;
        s.order = i;
        stamps.push_back(s);
    }
}

// Slow the head's apparent advance near the two pinches at p=0.25 and
// p=0.75 of the loop (where the strand turns sharply through the
// center). warp() is monotonic with warp(0)=0, warp(1)=1, but its
// derivative is small near the pinches — so more frames are rendered
// while the head transitions between lobes, smoothing the change in
// orientation that would otherwise read as a teleport.
static inline float warpPhase(float p)
{
    p -= floorf(p);
    // sin(4πp) = 0 at p = 0, 0.25, 0.5, 0.75 (lobe tips and pinches).
    // Derivative is 1 + k·4π·cos(4πp); at pinches cos(4π·0.25)=-1, so
    // the derivative drops to (1 - 4πk) — slow. At lobe tips it goes
    // to (1 + 4πk) — fast. Choose k so the slowdown is noticeable but
    // monotonic (need 1 - 4πk > 0, so k < ~0.08).
    return p + 0.07f * sinf(4.0f * PI * p);
}

void Render(unsigned char *dst, float phase)
{
    memset(dst, 0, 64000);
    static unsigned char headMask[64000];
    memset(headMask, 0xFF, 64000);

    std::vector<Stamp> stamps;
    stamps.reserve(3 * 1024);
    HeadInfo heads[3];

    float warped = warpPhase(phase);

    for (int i = 0; i < 3; i++)
    {
        float rot     = (TWO_PI / 3.0f) * (float)i;
        float zPhase  = (TWO_PI / 3.0f) * (float)i;
        snakeRot[i] = rot;
        buildSnake(stamps, heads[i], i, rot, zPhase, warped);
    }

    std::sort(stamps.begin(), stamps.end(),
              [](const Stamp& a, const Stamp& b) {
                  if (a.z != b.z) return a.z < b.z;
                  if (a.snake != b.snake) return a.snake < b.snake;
                  return a.order < b.order;
              });

    // Determine the draw order of heads by their depth so we can splice
    // each head into the body-stamp stream at the right z position.
    int headOrder[3] = {0, 1, 2};
    std::sort(headOrder, headOrder + 3,
              [&](int a, int b) { return heads[a].z < heads[b].z; });

    int nextHead = 0;
    for (const Stamp& s : stamps)
    {
        // Flush any heads whose depth is at or below this stamp's, so
        // they sit at their own z in the draw order and get overdrawn
        // by body strands that cross above them.
        while (nextHead < 3 && heads[headOrder[nextHead]].z <= s.z)
        {
            const HeadInfo& h = heads[headOrder[nextHead]];
            drawHead(dst, headMask, h.snake, h.cx, h.cy, 10.5f,
                     h.tangent, true, 0.75f);
            nextHead++;
        }
        stampBody(dst, headMask, s.snake, s.cx, s.cy, s.halfT,
                  s.taperT, s.tangent, s.uOffset);
    }
    while (nextHead < 3)
    {
        const HeadInfo& h = heads[headOrder[nextHead]];
        drawHead(dst, headMask, h.snake, h.cx, h.cy, 10.5f,
                 h.tangent, true, 0.75f);
        nextHead++;
    }
}

static void dumpPPM(const char *path, unsigned char *buf, VGA *v)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n320 200\n255\n");
    for (int i = 0; i < 64000; i++)
    {
        unsigned char idx = buf[i];
        unsigned char rgb[3] = {
            (unsigned char)((int)v->palette[idx][0] * 255 / 63),
            (unsigned char)((int)v->palette[idx][1] * 255 / 63),
            (unsigned char)((int)v->palette[idx][2] * 255 / 63),
        };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char *argv[])
{
    vga = new VGA;

    buildArcLut();

    Shade_Pal(  0,  15,   0, 0, 0,    0, 0, 0 );
    Shade_Pal( 16,  95,   2, 10, 10,   4, 24, 22 );
    Shade_Pal( 96, 175,   4, 24, 22,  14, 44, 38 );
    Shade_Pal(176, 223,  14, 44, 38,  40, 58, 50 );
    Shade_Pal(192, 239,  20, 40, 38,  48, 60, 52 );
    Shade_Pal(240, 255,  60, 50, 30, 63, 63, 55 );

    unsigned char *buf = new unsigned char[64000];
    memset(buf, 0, 64000);

    if (argc > 1 && std::string(argv[1]) == "dump")
    {
        float p0 = 0.0f;
        if (argc > 2) p0 = (float)atof(argv[2]);
        Render(buf, p0);
        dumpPPM("frame.ppm", buf, vga);
        std::cout << "wrote frame.ppm at phase=" << p0 << std::endl;
        delete vga;
        delete [] buf;
        return 0;
    }

    timer = new TIMER;
    long long startTime = timer->getCount(), frameCount = 0;

    bool running = true;
    SDL_Event event;
    float phase = 0.0f;
    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN)
                running = false;
        }
        if (!running) break;

        Render(buf, phase);
        memcpy(vga->page_draw, buf, 63040);
        vga->Update();

        phase -= 0.0016f;
        if (phase < 0.0f) phase += 1.0f;
        frameCount++;
    }

    long long totalTime = timer->getCount() - startTime;
    delete timer;
    float FPS = (float)(frameCount) * 1193180.0f / (float)(totalTime);

    delete vga;
    delete [] buf;

    std::cout << "OpenAI Snake — based on The Art of Demomaking by Alex J. Champandard" << std::endl;
    std::cout << FPS << " frames per second." << std::endl;
    return 0;
}
