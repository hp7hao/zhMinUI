/*
   Minimal DFB2 hello world — tests the rendering pipeline.

   Creates a fullscreen window, fills it with a blue background,
   draws colored rectangles, and flips in a loop.

   Run this instead of minui.elf to isolate GPU compositing issues.
*/

#include <stdio.h>
#include <unistd.h>
#include <directfb.h>

int main(int argc, char *argv[])
{
     DFBResult             ret;
     IDirectFB            *dfb    = NULL;
     IDirectFBDisplayLayer *layer = NULL;
     IDirectFBWindow      *window = NULL;
     IDirectFBSurface     *surface = NULL;
     DFBDisplayLayerConfig lconfig;
     DFBWindowDescription  wdesc;
     int                   w, h;
     int                   frame = 0;

     fprintf(stderr, "hellodfb: starting\n");

     ret = DirectFBInit(&argc, &argv);
     if (ret) {
          fprintf(stderr, "hellodfb: DirectFBInit failed: %s\n", DirectFBErrorString(ret));
          return 1;
     }

     ret = DirectFBCreate(&dfb);
     if (ret) {
          fprintf(stderr, "hellodfb: DirectFBCreate failed: %s\n", DirectFBErrorString(ret));
          return 1;
     }

     fprintf(stderr, "hellodfb: DFB created\n");

     /* Get the primary layer (we're a slave — cooperative level stays DEFAULT). */
     ret = dfb->GetDisplayLayer(dfb, DLID_PRIMARY, &layer);
     if (ret) {
          fprintf(stderr, "hellodfb: GetDisplayLayer failed: %s\n", DirectFBErrorString(ret));
          dfb->Release(dfb);
          return 1;
     }

     layer->GetConfiguration(layer, &lconfig);
     w = lconfig.width;
     h = lconfig.height;

     fprintf(stderr, "hellodfb: display %dx%d\n", w, h);

     /* Create a fullscreen window. */
     wdesc.flags  = DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_POSX | DWDESC_POSY |
                    DWDESC_CAPS | DWDESC_PIXELFORMAT;
     wdesc.width  = w;
     wdesc.height = h;
     wdesc.posx   = 0;
     wdesc.posy   = 0;
     wdesc.caps   = DWCAPS_ALPHACHANNEL;
     wdesc.pixelformat = DSPF_ARGB;

     ret = layer->CreateWindow(layer, &wdesc, &window);
     if (ret) {
          fprintf(stderr, "hellodfb: CreateWindow failed: %s\n", DirectFBErrorString(ret));
          layer->Release(layer);
          dfb->Release(dfb);
          return 1;
     }

     window->GetSurface(window, &surface);
     window->SetOpacity(window, 0xFF);
     window->RequestFocus(window);

     fprintf(stderr, "hellodfb: window created, entering render loop\n");

     /* Render loop — cycle through colors. */
     while (1) {
          int r = (frame * 3) & 0xFF;
          int g = (frame * 5 + 85) & 0xFF;
          int b = (frame * 7 + 170) & 0xFF;

          /* Clear to dark blue. */
          surface->SetColor(surface, 0x10, 0x10, 0x40, 0xFF);
          surface->FillRectangle(surface, 0, 0, w, h);

          /* Draw a moving colored rectangle. */
          int rx = (frame * 2) % (w - 100);
          int ry = (frame * 1) % (h - 80);
          surface->SetColor(surface, r, g, b, 0xFF);
          surface->FillRectangle(surface, rx, ry, 100, 80);

          /* Draw a white rectangle in the center. */
          surface->SetColor(surface, 0xFF, 0xFF, 0xFF, 0xFF);
          surface->FillRectangle(surface, w/2 - 50, h/2 - 30, 100, 60);

          /* Draw a red border at the edges to verify full-screen rendering. */
          surface->SetColor(surface, 0xFF, 0x00, 0x00, 0xFF);
          surface->FillRectangle(surface, 0, 0, w, 2);       /* top */
          surface->FillRectangle(surface, 0, h-2, w, 2);     /* bottom */
          surface->FillRectangle(surface, 0, 0, 2, h);       /* left */
          surface->FillRectangle(surface, w-2, 0, 2, h);     /* right */

          surface->Flip(surface, NULL, DSFLIP_NONE);

          frame++;
          if (frame % 60 == 0)
               fprintf(stderr, "hellodfb: frame %d\n", frame);

          usleep(16000); /* ~60fps */
     }

     surface->Release(surface);
     window->Release(window);
     layer->Release(layer);
     dfb->Release(dfb);

     return 0;
}
