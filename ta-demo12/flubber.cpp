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
 *  Compile with DJGPP:    gcc *.cpp -s -O2 -o flubber.exe -lstdcxx -lpng -lz
 *
 *  Ported to SDL2 for macOS.
 */


#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <cmath>
#include <SDL.h>
#include "vga.h"
#include "timer.h"
#include "texture.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// this handles all the graphics
VGA *vga;

// our timing module
TIMER *timer;

// the texture
TEXTURE *texture;
long text_offs;

// small buffer of 256x1 containing the light pattern (fake phong ;)
unsigned char *light;

// our 16 bit zbuffer, we don't need it much in this example, unless
// the flubber has a really weird shape
unsigned short *zbuffer;

// the look-up table
unsigned char *lut;

// some constants, number of spans per slice
#define SPANS 32
#define SPANMASK 31


/*
 * draw a horizontal double textured span
 */
void DrawSpan( int y, int x1, int tx1, int px1, int z1,
                      int x2, int tx2, int px2, int z2 )
{
  // quick check, if facing back then return
     if (x1>=x2) return;
  // compute deltas for interpolation
     int dx = x2-x1,
         dtx = (tx2-tx1)/dx,  // assume 16.16 fixed point
         dpx = (px2-px1)/dx,  // 16.16
         dz  = (z2-z1)/dx;    // doesn't matter, whatever parameters are is fine
  // get destination offset in buffer
     long offs = y*320+x1;
  // loop for all pixels concerned
     for (int i=0; i<dx; i++)
     {
      // check z buffer
         if (z1<zbuffer[offs])
         {
         // if visible load the texel from the translated texture
            int texel = texture->location[(((y+text_offs)<<8)&0xff00)+((tx1>>16)&0xff)],
         // and the texel from the light map
                lumel = light[(px1>>16)&0xff];
         // mix them together
            vga->page_draw[offs] = lut[(texel<<8)+lumel];
         // and update the zbuffer
            zbuffer[offs] = z1;
         }
      // interpolate our values
         px1 += dpx;
         tx1 += dtx;
         z1  += dz;
      // and find next pixel
         offs++;
     }
}

/*
 * naively scan all the colours to find the closest one
 */
unsigned char Closest_Colour( unsigned char *pal, unsigned char r,
                              unsigned char g, unsigned char b )
{
     long dist = 1<<30, newDist;
     unsigned char index;
  // for all colours loop
     for (int i=0; i<256; i++)
     {
      // calculate how far the current colour is from the required colour
         newDist = (r-pal[i*3])*(r-pal[i*3])
                 + (g-pal[i*3+1])*(g-pal[i*3+1])
                 + (b-pal[i*3+2])*(b-pal[i*3+2]);
      // if it's the same then we've found a match, so return it
         if (newDist==0) return i;
      // if the distance is closer, then remember this colour's index
         if (newDist<dist) {
            index = i;
            dist = newDist;
         }
     }
  // return the one we found closest
     return index;
}

/*
 * given a palette compute the best lookup table containing 256 shades
 * of each original colour
 */
void Compute_LUT( unsigned char *pal )
{
   unsigned char r, g, b, l;
   int i;
// for each colour
   for (int j=0; j<256; j++)
   {
       r = pal[j*3];
       g = pal[j*3+1];
       b = pal[j*3+2];
    // calculate shades from a third of the colour to the colour
       for (i=0; i<240; i++)
       {
        // get the index of the best colour
           lut[(j<<8)+i] = Closest_Colour( pal, r*(120+i)/360, g*(120+i)/360, b*(120+i)/360 );
       }
    // calc shades half way from the colour to white
       for (i=0; i<16; i++)
       {
           lut[(j<<8)+240+i] = Closest_Colour( pal, r+(255-r)*i/31, g+(255-g)*i/31, b+(255-b)*i/31 );
       }

   }
}


/*
 * the main program
 */
int main(int argc, char *argv[])
{
 // load the texture
    texture = new TEXTURE( "texture.png" );
    int i, j;
 // prepare the lighting
    light = new unsigned char[256];
    for (i=0; i<256; i++)
    {
     // calculate distance from the centre
        int c = abs(255-i*2);
     // check for overflow
        if (c>255) c = 255;
     // store lumel
        light[i] = 255-c;
    }
// compute the lookup table
    lut = new unsigned char[65536];
    Compute_LUT( texture->palette );
// prepare 3D data
    zbuffer = new unsigned short[64000];
// store points as new data type, the type vector would take up more
// space than necessary
    struct {
      float x, z;
    } pts[200][SPANS], nrm[200][SPANS];
// initialise each point of each slice
    for (i=0; i<SPANS; i++)
    {
        float angle = (float)i*M_PI*2/SPANS,
              ca = cos( angle ),
              sa = sin( angle );
        for (j=0; j<200; j++)
        {
         // this generates a flat cylinder
            float radx = 32, rady = 96;
         // store the points
            pts[j][i].x = radx * ca;
            pts[j][i].z = rady * sa;
        }
    }
 // now calculate the normals
    for (i=0; i<SPANS; i++)
    {
        for (j=0; j<200; j++)
        {
         // get coords of tangeant vector
            float nx = pts[j][(i+1)&SPANMASK].x - pts[j][(i-1)&SPANMASK].x,
                  nz = pts[j][(i+1)&SPANMASK].z - pts[j][(i-1)&SPANMASK].z,
         // and compute it's length
                  inorm = 1/sqrt(nx*nx+nz*nz);
         // now make sure the vector has length one
            nx *= inorm;
            nz *= inorm;
         // and store it
            nrm[j][i].x = nx;
            nrm[j][i].z = nz;
        }
    }
// set mode 320x200x8
    vga = new VGA;
    for (i=0; i<256; i++)
    {
        vga->SetColour( i, texture->palette[i*3]>>2, texture->palette[i*3+1]>>2,  texture->palette[i*3+2]>>2 );
    }
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

          currentTime = timer->getCount();
       // clear the zbuffer
          memset( zbuffer, 255, 128000 );
       // and determine how far to scroll the texture
          text_offs = currentTime / 31456;
       // clear the background
          vga->Clear();
       // draw all slices
          for (i=0; i<200; i++)
          {
              int xc[SPANS], zc[SPANS], nc[SPANS];
           // set a different rotation angle for each slice
              float angle = (float)currentTime / 1948613 +
              2 * M_PI * (cos( (float)currentTime/3179640 )
              * cos( -(float)i / 193 ) * sin( (float)currentTime/2714147 ) + 1 );
           // compute the sine and cosine in 8.8 fxd point
              float ca = 256 * cos( angle ),
                    sa = 256 * sin( angle );
           // and determine the horizontal position of this slice
              long xoffs = (long)(40960+10240 * cos( (float)currentTime/2945174 )
                                        * sin( (float)i/59 ) );
              for (j=0; j<SPANS; j++)
              {
               // rotate the point and get X coordinate
                  xc[j] = xoffs+(long)( pts[i][j].x * ca + pts[i][j].z * -sa );
               // rotate the point and get Z coordinate
                  zc[j] = (long)( pts[i][j].x * sa + pts[i][j].z * ca );
               // now get the coordinate in the lightmap by computing
               // the X component of the normal
                  long tmp = (long)( nrm[i][j].x * ca + nrm[i][j].z * -sa );
                  tmp = 256-tmp;
                  nc[j] = (128<<16)+(tmp<<15);
              }
           // now just draw all spans
              for (j=0; j<SPANS; j++)
              {
                  DrawSpan( i, xc[j]>>8, j<<20, nc[j], 49152+zc[j],
                            xc[(j+1)&SPANMASK]>>8, (j+1)<<20,
                            nc[(j+1)&SPANMASK], 49152+zc[(j+1)&SPANMASK] );
              }
          }
       // flush to video ram
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
    delete (texture);
    delete [] (zbuffer);
    delete [] (lut);
// make sure every one knows about flipcode :)
    std::cout << "                             The Art Of" << std::endl;
    std::cout << "                         D E M O M A K I N G " << std::endl << std::endl;
    std::cout << "                       by Alex J. Champandard" << std::endl << std::endl;
    std::cout << "       Go to http://www.flipcode.com/demomaking for more information." << std::endl;
    std::cout << std::endl << FPS << " frames per second." << std::endl;
// we've finished!
    return 0;
}
