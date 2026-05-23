/* Slow 8-bar minor loop: Am - F - C - G, each chord held 2 bars.
 *
 * Epic & low: melody pitched an octave below the previous arrangement,
 * notes sustained as half-notes (2 beats each) with no rests between,
 * bass and mid drone through each 2-bar chord region.
 */

#include "sauron_music.h"
#include "music.h"   /* note macros (A3, C4, E4, ...) */

uint16_t sauron_song_melody[SAURON_LOOP_TICKS];
uint16_t sauron_song_mid   [SAURON_LOOP_TICKS];
uint16_t sauron_song_bass  [SAURON_LOOP_TICKS];

#define REST 1
#define HALF_TICKS (SAURON_TICKS_PER_BEAT * 2)  /* half-note duration */
#define BAR_TICKS  (SAURON_TICKS_PER_BEAT * SAURON_BEATS_PER_BAR)

/* Place `count` consecutive ticks of value `v` into dst from *cursor. */
static void hold(uint16_t *dst, int *cursor, uint16_t v, int count)
{
    for (int i = 0; i < count; i++) dst[(*cursor)++] = v;
}

/* Place a sustained note: 1-tick attack-rest so successive different
 * notes get a clean strike, then the note value for the remaining
 * ticks. If two adjacent notes are identical the attack-rest still
 * gives them separation; for fully continuous drones use hold() directly. */
static void note(uint16_t *dst, int *cursor, uint16_t n, int ticks)
{
    if (n == REST) {
        hold(dst, cursor, REST, ticks);
        return;
    }
    hold(dst, cursor, REST, 1);
    hold(dst, cursor, n, ticks - 1);
}

void sauron_music_build(void)
{
    /* --- Melody (top voice) ---
     * Slow descending/ascending chord-tone lines, two half-notes per bar.
     * Pitched in the A3-E4 range so it sits in the same register as the
     * mid voice and reads as somber rather than busy. */
    int c = 0;

    /* Bars 1-2 (Am): A3 - C4 | E4 - C4 */
    note(sauron_song_melody, &c, A3, HALF_TICKS);
    note(sauron_song_melody, &c, C4, HALF_TICKS);
    note(sauron_song_melody, &c, E4, HALF_TICKS);
    note(sauron_song_melody, &c, C4, HALF_TICKS);

    /* Bars 3-4 (F): F3 - A3 | C4 - A3 */
    note(sauron_song_melody, &c, F3, HALF_TICKS);
    note(sauron_song_melody, &c, A3, HALF_TICKS);
    note(sauron_song_melody, &c, C4, HALF_TICKS);
    note(sauron_song_melody, &c, A3, HALF_TICKS);

    /* Bars 5-6 (C): G3 - C4 | E4 - C4 */
    note(sauron_song_melody, &c, G3, HALF_TICKS);
    note(sauron_song_melody, &c, C4, HALF_TICKS);
    note(sauron_song_melody, &c, E4, HALF_TICKS);
    note(sauron_song_melody, &c, C4, HALF_TICKS);

    /* Bars 7-8 (G -> Am turnaround): G3 - B3 | D4 - E4 */
    note(sauron_song_melody, &c, G3, HALF_TICKS);
    note(sauron_song_melody, &c, B3, HALF_TICKS);
    note(sauron_song_melody, &c, D4, HALF_TICKS);
    note(sauron_song_melody, &c, E4, HALF_TICKS);

    /* --- Mid voice (chord 5th, sustained drone per chord) --- */
    c = 0;
    note(sauron_song_mid, &c, E3, BAR_TICKS * 2);  /* Am: 5th = E */
    note(sauron_song_mid, &c, C3, BAR_TICKS * 2);  /* F:  5th = C */
    note(sauron_song_mid, &c, G3, BAR_TICKS * 2);  /* C:  5th = G */
    note(sauron_song_mid, &c, D3, BAR_TICKS * 2);  /* G:  5th = D */

    /* --- Bass (chord root, deep drone) --- */
    /* music.h's lowest defined notes are G1..B1 then C2 up. F1 doesn't
     * exist, so the F chord uses F2 — still well below the mid voice. */
    c = 0;
    note(sauron_song_bass, &c, A1, BAR_TICKS * 2);  /* Am */
    note(sauron_song_bass, &c, F2, BAR_TICKS * 2);  /* F  */
    note(sauron_song_bass, &c, C2, BAR_TICKS * 2);  /* C  */
    note(sauron_song_bass, &c, G1, BAR_TICKS * 2);  /* G  */
}
