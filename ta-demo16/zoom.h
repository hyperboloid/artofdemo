unsigned char *frac1, *frac2, *fractmp;

#define OR_VAL    -0.577816-9.31323E-10-1.16415E-10
#define OI_VAL    -0.631121-2.38419E-07+1.49012E-08

double dr, di, pr, zpi, sr, si;
long zoffs;
bool zoom_in;
double zx, zy;

void Start_Frac( double _sr, double _si, double er, double ei )
{
   dr = (er-_sr)/640.0f; di = (ei-_si)/400.0f;
   pr = _sr; zpi = _si; sr = _sr; si = _si; zoffs = 0;
}

void Compute_Frac()
{
   for (int j=0; j<4; j++)
   {
       pr = sr;
       for (int i=0; i<640; i++)
       {
           unsigned char c = 0;
           double vi = zpi, vr = pr, nvi, nvr;
           while ((vr*vr + vi*vi<4) && (c<255))
           { nvr = vr*vr - vi*vi + pr; nvi = 2*vi*vr + zpi; vi = nvi; vr = nvr; c++; }
           frac1[zoffs] = c; zoffs++; pr += dr;
       }
       zpi += di;
   }
}

void Done_Frac()
{ fractmp = frac1; frac1 = frac2; frac2 = fractmp; }

void Zoom( double z )
{
     int width = (int)((640<<16)/(256.0f*(1+z)))<<8,
         height = (int)((400<<16)/(256.0f*(1+z)))<<8,
         startx = ((640<<16)-width)>>1, starty = ((400<<16)-height)>>1,
         deltax = width/320, deltay = height/200, px, py = starty;
     long offs = 0;
     for (int j=0; j<200; j++)
     {
         px = startx;
         for (int i=0; i<320; i++)
         {
             vga->page_draw[offs] =
             (    frac2[(py>>16)*640+(px>>16)] * (0x100-((py>>8)&0xff)) * (0x100-((px>>8)&0xff))
                + frac2[(py>>16)*640+((px>>16)+1)] * (0x100-((py>>8)&0xff)) * ((px>>8)&0xff)
                + frac2[((py>>16)+1)*640+(px>>16)] * ((py>>8)&0xff) * (0x100-((px>>8)&0xff))
                + frac2[((py>>16)+1)*640+((px>>16)+1)] * ((py>>8)&0xff) * ((px>>8)&0xff) ) >> 16;
             px += deltax; offs++;
         }
         py += deltay;
     }
}

void updatePalette( int t )
{
    for (int i=0; i<256; i++)
    {
        vga->SetColour( i,
           (unsigned char)(32 - 31 * cos( i * PI / 128 + t*0.00141 )),
           (unsigned char)(32 - 31 * cos( i * PI / 128 + t*0.00141 )),
           (unsigned char)(32 - 31 * cos( i * PI / 64 + t*0.0136 )) );
    }
}

void Zoom_Init()
{
    int j;
    frac1 = new unsigned char[256000];
    frac2 = new unsigned char[256000];
    zx = 4.0; zy = 4.0; zoom_in = true;
    Start_Frac( OR_VAL-zx, OI_VAL-zy, OR_VAL+zx, OI_VAL+zy );
    for (j=0; j<100; j++) Compute_Frac();
    Done_Frac();
}

void Zoom_Run()
{
    int j, k = 0;
    updatePalette( 0 );
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

         if (zoom_in) { zx *= 0.5; zy *= 0.5; }
         else { zx *= 2; zy *= 2; }
         Start_Frac( OR_VAL-zx, OI_VAL-zy, OR_VAL+zx, OI_VAL+zy );
         j = 0;
         while ((j<100) && !bQuit && !effectTimeUp())
         {
             while (SDL_PollEvent(&event))
             {
                 if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN)
                     { bQuit = true; globalQuit = true; }
             }
             if (bQuit) break;
             j++;
             Compute_Frac();
             if (zoom_in) Zoom( (double)j/100.0f );
             else Zoom( 1.0f-(double)j/100.0f );
             updatePalette( k*100+j );
             vga->Update();
             frameCount++;
         }
         k++;
         if (k%38==0)
         {
            zoom_in = !zoom_in;
            if (zoom_in) { zx *= 0.5; zy *= 0.5; }
            else { zx *= 2.0; zy *= 2.0; }
            fractmp = frac1; frac1 = frac2; frac2 = fractmp;
         }
         Done_Frac();
    }
}

void Zoom_Done()
{
    delete [] frac1;
    delete [] frac2;
}
