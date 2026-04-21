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
 *   Compile with g++ on macOS:
 *       g++ *.cpp -std=c++11 -O2 -o fade $(sdl2-config --cflags --libs)
 *
 */

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <SDL.h>
#include "vga.h"
#include "timer.h"

// this handles all the graphics
VGA *vga;

// our timing module
TIMER *timer;

/*
 * the blending procedure... calculates an itermediary image given 2 other
 * pictures and a blending coefficient (from 0 to 256), and dumps the result
 * to the temporary buffer
 */
void blendImage( unsigned char *image1, unsigned char *image2, int coef )
{
// loop for all pixels
   for (int i=0; i<64000; i++)
   {
    // calculate the interpolated value in fixed point
       vga->page_draw[i] = image1[i] + ( ( image2[i] - image1[i] ) * coef >> 8 );
   }
}

// the main program
int main(int argc, char *argv[])
{
// set mode 320x200x8
    vga = new VGA;
// setup a greyscale palette
    for (int i=0; i<256; i++)
    {
        vga->SetColour( i, i>>2, i>>2, i>>2 );  //  i>>2 is equivalent to i/4
    }
// load the 2 greyscale images that we will blend
    unsigned char *pic1 = new unsigned char[64000],
                  *pic2 = new unsigned char[64000];
// load the simple logo
    FILE *raw_file = fopen( "picture1.raw", "rb" );
    fread( pic1, 1, 64000, raw_file );
    fclose( raw_file );
// load an old postcard of my hometown. Montbrison, Loire, France
    raw_file = fopen( "mtbrison.raw", "rb" );
    fread( pic2, 1, 64000, raw_file );
    fclose( raw_file );
// start the timer
    timer = new TIMER;
    long long startTime = timer->getCount(), frameCount = 0;
// run the main loop
    bool running = true;
    SDL_Event event;
    while (running)
    {
    // handle events (quit on key press or window close)
       while (SDL_PollEvent(&event))
       {
           if (event.type == SDL_QUIT ||
               event.type == SDL_KEYDOWN)
               running = false;
       }
    // get a time value within the range 0..1023 with a bit mask
       int testVal = (timer->getCount()>>14)&0x3ff;
      // just draw image 1
         if (testVal<256) memcpy( vga->page_draw, pic1, 64000 );
      // or fade image 1 to image 2
         else if (testVal<512) blendImage( pic1, pic2, testVal-256 );
      // or just draw image 2
         else if (testVal<768) memcpy( vga->page_draw, pic2, 64000 );
      // or fade image 2 to image 1
         else blendImage( pic2, pic1, testVal-768 );
    // dump our temporary buffer to the screen
       vga->Update();
    // we've just draw a frame
       frameCount++;
    }
// calculate total running time
    long long totalTime = timer->getCount() - startTime;
// and turn off the timer
    delete (timer);
// now get FPS
    float FPS = (float)(frameCount)*1193180.0f / (float)(totalTime);
// return to text mode
    delete (vga);
// free memory
    delete [] (pic1);
    delete [] (pic2);
// make sure every one knows about flipcode :)
    std::cout << "                             The Art Of" << std::endl;
    std::cout << "                         D E M O M A K I N G " << std::endl << std::endl;
    std::cout << "                       by Alex J. Champandard" << std::endl << std::endl;
    std::cout << "       Go to http://www.flipcode.com/demomaking for more information." << std::endl;
    std::cout << std::endl << FPS << " frames per second." << std::endl;
// we've finished!
    return 0;
}
