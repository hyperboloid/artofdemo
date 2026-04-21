unsigned char *plasma1, *plasma2;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define PI M_PI

/*
 * generate two 640x400 buffers containing our precalculated plasma functions
 */
void precalculate( unsigned char *buf1, unsigned char *buf2 )
{
    int i, j, dst = 0;
    for (j=0; j<400; j++)
    {
        for (i=0; i<640; i++)
        {
           buf1[dst] = (unsigned char)( 64 + 63 * ( sin( (double)hypot( 200-j, 320-i )/16 ) ) );
           buf2[dst] = (unsigned char)( 64 + 63 * sin( (float)i/(37+15*cos((float)j/74)) )
                                                * cos( (float)j/(31+11*sin((float)i/57))) );
           dst++;
        }
    }
}

void Plasma_Init()
{
// the two function buffers sized 640x400
    plasma1 = new unsigned char[256000];
    plasma2 = new unsigned char[256000];
// precalculate our 2 functions
    precalculate( plasma1, plasma2 );
}

void Plasma_Run()
{
// define the plasma movement
    int x1, y1, x2, y2, x3, y3;
    long dst, i, j, src1, src2, src3;
    bQuit = false;
    frameCount = 0;
    fade_coef = 1.0;
    flash_coef = 0;
// run the main loop
    SDL_Event event;
    while (!bQuit && !effectTimeUp())
    {
       while (SDL_PollEvent(&event))
       {
           if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN)
               { bQuit = true; globalQuit = true; }
       }
       if (bQuit) break;

    // we get the time for this frame only once
       long long currentTime = timer->getCount()>>14;
    // setup some nice colours
       for (i=0; i<256; i++)
       {
           unsigned char r = (unsigned char)((32 + 31 * cos( i * PI / 128 + (double)currentTime/37 )) * fade_coef / 64),
                         g = (unsigned char)((32 + 31 * sin( i * PI / 128 + (double)currentTime/35 )) * fade_coef / 64),
                         b = (unsigned char)((32 - 31 * cos( i * PI / 128 + (double)currentTime/41 )) * fade_coef / 64);
           vga->SetColour( i, (unsigned char)(r*(1-flash_coef)+63*flash_coef),
                              (unsigned char)(g*(1-flash_coef)+63*flash_coef),
                              (unsigned char)(b*(1-flash_coef)+63*flash_coef) );
       }
    // move plasma with more sine functions :)
       x1 = 160 + (int)(159.0f * cos( (double)currentTime/97 ));
       x2 = 160 + (int)(159.0f * sin( (double)-currentTime/114 ));
       x3 = 160 + (int)(159.0f * sin( (double)-currentTime/137 ));
       y1 = 100 + (int)(99.0f * sin( (double)currentTime/123 ));
       y2 = 100 + (int)(99.0f * cos( (double)-currentTime/75 ));
       y3 = 100 + (int)(99.0f * cos( (double)-currentTime/108 ));
       src1 = y1*640+x1;
       src2 = y2*640+x2;
       src3 = y3*640+x3;
       dst = 0;
       for (j=0; j<200; j++)
       {
           for (i=0; i<320; i++)
           {
               vga->page_draw[dst] =
               plasma1[src1]+plasma2[src2]+plasma2[src3]+(textbuf[dst]>>2);
               dst++; src1++; src2++; src3++;
           }
           src1 += 320; src2 += 320; src3 += 320;
       }
       vga->Update();
       frameCount++;
    }
}

void Plasma_Done()
{
    delete [] (plasma1);
    delete [] (plasma2);
}
