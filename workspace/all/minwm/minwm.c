/*
   This file is part of DirectFB-examples.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include <direct/clock.h>
#include <direct/util.h>
#include <directfb.h>
#include <directfb_util.h>
#include <math.h>

#include "util.h"

#ifdef USE_FONT_HEADERS
#include "decker.h"
#endif

#ifdef USE_IMAGE_HEADERS
#include "cursor_red.h"
#include "cursor_yellow.h"
#include "dfblogo.h"
#endif

/* DirectFB interfaces */
static IDirectFB             *dfb             = NULL;
static IDirectFBDisplayLayer *layer           = NULL;
static IDirectFBSurface      *cursor_surface  = NULL;
static IDirectFBWindow       *window1         = NULL;
static IDirectFBSurface      *window_surface1 = NULL;
static IDirectFBSurface      *cursor_surface1 = NULL;
static IDirectFBWindow       *window2         = NULL;
static IDirectFBSurface      *window_surface2 = NULL;
static IDirectFBSurface      *cursor_surface2 = NULL;
static IDirectFBWindow       *window_lock     = NULL;
static IDirectFBSurface      *window_surface_lock = NULL;
/* controller test window */
static IDirectFBWindow       *window_ctrl     = NULL;
static IDirectFBSurface      *window_surface_ctrl = NULL;
static IDirectFBEventBuffer  *event_buffer    = NULL;
static IDirectFBFont         *font            = NULL;

/* --- MinWM signal-based lock control (from keymon) --- */
#include <signal.h>
static volatile sig_atomic_t want_lock_signal = 0;
static volatile sig_atomic_t want_unlock_signal = 0;

static void sigusr1_lock(int signo) { (void)signo; want_lock_signal = 1; }
static void sigusr2_unlock(int signo) { (void)signo; want_unlock_signal = 1; }

static void install_signal_handlers(void)
{
     struct sigaction sa;
     memset(&sa, 0, sizeof(sa));
     sa.sa_handler = sigusr1_lock;
     sigaction(SIGUSR1, &sa, NULL);
     sa.sa_handler = sigusr2_unlock;
     sigaction(SIGUSR2, &sa, NULL);
}

/* default stacking class */
static int stacking_id = DWSC_MIDDLE;

static void dfb_shutdown( void )
{
     if (font)            font->Release( font );
     if (event_buffer)    event_buffer->Release( event_buffer );
     if (cursor_surface2) cursor_surface2->Release( cursor_surface2 );
     if (window_surface2) window_surface2->Release( window_surface2 );
     if (window2)         window2->Release( window2 );
     if (cursor_surface1) cursor_surface1->Release( cursor_surface1 );
     if (window_surface1) window_surface1->Release( window_surface1 );
     if (window1)         window1->Release( window1 );
     if (window_surface_ctrl) window_surface_ctrl->Release( window_surface_ctrl );
     if (window_ctrl)         window_ctrl->Release( window_ctrl );
     if (window_surface_lock) window_surface_lock->Release( window_surface_lock );
     if (window_lock)         window_lock->Release( window_lock );
     if (layer) {
          layer->SetCooperativeLevel( layer, DLSCL_ADMINISTRATIVE );
          layer->SetCursorOpacity( layer, 0xFF );
          if (cursor_surface) {
               layer->SetCursorShape( layer, NULL, 0, 0 );
               cursor_surface->Release( cursor_surface );
          }
          layer->SetCooperativeLevel( layer, DLSCL_SHARED );
          layer->Release( layer );
     }
     if (dfb) dfb->Release( dfb );
}

static void print_usage( void )
{
     printf( "DirectFB Window Demo\n\n" );
     printf( "Usage: df_window <stacking class>\n\n" );
}

int main( int argc, char *argv[] )
{
     int                       fontheight, stringwidth;
     int                       winx, winy, winwidth, winheight;
     DFBWindowID               id1;
     DFBDisplayLayerConfig     config;
     DFBFontDescription        fdsc;
     DFBSurfaceDescription     sdsc;
     DFBWindowDescription      wdsc;
     DFBDataBufferDescription  ddsc;
     IDirectFBDataBuffer      *buffer;
     IDirectFBImageProvider   *provider;
     IDirectFBWindow          *upper;
     IDirectFBWindow          *active            = NULL;
     DFBWindowID               id_ctrl;
     bool                      invisible_cursor1 = false;
     bool                      invisible_cursor2 = false;
     int                       cursor_enabled    = 1;
     int                       rotation          = 0;
     int                       grabbed           = 0;
     int                       startx            = 0;
     int                       starty            = 0;
     int                       endx              = 0;
     int                       endy              = 0;
     int                       winupdate         = 0;
     int                       quit              = 0;

     /* initialize DirectFB including command line parsing */
     DFBCHECK(DirectFBInit( &argc, &argv ));

     /* parse command line */
     if (argv[1] && !strcmp( argv[1], "--help" )) {
          print_usage();
          return 0;
     }

     if (argc > 1) {
          if (strcmp( argv[1], "upper" ) == 0)
               stacking_id = DWSC_UPPER;
          else if (strcmp( argv[1], "lower" ) == 0)
               stacking_id = DWSC_LOWER;
          else {
               print_usage();
               return 1;
          }
     }

     /* create the main interface */
     DFBCHECK(DirectFBCreate( &dfb ));

     /* register termination function */
     atexit( dfb_shutdown );

     /* get the primary display layer */
     DFBCHECK(dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer ));

     DFBCHECK(layer->GetConfiguration( layer, &config ));

     /* set cursor shape for the primary display layer */
     if (direct_getenv( "DEFAULT_CURSOR" )) {
          DFBCHECK(dfb->CreateImageProvider( dfb, direct_getenv( "DEFAULT_CURSOR" ), &provider ));
          provider->GetSurfaceDescription( provider, &sdsc );
          DFBCHECK(dfb->CreateSurface( dfb, &sdsc, &cursor_surface ));
          provider->RenderTo( provider, cursor_surface, NULL );
          layer->SetCooperativeLevel( layer, DLSCL_ADMINISTRATIVE );
          DFBCHECK(layer->SetCursorShape( layer, cursor_surface, 0, 0 ));
          layer->SetCooperativeLevel( layer, DLSCL_SHARED );
          provider->Release( provider );
     }

     /* load font */
     fdsc.flags  = DFDESC_HEIGHT;
     fdsc.height = CLAMP( (int) (config.width / 50.0 / 8) * 8, 8, 96 );

#ifdef USE_FONT_HEADERS
     ddsc.flags         = DBDESC_MEMORY;
     ddsc.memory.data   = GET_FONTDATA( decker );
     ddsc.memory.length = GET_FONTSIZE( decker );
#else
     ddsc.flags         = DBDESC_FILE;
     ddsc.file          = GET_FONTFILE( decker );
#endif
     DFBCHECK(dfb->CreateDataBuffer( dfb, &ddsc, &buffer ));
     DFBCHECK(buffer->CreateFont( buffer, &fdsc, &font ));
     buffer->Release( buffer );
     DFBCHECK(font->GetHeight( font, &fontheight ));
     DFBCHECK(font->GetStringWidth( font, " Press left mouse button and drag to move the window. ", -1, &stringwidth ));

     /* fill the window description. */
     wdsc.flags        = DWDESC_CAPS | DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_SURFACE_CAPS;
     wdsc.caps         = DWCAPS_ALPHACHANNEL;
     wdsc.surface_caps = DSCAPS_PREMULTIPLIED;

     if (stacking_id) {
          wdsc.flags    |= DWDESC_STACKING;
          wdsc.stacking  = stacking_id;
     }

     /* create window1 */
     wdsc.posx   = config.width / 5;
     wdsc.posy   = config.height / 6 + fontheight * 6;
     wdsc.width  = MIN( config.width - wdsc.posx - 20, 512 );
     wdsc.height = wdsc.width * 145 / 512;

     DFBCHECK(layer->CreateWindow( layer, &wdsc, &window1 ));

#ifdef USE_IMAGE_HEADERS
     ddsc.flags         = DBDESC_MEMORY;
     ddsc.memory.data   = GET_IMAGEDATA( dfblogo );
     ddsc.memory.length = GET_IMAGESIZE( dfblogo );
#else
     ddsc.flags         = DBDESC_FILE;
     ddsc.file          = GET_IMAGEFILE( dfblogo );
#endif
     DFBCHECK(dfb->CreateDataBuffer( dfb, &ddsc, &buffer ));
     DFBCHECK(buffer->CreateImageProvider( buffer, &provider ));
     buffer->Release( buffer );
     DFBCHECK(window1->GetSurface( window1, &window_surface1 ));
     provider->RenderTo( provider, window_surface1, NULL );
     window_surface1->SetDrawingFlags( window_surface1, DSDRAW_SRC_PREMULTIPLY );
     window_surface1->SetColor( window_surface1, 0xFF, 0x20, 0x20, 0x90 );
     window_surface1->DrawRectangle( window_surface1, 0, 0, wdsc.width, wdsc.height );
     provider->Release( provider );

#ifdef USE_IMAGE_HEADERS
     ddsc.flags         = DBDESC_MEMORY;
     ddsc.memory.data   = GET_IMAGEDATA( cursor_red );
     ddsc.memory.length = GET_IMAGESIZE( cursor_red );
#else
     ddsc.flags         = DBDESC_FILE;
     ddsc.file          = GET_IMAGEFILE( cursor_red );
#endif
     DFBCHECK(dfb->CreateDataBuffer( dfb, &ddsc, &buffer ));
     DFBCHECK(buffer->CreateImageProvider( buffer, &provider ));
     buffer->Release( buffer );
     provider->GetSurfaceDescription( provider, &sdsc );
     DFBCHECK(dfb->CreateSurface( dfb, &sdsc, &cursor_surface1 ));
     provider->RenderTo( provider, cursor_surface1, NULL );
     DFBCHECK(window1->SetCursorFlags( window1, DWCF_NONE ));
     DFBCHECK(window1->SetCursorShape( window1, cursor_surface1, 0, 0 ));
     provider->Release( provider );

     window_surface1->SetFont( window_surface1, font );

     /* create window2. */
     wdsc.posx   = 20;
     wdsc.posy   = config.height / 6;
     wdsc.width  = stringwidth;
     wdsc.height = wdsc.width / 2;

     DFBCHECK(layer->CreateWindow( layer, &wdsc, &window2 ));

     DFBCHECK(window2->GetSurface( window2, &window_surface2 ));
     window_surface2->SetDrawingFlags( window_surface2, DSDRAW_SRC_PREMULTIPLY );
     window_surface2->SetColor( window_surface2, 0x00, 0x30, 0x10, 0xC0 );
     window_surface2->DrawRectangle( window_surface2, 0, 0, wdsc.width, wdsc.height );
     window_surface2->SetColor( window_surface2, 0x80, 0xA0, 0x00, 0x90 );
     window_surface2->FillRectangle( window_surface2, 1, 1, wdsc.width - 2, wdsc.height - 2 );

#ifdef USE_IMAGE_HEADERS
     ddsc.flags         = DBDESC_MEMORY;
     ddsc.memory.data   = GET_IMAGEDATA( cursor_yellow );
     ddsc.memory.length = GET_IMAGESIZE( cursor_yellow );
#else
     ddsc.flags         = DBDESC_FILE;
     ddsc.file          = GET_IMAGEFILE( cursor_yellow );
#endif
     DFBCHECK(dfb->CreateDataBuffer( dfb, &ddsc, &buffer ));
     DFBCHECK(buffer->CreateImageProvider( buffer, &provider ));
     buffer->Release( buffer );
     provider->GetSurfaceDescription( provider, &sdsc );
     DFBCHECK(dfb->CreateSurface( dfb, &sdsc, &cursor_surface2 ));
     provider->RenderTo( provider, cursor_surface2, NULL );
     DFBCHECK(window2->SetCursorFlags( window2, DWCF_NONE ));
     DFBCHECK(window2->SetCursorShape( window2, cursor_surface2, 0, 0 ));
     provider->Release( provider );

     window_surface2->SetFont( window_surface2, font );

     /* create an event buffer */
     DFBCHECK(window1->CreateEventBuffer( window1, &event_buffer ));
     DFBCHECK(window2->AttachEventBuffer( window2, event_buffer ));

     /* move the cursor to the center of the window1 */
     DFBCHECK(window1->GetPosition( window1, &winx, &winy ));
     DFBCHECK(window1->GetSize( window1, &winwidth, &winheight ));
     layer->SetCooperativeLevel( layer, DLSCL_ADMINISTRATIVE );
     DFBCHECK(layer->WarpCursor( layer, winx + (winwidth >> 1), winy + (winheight >> 1) ));
     layer->SetCooperativeLevel( layer, DLSCL_SHARED );

     /* window1 settings */
     window1->GetID( window1, &id1 );

     window1->RaiseToTop( window1 );

     upper = window1;

     window1->SetOpacity( window1, 0xFF );

     window1->RequestFocus( window1 );

     /* window2 settings */
     window_surface2->SetColor( window_surface2, 0xCF, 0xBF, 0xFF, 0xFF );
     window_surface2->DrawString( window_surface2,
                                  " Move the mouse over a window to activate it.",
                                  -1, 0, 2, DSTF_TOPLEFT );

     window_surface2->SetColor( window_surface2, 0xCF, 0xCF, 0xCF, 0xFF );
     window_surface2->DrawString( window_surface2,
                                  " Press left mouse button and drag to move the window.",
                                  -1, 0, 2 + fontheight, DSTF_TOPLEFT );

     window_surface2->SetColor( window_surface2, 0xCF, 0xDF, 0x9F, 0xFF );
     window_surface2->DrawString( window_surface2,
                                  " Press middle mouse button to raise/lower the window.",
                                  -1, 0, 2 + fontheight * 2, DSTF_TOPLEFT );

     window_surface2->SetColor( window_surface2, 0xCF, 0xEF, 0x6F, 0xFF );
     window_surface2->DrawString( window_surface2,
                                  " Hold right mouse button to fade in/out the window.",
                                  -1, 0, 2 + fontheight * 3, DSTF_TOPLEFT );

     window_surface2->SetColor( window_surface2, 0xCF, 0xFF, 0x3F, 0xFF );
     window_surface2->DrawString( window_surface2,
                                  " Press r key to rotate the window.",
                                  -1, 0, 2 + fontheight * 4, DSTF_TOPLEFT );

     window2->SetOpacity( window2, 0xFF );

     /* create controller test window */
     wdsc.flags  = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_CAPS | DWDESC_SURFACE_CAPS;
     wdsc.posx   = (config.width * 3) / 5;
     wdsc.posy   = (config.height * 2) / 3;
     wdsc.width  = MIN( 360, config.width - wdsc.posx - 20 );
     wdsc.height = 160;
     wdsc.caps   = DWCAPS_ALPHACHANNEL;
     wdsc.surface_caps = DSCAPS_PREMULTIPLIED;
     DFBCHECK(layer->CreateWindow( layer, &wdsc, &window_ctrl ));
     DFBCHECK(window_ctrl->GetSurface( window_ctrl, &window_surface_ctrl ));
     window_ctrl->GetID( window_ctrl, &id_ctrl );
     window_ctrl->SetOpacity( window_ctrl, 0xFF );

     /* draw initial controller layout */
     {
          /* background */
          window_surface_ctrl->SetColor( window_surface_ctrl, 0x00, 0x00, 0x00, 0xA0 );
          window_surface_ctrl->FillRectangle( window_surface_ctrl, 0, 0, wdsc.width, wdsc.height );
          window_surface_ctrl->SetFont( window_surface_ctrl, font );
          window_surface_ctrl->SetColor( window_surface_ctrl, 0xFF, 0xFF, 0xFF, 0xFF );
          window_surface_ctrl->DrawString( window_surface_ctrl, "Controller Test", -1, 8, 4, DSTF_TOPLEFT );
          /* D-pad (left) */
          int dpx = 40, dpy = 90, dpw = 24, dps = 50;
          window_surface_ctrl->SetColor( window_surface_ctrl, 0x40, 0x40, 0x40, 0xFF );
          window_surface_ctrl->FillRectangle( window_surface_ctrl, dpx, dpy - dps/2, dpw, dps ); /* vertical */
          window_surface_ctrl->FillRectangle( window_surface_ctrl, dpx - dps/2 + dpw/2, dpy - dpw/2, dps, dpw ); /* horizontal */
          /* ABXY (right) */
          int cx = wdsc.width - 90, cy = 90, r = 16, gap = 34;
          window_surface_ctrl->SetColor( window_surface_ctrl, 0x80, 0x80, 0x80, 0xFF );
          window_surface_ctrl->FillRectangle( window_surface_ctrl, cx - r, cy - gap - r, 2*r, 2*r ); /* Y */
          window_surface_ctrl->FillRectangle( window_surface_ctrl, cx - r, cy + gap - r, 2*r, 2*r ); /* A */
          window_surface_ctrl->FillRectangle( window_surface_ctrl, cx - gap - r, cy - r, 2*r, 2*r ); /* X */
          window_surface_ctrl->FillRectangle( window_surface_ctrl, cx + gap - r, cy - r, 2*r, 2*r ); /* B */
          /* Start/Select (center) */
          int sx = wdsc.width/2 - 40, sy = 20;
          window_surface_ctrl->FillRectangle( window_surface_ctrl, sx, sy, 30, 12 );      /* Select */
          window_surface_ctrl->FillRectangle( window_surface_ctrl, sx + 50, sy, 30, 12 ); /* Start */
          /* Shoulder buttons L1/L2 (left top) and R1/R2 (right top) */
          int sh_h = 14, sh_w = 44, sh_gap = 6;
          int lbase_x = 8, lbase_y = 8 + 18; /* below title */
          int rbase_x = wdsc.width - sh_w - 8, rbase_y = 8 + 18;
          window_surface_ctrl->SetColor( window_surface_ctrl, 0x60, 0x60, 0x60, 0xFF );
          /* L1 / L2 */
          window_surface_ctrl->FillRectangle( window_surface_ctrl, lbase_x, lbase_y, sh_w, sh_h );
          window_surface_ctrl->FillRectangle( window_surface_ctrl, lbase_x, lbase_y + sh_h + sh_gap, sh_w, sh_h );
          window_surface_ctrl->SetColor( window_surface_ctrl, 0xFF, 0xFF, 0xFF, 0xFF );
          window_surface_ctrl->DrawString( window_surface_ctrl, "L1", -1, lbase_x + 4, lbase_y + 2, DSTF_TOPLEFT );
          window_surface_ctrl->DrawString( window_surface_ctrl, "L2", -1, lbase_x + 4, lbase_y + sh_h + sh_gap + 2, DSTF_TOPLEFT );
          /* R1 / R2 */
          window_surface_ctrl->SetColor( window_surface_ctrl, 0x60, 0x60, 0x60, 0xFF );
          window_surface_ctrl->FillRectangle( window_surface_ctrl, rbase_x, rbase_y, sh_w, sh_h );
          window_surface_ctrl->FillRectangle( window_surface_ctrl, rbase_x, rbase_y + sh_h + sh_gap, sh_w, sh_h );
          window_surface_ctrl->SetColor( window_surface_ctrl, 0xFF, 0xFF, 0xFF, 0xFF );
          window_surface_ctrl->DrawString( window_surface_ctrl, "R1", -1, rbase_x + 4, rbase_y + 2, DSTF_TOPLEFT );
          window_surface_ctrl->DrawString( window_surface_ctrl, "R2", -1, rbase_x + 4, rbase_y + sh_h + sh_gap + 2, DSTF_TOPLEFT );
     }

     /* attach controller window to event buffer */
     DFBCHECK(window_ctrl->AttachEventBuffer( window_ctrl, event_buffer ));

     /* create lockscreen window (full-screen, dark overlay with text) */
     wdsc.posx   = 0;
     wdsc.posy   = 0;
     wdsc.width  = config.width;
     wdsc.height = config.height;
     wdsc.flags  = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_CAPS | DWDESC_SURFACE_CAPS;
     wdsc.caps   = DWCAPS_ALPHACHANNEL;
     wdsc.surface_caps = DSCAPS_PREMULTIPLIED;
     DFBCHECK(layer->CreateWindow( layer, &wdsc, &window_lock ));
     DFBCHECK(window_lock->GetSurface( window_lock, &window_surface_lock ));
     /* semi-transparent black backdrop */
     window_surface_lock->SetColor( window_surface_lock, 0x00, 0x00, 0x00, 0xC0 );
     window_surface_lock->FillRectangle( window_surface_lock, 0, 0, wdsc.width, wdsc.height );
     /* lock text */
     window_surface_lock->SetFont( window_surface_lock, font );
     window_surface_lock->SetColor( window_surface_lock, 0xFF, 0xFF, 0xFF, 0xFF );
     window_surface_lock->DrawString( window_surface_lock,
                                      "LOCK SCREEN - Press any key",
                                      -1, wdsc.width / 2, wdsc.height / 2, DSTF_CENTER );
     window_lock->SetOpacity( window_lock, 0xFF );
     window_lock->RaiseToTop( window_lock );

     /* include lock window in the same event buffer */
     DFBCHECK(window_lock->AttachEventBuffer( window_lock, event_buffer ));

     /* main loop */
     int lock_visible = 1;
     unsigned int unlock_count = 0;
     const int unlock_required = 3;
     const unsigned int idle_timeout_ms = 3000;  /* 3s inactivity -> relock */
     unsigned int last_input_ms = (unsigned int) direct_clock_get_millis();
     while (!quit) {
          DFBWindowEvent evt;

          event_buffer->WaitForEventWithTimeout( event_buffer, 0, 10 );

          /* process event buffer */
          while (event_buffer->GetEvent( event_buffer, DFB_EVENT(&evt) ) == DFB_OK) {
               IDirectFBWindow *window;

               if (window_lock && lock_visible && evt.window_id == ({ DFBWindowID _id; window_lock->GetID(window_lock, &_id), _id; })) {
                    window = window_lock;
               } else if (evt.window_id == id1) {
                    window = window1;
              } else if (window_ctrl && evt.window_id == id_ctrl) {
                   window = window_ctrl;
               } else {
                    window = window2;
               }

               if (evt.type == DWET_GOTFOCUS) {
                    active = window;
               }
               else if (active) {
                    switch (evt.type) {
                         case DWET_BUTTONDOWN:
                              if (!grabbed) {
                                   grabbed = evt.buttons;
                                   startx  = evt.cx;
                                   starty  = evt.cy;
                                   window->GrabPointer( window );
                              }
                              break;

                         case DWET_BUTTONUP:
                              switch (evt.button) {
                                   case DIBI_LEFT:
                                   case DIBI_RIGHT:
                                        if (grabbed && !evt.buttons) {
                                             window->UngrabPointer( window );
                                             grabbed = 0;
                                        }
                                        break;
                                   case DIBI_MIDDLE:
                                        upper->LowerToBottom( upper );
                                        upper = (upper == window1) ? window2 : window1;
                                        break;
                                   default:
                                        break;
                              }
                              break;

                         case DWET_KEYDOWN:
                              if (grabbed)
                                   break;
                              switch (evt.key_id) {
                                   case DIKI_RIGHT:
                                        if (window_surface_ctrl) {
                                             window_surface_ctrl->SetColor( window_surface_ctrl, 0xFF, 0xFF, 0x00, 0xFF );
                                             window_surface_ctrl->FillRectangle( window_surface_ctrl, 40+25-12, 90-12, 24, 24 );
                                        }
                                        if (active) active->Move( active, 1, 0 );
                                        break;
                                   case DIKI_LEFT:
                                        if (window_surface_ctrl) {
                                             window_surface_ctrl->SetColor( window_surface_ctrl, 0xFF, 0xFF, 0x00, 0xFF );
                                             window_surface_ctrl->FillRectangle( window_surface_ctrl, 40-25+12, 90-12, 24, 24 );
                                        }
                                        if (active) active->Move( active, -1, 0 );
                                        break;
                                   case DIKI_UP:
                                        if (window_surface_ctrl) {
                                             window_surface_ctrl->SetColor( window_surface_ctrl, 0xFF, 0xFF, 0x00, 0xFF );
                                             window_surface_ctrl->FillRectangle( window_surface_ctrl, 40, 90-25-12, 24, 24 );
                                        }
                                        if (active) active->Move( active, 0, -1 );
                                        break;
                                   case DIKI_DOWN:
                                        if (window_surface_ctrl) {
                                             window_surface_ctrl->SetColor( window_surface_ctrl, 0xFF, 0xFF, 0x00, 0xFF );
                                             window_surface_ctrl->FillRectangle( window_surface_ctrl, 40, 90+25-12, 24, 24 );
                                        }
                                        if (active) active->Move( active, 0, 1 );
                                        break;
                                   default:
                                        break;
                              }
                              break;

                         case DWET_LOSTFOCUS:
                              if (!grabbed && active == window) {
                                   active = NULL;
                              }
                              break;

                         default:
                              break;
                    }
               }

               switch (evt.type) {
                    case DWET_BUTTONDOWN:
                         if (lock_visible) {
                              /* require 3 presses to unlock */
                              unlock_count++;
                              if (unlock_count >= (unsigned)unlock_required) {
                                   lock_visible = 0;
                                   unlock_count = 0;
                                   if (window_lock)
                                        window_lock->SetOpacity( window_lock, 0x00 );
                                   last_input_ms = (unsigned int) direct_clock_get_millis();
                              }
                              break;
                         }
                         last_input_ms = (unsigned int) direct_clock_get_millis();
                         /* fallthrough */
                    case DWET_MOTION:
                    case DWET_ENTER:
                    case DWET_LEAVE:
                         endx = evt.cx;
                         endy = evt.cy;
                         winx = evt.x;
                         winy = evt.y;
                         winupdate = 1;
                         break;

                    case DWET_KEYDOWN:
                         if (lock_visible) {
                              /* require 3 presses to unlock */
                              unlock_count++;
                              if (unlock_count >= (unsigned)unlock_required) {
                                   lock_visible = 0;
                                   unlock_count = 0;
                                   if (window_lock)
                                        window_lock->SetOpacity( window_lock, 0x00 );
                                   last_input_ms = (unsigned int) direct_clock_get_millis();
                              }
                              break;
                         }
                         last_input_ms = (unsigned int) direct_clock_get_millis();
                         switch (evt.key_symbol) {
                              case DIKS_SMALL_L:
                              case DIKS_CAPITAL_L:
                                   if (window_lock) {
                                        lock_visible = !lock_visible;
                                        window_lock->SetOpacity( window_lock, lock_visible ? 0xFF : 0x00 );
                                        if (lock_visible) {
                                             window_lock->RaiseToTop( window_lock );
                                             window_lock->RequestFocus( window_lock );
                                        }
                                   }
                                   break;
                              /* Controller test: highlight face buttons and start/select on key press */
                              case DIKS_SMALL_A: /* A button */
                                   window_surface_ctrl->SetColor( window_surface_ctrl, 0x00, 0xFF, 0x00, 0xFF );
                                   window_surface_ctrl->FillRectangle( window_surface_ctrl, wdsc.width - 90 - 16, 90 + 34 - 16, 32, 32 );
                                   break;
                              case DIKS_SMALL_B: /* B button */
                                   window_surface_ctrl->SetColor( window_surface_ctrl, 0xFF, 0x00, 0x00, 0xFF );
                                   window_surface_ctrl->FillRectangle( window_surface_ctrl, wdsc.width - 90 + 34 - 16, 90 - 16, 32, 32 );
                                   break;
                              case DIKS_SMALL_X: /* X button */
                                   window_surface_ctrl->SetColor( window_surface_ctrl, 0x00, 0x80, 0xFF, 0xFF );
                                   window_surface_ctrl->FillRectangle( window_surface_ctrl, wdsc.width - 90 - 34 - 16, 90 - 16, 32, 32 );
                                   break;
                              case DIKS_SMALL_Y: /* Y button */
                                   window_surface_ctrl->SetColor( window_surface_ctrl, 0xFF, 0x80, 0x00, 0xFF );
                                   window_surface_ctrl->FillRectangle( window_surface_ctrl, wdsc.width - 90 - 16, 90 - 34 - 16, 32, 32 );
                                   break;
                              case DIKS_RETURN:  /* Start */
                                   window_surface_ctrl->SetColor( window_surface_ctrl, 0xFF, 0xFF, 0xFF, 0xFF );
                                   window_surface_ctrl->FillRectangle( window_surface_ctrl, wdsc.width/2 - 40 + 50, 20, 30, 12 );
                                   break;
                              case DIKS_SPACE:   /* Select */
                                   window_surface_ctrl->SetColor( window_surface_ctrl, 0xFF, 0xFF, 0xFF, 0xFF );
                                   window_surface_ctrl->FillRectangle( window_surface_ctrl, wdsc.width/2 - 40, 20, 30, 12 );
                                   break;
                              /* Shoulder buttons highlights (multiple symbol aliases) */
                              case DIKS_SMALL_Q: /* L1 */
                              case DIKS_HOME:
                                   window_surface_ctrl->SetColor( window_surface_ctrl, 0xFF, 0xD0, 0x00, 0xFF );
                                   window_surface_ctrl->FillRectangle( window_surface_ctrl, 8, 26, 44, 14 );
                                   break;
                              case DIKS_SMALL_W: /* L2 */
                              case DIKS_END:
                                   window_surface_ctrl->SetColor( window_surface_ctrl, 0xFF, 0xA0, 0x00, 0xFF );
                                   window_surface_ctrl->FillRectangle( window_surface_ctrl, 8, 26 + 14 + 6, 44, 14 );
                                   break;
                              case DIKS_SMALL_E: /* R1 */
                              case DIKS_PAGE_UP:
                                   window_surface_ctrl->SetColor( window_surface_ctrl, 0x00, 0xD0, 0xFF, 0xFF );
                                   window_surface_ctrl->FillRectangle( window_surface_ctrl, wdsc.width - 44 - 8, 26, 44, 14 );
                                   break;
                              case DIKS_SMALL_R: /* R2 */
                              case DIKS_PAGE_DOWN:
                                   window_surface_ctrl->SetColor( window_surface_ctrl, 0x00, 0xA0, 0xFF, 0xFF );
                                   window_surface_ctrl->FillRectangle( window_surface_ctrl, wdsc.width - 44 - 8, 26 + 14 + 6, 44, 14 );
                                   break;
                              case DIKS_ESCAPE:
                              case DIKS_BACK:
                              case DIKS_STOP:
                              case DIKS_EXIT:
                                   /* quit main loop */
                                   quit = 1;
                                   break;

                              case DIKS_SMALL_I:
                                   if (active == window1) {
                                        invisible_cursor1 = !invisible_cursor1;
                             window1->SetCursorFlags( window1,
                                                      invisible_cursor1 ? DWCF_INVISIBLE : DWCF_NONE );
   }
                                   else {
                                        invisible_cursor2 = !invisible_cursor2;
                                        window2->SetCursorFlags( window2,
                                                                 invisible_cursor2 ? DWCF_INVISIBLE : DWCF_NONE );
         /* idle timeout -> show lock overlay and grab focus */
         {
              unsigned int now_ms = (unsigned int) direct_clock_get_millis();
              if (!lock_visible && (now_ms - last_input_ms >= idle_timeout_ms)) {
                   lock_visible = 1;
                   unlock_count = 0;
                   if (window_lock) {
                        window_lock->SetOpacity( window_lock, 0xFF );
                        window_lock->RaiseToTop( window_lock );
                        window_lock->RequestFocus( window_lock );
                   }
              }
         }
         /* external signals from keymon */
         if (want_lock_signal) {
              want_lock_signal = 0;
              lock_visible = 1;
              unlock_count = 0;
              if (window_lock) {
                   window_lock->SetOpacity( window_lock, 0xFF );
                   window_lock->RaiseToTop( window_lock );
                   window_lock->RequestFocus( window_lock );
              }
         }
         if (want_unlock_signal) {
              want_unlock_signal = 0;
              lock_visible = 0;
              unlock_count = 0;
              if (window_lock)
                   window_lock->SetOpacity( window_lock, 0x00 );
              last_input_ms = (unsigned int) direct_clock_get_millis();
         }
    }
                                   break;

                              case DIKS_SMALL_O:
                                   layer->SetCooperativeLevel( layer, DLSCL_ADMINISTRATIVE );
                                   layer->SetCursorOpacity( layer, sin( direct_clock_get_millis() / 300.0 ) * 85 + 170 );
                                   layer->SetCooperativeLevel( layer, DLSCL_SHARED );
                                   break;

                              case DIKS_SMALL_P:
                                   cursor_enabled = !cursor_enabled;
                                   layer->SetCooperativeLevel( layer, DLSCL_ADMINISTRATIVE );
                                   layer->EnableCursor( layer, cursor_enabled );
                                   layer->SetCooperativeLevel( layer, DLSCL_SHARED );
                                   break;

                              case DIKS_SMALL_T: /* rotate test */
                                   if (active) {
                                        rotation = (rotation + 90) % 360;
                                        active->SetRotation( active, rotation );
                                   }
                                   break;

                              default:
                                   break;
                         }
                         break;

                    default:
                         break;
               }
          }

          /* On key/button release, redraw controller layout to clear highlights */
          if (event_buffer->HasEvent( event_buffer )) {
               /* no-op: handled in loop */
          } else {
               if (window_surface_ctrl) {
                    /* simple redraw background and shapes (resets highlights) */
                    int cw, ch;
                    window_surface_ctrl->GetSize( window_surface_ctrl, &cw, &ch );
                    window_surface_ctrl->SetColor( window_surface_ctrl, 0x00, 0x00, 0x00, 0xA0 );
                    window_surface_ctrl->FillRectangle( window_surface_ctrl, 0, 0, cw, ch );
                    window_surface_ctrl->SetFont( window_surface_ctrl, font );
                    window_surface_ctrl->SetColor( window_surface_ctrl, 0xFF, 0xFF, 0xFF, 0xFF );
                    window_surface_ctrl->DrawString( window_surface_ctrl, "Controller Test", -1, 8, 4, DSTF_TOPLEFT );
                    /* redraw static controls */
                    int dpx = 40, dpy = 90, dpw = 24, dps = 50;
                    window_surface_ctrl->SetColor( window_surface_ctrl, 0x40, 0x40, 0x40, 0xFF );
                    window_surface_ctrl->FillRectangle( window_surface_ctrl, dpx, dpy - dps/2, dpw, dps ); /* vertical */
                    window_surface_ctrl->FillRectangle( window_surface_ctrl, dpx - dps/2 + dpw/2, dpy - dpw/2, dps, dpw ); /* horizontal */
                    int cx = cw - 90, cy = 90, r = 16, gap = 34;
                    window_surface_ctrl->SetColor( window_surface_ctrl, 0x80, 0x80, 0x80, 0xFF );
                    window_surface_ctrl->FillRectangle( window_surface_ctrl, cx - r, cy - gap - r, 2*r, 2*r ); /* Y */
                    window_surface_ctrl->FillRectangle( window_surface_ctrl, cx - r, cy + gap - r, 2*r, 2*r ); /* A */
                    window_surface_ctrl->FillRectangle( window_surface_ctrl, cx - gap - r, cy - r, 2*r, 2*r ); /* X */
                    window_surface_ctrl->FillRectangle( window_surface_ctrl, cx + gap - r, cy - r, 2*r, 2*r ); /* B */
                    int sx2 = cw/2 - 40, sy2 = 20;
                    window_surface_ctrl->FillRectangle( window_surface_ctrl, sx2, sy2, 30, 12 );      /* Select */
                    window_surface_ctrl->FillRectangle( window_surface_ctrl, sx2 + 50, sy2, 30, 12 ); /* Start */
                    /* shoulder buttons */
                    int sh_h = 14, sh_w = 44, sh_gap = 6;
                    int lbase_x = 8, lbase_y = 8 + 18;
                    int rbase_x = cw - sh_w - 8, rbase_y = 8 + 18;
                    window_surface_ctrl->SetColor( window_surface_ctrl, 0x60, 0x60, 0x60, 0xFF );
                    window_surface_ctrl->FillRectangle( window_surface_ctrl, lbase_x, lbase_y, sh_w, sh_h ); /* L1 */
                    window_surface_ctrl->FillRectangle( window_surface_ctrl, lbase_x, lbase_y + sh_h + sh_gap, sh_w, sh_h ); /* L2 */
                    window_surface_ctrl->FillRectangle( window_surface_ctrl, rbase_x, rbase_y, sh_w, sh_h ); /* R1 */
                    window_surface_ctrl->FillRectangle( window_surface_ctrl, rbase_x, rbase_y + sh_h + sh_gap, sh_w, sh_h ); /* R2 */
                    window_surface_ctrl->SetColor( window_surface_ctrl, 0xFF, 0xFF, 0xFF, 0xFF );
                    window_surface_ctrl->DrawString( window_surface_ctrl, "L1", -1, lbase_x + 4, lbase_y + 2, DSTF_TOPLEFT );
                    window_surface_ctrl->DrawString( window_surface_ctrl, "L2", -1, lbase_x + 4, lbase_y + sh_h + sh_gap + 2, DSTF_TOPLEFT );
                    window_surface_ctrl->DrawString( window_surface_ctrl, "R1", -1, rbase_x + 4, rbase_y + 2, DSTF_TOPLEFT );
                    window_surface_ctrl->DrawString( window_surface_ctrl, "R2", -1, rbase_x + 4, rbase_y + sh_h + sh_gap + 2, DSTF_TOPLEFT );
               }
          }

          if (active) {
               if (grabbed == DIBM_LEFT) {
                    if (startx == endx && starty == endy) {
                         if (event_buffer->WaitForEventWithTimeout( event_buffer, 2, 0 ) == DFB_TIMEOUT) {
                              /* quit main loop */
                              quit = 1;
                         }
                    }
                    else {
                         active->Move( active, endx - startx, endy - starty );
                         startx = endx;
                         starty = endy;
                    }
               }
               else if (grabbed == DIBM_RIGHT) {
                    active->SetOpacity( active, sin( direct_clock_get_millis() / 300.0 ) * 85 + 170 );
               }
               else if (winupdate) {
                    DFBRectangle rect;
                    DFBRegion    region;
                    char         buf[32];

                    snprintf( buf, sizeof(buf), "x/y: %4d,%4d", winx, winy );

                    DFBCHECK(font->GetStringExtents( font, buf, -1, &rect, NULL ));

                    rect.x  = 1;
                    rect.y  = 1;
                    rect.w += rect.w / 3;
                    rect.h += 10;

                    window_surface1->SetColor( window_surface1, 0x10, 0x10, 0x10, 0x77 );
                    window_surface1->FillRectangles( window_surface1, &rect, 1 );

                    window_surface1->SetColor( window_surface1, 0x88, 0xCC, 0xFF, 0xAA );
                    window_surface1->DrawString( window_surface1, buf, -1, rect.h / 4, 5, DSTF_TOPLEFT );

                    region = DFB_REGION_INIT_FROM_RECTANGLE( &rect );

                    window_surface1->Flip( window_surface1, &region, DSFLIP_NONE );

                    winupdate = 0;
               }
          }
     }

     return 42;
}
