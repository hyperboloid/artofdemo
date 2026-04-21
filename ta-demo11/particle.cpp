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
 *       Compile with DJGPP:    gcc *.cpp -s -O2 -o particle.exe -lstdcxx
 *
 *       Ported to SDL2 for macOS.
 */


#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <SDL.h>
#include "vector.h"
#include "matrix.h"
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

// number of points in the object
#define MAXPTS 4096

// matrix that describes the rotation of the object
MATRIX obj;
// how far it is from the viewer
float base_dist;

// store the precalculated scaling values
int scaleX[320];
int scaleY[200];


/*
 * scale an image away from a given point
 */
void Rescale( unsigned char *src, unsigned char *dst )
{
    long offs = 0;
    for (int j=0; j<200; j++)
    {
        for (int i=0; i<320; i++)
        {
         // get value from pixel in scaled image, and store
            dst[offs] = src[scaleY[j]*320+scaleX[i]];
            offs++;
        }
    }
}

/*
 * smooth a buffer
 */
void Blur( unsigned char *src, unsigned char *dst )
{
     int offs = 320;
     unsigned char b;
     for (int j=1; j<199; j++)
     {
     // set first pixel of the line to 0
         dst[offs] = 0; offs++;
     // calculate the filter for all the other pixels
         for (int i=1; i<319; i++)
         {
         // calculate the average
             b = ( src[offs-321] + src[offs-320] + src[offs-319]
                 + src[offs-1]   +               + src[offs+1]
                 + src[offs+319] + src[offs+320] + src[offs+321] ) >> 3;
             if (b>16) b-=16; else b=0;
          // store the pixel
             dst[offs] = b;
             offs++;
         }
     // set last pixel of the line to 0
         dst[offs] = 0; offs++;
     }
}


/*
 * draw one single particle
 */
void Draw( unsigned char *where, VECTOR v )
{
  // calculate the screen coordinates of the particle
     float iz = 1/(v[2]+base_dist),
           x = 160+160*v[0]*iz,
           y = 100+160*v[1]*iz;
  // clipping
     if ((x<0) || (x>319) || (y<0) || (y>199)) return;
  // convert to fixed point to help antialiasing
     int sx = (int)(x*8.0f),
         sy = (int)(y*8.0f);
  // compute offset
     long offs = (sy>>3)*320+(sx>>3);
     sx = sx & 0x7;
     sy = sy & 0x7;
  // add antialiased particle to buffer, check for overflow
     int c = where[offs];   c += (7-sx)*(7-sy);
     if (c>255) c = 255;  where[offs] = c;
     c = where[offs+1];     c += (7-sy)*sx;
     if (c>255) c = 255;  where[offs+1] = c;
     c = where[offs+320];   c += sy*(7-sx);
     if (c>255) c = 255;  where[offs+320] = c;
     c = where[offs+321];   c += sy*sx;
     if (c>255) c = 255;  where[offs+321] = c;
}

// the main program
int main(int argc, char *argv[])
{
// set mode 320x200x8
    vga = new VGA;
// setup the greyscale palette
    int i;
    for (i=0; i<256; i++) vga->SetColour( i, i>>2, i>>2, i>>2 );
// generate our points
    VECTOR *pts = new VECTOR[MAXPTS];
    for (i=0; i<MAXPTS; i++)
        pts[i] = ( rotX( 2.0f*M_PI*sin( (float)i/203 ) )
                 * rotY( 2.0f*M_PI*cos( (float)i/157 ) )
                 * rotZ( -2.0f*M_PI*cos( (float)i/181 ) ) ) * VECTOR( 64+16*sin( (float)i/191 ), 0, 0 );
// prepare the two buffers
   unsigned char *page1 = new unsigned char[64000],
                 *page2 = vga->page_draw;
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

       currentTime = timer->getCount()>>14;
    // calculate a new colour
       unsigned char r = (unsigned char)(32 + 31 * cos( i * PI / 256 + (float)currentTime/74 )),
                     g = (unsigned char)(32 + 31 * cos( i * PI / 256 + (float)currentTime/63 )),
                     b = (unsigned char)(32 + 31 * cos( i * PI / 256 + (float)currentTime/81 ));
    // fade palette from black (0) to colour (128) to white (255)
       for (i=0; i<128; i++)
       {
           vga->SetColour( i, r*i/128, g*i/128, b*i/128 );
           vga->SetColour( 128+i, r+(63-r)*i/128, g+(63-g)*i/128, b+(63-b)*i/128 );
       }
    // recompute parameters for image rescaling
       int sx = (int)(160-80*sin((float)currentTime/559)),
           sy = (int)(100+50*sin((float)currentTime/611));
       for (i=0; i<320; i++) scaleX[i] = (int)(sx+(i-sx)*0.85f);
       for (i=0; i<200; i++) scaleY[i] = (int)(sy+(i-sy)*0.85f);
    // rescale the image
       Rescale( page2, page1 );
    // blur it
       Blur( page1, page2 );
    // setup the position of the object
       base_dist = 256 + 64 * sin( (float)currentTime/327 );
       obj = rotX( 2.0f*M_PI*sin( (float)currentTime/289 ) )
           * rotY( 2.0f*M_PI*cos( (float)currentTime/307 ) )
           * rotZ( -2.0f*M_PI*sin( (float)currentTime/251 ) );
    // draw the particles
       for (i=0; i<MAXPTS; i++) Draw( page2, obj * pts[i] );
    // dump our temporary buffer to the screen
       vga->Update();
       frameCount++;
    }
// calculate total running time
    long long totalTime = timer->getCount() - startTime;
// and turn off the timer
    delete (timer);
// free memory
    delete [] (page1);
    delete [] (pts);
// now get FPS
    float FPS = (float)(frameCount)*1193180.0f / (float)(totalTime);
// return to text mode
    delete (vga);
// make sure every one knows about flipcode :)
    std::cout << "                             The Art Of" << std::endl;
    std::cout << "                         D E M O M A K I N G " << std::endl << std::endl;
    std::cout << "                       by Alex J. Champandard" << std::endl << std::endl;
    std::cout << "       Go to http://www.flipcode.com/demomaking for more information." << std::endl;
    std::cout << std::endl << FPS << " frames per second." << std::endl;
// we've finished!
    return 0;
}
