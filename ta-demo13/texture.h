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


#ifndef __TEXTURE_H_
#define __TEXTURE_H_

#include <cstdio>
#include <cstdlib>

class TEXTURE
{
   public:

// constructor
   TEXTURE( const char *filename );
// destructor
   ~TEXTURE();

// memory pointers, should be private and accessible via a wrapper
// but i'm a naughty boy :)
   unsigned char *location, *palette;

};

#endif
