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
 *  Compile with DJGPP:    gcc *.cpp -s -O2 -o plasma.exe -lstdcxx
 *
 *  Ported to SDL2 for macOS.
 */


#include <iostream>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <SDL.h>
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

/*
 * generate two 640x400 buffers containing our precalculated plasma functions
 */
void precalculate( unsigned char *buf1, unsigned char *buf2 )
{
    int i, j, dst = 0;
    for (j=0; j<400; j++)
    {
        for (i=0; i<640; i++)
        {
           buf1[dst] = (unsigned char)( 64 + 63 * ( sin( (double)hypot( 200-j, 320-i )/16 ) ) );
           buf2[dst] = (unsigned char)( 64 + 63 * sin( (float)i/(37+15*cos((float)j/74)) )
                                                * cos( (float)j/(31+11*sin((float)i/57))) );
           dst++;
        }
    }
}



// the main program
int main(int argc, char *argv[])
{
// set mode 320x200x8
    vga = new VGA;
// the two function buffers sized 640x400
    unsigned char *plasma1 = new unsigned char[256000];
    unsigned char *plasma2 = new unsigned char[256000];
// precalculate our 2 functions
    precalculate( plasma1, plasma2 );
// buffer to store the static image, only 320x200 since we don't move it
    unsigned char *image = new unsigned char[64000];
// load the image from disk
    FILE *raw_file = fopen( "image.raw", "rb" );
    if (!raw_file) {
        std::cerr << "Could not open image.raw" << std::endl;
        delete vga;
        delete [] plasma1;
        delete [] plasma2;
        delete [] image;
        return 1;
    }
    fread( image, 1, 64000, raw_file );
    fclose( raw_file );
// define the plasma movement
    int x1, y1, x2, y2, x3, y3;
    long dst, i, j, src1, src2, src3;
// start the timer
    timer = new TIMER;
    long long startTime = timer->getCount(), frameCount = 0, currentTime;
// run the main loop
    bool running = true;
    SDL_Event event;
    while (running)
    {
    // handle SDL events
       while (SDL_PollEvent(&event))
       {
           if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN)
               running = false;
       }
       if (!running) break;

    // we get the time for this frame only once so that we are sure it stays
    // the same for all our calculations :)
       currentTime = timer->getCount()>>14;
    // setup some nice colours, different every frame
    // this is a palette that wraps around itself, with different period sine
    // functions to prevent monotonous colours
       for (i=0; i<256; i++)
       {
           vga->SetColour( i, (unsigned char)(32 + 31 * cos( i * PI / 128 + (double)currentTime/74 )),
                              (unsigned char)(32 + 31 * sin( i * PI / 128 + (double)currentTime/63 )),
                              (unsigned char)(32 - 31 * cos( i * PI / 128 + (double)currentTime/81 )) );
       }
    // move plasma with more sine functions :)
       x1 = 160 + (int)(159.0f * cos( (double)currentTime/97 ));
       x2 = 160 + (int)(159.0f * sin( (double)-currentTime/114 ));
       x3 = 160 + (int)(159.0f * sin( (double)-currentTime/137 ));
       y1 = 100 + (int)(99.0f * sin( (double)currentTime/123 ));
       y2 = 100 + (int)(99.0f * cos( (double)-currentTime/75 ));
       y3 = 100 + (int)(99.0f * cos( (double)-currentTime/108 ));
    // we only select the part of the precalculated buffer that we need
       src1 = y1*640+x1;
       src2 = y2*640+x2;
       src3 = y3*640+x3;
    // draw the plasma... this is where most of the time is spent.
       dst = 0;
       for (j=0; j<200; j++)
       {
           for (i=0; i<320; i++)
           {
           // plot the pixel as a sum of all our plasma functions
               vga->page_draw[dst] =
               plasma1[src1]+plasma2[src2]+plasma2[src3]+image[dst];
               dst++; src1++; src2++; src3++;
           }
        // get the next line in the precalculated buffers
           src1 += 320; src2 += 320; src3 += 320;
       }
    // dump our temporary buffer to the screen
       vga->Update();
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
    delete [] (image);
    delete [] (plasma1);
    delete [] (plasma2);
// make sure every one knows about flipcode :)
    std::cout << "                             The Art Of" << std::endl;
    std::cout << "                         D E M O M A K I N G " << std::endl << std::endl;
    std::cout << "                       by Alex J. Champandard" << std::endl << std::endl;
    std::cout << "       Go to http://www.flipcode.com/demomaking for more information." << std::endl;
    std::cout << std::endl << FPS << " frames per second." << std::endl;
// we've finished!
    return 0;
}
