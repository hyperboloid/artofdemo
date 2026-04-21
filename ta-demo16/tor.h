unsigned char *tor_light;
unsigned char *tor_lut;
TEXTURE *tor_text;

#define TSLICES 32
#define TSPANS 16
#define TEXT_RADIUS 64
#define TINT_RADIUS 24

struct { VECTOR *vertices, *normals; } torg, tcur;
typedef struct { int p[4]; int tx[4]; int ty[4]; VECTOR normal, centre; } TPOLY;
TPOLY *tpolies;
int tnum_polies, tnum_vertices;

#include "poly.h"

MATRIX tobjrot;
VECTOR tobjpos;

void TDrawPolies()
{
   int i;
   for (int n=0; n<tnum_polies; n++)
   {
       VECTOR ncent = tobjrot * tpolies[n].centre,
              nnorm = tobjrot * tpolies[n].normal;
       if ((ncent[0]+tobjpos[0])*nnorm[0]
          +(ncent[1]+tobjpos[1])*nnorm[1]
          +(ncent[2]+tobjpos[2])*nnorm[2]<0)
       {
          InitEdgeTable();
          for (i=0; i<4; i++)
          {
             ScanEdge(
                tcur.vertices[tpolies[n].p[i]],
                tpolies[n].tx[i], tpolies[n].ty[i],
                (int)(65536*(128+127*tcur.normals[tpolies[n].p[i]][0])),
                (int)(65536*(128+127*tcur.normals[tpolies[n].p[i]][1])),
                tcur.vertices[tpolies[n].p[(i+1)&3]],
                tpolies[n].tx[(i+1)&3], tpolies[n].ty[(i+1)&3],
                (int)(65536*(128+127*tcur.normals[tpolies[n].p[(i+1)&3]][0])),
                (int)(65536*(128+127*tcur.normals[tpolies[n].p[(i+1)&3]][1]))
             );
          }
          if (poly_minY<0) poly_minY = 0;
          if (poly_maxY>200) poly_maxY = 200;
          if ((poly_minY<poly_maxY) && (poly_maxY>0) && (poly_minY<200))
          {
             for (i=poly_minY; i<poly_maxY; i++)
                 DrawSpan( i, &edge_table[i][0], &edge_table[i][1] );
          }
       }
   }
}

void tinit_object()
{
     tnum_vertices = TSLICES*TSPANS;
     torg.vertices = new VECTOR[tnum_vertices];
     tcur.vertices = new VECTOR[tnum_vertices];
     torg.normals = new VECTOR[tnum_vertices];
     tcur.normals = new VECTOR[tnum_vertices];
     int i, j, k = 0;
     for (i=0; i<TSLICES; i++)
     {
         float ext_angle = (float)i*M_PI*2.0f/TSLICES, ca = cos(ext_angle), sa = sin(ext_angle);
         for (j=0; j<TSPANS; j++)
         {
             float int_angle = (float)j*M_PI*2.0f/TSPANS,
                   int_rad = TEXT_RADIUS + TINT_RADIUS * cos(int_angle);
             torg.vertices[k] = VECTOR(int_rad*ca, TINT_RADIUS*sin(int_angle), int_rad*sa);
             torg.normals[k] = normalize(torg.vertices[k] - VECTOR(TEXT_RADIUS*ca, 0, TEXT_RADIUS*sa));
             k++;
         }
     }
     tnum_polies = TSPANS*TSLICES;
     tpolies = new TPOLY[tnum_polies];
     for (i=0; i<TSLICES; i++)
     {
         for (j=0; j<TSPANS; j++)
         {
             TPOLY &P = tpolies[i*TSPANS+j];
             P.p[0] = i*TSPANS+j; P.p[1] = i*TSPANS+((j+1)%TSPANS);
             P.p[3] = ((i+1)%TSLICES)*TSPANS+j; P.p[2] = ((i+1)%TSLICES)*TSPANS+((j+1)%TSPANS);
             P.tx[0] = (i*512/TSLICES)<<16; P.tx[1] = (i*512/TSLICES)<<16;
             P.tx[3] = ((i+1)*512/TSLICES)<<16; P.tx[2] = ((i+1)*512/TSLICES)<<16;
             P.ty[0] = (j*512/TSPANS)<<16; P.ty[1] = ((j+1)*512/TSPANS)<<16;
             P.ty[3] = (j*512/TSPANS)<<16; P.ty[2] = ((j+1)*512/TSPANS)<<16;
             VECTOR d1 = normalize(torg.vertices[P.p[2]]-torg.vertices[P.p[0]]),
                    d2 = normalize(torg.vertices[P.p[3]]-torg.vertices[P.p[1]]),
                    temp = VECTOR(d1[1]*d2[2]-d1[2]*d2[1], d1[2]*d2[0]-d1[0]*d2[2], d1[0]*d2[1]-d1[1]*d2[0]);
             P.normal = normalize(temp);
             temp = torg.vertices[P.p[0]]+torg.vertices[P.p[1]]+torg.vertices[P.p[2]]+torg.vertices[P.p[3]];
             P.centre = VECTOR(temp[0]*0.25, temp[1]*0.25, temp[2]*0.25);
         }
     }
}

void TTransformPts()
{
   for (int i=0; i<tnum_vertices; i++)
   {
       tcur.normals[i] = tobjrot * torg.normals[i];
       tcur.vertices[i] = tobjrot * torg.vertices[i];
       tcur.vertices[i][2] += tobjpos[2];
       tcur.vertices[i][0] = 200 * (tcur.vertices[i][0]+tobjpos[0]) / tcur.vertices[i][2] + 160;
       tcur.vertices[i][1] = 200 * (tcur.vertices[i][1]+tobjpos[1]) / tcur.vertices[i][2] + 100;
   }
}

void Compute_LUT2( unsigned char *pal )
{
   unsigned char r, g, b; int i;
   for (int j=0; j<256; j++)
   {
       r = pal[j*3]; g = pal[j*3+1]; b = pal[j*3+2];
       for (i=0; i<224; i++) tor_lut[(j<<8)+i] = Closest_Colour(pal, r, g, b);
       for (i=0; i<32; i++) tor_lut[(j<<8)+224+i] = Closest_Colour(pal, r+(255-r)*i/32, g+(255-g)*i/32, b+(255-b)*i/32);
   }
}

void Tor_Init()
{
    tor_text = new TEXTURE( "tor.png" );
    int i, j;
    tor_light = new unsigned char[256*256];
    for (j=0; j<256; j++)
       for (i=0; i<256; i++)
       { int c = ((128-i)*(128-i)+(128-j)*(128-j))/80; if (c>255) c=255; tor_light[(j<<8)+i]=255-c; }
    tor_lut = new unsigned char[65536];
    Compute_LUT2( tor_text->palette );
    tinit_object();
}

void Tor_Run()
{
    int i;
    bQuit = false;
    SDL_Event event;
    while (!bQuit && !effectTimeUp())
    {
       while (SDL_PollEvent(&event))
       {
           if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN)
               { bQuit = true; globalQuit = true; }
       }
       if (bQuit) break;

          for (i=0; i<256; i++)
          {
              vga->SetColour( i,
              (unsigned char)((tor_text->palette[i*3]>>2)*(1-flash_coef)+63*flash_coef),
              (unsigned char)((tor_text->palette[i*3+1]>>2)*(1-flash_coef)+63*flash_coef),
              (unsigned char)((tor_text->palette[i*3+2]>>2)*(1-flash_coef)+63*flash_coef) );
          }
          long long currentTime = timer->getCount();
          memset( zbuffer, 255, 128000 );
          vga->Clear();
          tobjrot = rotX((float)currentTime/1124548.0f + M_PI*cos((float)currentTime/2234117.0f))
                  * rotY((float)currentTime/1345179.0f + M_PI*sin((float)currentTime/2614789.0f))
                  * rotZ((float)currentTime/1515713.0f - M_PI*cos((float)currentTime/2421376.0f));
          tobjpos = VECTOR(
            48*cos((float)currentTime/1266398.0f),
            48*sin((float)currentTime/1424781.0f),
            200+80*sin((float)currentTime/1912378.0f));
          TTransformPts();
          TDrawPolies();
          vga->Update();
          frameCount++;
    }
}

void Tor_Done()
{
    delete tor_text;
    delete [] tor_lut;
    delete [] torg.vertices;
    delete [] tcur.vertices;
    delete [] torg.normals;
    delete [] tcur.normals;
    delete [] tpolies;
}
