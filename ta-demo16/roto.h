TEXTURE *roto_text;

void Roto_Init()
{
    roto_text = new TEXTURE( "roto.png" );
}

void TextureScreen( int Ax, int Ay, int Bx, int By, int Cx, int Cy )
{
     int dxdx = (Bx-Ax)/320, dydx = (By-Ay)/320, dxdy = (Cx-Ax)/200, dydy = (Cy-Ay)/200;
     long offs = 0;
     for (int j=0; j<200; j++)
     {
          Cx = Ax; Cy = Ay;
          for (int i=0; i<320; i++)
          {
              vga->page_draw[offs] = roto_text->location[((Cy>>8)&0xff00)+((Cx>>16)&0xff)];
              Cx += dxdx; Cy += dydx; offs++;
          }
          Ax += dxdy; Ay += dydy;
     }
}

void BlockTextureScreen( int Ax, int Ay, int Bx, int By, int Cx, int Cy )
{
     int dxdx = (Bx-Ax)/40, dydx = (By-Ay)/40, dxdy = (Cx-Ax)/25, dydy = (Cy-Ay)/25;
     int dxbdx = (Bx-Ax)/320, dybdx = (By-Ay)/320, dxbdy = (Cx-Ax)/200, dybdy = (Cy-Ay)/200;
     long baseoffs = 0, offs;
     for (int j=0; j<25; j++)
     {
          Cx = Ax; Cy = Ay;
          for (int i=0; i<40; i++)
          {
              offs = baseoffs;
              int Ex = Cx, Ey = Cy;
              for (int y=0; y<8; y++)
              {
                  int Fx = Ex, Fy = Ey;
                  for (int x=0; x<8; x++)
                  {
                      vga->page_draw[offs] = roto_text->location[((Fy>>8)&0xff00)+((Fx>>16)&0xff)];
                      Fx += dxbdx; Fy += dybdx; offs++;
                  }
                  Ex += dxbdy; Ey += dybdy; offs += 312;
              }
              baseoffs += 8; Cx += dxdx; Cy += dydx;
          }
          baseoffs += 320*7; Ax += dxdy; Ay += dydy;
     }
}

void DoRotoZoom( float cx, float cy, float radius, float angle )
{
   int x1 = (int)( 65536.0f * (cx + radius * cos( angle )) ),
       y1 = (int)( 65536.0f * (cy + radius * sin( angle )) ),
       x2 = (int)( 65536.0f * (cx + radius * cos( angle + 2.02458 )) ),
       y2 = (int)( 65536.0f * (cy + radius * sin( angle + 2.02458 )) ),
       x3 = (int)( 65536.0f * (cx + radius * cos( angle - 1.11701 )) ),
       y3 = (int)( 65536.0f * (cy + radius * sin( angle - 1.11701 )) );
   BlockTextureScreen( x1, y1, x2, y2, x3, y3 );
}

void Roto_Run()
{
    bQuit = false;
    long long currentTime, lastTime = timer->getCount();
    float angle, base_angle = 0, angle_coef = 1/731287.0f;
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
              (unsigned char)((roto_text->palette[i*3]>>2)*(1-flash_coef)+63*flash_coef),
              (unsigned char)((roto_text->palette[i*3+1]>>2)*(1-flash_coef)+63*flash_coef),
              (unsigned char)((roto_text->palette[i*3+2]>>2)*(1-flash_coef)+63*flash_coef) );
          }
          currentTime = timer->getCount();
          angle = base_angle+(float)(currentTime-lastTime) * angle_coef;
          DoRotoZoom(
          2048 * sin((float)currentTime/21547814.0f),
          2048 * cos((float)currentTime/18158347.0f),
          256.0f+192.0f*cos((float)currentTime/4210470.0f),
          angle );
          vga->Update();
          frameCount++;
    }
}

void Roto_Done()
{
     delete (roto_text);
}
