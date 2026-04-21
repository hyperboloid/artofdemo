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
 *      Compile with DJGPP:    gcc *.cpp -s -O2 -o zoom.exe -lstdcxx
 *
 *      Ported to SDL2 for macOS.
 */


#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cmath>
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

// our precalculated mandelbrot fractals
unsigned char *frac1, *frac2, *fractmp;

// define the point in the complex plane to which we will zoom into
#define OR_VAL    -0.577816-9.31323E-10-1.16415E-10
#define OI_VAL    -0.631121-2.38419E-07+1.49012E-08

/*
 * global variables used for calculating our fractal
 */
double dr, di, pr, pi, sr, si;
long offs;

/*
 * initialize the fractal computation
 */
void Start_Frac( double _sr, double _si, double er, double ei )
{
// compute deltas for interpolation in complex plane
   dr = (er-_sr)/640.0f;
   di = (ei-_si)/400.0f;
// remember start values
   pr = _sr;
   pi = _si;
   sr = _sr;
   si = _si;
   offs = 0;
}

/*
 * compute 4 lines of fracal
 */
void Compute_Frac()
{
   for (int j=0; j<4; j++)
   {
       pr = sr;
       for (int i=0; i<640; i++)
       {
           unsigned char c = 0;
           double vi = pi, vr = pr, nvi, nvr;
        // loop until distance is above 2, or counter hits limit
           while ((vr*vr + vi*vi<4) && (c<255))
           {
              // compute Z(n+1) given Z(n)
                 nvr = vr*vr - vi*vi + pr;
                 nvi = 2 * vi * vr + pi;

             // that becomes Z(n)
                 vi = nvi;
                 vr = nvr;

             // increment counter
                 c++;
           }
        // store colour
           frac1[offs] = c;
           offs++;
        // interpolate X
           pr += dr;
       }
    // interpolate Y
       pi += di;
   }
}

/*
 * finished the computation, swap buffers
 */
void Done_Frac()
{
   fractmp = frac1;
   frac1 = frac2;
   frac2 = fractmp;
}

/*
 * the zooming procedure
 * takes a 640x400 bitmap, and scales it to 320x200 given the zooming coef
 */
void Zoom( double z )
{
  // what's the size of rectangle in the source image we want to display
     int width = (int)((640<<16)/(256.0f*(1+z)))<<8,
         height = (int)((400<<16)/(256.0f*(1+z)))<<8,
  // where do we start our interpolation
         startx = ((640<<16)-width)>>1,
         starty = ((400<<16)-height)>>1,
  // get our deltas
         deltax = width/320,
         deltay = height/200,
         px, py = starty;
     long offs = 0;
     for (int j=0; j<200; j++)
     {
      // set start value
         px = startx;
         for (int i=0; i<320; i++)
         {
          // load the bilinear filtered texel
             vga->page_draw[offs] =
             (    frac2[(py>>16)*640+(px>>16)] * (0x100-((py>>8)&0xff)) * (0x100-((px>>8)&0xff))
                + frac2[(py>>16)*640+((px>>16)+1)] * (0x100-((py>>8)&0xff)) * ((px>>8)&0xff)
                + frac2[((py>>16)+1)*640+(px>>16)] * ((py>>8)&0xff) * (0x100-((px>>8)&0xff))
                + frac2[((py>>16)+1)*640+((px>>16)+1)] * ((py>>8)&0xff) * ((px>>8)&0xff) ) >> 16;
          // interpolate X
             px += deltax;
             offs++;
         }
      // interpolate Y
         py += deltay;
     }
}

/*
 * calculate some weird colours to make things look less boring
 */
void updatePalette( int t )
{
    for (int i=0; i<256; i++)
    {
        vga->SetColour( i,
           (unsigned char)(32 - 31 * cos( i * PI / 128 + t*0.00141 )),
           (unsigned char)(32 - 31 * cos( i * PI / 128 + t*0.00141 )),
           (unsigned char)(32 - 31 * cos( i * PI / 64 + t*0.0136 )) );
    }
}

/*
 * the main program
 */
int main(int argc, char *argv[])
{
// setup the palette
    int i, j, k = 0;
// allocate memory for our fractal
    frac1 = new unsigned char[256000];
    frac2 = new unsigned char[256000];
// set original zooming settings
      double zx = 4.0, zy = 4.0;
      bool zoom_in = true;
// calculate the first fractal
    std::cout << "Please wait..." << std::endl;
    Start_Frac( OR_VAL-zx, OI_VAL-zy, OR_VAL+zx, OI_VAL+zy );
    for (j=0; j<100; j++) Compute_Frac();
    Done_Frac();
// set mode 320x200x8
    vga = new VGA;
    updatePalette( 0 );
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

      // adjust zooming coefficient for next view
         if (zoom_in)
         {
            zx *= 0.5;
            zy *= 0.5;
         }
         else {
            zx *= 2;
            zy *= 2;
         }
      // start calculating the next fractal
         Start_Frac( OR_VAL-zx, OI_VAL-zy, OR_VAL+zx, OI_VAL+zy );
         j = 0;
         while ((j<100) && running)
         {
          // handle SDL events inside inner loop too
             while (SDL_PollEvent(&event))
             {
                 if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN)
                     running = false;
             }
             if (!running) break;

             j++;
          // calc another few lines
             Compute_Frac();
          // display the old fractal, zooming in or out
             if (zoom_in) Zoom( (double)j/100.0f );
             else Zoom( 1.0f-(double)j/100.0f );
          // select some new colours
             updatePalette( k*100+j );
          // dump to screen
             vga->Update();
             frameCount++;
         }
      // one more image displayed
         k++;
      // check if we've gone far enough
         if (k%38==0)
         {
         // if so, reverse direction
            zoom_in = !zoom_in;
            if (zoom_in) {
               zx *= 0.5;
               zy *= 0.5;
            }
            else {
               zx *= 2.0;
               zy *= 2.0;
            }
         // and make sure we use the same fractal again, in the other direction
            fractmp = frac1;
            frac1 = frac2;
            frac2 = fractmp;
         }
         Done_Frac();
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
    delete [] (frac1);
    delete [] (frac2);
// make sure every one knows about flipcode :)
    std::cout << "                             The Art Of" << std::endl;
    std::cout << "                         D E M O M A K I N G " << std::endl << std::endl;
    std::cout << "                       by Alex J. Champandard" << std::endl << std::endl;
    std::cout << "       Go to http://www.flipcode.com/demomaking for more information." << std::endl;
    std::cout << std::endl << FPS << " frames per second." << std::endl;
// we've finished!
    return 0;
}
