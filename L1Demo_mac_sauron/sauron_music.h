/* Sauron 8-bar minor loop. Designed to be shared between the PIC
 * audio ISR and the macOS SDL2 audio callback.
 *
 * Format matches the original demo's song_chNf[] convention: one entry
 * per "audio tick" (1 tick = 1953 PIC Timer1 ISR fires ≈ 31.25 ms at
 * FCY=16MHz/PR1=256, or the equivalent count of SDL audio samples). A
 * value of 1 is a rest; any other value is a phase-increment lookup
 * matching the note macros in music.h. */
#ifndef SAURON_MUSIC_H
#define SAURON_MUSIC_H

#include <stdint.h>
#include <stddef.h>

/* Slow, epic tempo: ~60 BPM with half-note melody. One "beat" = 32 PIC
 * audio ticks ≈ 1 second. 4 beats per bar, 8 bars per loop = 1024 ticks
 * per loop (~32 seconds). Each 2-bar chord region holds 256 ticks. */
#define SAURON_TICKS_PER_BEAT   32
#define SAURON_BEATS_PER_BAR    4
#define SAURON_BARS             8
#define SAURON_LOOP_TICKS       (SAURON_TICKS_PER_BEAT * \
                                 SAURON_BEATS_PER_BAR * SAURON_BARS)

extern uint16_t sauron_song_melody[SAURON_LOOP_TICKS];   /* channel 2 */
extern uint16_t sauron_song_mid   [SAURON_LOOP_TICKS];   /* channel 3 */
extern uint16_t sauron_song_bass  [SAURON_LOOP_TICKS];   /* channel 4 */

/* Fill the three arrays with the Am - F - C - G loop. Call once before
 * starting audio playback. */
void sauron_music_build(void);

#endif
