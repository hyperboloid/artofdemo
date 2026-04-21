TEXTURE *flub_text;
long text_offs;
unsigned char *light;
unsigned short *zbuffer;
unsigned char *flub_lut;

#define SPANS 32
#define SPANMASK 31

struct { float x, z; } fpts[200][SPANS], fnrm[200][SPANS];

void DrawSpan( int y, int x1, int tx1, int px1, int z1,
                      int x2, int tx2, int px2, int z2 )
{
     if (x1>=x2) return;
     int dx = x2-x1, dtx = (tx2-tx1)/dx, dpx = (px2-px1)/dx, dz = (z2-z1)/dx;
     long offs = y*320+x1;
     for (int i=0; i<dx; i++)
     {
         if (z1<zbuffer[offs])
         {
            int texel = flub_text->location[(((y+text_offs)<<8)&0xff00)+((tx1>>16)&0xff)],
                lumel = light[(px1>>16)&0xff];
            vga->page_draw[offs] = flub_lut[(texel<<8)+lumel];
            zbuffer[offs] = z1;
         }
         px1 += dpx; tx1 += dtx; z1 += dz; offs++;
     }
}

unsigned char Closest_Colour( unsigned char *pal, unsigned char r,
                              unsigned char g, unsigned char b )
{
     long dist = 1<<30, newDist;
     unsigned char index;
     for (int i=0; i<256; i++)
     {
         newDist = (r-pal[i*3])*(r-pal[i*3])
                 + (g-pal[i*3+1])*(g-pal[i*3+1])
                 + (b-pal[i*3+2])*(b-pal[i*3+2]);
         if (newDist==0) return i;
         if (newDist<dist) { index = i; dist = newDist; }
     }
     return index;
}

void Compute_LUT1( unsigned char *pal )
{
   unsigned char r, g, b;
   int i;
   for (int j=0; j<256; j++)
   {
       r = pal[j*3]; g = pal[j*3+1]; b = pal[j*3+2];
       for (i=0; i<240; i++)
           flub_lut[(j<<8)+i] = Closest_Colour( pal, r*(120+i)/360, g*(120+i)/360, b*(120+i)/360 );
       for (i=0; i<16; i++)
           flub_lut[(j<<8)+240+i] = Closest_Colour( pal, r+(255-r)*i/31, g+(255-g)*i/31, b+(255-b)*i/31 );
   }
}

void Flubber_Init()
{
    int i, j;
    flub_text = new TEXTURE( "flubber.png" );
    light = new unsigned char[256];
    for (i=0; i<256; i++)
    { int c = abs(255-i*2); if (c>255) c = 255; light[i] = 255-c; }
    flub_lut = new unsigned char[65536];
    Compute_LUT1( flub_text->palette );
    zbuffer = new unsigned short[64000];
    for (i=0; i<SPANS; i++)
    {
        float angle = (float)i*M_PI*2/SPANS, ca = cos(angle), sa = sin(angle);
        for (j=0; j<200; j++)
        { float radx = 32, rady = 96; fpts[j][i].x = radx*ca; fpts[j][i].z = rady*sa; }
    }
    for (i=0; i<SPANS; i++)
        for (j=0; j<200; j++)
        {
            float nx = fpts[j][(i+1)&SPANMASK].x - fpts[j][(i-1)&SPANMASK].x,
                  nz = fpts[j][(i+1)&SPANMASK].z - fpts[j][(i-1)&SPANMASK].z,
                  inorm = 1/sqrt(nx*nx+nz*nz);
            fnrm[j][i].x = nx*inorm; fnrm[j][i].z = nz*inorm;
        }
}

void Flubber_Run()
{
    int i, j;
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
              (unsigned char)((flub_text->palette[i*3]>>2)*(1-flash_coef)+63*flash_coef),
              (unsigned char)((flub_text->palette[i*3+1]>>2)*(1-flash_coef)+63*flash_coef),
              (unsigned char)((flub_text->palette[i*3+2]>>2)*(1-flash_coef)+63*flash_coef) );
          }
          long long currentTime = timer->getCount();
          memset( zbuffer, 255, 128000 );
          text_offs = currentTime / 31456;
          vga->Clear();
          for (i=0; i<200; i++)
          {
              int xc[SPANS], zc[SPANS], nc[SPANS];
              float angle = (float)currentTime / 1948613 +
              2 * M_PI * (cos( (float)currentTime/3179640 )
              * cos( -(float)i / 193 ) * sin( (float)currentTime/2714147 ) + 1 );
              float ca = 256 * cos(angle), sa = 256 * sin(angle);
              long xoffs = (long)(40960+10240 * cos( (float)currentTime/2945174 ) * sin( (float)i/59 ) );
              for (j=0; j<SPANS; j++)
              {
                  xc[j] = xoffs+(long)( fpts[i][j].x * ca + fpts[i][j].z * -sa );
                  zc[j] = (long)( fpts[i][j].x * sa + fpts[i][j].z * ca );
                  long tmp = (long)( fnrm[i][j].x * ca + fnrm[i][j].z * -sa );
                  tmp = 256-tmp;
                  nc[j] = (128<<16)+(tmp<<15);
              }
              for (j=0; j<SPANS; j++)
              {
                  DrawSpan( i, xc[j]>>8, j<<20, nc[j], 49152+zc[j],
                            xc[(j+1)&SPANMASK]>>8, (j+1)<<20,
                            nc[(j+1)&SPANMASK], 49152+zc[(j+1)&SPANMASK] );
              }
          }
          vga->Update();
          frameCount++;
    }
}

void Flubber_Done()
{
    delete flub_text;
    delete [] zbuffer;
    delete [] flub_lut;
}
