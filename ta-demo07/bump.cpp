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
 *  Compile with DJGPP:    gcc *.cpp -s -O2 -o bump.exe -lstdcxx -lpng -lz
 *
 *  Ported to SDL2 for macOS.
 */


#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <SDL.h>
#include "vga.h"
#include "timer.h"
#include "texture.h"


// this handles all the graphics
VGA *vga;

// our timing module
TIMER *timer;

// the look-up table
unsigned char *lut;

// size of the spot light
#define LIGHTSIZE 2.4f

// contains the precalculated spotlight
unsigned char *light;

/*
 * this needs a bump map and a colour map, and 2 light coordinates
 * it computes the output colour with the look up table
 */
void Bump( unsigned char *bm, unsigned char *cm, int lx1, int ly1, int lx2, int ly2, int zoom )
{
     int i, j, px, py, x, y, offs, c;
  // we skip the first line since there are no pixels above
  // to calculate the slope with
     offs = 320;
  // loop for all the other lines
     for (j=1; j<200; j++)
     {
      // likewise, skip first pixel since there are no pixels on the left
         vga->page_draw[offs] = 0; offs++;
         for (i=1; i<320; i++)
         {
          // calculate coordinates of the pixel we need in light map
          // given the slope at this point, and the zoom coefficient
             px = (i*zoom>>8) + bm[offs-1] - bm[offs];
             py = (j*zoom>>8) + bm[offs-320] - bm[offs];
          // add the movement of the first light
             x = px + lx1;
             y = py + ly1;
          // check if the coordinates are inside the light buffer
             if ((y>=0) && (y<256) && (x>=0) && (x<256))
             // if so get the pixel
                c = light[(y<<8)+x];
          // otherwise assume intensity 0
             else c = 0;
          // now do the same for the second light
             x = px + lx2;
             y = py + ly2;
          // this time we add the light's intensity to the first value
             if ((y>=0) && (y<256) && (x>=0) && (x<256))
                c += light[(y<<8)+x];
          // make sure it's not too big
             if (c>255) c = 255;
          // look up the colour multiplied by the light coeficient
             vga->page_draw[offs] = lut[(cm[offs]<<8)+c];
             offs++;
         }
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
// for each colour
   for (int j=0; j<256; j++)
   {
       r = pal[j*3];
       g = pal[j*3+1];
       b = pal[j*3+2];
    // for all 256 intensities
       for (int i=0; i<256; i++)
       {
        // get the index of the best colour
           lut[(j<<8)+i] = Closest_Colour( pal, (r*i)/255, (g*i)/255, (b*i)/255 );
       }
   }
}

/*
 * generate a "spot light" pattern
 */
void Compute_Light()
{
    for (int j=0; j<256; j++)
        for (int i=0; i<256; i++)
        {
         // get the distance from the centre
            float dist = (128-i)*(128-i) + (128-j)*(128-j);
            if (fabs(dist)>1) dist = sqrt( dist );
         // then fade if according to the distance, and a random coefficient
            int c = (int)(LIGHTSIZE*dist)+(rand()&7)-3;
         // clip it
            if (c<0) c = 0;
            if (c>255) c = 255;
         // and store it
            light[(j<<8)+i] = 255-c;
        }
}

/*
 * the main program
 */
int main(int argc, char *argv[])
{
// set mode 320x200x8
    vga = new VGA;
// load the bump map and the colour map
   TEXTURE *bm = new TEXTURE( "bump.png" );
   TEXTURE *cm = new TEXTURE( "map.png" );
// setup the palette
    for (int i=0; i<256; i++)
    {
        vga->SetColour( i, cm->palette[i*3]>>2, cm->palette[i*3+1]>>2,  cm->palette[i*3+2]>>2 );
    }
// compute the lookup table
   lut = new unsigned char[65536];
   Compute_LUT( cm->palette );
// contains the image of the spotlight
   light = new unsigned char[65536];
// generate the light pattern
   Compute_Light();
// light movement
    int x1, y1, x2, y2, z;
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
    // move the light.... more sines :)
       x1 = (int)(128.0f * cos( (double)currentTime/64 ))-20;
       y1 = (int)(128.0f * sin( (double)-currentTime/45 ))+20;
       x2 = (int)(128.0f * cos( (double)-currentTime/51 ))-20;
       y2 = (int)(128.0f * sin( (double)currentTime/71 ))+20;
       z = 192+(int)(127.0f * sin( (double)currentTime/112 ));
    // draw the bumped image
       Bump( bm->location, cm->location, x1, y1, x2, y2, z );
    // dump to screen
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
    delete (bm);
    delete (cm);
    delete [] (light);
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
