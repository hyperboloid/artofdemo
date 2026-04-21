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


#include <cstring>
#include "vga.h"

#define SCREEN_W 320
#define SCREEN_H 200
#define SCALE 3

/*
 * initialize mode 320x200x8 and prepare the double buffering
 */
VGA::VGA()
{
   SDL_Init( SDL_INIT_VIDEO );
   window = SDL_CreateWindow( "Flubber - The Art of Demomaking",
       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
       SCREEN_W * SCALE, SCREEN_H * SCALE, 0 );
   renderer = SDL_CreateRenderer( window, -1, SDL_RENDERER_PRESENTVSYNC );
   SDL_RenderSetLogicalSize( renderer, SCREEN_W, SCREEN_H );
   texture = SDL_CreateTexture( renderer, SDL_PIXELFORMAT_ARGB8888,
       SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H );

// setup double buffering
   page_draw = new unsigned char[64000];
// clear our temporary page
   memset( page_draw, 0, 64000 );
// clear palette
   memset( palette, 0, sizeof(palette) );
}

/*
 * set a colour in the palette (values 0-63, like original VGA)
 */
void VGA::SetColour( unsigned char i, unsigned char r, unsigned char g, unsigned char b )
{
   palette[i][0] = r;
   palette[i][1] = g;
   palette[i][2] = b;
}

/*
 * return to text mode, and clean up
 */
VGA::~VGA()
{
   delete [] page_draw;
   SDL_DestroyTexture( texture );
   SDL_DestroyRenderer( renderer );
   SDL_DestroyWindow( window );
   SDL_Quit();
}

/*
 * flip the double buffer to the screen
 */
void VGA::Update()
{
// convert indexed page_draw to ARGB8888 and upload to texture
   Uint32 pixels[SCREEN_W * SCREEN_H];
   for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
   {
      unsigned char idx = page_draw[i];
      // VGA palette is 0-63, scale to 0-255
      Uint32 r = palette[idx][0] * 255 / 63;
      Uint32 g = palette[idx][1] * 255 / 63;
      Uint32 b = palette[idx][2] * 255 / 63;
      pixels[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
   }
   SDL_UpdateTexture( texture, NULL, pixels, SCREEN_W * sizeof(Uint32) );
   SDL_RenderClear( renderer );
   SDL_RenderCopy( renderer, texture, NULL, NULL );
   SDL_RenderPresent( renderer );
}

/*
 * draw a pixel in the temporary buffer
 */
void VGA::PutPixel( int x, int y, unsigned c )
{
// clipping: check if pixel is out of the screen
   if ((x<0) || (x>319) || (y<0) || (y>199)) return;
// set the memory
   page_draw[(y<<8)+(y<<6)+x] = c;  // same as y*320+x, but slightly quicker
}

/*
 * clear the double buffer
 */
void VGA::Clear()
{
   memset( page_draw, 0, 64000 );
}
