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
 *  Compile with DJGPP:    gcc *.cpp -s -O2 -o tor.exe -lstdcxx -lpng -lz
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
#include "vector.h"
#include "matrix.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// this handles all the graphics
VGA *vga;

// our timing module
TIMER *timer;

// the texture
TEXTURE *texture;

// buffer of 256x256 containing the light pattern (fake phong ;)
unsigned char *light;

// our 16 bit zbuffer
unsigned short *zbuffer;

// the look-up table
unsigned char *lut;

// properties of our torus
#define SLICES 32
#define SPANS 16
#define EXT_RADIUS 64
#define INT_RADIUS 24


// we need two structures, one that holds the position of all vertices
// in object space,  and the other in screen space. the coords in world
// space doesn't need to be stored
struct
{
   VECTOR *vertices, *normals;
} org, cur;

// this structure contains all the relevant data for each poly
typedef struct
{
   int p[4];  // pointer to the vertices
   int tx[4]; // static X texture index
   int ty[4]; // static Y texture index
   VECTOR normal, centre;
} POLY;

POLY *polies;

// count values
int num_polies;
int num_vertices;

// contains all our polygon drawing procedures
// not very elegant to include them like this, but it's cleaner and it works
#include "poly.h"

// object position and orientation
MATRIX objrot;
VECTOR objpos;

/*
 * cull and draw the visible polies
 */
void DrawPolies()
{
   int i;
   for (int n=0; n<num_polies; n++)
   {
    // rotate the centre and normal of the poly to check if it is actually visible.
       VECTOR ncent = objrot * polies[n].centre,
              nnorm = objrot * polies[n].normal;

    // calculate the dot product, and check it's sign
       if ((ncent[0]+objpos[0])*nnorm[0]
          +(ncent[1]+objpos[1])*nnorm[1]
          +(ncent[2]+objpos[2])*nnorm[2]<0)
       {
       // the polygon is visible, so setup the edge table
          InitEdgeTable();
       // process all our edges
          for (i=0; i<4; i++)
          {
             ScanEdge(
             // the vertex in screen space
                cur.vertices[polies[n].p[i]],
             // the static texture coordinates
                polies[n].tx[i], polies[n].ty[i],
             // the dynamic text coords computed with the normals
                (int)(65536*(128+127*cur.normals[polies[n].p[i]][0])),
                (int)(65536*(128+127*cur.normals[polies[n].p[i]][1])),
             // second vertex in screen space
                cur.vertices[polies[n].p[(i+1)&3]],
             // static text coords
                polies[n].tx[(i+1)&3], polies[n].ty[(i+1)&3],
             // dynamic texture coords
                (int)(65536*(128+127*cur.normals[polies[n].p[(i+1)&3]][0])),
                (int)(65536*(128+127*cur.normals[polies[n].p[(i+1)&3]][1]))
             );
          }
        // quick clipping
          if (poly_minY<0) poly_minY = 0;
          if (poly_maxY>200) poly_maxY = 200;
       // do we have to draw anything?
          if ((poly_minY<poly_maxY) && (poly_maxY>0) && (poly_minY<200))
          {
       // if so just draw relevant lines
             for (i=poly_minY; i<poly_maxY; i++)
             {
                 DrawSpan( i, &edge_table[i][0], &edge_table[i][1] );
             }
          }
       }
   }
}


/*
 * generate a torus object
 */
void init_object()
{
  // allocate necessary memory for points and their normals
     num_vertices = SLICES*SPANS;
     org.vertices = new VECTOR[num_vertices];
     cur.vertices = new VECTOR[num_vertices];
     org.normals = new VECTOR[num_vertices];
     cur.normals = new VECTOR[num_vertices];
     int i, j, k = 0;
  // now create all the points and their normals, start looping
  // round the origin (circle C1)
     for (i=0; i<SLICES; i++)
     {
      // find angular position
         float ext_angle = (float)i*M_PI*2.0f/SLICES,
               ca = cos( ext_angle ),
               sa = sin( ext_angle );
      // now loop round C2
         for (j=0; j<SPANS; j++)
         {
             float int_angle = (float)j*M_PI*2.0f/SPANS,
                   int_rad   = EXT_RADIUS + INT_RADIUS * cos( int_angle );
          // compute position of vertex by rotating it round C1
             org.vertices[k] = VECTOR(
                 int_rad * ca,
                 INT_RADIUS*sin( int_angle ),
                 int_rad * sa );
          // then find the normal, i.e. the normalised vector representing the
          // distance to the correpsonding point on C1
             org.normals[k] = normalize( org.vertices[k] -
                 VECTOR( EXT_RADIUS*ca, 0, EXT_RADIUS*sa ) );
             k++;
        }
     }

  // now initialize the polygons, there are as many quads as vertices
     num_polies = SPANS*SLICES;
     polies = new POLY[num_polies];
  // perform the same loop
     for (i=0; i<SLICES; i++)
     {
         for (j=0; j<SPANS; j++)
         {
             POLY &P = polies[i*SPANS+j];

          // setup the pointers to the 4 concerned vertices
             P.p[0] = i*SPANS+j;
             P.p[1] = i*SPANS+((j+1)%SPANS);
             P.p[3] = ((i+1)%SLICES)*SPANS+j;
             P.p[2] = ((i+1)%SLICES)*SPANS+((j+1)%SPANS);

          // now compute the static texture refs (X)
             P.tx[0] = (i*512/SLICES)<<16;
             P.tx[1] = (i*512/SLICES)<<16;
             P.tx[3] = ((i+1)*512/SLICES)<<16;
             P.tx[2] = ((i+1)*512/SLICES)<<16;

          // now compute the static texture refs (Y)
             P.ty[0] = (j*512/SPANS)<<16;
             P.ty[1] = ((j+1)*512/SPANS)<<16;
             P.ty[3] = (j*512/SPANS)<<16;
             P.ty[2] = ((j+1)*512/SPANS)<<16;

          // get the normalized diagonals
             VECTOR d1 = normalize( org.vertices[P.p[2]]-org.vertices[P.p[0]] ),
                    d2 = normalize( org.vertices[P.p[3]]-org.vertices[P.p[1]] ),
          // and their dot product
                    temp = VECTOR(  d1[1] * d2[2] - d1[2] * d2[1],
                                    d1[2] * d2[0] - d1[0] * d2[2],
                                    d1[0] * d2[1] - d1[1] * d2[0] );
          // normalize that and we get the face's normal
             P.normal = normalize( temp );

          // the centre of the face is just the average of the 4 corners
          // we could use this for depth sorting
             temp = org.vertices[P.p[0]] + org.vertices[P.p[1]]
                  + org.vertices[P.p[2]] + org.vertices[P.p[3]];
             P.centre = VECTOR( temp[0]*0.25, temp[1]*0.25, temp[2]*0.25 );
         }
     }
}

/*
 * rotate and project all vertices, and just rotate point normals
 */
void TransformPts()
{
   for (int i=0; i<num_vertices; i++)
   {
    // perform rotation
       cur.normals[i] = objrot * org.normals[i];
       cur.vertices[i] = objrot * org.vertices[i];
    // now project onto the screen
       cur.vertices[i][2] += objpos[2];
       cur.vertices[i][0] = 200 * ( cur.vertices[i][0] + objpos[0] ) / cur.vertices[i][2] + 160;
       cur.vertices[i][1] = 200 * ( cur.vertices[i][1] + objpos[1] ) / cur.vertices[i][2] + 100;
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
    // the first 224 colours are identical to the original colour
       for (i=0; i<224; i++)
       {
           lut[(j<<8)+i] = Closest_Colour( pal, r, g, b );
       }
    // now fade to white for the last 32 colours
       for (i=0; i<32; i++)
       {
           lut[(j<<8)+224+i] = Closest_Colour( pal, r+(255-r)*i/32, g+(255-g)*i/32, b+(255-b)*i/32 );
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
    light = new unsigned char[256*256];
    for (j=0; j<256; j++)
    {
       for (i=0; i<256; i++)
       {
       // calculate distance from the centre
          int c = ((128-i)*(128-i)+(128-j)*(128-j))/80;
       // check for overflow
          if (c>255) c = 255;
       // store lumel
          light[(j<<8)+i] = 255-c;
       }
    }
// compute the lookup table
    lut = new unsigned char[65536];
    Compute_LUT( texture->palette );
// prepare 3D data
    zbuffer = new unsigned short[64000];
    init_object();
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
       // clear the background
          vga->Clear();
       // set the torus' rotation
          objrot = rotX( (float)currentTime/1124548.0f + M_PI*cos( (float)currentTime/2234117.0f ))
                  * rotY( (float)currentTime/1345179.0f + M_PI*sin( (float)currentTime/2614789.0f ) )
                  * rotZ( (float)currentTime/1515713.0f - M_PI*cos( (float)currentTime/2421376.0f ));
       // and it's position
          objpos = VECTOR(
            48*cos( (float)currentTime/1266398.0f ),
            48*sin( (float)currentTime/1424781.0f ),
            200+80*sin( (float)currentTime/1912378.0f ) );
       // rotate and project our points
          TransformPts();
       // and draw the polygons
          DrawPolies();
       // flush to video ram
          vga->Update();
          frameCount++;
    }
// calculate total running time
    long long totalTime = timer->getCount() - startTime;
// and turn off the timer
    delete timer;
// now get FPS
    float FPS = (float)(frameCount)*1193180.0f / (float)(totalTime);
// return to text mode
    delete vga;
// free memory
    delete (texture);
    delete [] (zbuffer);
    delete [] (lut);
    delete [] (org.vertices);
    delete [] (cur.vertices);
    delete [] (org.normals);
    delete [] (cur.normals);
    delete [] (polies);
// make sure every one knows about flipcode :)
    std::cout << "                             The Art Of" << std::endl;
    std::cout << "                         D E M O M A K I N G " << std::endl << std::endl;
    std::cout << "                       by Alex J. Champandard" << std::endl << std::endl;
    std::cout << "       Go to http://www.flipcode.com/demomaking for more information." << std::endl;
    std::cout << std::endl << FPS << " frames per second." << std::endl;
// we've finished!
    return 0;
}
