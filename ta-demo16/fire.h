unsigned char *fire1, *fire2;

/*
 * create a shade of colours in the palette from s to e
 */
void Shade_Pal( int s, int e, int r1, int g1, int b1, int r2, int g2, int b2 )
{
    int i;
    float k;
    r1 = (int)( r1 * (1-flash_coef) + 63 * flash_coef );
    g1 = (int)( g1 * (1-flash_coef) + 63 * flash_coef );
    b1 = (int)( b1 * (1-flash_coef) + 63 * flash_coef );
    r2 = (int)( r2 * (1-flash_coef) + 63 * flash_coef );
    g2 = (int)( g2 * (1-flash_coef) + 63 * flash_coef );
    b2 = (int)( b2 * (1-flash_coef) + 63 * flash_coef );
    for (i=0; i<=e-s; i++)
    {
        k = (float)i/(float)(e-s);
        vga->SetColour( s+i, (int)(r1+(r2-r1)*k), (int)(g1+(g2-g1)*k), (int)(b1+(b2-b1)*k) );
    }
}

/*
 * adds some hot pixels to a buffer
 */
void Heat( unsigned char *dst )
{
     int i, j;
// add some random hot spots where the text is
     for (i=12000; i<52480; i++)
     {
         if (textbuf[i]>dst[i]) dst[i] = rand()&(textbuf[i]>>1);
     }
     j = (rand() & 511);
// add some random hot spots at the bottom of the buffer
     for (i=0; i<j; i++)
     {
         dst[63040+(rand()%960)] = 255;
     }
}

/*
 * smooth a buffer upwards, make sure not to read pixels that are outside of
 * the buffer!
 */
void Blur_Up( unsigned char *src, unsigned char *dst )
{
     int offs = 0;
     unsigned char b;
     for (int j=0; j<198; j++)
     {
     // set first pixel of the line to 0
         dst[offs] = 0; offs++;
     // calculate the filter for all the other pixels
         for (int i=1; i<319; i++)
         {
         // calculate the average
             b = ( src[offs-1]   +               + src[offs+1]
                 + src[offs+319] + src[offs+320] + src[offs+321]
                 + src[offs+639] + src[offs+640] + src[offs+641] ) >> 3;
          // decrement the sum by one so that the fire looses intensity
             if (b>0) b--;
          // store the pixel
             dst[offs] = b;
             offs++;
         }
     // set last pixel of the line to 0
         dst[offs] = 0; offs++;
     }
// clear the last 2 lines
     memset( dst+offs, 0, 640 );
}

void Fire_Init()
{
// buffer to store the image
    textbuf = new unsigned char[64000];
// two fire buffers
    fire1 = new unsigned char[64000];
    fire2 = new unsigned char[64000];
// clear the buffers
    memset( fire1, 0, 64000 );
    memset( fire2, 0, 64000 );
// load the background image
    TEXTURE *texture = new TEXTURE( "logo.png" );
    memcpy( textbuf, texture->location, 64000 );
    delete (texture);
}

void Fire_Run()
{
    vga->Clear();
    vga->Update();
 // a temporary variable to swap the two buffers
    unsigned char *tmp;
    bQuit = false;
 // run the loop
    SDL_Event event;
    while (!bQuit && !effectTimeUp())
    {
       while (SDL_PollEvent(&event))
       {
           if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN)
               { bQuit = true; globalQuit = true; }
       }
       if (bQuit) break;

    // setup some fire-like colours
       Shade_Pal( 0, 23, 0, 0, 0, 0, 0, 31 );
       Shade_Pal( 24, 47, 0, 0, 31, 63, 0, 0 );
       Shade_Pal( 48, 63, 63, 0, 0, 63, 63, 0 );
       Shade_Pal( 64, 127, 63, 63, 0, 63, 63, 63 );
       Shade_Pal( 128, 255, 63, 63, 63, 63, 63, 63 );

    // heat the fire
       Heat( fire1 );
    // apply the filter
       Blur_Up( fire1, fire2 );
    // dump our temporary buffer to the screen
       memcpy( vga->page_draw, fire2, 63040 );
       vga->Update();
    // swap our two fire buffers
       tmp = fire1;
       fire1 = fire2;
       fire2 = tmp;
    // that's one more done!
       frameCount++;
    }
}

void Fire_Done()
{
    delete [] (fire1);
    delete [] (fire2);
}
