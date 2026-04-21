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
 *  Compile with DJGPP:    gcc *.cpp -s -O2 -o distort.exe -lstdcxx -lpng -lz
 *
 *  Ported to SDL2 for macOS.
 */


#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <SDL.h>
#include "texture.h"
#include "vga.h"
#include "timer.h"

// this handles all the graphics
VGA *vga;

// our timing module
TIMER *timer;

// displacement buffers
char *dispX, *dispY;


/*
 * calculate a distorion function for X and Y in 5.3 fixed point
 */
void precalculate()
{
    int i, j, dst;
    dst = 0;
    for (j=0; j<400; j++)
    {
        for (i=0; i<640; i++)
        {
            float x = (float)i;
            float y = (float)j;
         // notice the values contained in the buffers are signed
         // i.e. can be both positive and negative
            dispX[dst] = (signed char)(8*(2*(sin(x/20) + sin(x*y/2000)
                       + sin((x+y)/100) + sin((y-x)/70) + sin((x+4*y)/70)
                       + 2*sin(hypot(256-x,(150-y/8))/40) )));
         // also notice we multiply by 8 to get 5.3 fixed point distortion
         // coefficients for our bilinear filtering
            dispY[dst] = (signed char)(8*((cos(x/31) + cos(x*y/1783) +
                       + 2*cos((x+y)/137) + cos((y-x)/55) + 2*cos((x+8*y)/57)
                       + cos(hypot(384-x,(274-y/9))/51) )));
            dst++;
        }
    }
}


/*
 *   copy an image to the screen with added distortion.
 *   no bilinear filtering.
 */
void Distort( unsigned char *img, int x1, int y1, int x2, int y2 )
{
// setup the offsets in the buffers
     int dst = 0,
         src1 = y1*640+x1,
         src2 = y2*640+x2;
     int dX, dY;
     unsigned char c;
// loop for all lines
     for (int j=0; j<200; j++)
     {
     // for all pixels
         for (int i=0; i<320; i++)
         {
          // get distorted coordinates, use the integer part of the distortion
          // buffers and truncate to closest texel
             dY = j+(dispY[src1]>>3);
             dX = i+(dispX[src2]>>3);
          // check the texel is valid
             if ((dY>=0) && (dY<199) && (dX>=0) && (dX<319))
             {
              // copy it to the screen
                 vga->page_draw[dst] = img[dY*320+dX];
             }
          // otherwise, just set it to black
             else vga->page_draw[dst] = 0;
          // next pixel
             dst++; src1++; src2++;
         }
      // next line
         src1 += 320;
         src2 += 320;
     }
}

/*
 *   copy an image to the screen with added distortion.
 *   with bilinear filtering.
 */
void Distort_Bili( unsigned char *img, int x1, int y1, int x2, int y2 )
{
// setup the offsets in the buffers
     int dst = 0,
         src1 = y1*640+x1,
         src2 = y2*640+x2;
     int dX, dY, cX, cY;
     unsigned char c;
// loop for all lines
     for (int j=0; j<200; j++)
     {
     // for all pixels
         for (int i=0; i<320; i++)
         {
          // get distorted coordinates, by using the truncated integer part
          // of the distortion coefficients
             dY = j+(dispY[src1]>>3);
             dX = i+(dispX[src2]>>3);
          // get the linear interpolation coefficiants by using the fractionnal
          // part of the distortion coefficients
             cY = dispY[src1]&0x7;
             cX = dispX[src2]&0x7;
          // check if the texel is valid
             if ((dY>=0) && (dY<199) && (dX>=0) && (dX<319))
             {
               // load the 4 surrounding texels and multiply them by the
               // right bilinear coefficients, then get rid of the fractionnal
               // part by shifting right by 6
                  vga->page_draw[dst] = ( img[dY*320+dX] * (0x8-cX) * (0x8-cY)
                                        + img[dY*320+dX+1] * cX * (0x8-cY)
                                        + img[dY*320+dX+320] * (0x8-cX) * cY
                                        + img[dY*320+dX+321] * cX * cY ) >> 6;
             }
          // otherwise, just make it black
             else vga->page_draw[dst] = 0;
             dst++; src1++; src2++;
         }
      // next line
         src1 += 320;
         src2 += 320;
     }
}


// the main program
int main(int argc, char *argv[])
{
// set mode 320x200x8
    vga = new VGA;
// the distortion buffer
    dispX = new char[256000];
    dispY = new char[256000];
// create two distortion functions
    precalculate();
// load the background image
    TEXTURE *image = new TEXTURE( "image.png" );
// setup the greyscale palette
    for (int i=0; i<256; i++) vga->SetColour( i, i>>2, i>>2, i>>2 );
// define the distortion buffer movement
    int x1, y1, x2, y2;
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
    // move distortion buffer
       x1 = 160 + (int)(159.0f * cos( (double)currentTime/205 ));
       x2 = 160 + (int)(159.0f * sin( (double)-currentTime/197 ));
       y1 = 100 + (int)(99.0f * sin( (double)currentTime/231 ));
       y2 = 100 + (int)(99.0f * cos( (double)-currentTime/224 ));
    // draw the effect
       if ((currentTime&511)<256) Distort( image->location, x1, y1, x2, y2 );
       else Distort_Bili( image->location, x1, y1, x2, y2 );
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
    delete (image);
    delete [] (dispX);
    delete [] (dispY);
// make sure every one knows about flipcode :)
    std::cout << "                             The Art Of" << std::endl;
    std::cout << "                         D E M O M A K I N G " << std::endl << std::endl;
    std::cout << "                       by Alex J. Champandard" << std::endl << std::endl;
    std::cout << "       Go to http://www.flipcode.com/demomaking for more information." << std::endl;
    std::cout << std::endl << FPS << " frames per second." << std::endl;
// we've finished!
    return 0;
}
