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
 *
 *                Ported to SDL2 for macOS.
 */

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <cstring>
#include <SDL.h>
#include <SDL_mixer.h>
#include "vector.h"
#include "matrix.h"
#include "texture.h"
#include "vga.h"
#include "timer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define PI M_PI

// this handles all the graphics
VGA *vga;

// our timing module
TIMER *timer;
long long startTime, frameCount;

// synchronisation variables
bool bQuit = false;
bool globalQuit = false;
float fade_coef = 1.0;
float flash_coef = 0;

// temporary buffer containing the logo
unsigned char *textbuf;

// effect duration in timer ticks (~8 seconds each)
static long long effect_start_time;
static const long long EFFECT_DURATION = 8LL * 1193180LL;

bool effectTimeUp() {
    return (timer->getCount() - effect_start_time) > EFFECT_DURATION;
}

void startEffect() {
    effect_start_time = timer->getCount();
    bQuit = false;
    flash_coef = 0;
    fade_coef = 1.0;
}

// include each sub routine
#include "fire.h"
#include "plasma.h"
#include "roto.h"
#include "hole.h"
#include "particle.h"
#include "flubber.h"
#include "tor.h"
#include "bump.h"
#include "zoom.h"


/*
 * the main program
 */
int main(int argc, char *argv[])
{
    std::cout << std::endl << "Initializing effects..." << std::endl;

// initialize SDL mixer for music
    if (SDL_Init( SDL_INIT_AUDIO ) < 0) {
        std::cerr << "SDL audio init failed: " << SDL_GetError() << std::endl;
    }
    Mix_Music *music = nullptr;
    if (Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 4096 ) == 0) {
        music = Mix_LoadMUS( "lord.xm" );
    }

// initialise each effect
    Plasma_Init();
    Fire_Init();
    Roto_Init();
    Hole_Init();
    Flubber_Init();
    Tor_Init();
    Bump_Init();
    Zoom_Init();

// set mode 320x200x8
    vga = new VGA;
    Particle_Init();

// start the timer
    timer = new TIMER;
    startTime = timer->getCount();
    frameCount = 0;

// start music
    if (music) Mix_PlayMusic( music, -1 );

// run each effect for ~8 seconds
    startEffect(); Plasma_Run();
    if (!globalQuit) { startEffect(); Fire_Run(); }
    if (!globalQuit) { startEffect(); Roto_Run(); }
    if (!globalQuit) { startEffect(); Hole_Run(); }
    if (!globalQuit) { startEffect(); Particle_Run(); }
    if (!globalQuit) { startEffect(); Flubber_Run(); }
    if (!globalQuit) { startEffect(); Tor_Run(); }
    if (!globalQuit) { startEffect(); Bump_Run(); }
    if (!globalQuit) { startEffect(); Zoom_Run(); }

// show logo at the end
    if (!globalQuit)
    {
        bQuit = false;
        memcpy( vga->page_draw, textbuf, 64000 );
        Shade_Pal( 0, 255, 0, 0, 0, 63, 63, 63 );
        SDL_Event event;
        long long endStart = timer->getCount();
        while (!globalQuit && (timer->getCount() - endStart) < EFFECT_DURATION)
        {
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN)
                    globalQuit = true;
            }
            vga->Update();
        }
    }

// calculate total running time
    long long totalTime = timer->getCount() - startTime;
// and turn off the timer
    delete timer;
// deinit
    Fire_Done();
    Plasma_Done();
    Roto_Done();
    Hole_Done();
    Particle_Done();
    Flubber_Done();
    Tor_Done();
    Bump_Done();
    Zoom_Done();
// return to text mode
    delete vga;
    delete [] textbuf;
// shut down music
    if (music) {
        Mix_HaltMusic();
        Mix_FreeMusic( music );
    }
    Mix_CloseAudio();
// make sure every one knows about flipcode :)
    std::cout << "                             The Art Of" << std::endl;
    std::cout << "                         D E M O M A K I N G " << std::endl << std::endl;
    std::cout << "                       by Alex J. Champandard" << std::endl << std::endl;
    std::cout << "       Go to http://www.flipcode.com/demomaking for more information." << std::endl;
// we've finished!
    return 0;
}
