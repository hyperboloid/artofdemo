TEXTURE *hole_text;
unsigned char *texcoord;

float get_x_pos( float f ) { return - 16 * sin(  f * M_PI / 256 ); }
float get_y_pos( float f ) { return - 16 * sin(  f * M_PI / 256 ); }
float get_radius( float f ) { return 128; }

void Hole_Init()
{
    texcoord = new unsigned char[128000];
    long offs = 0;
    for (int j=-100; j<100; j++) {
        for (int i=-160; i<160; i++)
        {
             float dx = (float)i / 200;
             float dy = (float)-j / 200;
             float dz = 1;
             float d = 20/sqrt( dx*dx + dy*dy + 1 );
             dx *= d; dy *= d; dz *= d;
             float x = 0, y = 0, z = 0;
             d = 16;
             while (d>0)
             {
                   while (((x-get_x_pos(z))*(x-get_x_pos(z))+(y-get_y_pos(z))*(y-get_y_pos(z)) < get_radius(z)) && (z<1024))
                   { x += dx; y += dy; z += dz; }
                   x -= dx;  dx /= 2;
                   y -= dy;  dy /= 2;
                   z -= dz;  dz /= 2;
                   d -= 1;
             }
             x -= get_x_pos(z);
             y -= get_y_pos(z);
             float ang = atan2( y, x ) * 256 / M_PI;
             texcoord[offs] = (unsigned char)ang;
             texcoord[offs+1] = (unsigned char)z;
             offs += 2;
        }
    }
    hole_text = new TEXTURE( "hole.png" );
}

void Draw_Hole( unsigned char *buf, unsigned char du, unsigned char dv )
{
   long doffs = 0, soffs = 0;
   for (int j=0; j<200; j++) {
       for (int i=0; i<320; i++) {
         unsigned char u = texcoord[soffs] + du;
         unsigned char v = texcoord[soffs+1] + dv;
         buf[doffs] = hole_text->location[(v<<8)+u];
         doffs++;
         soffs+=2;
       }
   }
}

void Hole_Run()
{
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
              (unsigned char)((hole_text->palette[i*3]>>2)*(1-flash_coef)+63*flash_coef),
              (unsigned char)((hole_text->palette[i*3+1]>>2)*(1-flash_coef)+63*flash_coef),
              (unsigned char)((hole_text->palette[i*3+2]>>2)*(1-flash_coef)+63*flash_coef) );
          }
          Draw_Hole( vga->page_draw, timer->getCount()>>16, timer->getCount()>>14 );
          vga->Update();
          frameCount++;
    }
}

void Hole_Done()
{
    delete [] texcoord;
}
