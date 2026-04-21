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
 *   Ported to SDL2 for macOS.
 */


#ifndef __VGA_H_
#define __VGA_H_

#include <SDL.h>

class VGA
{
   public:

// constructor
   VGA();
// destructor
   ~VGA();
// dumps temporary buffer to the screen
   void Update();
// draw a pixel in temporary buffer
   void PutPixel( int x, int y, unsigned c );
// set a color in the palette
   void SetColour( unsigned char i, unsigned char r, unsigned char g, unsigned char b );
// clear temp buf
   void Clear();

// kids! don't try this at home :)
// private:

// double buffer (indexed color, 320x200)
   unsigned char *page_draw;
// palette: 256 entries, each with r, g, b (0-63 range like VGA)
   unsigned char palette[256][3];
// SDL objects
   SDL_Window *window;
   SDL_Renderer *renderer;
   SDL_Texture *texture;
};

#endif
