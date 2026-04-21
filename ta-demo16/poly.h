// one entry of the edge table
typedef struct {
   int x, px, py, tx, ty, z;
} edge_data;

// store two edges per horizontal line
edge_data edge_table[200][2];

// remember the highest and the lowest point of the polygon
int poly_minY, poly_maxY;


/*
 * clears all entries in the edge table
 */
void InitEdgeTable()
{
   for (int i=0; i<200; i++)
   {
       edge_table[i][0].x = -1;
       edge_table[i][1].x = -1;
   }
   poly_minY = 200;
   poly_maxY = -1;
}

/*
 * scan along one edge of the poly, i.e. interpolate all values and store
 * in the edge table
 */
void ScanEdge( VECTOR p1, int tx1, int ty1, int px1, int py1,
               VECTOR p2, int tx2, int ty2, int px2, int py2 )
{
  // we can't handle this case, so we recall the proc with reversed params
  // saves having to swap all the vars, but it's not good practice
     if (p2[1]<p1[1]) {
        ScanEdge( p2, tx2, ty2, px2, py2, p1, tx1, ty1, px1, py1 );
        return;
     }
  // convert to fixed point
     int x1 = (int)(p1[0]*65536),
         y1 = (int)(p1[1]),
         z1 = (int)(p1[2]*16),
         x2 = (int)(p2[0]*65536),
         y2 = (int)(p2[1]),
         z2 = (int)(p2[2]*16);
  // update the min and max of the current polygon
     if (y1<poly_minY) poly_minY = y1;
     if (y2>poly_maxY) poly_maxY = y2;
  // compute deltas for interpolation
     int dy = y2-y1;
     if (dy==0) return;
     int dx = (x2-x1)/dy,                // assume 16.16 fixed point
         dtx = (tx2-tx1)/dy,
         dty = (ty2-ty1)/dy,
         dpx = (px2-px1)/dy,
         dpy = (py2-py1)/dy,
         dz  = (z2-z1)/dy;              // probably 12.4, but doesn't matter
  // interpolate along the edge
     for (int y=y1; y<y2; y++)
     {
      // don't go out of the screen
         if (y>199) return;
      // only store if inside the screen, we should really clip
         if (y>=0)
         {
         // is first slot free?
            if (edge_table[y][0].x==-1)
            { // if so, use that
               edge_table[y][0].x = x1;
               edge_table[y][0].tx = tx1;
               edge_table[y][0].ty = ty1;
               edge_table[y][0].px = px1;
               edge_table[y][0].py = py1;
               edge_table[y][0].z  = z1;
            }
            else { // otherwise use the other
               edge_table[y][1].x = x1;
               edge_table[y][1].tx = tx1;
               edge_table[y][1].ty = ty1;
               edge_table[y][1].px = px1;
               edge_table[y][1].py = py1;
               edge_table[y][1].z  = z1;
            }
         }
      // interpolate our values
         x1  += dx;
         px1 += dpx;
         py1 += dpy;
         tx1 += dtx;
         ty1 += dty;
         z1  += dz;
     }
}

/*
 * draw a horizontal double textured span
 */
void DrawSpan( int y, edge_data *p1, edge_data *p2 )
{
  // quick check, if facing back then draw span in the other direction,
  // avoids having to swap all the vars... not a very elegant
     if ( p1->x > p2->x )
     {
        DrawSpan( y, p2, p1 );
        return;
     };
  // load starting points
     int z1  = p1->z,
         px1 = p1->px,
         py1 = p1->py,
         tx1 = p1->tx,
         ty1 = p1->ty,
         x1 = p1->x>>16,
         x2 = p2->x>>16;
  // check if it's inside the screen
     if ((x1>319) || (x2<0)) return;
  // compute deltas for interpolation
     int dx =  x2 - x1;
     if (dx==0) return;
     int dtx = ( p2->tx - p1->tx ) / dx,  // assume 16.16 fixed point
         dty = ( p2->ty - p1->ty ) / dx,
         dpx = ( p2->px - p1->px ) / dx,
         dpy = ( p2->py - p1->py ) / dx,
         dz  = ( p2->z  - p1->z  ) / dx;
  // get destination offset in buffer
     long offs = y*320+x1;
  // loop for all pixels concerned
     for (int i=x1; i<x2; i++)
     {
          if (i>319) return;
      // check z buffer
          if (i>=0) if (z1<zbuffer[offs])
          {
         // if visible load the texel from the translated texture
            int texel = tor_text->location[((ty1>>8)&0xff00)+((tx1>>16)&0xff)],
         // and the texel from the light map
                lumel = tor_light[((py1>>8)&0xff00)+((px1>>16)&0xff)];
         // mix them together, and store
            vga->page_draw[offs] = tor_lut[(texel<<8)+lumel];
         // and update the zbuffer
            zbuffer[offs] = z1;
         }
      // interpolate our values
         px1 += dpx;
         py1 += dpy;
         tx1 += dtx;
         ty1 += dty;
         z1  += dz;
      // and find next pixel
         offs++;
     }
}
