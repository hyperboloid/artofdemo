#include <stdint.h>

volatile uint16_t _g_sfr_sink = 0;
volatile uint16_t _g_sfr_one  = 1;

volatile uint16_t ANSB, ANSC, ANSD, ANSF, ANSG;
volatile uint16_t TRISB, PORTB;
volatile uint16_t PR1;
volatile uint16_t G1CMDL, G1CMDH;
volatile uint16_t G1DPADRL, G1DPADRH;
volatile uint16_t G1W1ADRL, G1W1ADRH;
volatile uint16_t G1W2ADRL, G1W2ADRH;
volatile uint16_t G1PUW, G1PUH;
volatile uint16_t G1CLUTWR;
