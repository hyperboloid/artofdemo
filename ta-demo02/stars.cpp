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
 *   Compile with DJGPP:    gcc *.cpp -s -O2 -o stars.exe -lstdcxx
 *
 */

#include <iostream>
#include <stdlib.h>    // for function rand()
#include <conio>     //              kbhit()
#include "vga.h"

// change this to adjust the number of stars
#define MAXSTARS 256

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
int main()
{
// allocate memory for all our stars
    stars = new TStar[MAXSTARS];
// randomly generate some stars
    for (int i=0; i<MAXSTARS; i++)
    {
        stars[i].x = rand() % 320;
        stars[i].y = rand() % 200;
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
    while (!kbhit())
    {
    // clear the temporary buffer
       vga->Clear();
    // update all stars
       for (int i=0; i<MAXSTARS; i++)
       {
        // move this star right, determine how fast depending on which
        // plane it belongs to
           stars[i].x += (1+(float)stars[i].plane)*0.15;
        // check if it's gone out of the right of the screen
           if (stars[i].x>320)
           {
           // if so, make it return to the left
              stars[i].x = 0;
           // and randomly change the y position
              stars[i].y = rand() % 200;
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
    cout << "                             The Art Of" << endl;
    cout << "                         D E M O M A K I N G " << endl << endl;
    cout << "                       by Alex J. Champandard" << endl << endl;
    cout << "       Go to http://www.flipcode.com/demomaking for more information." << endl;
// we've finished!
    return 0;
}
