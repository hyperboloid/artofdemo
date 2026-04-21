TEXTURE *bm, *cm;
unsigned char *bump_lut;
#define LIGHTSIZE 2.4f
unsigned char *bump_light;

void Bump( unsigned char *bm, unsigned char *cm, int lx1, int ly1, int lx2, int ly2, int zoom )
{
     int i, j, px, py, x, y, offs, c;
     offs = 320;
     for (j=1; j<200; j++)
     {
         vga->page_draw[offs] = 0; offs++;
         for (i=1; i<320; i++)
         {
             px = (i*zoom>>8) + bm[offs-1] - bm[offs];
             py = (j*zoom>>8) + bm[offs-320] - bm[offs];
             x = px + lx1; y = py + ly1;
             if ((y>=0) && (y<256) && (x>=0) && (x<256)) c = bump_light[(y<<8)+x]; else c = 0;
             x = px + lx2; y = py + ly2;
             if ((y>=0) && (y<256) && (x>=0) && (x<256)) c += bump_light[(y<<8)+x];
             if (c>255) c = 255;
             vga->page_draw[offs] = bump_lut[(cm[offs]<<8)+c];
             offs++;
         }
     }
}

void Compute_LUT3( unsigned char *pal )
{
   unsigned char r, g, b;
   for (int j=0; j<256; j++)
   {
       r = pal[j*3]; g = pal[j*3+1]; b = pal[j*3+2];
       for (int i=0; i<256; i++)
           bump_lut[(j<<8)+i] = Closest_Colour(pal, (r*i)/255, (g*i)/255, (b*i)/255);
   }
}

void Compute_Light()
{
    for (int j=0; j<256; j++)
        for (int i=0; i<256; i++)
        {
            float dist = (128-i)*(128-i) + (128-j)*(128-j);
            if (fabs(dist)>1) dist = sqrt(dist);
            int c = (int)(LIGHTSIZE*dist)+(rand()&7)-3;
            if (c<0) c = 0; if (c>255) c = 255;
            bump_light[(j<<8)+i] = 255-c;
        }
}

void Bump_Init()
{
   bm = new TEXTURE( "bumpmap.png" );
   cm = new TEXTURE( "colmap.png" );
   bump_lut = new unsigned char[65536];
   Compute_LUT3( cm->palette );
   bump_light = new unsigned char[65536];
   Compute_Light();
}

void Bump_Run()
{
    int x1, y1, x2, y2, z;
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

       for (int i=0; i<256; i++)
       {
           vga->SetColour( i,
           (unsigned char)((cm->palette[i*3]>>2)*(1-flash_coef)+63*flash_coef),
           (unsigned char)((cm->palette[i*3+1]>>2)*(1-flash_coef)+63*flash_coef),
           (unsigned char)((cm->palette[i*3+2]>>2)*(1-flash_coef)+63*flash_coef) );
       }
       long long currentTime = timer->getCount()>>14;
       x1 = (int)(128.0f * cos((double)currentTime/64))-20;
       y1 = (int)(128.0f * sin((double)-currentTime/45))+20;
       x2 = (int)(128.0f * cos((double)-currentTime/51))-20;
       y2 = (int)(128.0f * sin((double)currentTime/71))+20;
       z = 192+(int)(127.0f * sin((double)currentTime/112));
       Bump( bm->location, cm->location, x1, y1, x2, y2, z );
       vga->Update();
       frameCount++;
    }
}

void Bump_Done()
{
    delete bm; delete cm;
    delete [] bump_light; delete [] bump_lut;
}
