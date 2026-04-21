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
 *                 Ported to SDL2_mixer for macOS.
 *
 */


#include <iostream>
#include <SDL.h>
#include <SDL_mixer.h>

int main(int argc, char *argv[])
{
 // initialize SDL audio and video (need video for the window)
    if (SDL_Init( SDL_INIT_AUDIO | SDL_INIT_VIDEO ) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

 // initialize SDL_mixer with MOD support
    if (Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 4096 ) < 0) {
        std::cerr << "Mix_OpenAudio failed: " << Mix_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

 // load the XM module
    Mix_Music *music = Mix_LoadMUS( "lord.xm" );
    if (!music) {
        std::cerr << "Could not load lord.xm: " << Mix_GetError() << std::endl;
        Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }

 // start playing (loop forever)
    Mix_PlayMusic( music, -1 );

    std::cout << "Playing lord.xm... Press any key in the window to stop." << std::endl;

 // We need a window to receive key events
    SDL_Window *window = SDL_CreateWindow( "Music - The Art of Demomaking",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        320, 200, 0 );

 // run a minimal event loop
    bool running = true;
    SDL_Event event;
    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN)
                running = false;
        }
        SDL_Delay(16);
    }

 // stop and clean up
    Mix_HaltMusic();
    Mix_FreeMusic( music );
    Mix_CloseAudio();
    SDL_DestroyWindow( window );
    SDL_Quit();

    return 0;
}
