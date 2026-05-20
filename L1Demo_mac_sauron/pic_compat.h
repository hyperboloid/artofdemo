/* Compatibility shim so the original PIC24 sources compile on macOS.
 * Force-included via -include in the Makefile so it lands BEFORE any of
 * the PIC headers (gpu.h, sprites.h, fonts.h, audio.h). */
#ifndef PIC_COMPAT_H
#define PIC_COMPAT_H

#include <stdint.h>

/* PIC24 memory-space qualifiers — drop them on host. */
#define __eds__
#define __prog__

/* Swallow Microchip __attribute__ extensions clang doesn't understand.
 * `section("DISPLAY")`/`section("FONTS")` aren't valid mach-o specs either,
 * so swallow `section` entirely on host. */
#define space(...)
#define section(...)
#define auto_psv
#define interrupt

/* Tiny inline ops the PIC SDK provides. */
#define Nop()         ((void)0)
#define __delay_ms(x) ((void)0)

#ifndef NULL
#define NULL ((void*)0)
#endif

#endif
