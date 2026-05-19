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

// Angles measured CCW from +x. In screen space +y is DOWN, so "CCW in
// math" appears "CW visually" — fine, the snake just wraps one way.
// Body extends MORE than one revolution; the tail tip wraps past the
// head's leading edge so it enters the mouth.
static const float TAIL_OVERLAP = -0.30f;

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
 * Stamp a body segment as a filled disk at (cx, cy) with the given
 * half-thickness. Per-pixel shading uses the pixel's own polar position
 * around the ring center (CX, CY) — not the stamp's local frame — so
 * overlapping stamps all produce the same color at a given pixel.
 */
static void stampBody(unsigned char *dst, float cx, float cy, float halfT,
                      float taperT, float thetaLead, bool applyLeadingCull)
{
    if (halfT < 0.5f) return;
    int x0 = (int)floorf(cx - halfT) - 1;
    int x1 = (int)ceilf (cx + halfT) + 1;
    int y0 = (int)floorf(cy - halfT) - 1;
    int y1 = (int)ceilf (cy + halfT) + 1;
    if (x0 < 0) x0 = 0; if (x1 > 319) x1 = 319;
    if (y0 < 0) y0 = 0; if (y1 > 199) y1 = 199;

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

            // Cull pixels forward of the body's leading edge so the body
            // ends in a flat radial line instead of a rounded half-disk.
            // Only applied for stamps near thetaLead (the first few
            // stamps); the tail-side stamps must NOT be culled here even
            // though their pixels are technically "forward" of thetaLead
            // in wrapped angle.
            if (applyLeadingCull)
            {
                float thetaPx = atan2f((float)y - CY, (float)x - CX);
                float dT = thetaPx - thetaLead;
                while (dT >  PI) dT -= TWO_PI;
                while (dT < -PI) dT += TWO_PI;
                if (dT > 0.0f) continue;
            }

            // signed radial-across coordinate, measured from the true
            // ring center (CX, CY). Using the global radial — not the
            // stamp's local radial — keeps each pixel's vPix constant
            // across all overlapping stamps, so the max-blended result
            // doesn't artificially brighten pixels that happen to be
            // near multiple stamps. This way the head, which computes
            // vPix the same way, gets the same brightness.
            float distR = sqrtf(((float)x - CX)*((float)x - CX) +
                                ((float)y - CY)*((float)y - CY));
            float vPix = distR - R_RING;
            float t = vPix / halfT;
            t = clampf(t, -1.0f, 1.0f);
            // cross-section "roundness" (1 at center of tube, 0 at edges)
            float across = sqrtf(1.0f - t*t);

            // Per-pixel arc-length: use the pixel's own angle around the
            // ring center, NOT the stamp center's `s`. Without this, every
            // pixel in one disk stamp shares a single u and we just get
            // horizontal bands instead of a 2D scale grid.
            float pxAngle = atan2f((float)y - CY, (float)x - CX);
            float uPix = pxAngle * R_RING; // arc length along the ring

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
    // Three-stage curve to match the reference head silhouette:
    //   0..0.30  rise from body-width (1.0) to slightly-wider cheek (1.05)
    //   0.30..0.85  ease taper from cheek toward the snout (still fat)
    //   0.85..1.0  sharp taper to a pointed/angular snout tip
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
        // sharp angular taper at the snout tip
        float t = (u - 0.85f) / 0.15f;
        return 0.55f * (1.0f - t) + 0.18f * t;
    }
}

static void drawHead(unsigned char *dst, float cx, float cy, float radius,
                     bool drawMouthCutout, float jawOpen)
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

    // Hinge for the mouth-wedge cutout, forward of the back of the head
    // so the rear of the lower jaw stays flush with the body.
    const float hingeAlong = backAlong + radius * 0.55f;

    // Forward direction at the head, in ring-frame terms: +theta_ring.
    // We'll use ring-curved coordinates so the head follows the same
    // curvature as the body. For each pixel:
    //   along  = (theta_at_pixel - thetaHead) * R_RING  (arc length forward)
    //   across = (distance_from_ring_center - R_RING)   (radial offset)
    // This naturally curves the head around the ring just like the body.
    // Head's centerline angle around the ring, recovered from (cx, cy).
    float thetaHeadLocal = atan2f(cy - CY, cx - CX);

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

            // Lower jaw: render the full (un-rotated) lower outline of
            // the head, identical in width profile to the upper, so the
            // head's silhouette is a symmetric tube matching the body.
            // Then carve out a mouth wedge to suggest the open mouth.
            // This keeps the head's outer outline smooth where it meets
            // the body (no rotated-jaw seam).
            bool inLower = false;
            if (u >= 0.0f && u <= 1.0f && across < biteLine)
            {
                float halfW = radius * 0.59f * headProfile(u);
                float bot = biteLine - halfW;
                if (across >= bot) inLower = true;
            }

            // Mouth wedge: a triangular cutout extending from the hinge
            // forward to the snout tip. Its half-width below the bite
            // line grows linearly with along-from-hinge and with jawOpen.
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
                    inLower = false; // wedge takes priority over solid jaw
                    // Border: pixels close to either the upper-lip line
                    // (depthIntoMouth ~= 0) or the lower-lip wedge edge
                    // (depthIntoMouth ~= wedgeDepth) get marked so we
                    // can outline the mouth opening in dark.
                    if (depthIntoMouth < 1.2f ||
                        (wedgeDepth - depthIntoMouth) < 1.2f)
                    {
                        onMouthBorder = true;
                    }
                }
            }

            if (!inUpper && !inLower && !inMouth) continue;

            // ---- shading ----
            int colour;

            // Eye / brow / nostril features only apply to the upper jaw.
            // The cascade below guards each branch with inUpper, so skip
            // the feature math entirely for non-upper pixels.
            float eyeD2 = 1e9f, hlD2 = 1e9f, browVal = 1e9f, nostrilD2 = 1e9f;
            bool onLip = false;
            if (inUpper)
            {
                float eyeAlong  = along  - radius * 0.15f;
                float eyeAcross = across - radius * 0.40f;
                eyeD2 = eyeAlong*eyeAlong + eyeAcross*eyeAcross;
                // Highlight is a single pixel offset upper-back of the pupil.
                float hlAlong  = eyeAlong  + 1.0f;
                float hlAcross = eyeAcross - 1.0f;
                hlD2 = hlAlong*hlAlong + hlAcross*hlAcross;

                // Brow ridge: thick dark band above the eye, an ellipse
                // whose center slopes down with along — high-back to
                // low-front (over the eye).
                float browAlong = along - radius * 0.18f;
                float browCenterAcross = 0.62f * radius - browAlong * 0.04f;
                float browAcrossOffset = across - browCenterAcross;
                // Half-length 0.30*r along, half-thickness 1.8 px across.
                browVal = (browAlong * browAlong) / ((radius * 0.30f) * (radius * 0.30f))
                        + (browAcrossOffset * browAcrossOffset) / (1.8f * 1.8f);

                // Nostril: small dark dot near the snout tip.
                float nostrilAlong  = along  - radius * 0.90f;
                float nostrilAcross = across - radius * 0.20f;
                nostrilD2 = nostrilAlong*nostrilAlong + nostrilAcross*nostrilAcross;

                // Lip crease: dark pixels along the bite-line (not in
                // front of the open mouth wedge).
                onLip = fabsf(across - biteLine) < 1.0f && u > 0.05f && u < 0.95f;
            }

            if (onMouthBorder)
            {
                colour = 2; // very dark lip outline so it shows over the tail
            }
            else if (inMouth)
            {
                colour = 8; // dark mouth cavity
            }
            else if (hlD2 < 1.0f)
            {
                colour = 250; // small bright catchlight
            }
            else if (eyeD2 < 4.0f)
            {
                colour = 4; // dark pupil
            }
            else if (eyeD2 < 9.0f)
            {
                colour = 24; // deep eye-socket shadow
            }
            else if (browVal < 1.0f)
            {
                colour = 28; // brow ridge — dark angled band
            }
            else if (nostrilD2 < 1.5f)
            {
                colour = 18; // nostril
            }
            else if (onLip)
            {
                colour = 22; // lip crease along bite-line
            }
            else
            {
                // Use EXACTLY the same shading formula as the body so
                // the head reads as part of the same tube. Reuse thetaPx
                // and across (== distR - R_RING) already in scope.
                const float SCALE_U = 5.0f;
                const float SCALE_V = 4.0f;
                float uPix = thetaPx * R_RING;
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

                // Use the body's centerline (across=0, i.e. R_RING) as
                // the shading axis, exactly like the body does. This
                // way the bright middle of the tube falls on the same
                // line for head and body, so the scale highlights have
                // matching density and brightness.
                float t = vPix / BODY_THICK_HEAD;
                t = clampf(t, -1.0f, 1.0f);
                float roundness = sqrtf(1.0f - t*t);

                float lit = 0.5f - 0.5f * t;
                float shade = 0.30f + 0.50f * lit;
                shade *= 0.55f + 0.45f * roundness;
                shade += 0.30f * crown * roundness;
                shade -= 0.22f * edgeDark;
                shade = clampf(shade, 0.0f, 1.0f);
                int base = 16 + (int)(shade * 207.0f);
                colour = clampi(base, 16, 223);
            }

            // Mouth border writes unconditionally so the dark lip
            // outline is visible over the tail going through the
            // mouth. Everything else uses max-blend so head and body
            // merge smoothly at the join.
            if (onMouthBorder)
            {
                dst[row + x] = (unsigned char)colour;
            }
            else if ((unsigned char)colour > dst[row + x])
            {
                dst[row + x] = (unsigned char)colour;
            }
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

    // Head centerline angle drives the rotation. The head sits ON the
    // body centerline at exactly thetaHead so head and body share a
    // radial frame at the join; the body extends slightly past thetaHead
    // (overlapping the back of the head) so the seam is hidden.
    float thetaHead = phase;
    float headCX = CX + R_RING * cosf(thetaHead);
    float headCY = CY + R_RING * sinf(thetaHead);
    float headR  = 22.0f;

    // Walk the body centerline. Body extends from inside the back of
    // the head (so the seam is buried under head pixels) all the way
    // around to just past the head, where the tail tip enters the mouth.
    int steps = 720; // plenty of overlap for a continuous body
    float thetaStart = thetaHead - 0.18f;
    float thetaEnd   = thetaHead - TWO_PI + TAIL_OVERLAP;
    float dTheta = (thetaEnd - thetaStart) / (float)steps;

    for (int i = 0; i <= steps; i++)
    {
        float t01 = (float)i / (float)steps; // 0 at head end, 1 at tail tip
        float th = thetaStart + dTheta * (float)i;
        float cx = CX + R_RING * cosf(th);
        float cy = CY + R_RING * sinf(th);

        // taper: full thickness at head, pinches to tail tip.
        // Use a smoothstep-ish curve so it stays thick most of the body
        // and tapers more sharply near the tip.
        // Body tapers to a clear tail shape in the last ~25%, so the
        // tail entering the mouth reads as an actual tail (thinner
        // than the body) rather than a uniform tube.
        float taper;
        if (t01 < 0.75f)
        {
            taper = 1.0f;
        }
        else
        {
            float u = (t01 - 0.75f) / 0.25f;
            u = clampf(u, 0.0f, 1.0f);
            float s = u * u * (3.0f - 2.0f * u);
            // Don't fully pinch — leave tail at ~40% so it's visible
            // going into the mouth.
            taper = 1.0f - s * 0.60f;
        }
        float halfT = BODY_THICK_TAIL + (BODY_THICK_HEAD - BODY_THICK_TAIL) * taper;
        // Only the first 30 stamps near the leading edge need the
        // forward-cull (to flatten the body's start). Beyond that the
        // cull would incorrectly clip the tail tip on the other side.
        bool applyCull = (i < 30);
        stampBody(dst, cx, cy, halfT, taper, thetaStart, applyCull);
    }

    // Jaws held open at a fixed opening.
    float jawOpen = 0.75f;

    // Head goes on top of the tail tip
    drawHead(dst, headCX, headCY, headR, true, jawOpen);
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
