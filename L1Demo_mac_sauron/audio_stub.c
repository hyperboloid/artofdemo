/* Host audio stub: just provides the globals scene/main reference. The real
 * audio path is intentionally omitted — main_sdl.c advances time_sec from
 * SDL ticks instead of from a timer ISR. */

#include <stdint.h>
#include "audio.h"

volatile uint32_t time_sec    = 0;
volatile uint32_t time_subsec = 0;

volatile uint8_t  audio_mode  = 0;
volatile unsigned short ch1_val = 0;
volatile unsigned short ch2_val = 0;
volatile unsigned short ch3_val = 0;

void audio_init(void) { /* no-op on host */ }
void audio_isr(void)  { /* no-op on host */ }
