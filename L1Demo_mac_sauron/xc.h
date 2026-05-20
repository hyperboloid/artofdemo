/* Stand-in for Microchip's xc.h so the original sources compile on macOS.
 * Defines the PIC24FJ256DA206 SFR bitfields used by gpu.c/audio.c as plain
 * uint16_t globals. They're declared in pic_sfr.c. */
#ifndef XC_H_STUB
#define XC_H_STUB

#include <stdint.h>
#include "pic_compat.h"

/* Generic SFR placeholders. Anything the original sources poke ends up here. */
extern volatile uint16_t _g_sfr_sink;

#define SFR_DECL(name) extern volatile uint16_t name

SFR_DECL(ANSB); SFR_DECL(ANSC); SFR_DECL(ANSD); SFR_DECL(ANSF); SFR_DECL(ANSG);
SFR_DECL(TRISB); SFR_DECL(PORTB);
SFR_DECL(PR1);
SFR_DECL(G1CMDL); SFR_DECL(G1CMDH);
SFR_DECL(G1DPADRL); SFR_DECL(G1DPADRH);
SFR_DECL(G1W1ADRL); SFR_DECL(G1W1ADRH);
SFR_DECL(G1W2ADRL); SFR_DECL(G1W2ADRH);
SFR_DECL(G1PUW); SFR_DECL(G1PUH);
SFR_DECL(G1CLUTWR);

/* Bit-fields the original code uses through Microchip's _Foo macros.
 * We expose them as plain lvalues. */
#define _VMRGNIF  _g_sfr_sink
#define _VMRGNIE  _g_sfr_sink
#define _HMRGNIF  _g_sfr_sink
#define _HMRGNIE  _g_sfr_sink
#define _GFX1IE   _g_sfr_sink
#define _GFX1IF   _g_sfr_sink
#define _G1CLKSEL _g_sfr_sink
#define _GCLKDIV  _g_sfr_sink
#define _DPMODE   _g_sfr_sink
#define _GDBEN    _g_sfr_sink
#define _DPW      _g_sfr_sink
#define _PUW      _g_sfr_sink
#define _DPH      _g_sfr_sink
#define _PUH      _g_sfr_sink
#define _DPWT     _g_sfr_sink
#define _DPHT     _g_sfr_sink
#define _DPCLKPOL _g_sfr_sink
#define _DPENOE   _g_sfr_sink
#define _DPENPOL  _g_sfr_sink
#define _DPVSOE   _g_sfr_sink
#define _DPHSOE   _g_sfr_sink
#define _DPVSPOL  _g_sfr_sink
#define _DPHSPOL  _g_sfr_sink
#define _ACTLINE  _g_sfr_sink
#define _VENST    _g_sfr_sink
#define _ACTPIX   _g_sfr_sink
#define _HENST    _g_sfr_sink
#define _VSST     _g_sfr_sink
#define _HSST     _g_sfr_sink
#define _VSLEN    _g_sfr_sink
#define _HSLEN    _g_sfr_sink
#define _DPPWROE  _g_sfr_sink
#define _DPPINOE  _g_sfr_sink
#define _DPPOWER  _g_sfr_sink
#define _DPBPP    _g_sfr_sink
#define _PUBPP    _g_sfr_sink
#define _G1EN     _g_sfr_sink
#define _CLUTEN   _g_sfr_sink
#define _CLUTADR  _g_sfr_sink
#define _CLUTRWEN _g_sfr_sink
#define _CLUTBUSY _g_sfr_sink   /* always 0 -> never busy */
#define _CMDMPT   _g_sfr_one    /* always 1 -> command queue empty */
#define _CMDFUL   _g_sfr_sink   /* always 0 -> command queue never full */
#define _IPUBUSY  _g_sfr_sink
#define _RCCBUSY  _g_sfr_sink
#define _CHRBUSY  _g_sfr_sink
#define _T1IP     _g_sfr_sink
#define _TON      _g_sfr_sink
#define _T1IF     _g_sfr_sink
#define _T1IE     _g_sfr_sink

extern volatile uint16_t _g_sfr_one;

#endif
