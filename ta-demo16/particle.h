#define MAXPTS 4096

MATRIX obj;
float base_dist;
int scaleX[320];
int scaleY[200];
unsigned char *page1, *page2;
VECTOR *part_pts;

void Rescale( unsigned char *src, unsigned char *dst )
{
    long offs = 0;
    for (int j=0; j<200; j++)
        for (int i=0; i<320; i++)
        { dst[offs] = src[scaleY[j]*320+scaleX[i]]; offs++; }
}

void Blur( unsigned char *src, unsigned char *dst )
{
     int offs = 320;
     unsigned char b;
     for (int j=1; j<199; j++)
     {
         dst[offs] = 0; offs++;
         for (int i=1; i<319; i++)
         {
             b = ( src[offs-321] + src[offs-320] + src[offs-319]
                 + src[offs-1]   +               + src[offs+1]
                 + src[offs+319] + src[offs+320] + src[offs+321] ) >> 3;
             if (b>16) b-=16; else b=0;
             dst[offs] = b; offs++;
         }
         dst[offs] = 0; offs++;
     }
}

void Draw( unsigned char *where, VECTOR v )
{
     float iz = 1/(v[2]+base_dist),
           x = 160+160*v[0]*iz,
           y = 100+160*v[1]*iz;
     if ((x<0) || (x>319) || (y<0) || (y>199)) return;
     int sx = (int)(x*8.0f), sy = (int)(y*8.0f);
     long offs = (sy>>3)*320+(sx>>3);
     sx = sx & 0x7; sy = sy & 0x7;
     int c = where[offs];   c += (7-sx)*(7-sy); if (c>255) c = 255; where[offs] = c;
     c = where[offs+1];     c += (7-sy)*sx;      if (c>255) c = 255; where[offs+1] = c;
     c = where[offs+320];   c += sy*(7-sx);      if (c>255) c = 255; where[offs+320] = c;
     c = where[offs+321];   c += sy*sx;          if (c>255) c = 255; where[offs+321] = c;
}

void Particle_Init()
{
    part_pts = new VECTOR[MAXPTS];
    for (int i=0; i<MAXPTS; i++)
        part_pts[i] = ( rotX( 2.0f*M_PI*sin( (float)i/203 ) )
                 * rotY( 2.0f*M_PI*cos( (float)i/157 ) )
                 * rotZ( -2.0f*M_PI*cos( (float)i/181 ) ) ) * VECTOR( 64+16*sin( (float)i/191 ), 0, 0 );
   page1 = new unsigned char[64000];
   page2 = vga->page_draw;
}

void Particle_Run()
{
    vga->Clear();
    vga->Update();
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

       long long currentTime = timer->getCount()>>14;
       unsigned char r = (unsigned char)(32 + 31 * cos( (float)currentTime/74 )),
                     g = (unsigned char)(32 + 31 * cos( (float)currentTime/63 )),
                     b = (unsigned char)(32 + 31 * cos( (float)currentTime/81 ));
       for (i=0; i<128; i++)
       {
           vga->SetColour( i, (unsigned char)((r*i/128)*(1-flash_coef)+63*flash_coef),
                              (unsigned char)((g*i/128)*(1-flash_coef)+63*flash_coef),
                              (unsigned char)((b*i/128)*(1-flash_coef)+63*flash_coef) );
           vga->SetColour( 128+i, (unsigned char)((r+(63-r)*i/128)*(1-flash_coef)+63*flash_coef),
                                  (unsigned char)((g+(63-g)*i/128)*(1-flash_coef)+63*flash_coef),
                                  (unsigned char)((b+(63-b)*i/128)*(1-flash_coef)+63*flash_coef) );
       }
       int sx = (int)(160-80*sin((float)currentTime/159)),
           sy = (int)(100+50*sin((float)currentTime/211));
       for (i=0; i<320; i++) scaleX[i] = (int)(sx+(i-sx)*0.85f);
       for (i=0; i<200; i++) scaleY[i] = (int)(sy+(i-sy)*0.85f);
       Rescale( page2, page1 );
       Blur( page1, page2 );
       base_dist = 256 + 64 * sin( (float)currentTime/327 );
       obj = rotX( 2.0f*M_PI*sin( (float)currentTime/289 ) )
           * rotY( 2.0f*M_PI*cos( (float)currentTime/307 ) )
           * rotZ( -2.0f*M_PI*sin( (float)currentTime/251 ) );
       for (i=0; i<MAXPTS; i++) Draw( page2, obj * part_pts[i] );
       vga->Update();
       frameCount++;
    }
}

void Particle_Done()
{
    delete [] page1;
    delete [] part_pts;
}
