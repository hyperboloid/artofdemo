/*
 *                          The Art Of
 *                      D E M O M A K I N G
 *
 *                     by Alex J. Champandard
 *                          Base Sixteen
 *
 *                http://www.flipcode.com/demomaking
 *
 *  Ouroboros — a serpent eating its own tail.
 *
 *  Approach: rasterize the snake by walking a parametric centerline
 *  (a circle in the plane) from head to tail and stamping a circular
 *  cross-section at each step. The cross-section radius tapers along
 *  the length so the body is thick near the head and pinches to a
 *  point at the tail. The head is drawn as a separate wedge/triangle
 *  shape whose open jaws bite over the tail tip.
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

static const float PI = 3.14159265358979f;
static const float TWO_PI = 6.28318530717958f;

// Screen / ring geometry
static const float CX = 160.0f;
static const float CY = 100.0f;
static const float R_RING = 65.0f;

// How thick the body is at the head end (near the head), and how
// quickly it tapers toward the tail.
static const float BODY_THICK_HEAD = 14.0f;
static const float BODY_THICK_TAIL = 1.5f;

// The body covers angle from THETA_BODY_START (at the head end) around
// the long way to THETA_BODY_END (at the tail tip). Angles measured CCW
// from +x; the head sits near theta=0 and we go CCW (positive theta)
// for one full loop minus the head wedge.
//
// In screen space, +x is right and +y is DOWN, so "CCW in math" is
// "CW visually". This is fine — the snake just appears to wrap one way.
//
// Head occupies a small wedge near theta=0. Body starts just past it
// and continues for nearly a full revolution; tail tip ends slightly
// short of theta=0 so it enters the mouth.
static const float HEAD_WEDGE = 0.55f;          // ~32° wide head footprint
static const float BODY_ARC   = TWO_PI - HEAD_WEDGE * 0.6f; // body length in radians
static const float TAIL_OVERLAP = 0.30f;        // tail tip enters head's mouth

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
 * Stamp a body segment at (cx, cy) with the given half-thickness, scale
 * pattern offset s (arc length used to drive the scale stripes), and
 * radial unit vector (nx, ny) (pointing outward from ring center, used
 * for cross-section shading).
 *
 * For each pixel within the disk we compute:
 *   t = (pixel - center) . (nx, ny)      // signed radial offset
 *   so t = -halfT (inner side) .. +halfT (outer side)
 * and use t to do tube shading; the scale stripes are driven by `s`.
 */
static void stampBody(unsigned char *dst, float cx, float cy, float halfT,
                      float s, float taperT, float scaleUOffset)
{
    (void)s;
    if (halfT < 0.5f) return;
    int x0 = (int)floorf(cx - halfT) - 1;
    int x1 = (int)ceilf (cx + halfT) + 1;
    int y0 = (int)floorf(cy - halfT) - 1;
    int y1 = (int)ceilf (cy + halfT) + 1;
    if (x0 < 0) x0 = 0; if (x1 > 319) x1 = 319;
    if (y0 < 0) y0 = 0; if (y1 > 199) y1 = 199;

    // outward radial unit vector at this centerline point
    float rlen = sqrtf((cx - CX)*(cx - CX) + (cy - CY)*(cy - CY));
    float nx = 0, ny = 0;
    if (rlen > 1e-4f) { nx = (cx - CX) / rlen; ny = (cy - CY) / rlen; }

    float h2 = halfT * halfT;

    // Diamond scale tessellation, woodcut style. The body is parameterized
    // by (u, v) where u = arc-length along the body and v = signed offset
    // across the tube in pixels. We tile a diamond/rhombus lattice in
    // (u, v) and shade each scale: bright crown at the center, dark
    // crevice at the edges, with a row offset every other v-row so
    // scales overlap like roof tiles.
    const float SCALE_U = 5.0f;   // along-body scale spacing (px)
    const float SCALE_V = 4.0f;   // across-body scale spacing (px)

    for (int y = y0; y <= y1; y++)
    {
        float dy = (float)y - cy;
        float dy2 = dy * dy;
        int row = y * 320;
        for (int x = x0; x <= x1; x++)
        {
            float dx = (float)x - cx;
            float d2 = dx*dx + dy2;
            if (d2 > h2) continue;

            // signed radial-across coordinate (pixels, then normalized)
            float vPix = dx * nx + dy * ny;
            float t = vPix / halfT;
            t = clampf(t, -1.0f, 1.0f);
            // cross-section "roundness" (1 at center of tube, 0 at edges)
            float across = sqrtf(1.0f - t*t);

            // Per-pixel arc-length: use the pixel's own angle around the
            // ring center, NOT the stamp center's `s`. Subtract scaleUOffset
            // (driven by phase) so the scale pattern travels WITH the snake
            // instead of staying fixed in screen space.
            float pxAngle = atan2f((float)y - CY, (float)x - CX);
            float uPix = pxAngle * R_RING - scaleUOffset;

            // Scale lattice coordinates.
            float vCell = vPix / SCALE_V;
            int vRow = (int)floorf(vCell + 1000.0f) - 1000;
            float vFrac = vCell - (float)vRow; // 0..1 within the row
            // every other row is shifted by half a u-cell
            float uShift = (vRow & 1) ? 0.5f : 0.0f;
            float uCell = uPix / SCALE_U + uShift;
            int uIdx = (int)floorf(uCell + 1000.0f) - 1000;
            float uFrac = uCell - (float)uIdx; // 0..1 within the cell

            // Diamond distance from center of cell (0 at center, 1 at corner)
            float du = uFrac - 0.5f;
            float dv = vFrac - 0.5f;
            float diamond = fabsf(du) + fabsf(dv); // 0..1 (corner at 1)

            // Scale shading: bright crown, dark edge.
            // crown = 1 at scale center, 0 at scale boundary
            float crown = 1.0f - diamond * 2.0f;
            if (crown < 0) crown = 0;
            // small dark gap right at the boundary
            float edgeDark = (diamond > 0.42f) ? 1.0f : 0.0f;

            // tube shading: lit on inner side (toward ring center)
            float lit = 0.5f - 0.5f * t;

            // combine
            float shade = 0.30f + 0.50f * lit;          // base tube light
            shade *= 0.55f + 0.45f * across;            // round falloff
            shade += 0.30f * crown * across;            // scale highlight
            shade -= 0.22f * edgeDark;                  // scale crevice
            shade *= 0.55f + 0.45f * taperT;            // tail darkening
            shade = clampf(shade, 0.0f, 1.0f);

            int colour = 16 + (int)(shade * 207.0f);
            colour = clampi(colour, 16, 223);
            unsigned char c = (unsigned char)colour;
            if (c > dst[row + x]) dst[row + x] = c;
        }
    }
}

/*
 * Draw a filled disk in amber palette range, with eye markings on the
 * outer-upper side. Used for the head.
 */
/*
 * Width profile for a snake-head outline. Returns half-width at
 * position u (0..1, where 0 = back of head, 1 = snout tip).
 * Peaks near u=0.25 (the cheeks) and tapers to 0 at the snout tip.
 */
static inline float headProfile(float u)
{
    if (u < 0.0f || u > 1.0f) return 0.0f;
    // Two-stage curve: starts at 1.0 at the back (matching the body
    // width) so the head merges seamlessly into the body, swells very
    // slightly at the cheeks (u=0.30), then tapers to a rounded snout.
    if (u < 0.30f)
    {
        float t = u / 0.30f;
        return 1.0f + 0.05f * t * t;
    }
    else
    {
        float t = (u - 0.30f) / 0.70f;
        float s = t * t * (3.0f - 2.0f * t);
        return 1.05f * (1.0f - s) + 0.32f * s;
    }
}

static void drawHead(unsigned char *dst, float cx, float cy, float radius,
                     float fwdX, float fwdY, float outX, float outY,
                     bool drawMouthCutout, float jawOpen, float scaleUOffset)
{
    // Snake head built from a single outline that's split horizontally
    // by the bite-line into an upper jaw (across >= biteLine) and a
    // lower jaw (across < biteLine). The lower jaw rotates around a
    // hinge at the back of the head to open the mouth.
    //
    // Local frame:
    //   along  = forward (+) / backward (-) along the head's axis
    //   across = outward from ring center (+) / inward (-)
    // The snout sits on the bite-line, lifted slightly upward.

    const float headLen = radius * 2.0f;     // total length, back to snout
    const float backAlong = -radius * 0.7f;  // where the back of the head sits
    const float biteLine  = -radius * 0.05f; // across value of the closed mouth (slightly inside ring centerline)

    // bounding box (a bit generous to allow for jaw rotation)
    int bx0 = (int)floorf(cx - headLen) - 2;
    int bx1 = (int)ceilf (cx + headLen) + 2;
    int by0 = (int)floorf(cy - headLen) - 2;
    int by1 = (int)ceilf (cy + headLen) + 2;
    if (bx0 < 0) bx0 = 0; if (bx1 > 319) bx1 = 319;
    if (by0 < 0) by0 = 0; if (by1 > 199) by1 = 199;

    // Hinge for lower jaw, moved forward from the back so the rear of
    // the lower jaw stays flush with the body (no diagonal notch where
    // the jaw rotation cuts away from the body silhouette).
    const float hingeAlong  = backAlong + radius * 0.55f;
    const float hingeAcross = biteLine - radius * 0.05f;
    float jawAngle = jawOpen * 0.55f;
    float cosJaw = cosf(jawAngle);
    float sinJaw = sinf(jawAngle);

    // Forward direction at the head, in ring-frame terms: +theta_ring.
    // We'll use ring-curved coordinates so the head follows the same
    // curvature as the body. For each pixel:
    //   along  = (theta_at_pixel - thetaHead) * R_RING  (arc length forward)
    //   across = (distance_from_ring_center - R_RING)   (radial offset)
    // This naturally curves the head around the ring just like the body.
    float thetaHeadLocal = atan2f(cy - CY, cx - CX); // same as thetaHead

    for (int y = by0; y <= by1; y++)
    {
        float dyR = (float)y - CY;
        int row = y * 320;
        for (int x = bx0; x <= bx1; x++)
        {
            float dxR = (float)x - CX;
            float distR = sqrtf(dxR*dxR + dyR*dyR);
            float thetaPx = atan2f(dyR, dxR);
            // wrap delta into (-pi, pi]
            float dTheta = thetaPx - thetaHeadLocal;
            while (dTheta >  PI) dTheta -= TWO_PI;
            while (dTheta < -PI) dTheta += TWO_PI;
            float along  = dTheta * R_RING;
            float across = distR - R_RING;

            // u parameter along the head outline, 0 at back, 1 at snout tip
            float u = (along - backAlong) / headLen;

            bool inUpper = false;
            if (u >= 0.0f && u <= 1.0f && across >= biteLine)
            {
                float halfW = radius * 0.69f * headProfile(u);
                // upper jaw outline: above bite-line, up to halfW high.
                float top = biteLine + halfW;
                if (across <= top) inUpper = true;
            }

            bool inLower = false;
            // Transform pixel into lower-jaw local frame: undo the jaw
            // rotation around (hingeAlong, hingeAcross).
            float lA_save = 0.0f, lC_save = 0.0f;
            float uL_save = -1.0f;
            {
                float pA = along  - hingeAlong;
                float pC = across - hingeAcross;
                float lA =  cosJaw * pA - sinJaw * pC;
                float lC =  sinJaw * pA + cosJaw * pC;
                lA += hingeAlong;
                lC += hingeAcross;
                lA_save = lA; lC_save = lC;
                float uL = (lA - backAlong) / headLen;
                uL_save = uL;
                if (uL >= 0.0f && uL <= 0.95f && lC <= biteLine)
                {
                    float halfW = radius * 0.59f * headProfile(uL);
                    float bot = biteLine - halfW;
                    if (lC >= bot) inLower = true;
                }
            }

            // Mouth interior: the wedge between the upper jaw's bite-line
            // and the rotated lower jaw's top. Drawn as a dark fill so
            // the open mouth looks like an actual cavity, not a gap.
            bool inMouth = false;
            if (!inUpper && !inLower && drawMouthCutout && jawOpen > 0.05f)
            {
                // Must be below the upper jaw (across < biteLine area but
                // above the lower jaw's rotated top edge).
                if (u >= 0.05f && u <= 0.95f && across < biteLine)
                {
                    // And on the lower-jaw side of the hinge rotation —
                    // i.e. lC_save (the de-rotated across) should be
                    // ABOVE the lower jaw's top in its local frame
                    // (because the actual jaw has rotated away).
                    if (uL_save >= 0.0f && uL_save <= 0.95f &&
                        lC_save >= biteLine - 0.5f)
                    {
                        inMouth = true;
                    }
                }
            }

            if (!inUpper && !inLower && !inMouth) continue;

            // ---- shading ----
            int colour;

            // Eye: smaller, with a dark iris and a tiny bright highlight.
            float eyeAlong  = along  - radius * 0.15f;
            float eyeAcross = across - radius * 0.40f;
            float eyeD2 = eyeAlong*eyeAlong + eyeAcross*eyeAcross;
            // Highlight is a single pixel offset upper-back of the pupil.
            float hlAlong  = eyeAlong  + 1.0f;
            float hlAcross = eyeAcross - 1.0f;
            float hlD2 = hlAlong*hlAlong + hlAcross*hlAcross;

            if (inMouth)
            {
                colour = 8; // dark mouth cavity
            }
            else if (inUpper && hlD2 < 1.0f)
            {
                colour = 250; // small bright catchlight
            }
            else if (inUpper && eyeD2 < 5.0f)
            {
                colour = 4; // dark pupil/iris
            }
            else if (inUpper && eyeD2 < 8.0f)
            {
                colour = 30; // eye-socket shadow ring
            }
            else
            {
                // Use EXACTLY the same shading formula as the body so
                // the head reads as part of the same tube. The body's
                // tube shading depends on `t` (signed across) and
                // `roundness` (cross-section curvature). We derive both
                // from the head's local geometry below.
                float pxAngle = atan2f((float)y - CY, (float)x - CX);
                float uPix = pxAngle * R_RING - scaleUOffset;
                float vPix = sqrtf(((float)x - CX)*((float)x - CX) +
                                   ((float)y - CY)*((float)y - CY)) - R_RING;
                const float SCALE_U = 5.0f;
                const float SCALE_V = 4.0f;
                float vCell = vPix / SCALE_V;
                int vRow = (int)floorf(vCell + 1000.0f) - 1000;
                float vFrac = vCell - (float)vRow;
                float uShift = (vRow & 1) ? 0.5f : 0.0f;
                float uCell = uPix / SCALE_U + uShift;
                int uIdx = (int)floorf(uCell + 1000.0f) - 1000;
                float uFrac = uCell - (float)uIdx;
                (void)uIdx;
                float diamond = fabsf(uFrac - 0.5f) + fabsf(vFrac - 0.5f);
                float crown = 1.0f - diamond * 2.0f;
                if (crown < 0) crown = 0;
                float edgeDark = (diamond > 0.42f) ? 1.0f : 0.0f;

                // Match the body's tube shading. Compute the head's
                // local half-thickness at this pixel so we can normalize
                // `across` to [-1, +1] just like the body does.
                float halfT_local;
                if (inUpper)
                {
                    halfT_local = radius * 0.69f * headProfile(u);
                }
                else
                {
                    // For lower jaw use its rotated u (uL_save)
                    float ul = uL_save;
                    if (ul < 0.0f) ul = 0.0f; if (ul > 1.0f) ul = 1.0f;
                    halfT_local = radius * 0.59f * headProfile(ul);
                }
                if (halfT_local < 1.0f) halfT_local = 1.0f;
                // Signed across normalized to -1..+1 within the head's
                // local cross-section. Re-center on the bite-line so it
                // straddles 0 like the body's tube does.
                float crossLocal = inUpper ? (across - biteLine)
                                           : (lC_save - biteLine);
                float t = crossLocal / halfT_local;
                t = clampf(t, -1.0f, 1.0f);
                float roundness = sqrtf(1.0f - t*t);

                // Now the IDENTICAL body shading formula:
                float lit = 0.5f - 0.5f * t;        // inner-lit
                float shade = 0.30f + 0.50f * lit;
                shade *= 0.55f + 0.45f * roundness;
                shade += 0.30f * crown * roundness;
                shade -= 0.22f * edgeDark;
                // No taperT term — head is full thickness.
                shade = clampf(shade, 0.0f, 1.0f);
                int base = 16 + (int)(shade * 207.0f);
                colour = clampi(base, 16, 223);
            }

            if ((unsigned char)colour > dst[row + x])
                dst[row + x] = (unsigned char)colour;
        }
    }

    // Fangs: two triangular teeth hanging from the upper jaw, near the
    // front. Each fang is a small filled triangle, 2 px wide at the top
    // and tapering to a point.
    if (drawMouthCutout && jawOpen > 0.15f)
    {
        for (int side = -1; side <= 1; side += 2)
        {
            float fAlong0 = radius * 1.00f; // upper jaw side, near the tip
            float fAlong1 = radius * 1.16f;
            float fAcrossTop = biteLine - 0.5f;
            float fAcrossBot = biteLine - (3.5f + 2.0f * jawOpen);
            int fyTop;
            // Rasterize each fang as a small filled triangle by scanning
            // along the fang axis and writing a 1-2 px tooth.
            int steps = 6;
            for (int step = 0; step <= steps; step++)
            {
                float u = (float)step / (float)steps;
                float a = fAlong0 + (fAlong1 - fAlong0) * u;
                float c = fAcrossTop + (fAcrossBot - fAcrossTop) * u;
                // width tapers from 1.4 px at top to 0 at point
                int halfW = (u < 0.6f) ? 1 : 0;
                for (int w = -halfW; w <= halfW; w++)
                {
                    int px = (int)(cx + fwdX * a + outX * c) + (int)(-fwdY * w) * side;
                    int py = (int)(cy + fwdY * a + outY * c) + (int)( fwdX * w) * side;
                    (void)fyTop;
                    if (px >= 0 && px <= 319 && py >= 0 && py <= 199)
                    {
                        dst[py * 320 + px] = 252;
                    }
                }
            }
        }
    }
}

/*
 * Forked tongue: two short line segments emerging from the mouth.
 */
static void drawTongue(unsigned char *dst, float cx, float cy,
                       float fwdX, float fwdY, float outX, float outY,
                       float phase, float jawOpen)
{
    // Tongue only emerges when mouth is at least partly open. Below
    // this threshold the tongue is retracted (not drawn).
    if (jawOpen < 0.25f) return;
    float ext = clampf((jawOpen - 0.25f) / 0.75f, 0.0f, 1.0f);
    float wig = sinf(phase * 4.0f) * 0.25f;
    float baseAlong = 14.0f;
    float tipAlong  = baseAlong + 12.0f * ext;
    float forkSpread = (3.5f + sinf(phase * 6.0f) * 0.5f) * ext;
    // base point
    float bx = cx + fwdX * baseAlong;
    float by = cy + fwdY * baseAlong;
    // tip points (two forks)
    for (int side = -1; side <= 1; side += 2)
    {
        float tAcross = side * forkSpread + wig * side;
        float tx = cx + fwdX * tipAlong + outX * tAcross;
        float ty = cy + fwdY * tipAlong + outY * tAcross;
        // rasterize a short line from (bx,by) to (tx,ty)
        int steps = 12;
        for (int i = 0; i <= steps; i++)
        {
            float u = (float)i / (float)steps;
            int px = (int)(bx + (tx - bx) * u);
            int py = (int)(by + (ty - by) * u);
            if (px < 0 || px > 319 || py < 0 || py > 199) continue;
            // red-pink tongue: reuse high amber range with a bright tip
            unsigned char c = (i > steps - 3) ? 248 : 230;
            if (c > dst[py * 320 + px]) dst[py * 320 + px] = c;
        }
    }
}

/*
 * Render one frame. We walk the snake from head (theta near 0, going CCW)
 * along the ring centerline, stamping disks of decreasing radius until
 * we reach the tail tip.
 *
 * The head is drawn last so it sits on top of the tail tip (biting).
 */
void Render(unsigned char *dst, float phase)
{
    memset(dst, 0, 64000);

    // Place the head: centerline theta = phase (so the snake rotates).
    float thetaHead = phase;
    // Forward direction at the head = tangent to ring, pointing in the
    // direction the body extends FROM (so the mouth opens toward where
    // the tail tip will arrive).
    // The body extends CCW (increasing theta). The tail comes back
    // around to just BEFORE thetaHead, so the head's mouth should
    // point in the -CCW tangent direction (i.e. toward smaller theta).
    float cosH = cosf(thetaHead), sinH = sinf(thetaHead);
    float headCenterlineX = CX + R_RING * cosH;
    float headCenterlineY = CY + R_RING * sinH;
    // outward radial direction at head:
    float outHX = cosH;
    float outHY = sinH;
    // tangent (CCW direction = +theta direction):
    float tgHX = -sinH;
    float tgHY =  cosH;
    // The head sits ON the body centerline at exactly thetaHead with
    // no offset, so the head's coordinate frame matches the body's
    // radial frame at the join. The body ends slightly past thetaHead
    // (overlapping with the back of the head) so the seam is hidden.
    float headR = 22.0f;
    float headCX = headCenterlineX;
    float headCY = headCenterlineY;
    // Head forward direction = +tangent (direction of motion)
    float fwdHX = tgHX;
    float fwdHY = tgHY;

    // Walk the body centerline. Body extends from inside the back of
    // the head (so the seam is buried under head pixels) all the way
    // around to just past the head, where the tail tip enters the mouth.
    int steps = 720; // plenty of overlap for a continuous body
    float thetaStart = thetaHead - 0.18f;
    float thetaEnd   = thetaHead - TWO_PI + TAIL_OVERLAP;
    float dTheta = (thetaEnd - thetaStart) / (float)steps;

    // Scale UV offset: scales rotate WITH the snake so they appear
    // attached to the body rather than to screen space.
    float scaleUOffset = phase * R_RING;

    for (int i = 0; i <= steps; i++)
    {
        float t01 = (float)i / (float)steps; // 0 at head end, 1 at tail tip
        float th = thetaStart + dTheta * (float)i;
        float cx = CX + R_RING * cosf(th);
        float cy = CY + R_RING * sinf(th);

        // taper: full thickness at head, pinches to tail tip.
        // Use a smoothstep-ish curve so it stays thick most of the body
        // and tapers more sharply near the tip.
        float taper;
        if (t01 < 0.7f)
        {
            // mostly full body
            float u = t01 / 0.7f;
            taper = 1.0f - 0.25f * u; // 1.0 -> 0.75
        }
        else
        {
            float u = (t01 - 0.7f) / 0.3f; // 0..1 along the last 30%
            u = clampf(u, 0.0f, 1.0f);
            // smooth pinch
            float s = u * u * (3.0f - 2.0f * u);
            taper = 0.75f * (1.0f - s);
        }
        float halfT = BODY_THICK_TAIL + (BODY_THICK_HEAD - BODY_THICK_TAIL) * taper;
        float arcS  = (th - thetaStart) * R_RING;
        stampBody(dst, cx, cy, halfT, arcS, taper, scaleUOffset);
    }

    // Jaws held open at a fixed opening.
    float jawOpen = 0.75f;

    // Head goes on top of the tail tip
    drawHead(dst, headCX, headCY, headR, fwdHX, fwdHY, outHX, outHY, true, jawOpen, scaleUOffset);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    vga = new VGA;

    // 0..15 background black
    Shade_Pal(  0,  15,   0, 0, 0,    0, 0, 0 );
    // 16..95 deep green
    Shade_Pal( 16,  95,   2, 8, 2,    6, 28, 6 );
    // 96..175 mid green -> bright green
    Shade_Pal( 96, 175,   6, 28, 6,  18, 50, 14 );
    // 176..223 bright green -> highlight yellow-green
    Shade_Pal(176, 223,  18, 50, 14, 45, 60, 28 );
    // 192..239 head amber (overlaps 192..223 slightly — last write wins)
    Shade_Pal(192, 239,  25, 32, 8,  58, 45, 12 );
    // 240..255 eye & tongue highlight
    Shade_Pal(240, 255,  60, 30, 30, 63, 63, 55 );

    unsigned char *buf = new unsigned char[64000];
    memset(buf, 0, 64000);

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

        phase += 0.010f;
        if (phase > TWO_PI) phase -= TWO_PI;
        frameCount++;
    }

    long long totalTime = timer->getCount() - startTime;
    delete timer;
    float FPS = (float)(frameCount) * 1193180.0f / (float)(totalTime);

    delete vga;
    delete [] buf;

    std::cout << "Ouroboros — based on The Art of Demomaking by Alex J. Champandard" << std::endl;
    std::cout << FPS << " frames per second." << std::endl;
    return 0;
}
