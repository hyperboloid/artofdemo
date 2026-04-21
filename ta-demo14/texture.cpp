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
 *   Ported to SDL2_image for macOS.
 */


#include <SDL.h>
#include <SDL_image.h>
#include <cstring>
#include "texture.h"

TEXTURE::TEXTURE( const char *filename )
{
   location = nullptr;
   palette = nullptr;

   SDL_Surface *surface = IMG_Load( filename );
   if (!surface) {
      fprintf( stderr, "Could not load %s: %s\n", filename, IMG_GetError() );
      return;
   }

   // If the surface has a palette (8-bit indexed), extract it
   if (surface->format->palette)
   {
      int ncolors = surface->format->palette->ncolors;
      palette = new unsigned char[768];
      memset( palette, 0, 768 );
      for (int i = 0; i < ncolors && i < 256; i++)
      {
         palette[i*3]   = surface->format->palette->colors[i].r;
         palette[i*3+1] = surface->format->palette->colors[i].g;
         palette[i*3+2] = surface->format->palette->colors[i].b;
      }

      // Copy pixel data directly (already indexed)
      int size = surface->w * surface->h;
      location = new unsigned char[size];
      unsigned char *src = (unsigned char *)surface->pixels;
      for (int y = 0; y < surface->h; y++)
      {
         memcpy( location + y * surface->w,
                 src + y * surface->pitch,
                 surface->w );
      }
   }
   else
   {
      // Convert to 8-bit greyscale
      int size = surface->w * surface->h;
      location = new unsigned char[size];
      SDL_Surface *conv = SDL_ConvertSurfaceFormat( surface, SDL_PIXELFORMAT_ARGB8888, 0 );
      for (int y = 0; y < conv->h; y++)
      {
         Uint32 *row = (Uint32 *)((Uint8 *)conv->pixels + y * conv->pitch);
         for (int x = 0; x < conv->w; x++)
         {
            Uint8 r, g, b;
            SDL_GetRGB( row[x], conv->format, &r, &g, &b );
            location[y * conv->w + x] = (unsigned char)((r + g + b) / 3);
         }
      }
      SDL_FreeSurface( conv );
   }

   SDL_FreeSurface( surface );
}

TEXTURE::~TEXTURE()
{
   if (location) delete [] location;
   if (palette) delete [] palette;
}
