/*
 *                          The Art Of
 *                      D E M O M A K I N G
 *
 *
 *                     by Alex J. Champandard
 *                          Base Sixteen
 *
 *
 *                http://www.flipcode.com/demomaking
 *
 *                This file is in the public domain.
 *                      Use at your own risk.
 *
 *   Ported to std::chrono for macOS.
 */


#include <chrono>
#include "timer.h"

static auto epoch = std::chrono::high_resolution_clock::now();

static long long microsNow()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - epoch ).count();
}

/*
 * constructor, in our case nothing needs to be done
 */
TIMER::TIMER()
{
   startTime = microsNow();
}

/*
 * destructor, also nothing to do
 */
TIMER::~TIMER()
{
}

/*
 * Return a count compatible with the original uclock() scale.
 * uclock() on DJGPP ticks at 1193180 Hz.
 * We convert from microseconds: ticks = micros * 1193180 / 1000000
 */
long long TIMER::getCount()
{
    long long micros = microsNow() - startTime;
    return micros * 1193180LL / 1000000LL;
}
