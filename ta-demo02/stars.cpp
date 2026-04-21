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
 *       g++ *.cpp -std=c++11 -O2 -o stars $(sdl2-config --cflags --libs)
 *
 */

#include <iostream>
#include <cstdlib>
#include <SDL.h>
#include "vga.h"

// change this to adjust the number of stars
#define MAXSTARS 256
#define WIDTH 320
#define HEIGHT 200

// this record contains the information for one star
struct TStar
{
       float x, y;             // position of the star
       unsigned char plane;    // remember which plane it belongs to
};

// this is a pointer to an array of stars
TStar *stars;

// this handles all the graphics
VGA *vga;

// the main program
int main(int argc, char *argv[])
{
// allocate memory for all our stars
    stars = new TStar[MAXSTARS];
// randomly generate some stars
    for (int i=0; i<MAXSTARS; i++)
    {
        stars[i].x = rand() % WIDTH;
        stars[i].y = rand() % HEIGHT;
        stars[i].plane = rand() % 3;     // star colour between 0 and 2
    }
// set mode 320x200x8
    vga = new VGA;
// we need 4 colours shaded from black to white
    vga->SetColour( 0,  0,  0,  0 );  // black (default for index 0 anyway)
    vga->SetColour( 1, 24, 24, 24 );  // dark grey
    vga->SetColour( 2, 48, 48, 48 );  // light grey
    vga->SetColour( 3, 63, 63, 63 );  // white
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
    // clear the temporary buffer
       vga->Clear();
    // update all stars
       for (int i=0; i<MAXSTARS; i++)
       {
        // move this star down, determine how fast depending on which
        // plane it belongs to
           stars[i].y += (0.8+(float)stars[i].plane)*0.10;

	//randomly shake left or right
	if ((int)stars[i].y % 3 == 0) {
		int move = rand() % 3;
		move--;
		if (stars[i].x+move >= 0 && stars[i].x+move < WIDTH) {
			stars[i].x += (float)move*0.5;
		}
		else {
			stars[i].x -= (float)move*0.5;
		}
	}
        // check if it's gone out of the bottom of the screen
           if (stars[i].y>HEIGHT)
           {
           // if so, make it return to the left
              stars[i].y = 0;
           // and randomly change the y position
              stars[i].x = rand() % WIDTH;
              stars[i].plane = rand() % 3;
           }
        // draw this star, with a colour depending on the plane
           vga->PutPixel( (int)stars[i].x, (int)stars[i].y, 1+stars[i].plane );
       }
    // dump our temporary buffer to the screen
       vga->Update();
    }
// return to text mode
    delete (vga);
// free memory
    delete [] (stars);
// make sure every one knows about flipcode :)
    std::cout << "                             The Art Of" << std::endl;
    std::cout << "                         D E M O M A K I N G " << std::endl << std::endl;
    std::cout << "                       by Alex J. Champandard" << std::endl << std::endl;
    std::cout << "       Go to http://www.flipcode.com/demomaking for more information." << std::endl;
// we've finished!
    return 0;
}
