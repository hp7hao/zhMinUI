/*
   EGL Mali fbdev system module for DirectFB2.

   Based on DirectFB2-eglrpi, adapted for Mali fbdev EGL.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.
*/

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <unistd.h>

#include <core/core.h>
#include <core/core_system.h>
#include <core/layers.h>
#include <core/screens.h>
#include <core/surface_pool.h>
#include <fusion/shmalloc.h>

#include "egl_system.h"

D_DEBUG_DOMAIN( EGL_System, "EGL/System", "EGL Mali fbdev System Module" );

DFB_CORE_SYSTEM( egl_mali_fbdev )

/**********************************************************************************************************************/

extern const ScreenFuncs       eglScreenFuncs;
extern const DisplayLayerFuncs eglPrimaryLayerFuncs;
extern const SurfacePoolFuncs  eglSurfacePoolFuncs;

static DFBResult
local_init( EGLMaliData *egl )
{
     CoreScreen                *screen;
     EGLConfig                  config;
     EGLint                     num_config;
     struct fb_var_screeninfo   vinfo;
     const char                *fb_device;
     const EGLint               config_attr[]  = { EGL_RED_SIZE,   8,
                                                   EGL_GREEN_SIZE, 8,
                                                   EGL_BLUE_SIZE,  8,
                                                   EGL_ALPHA_SIZE, 8,
                                                   EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                                                   EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                                                   EGL_NONE };
     const EGLint               context_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2,
                                                   EGL_NONE };

     /* Open framebuffer device. */
     fb_device = getenv( "FRAMEBUFFER" );
     if (!fb_device)
          fb_device = "/dev/fb0";

     egl->fb_fd = open( fb_device, O_RDWR | O_CLOEXEC );
     if (egl->fb_fd < 0) {
          D_ERROR( "EGL/System: Failed to open '%s'!\n", fb_device );
          return DFB_INIT;
     }

     /* Get display dimensions from framebuffer. */
     if (ioctl( egl->fb_fd, FBIOGET_VSCREENINFO, &vinfo ) < 0) {
          D_ERROR( "EGL/System: FBIOGET_VSCREENINFO failed!\n" );
          close( egl->fb_fd );
          return DFB_INIT;
     }

     egl->size.w = vinfo.xres;
     egl->size.h = vinfo.yres;

     D_INFO( "EGL/System: Display %dx%d from fbdev\n", egl->size.w, egl->size.h );

     /* Set up Mali fbdev native window. */
     egl->native_window.width  = egl->size.w;
     egl->native_window.height = egl->size.h;

     /* Initialize EGL. */
     egl->eglDisplay = eglGetDisplay( EGL_DEFAULT_DISPLAY );
     if (egl->eglDisplay == EGL_NO_DISPLAY) {
          D_ERROR( "EGL/System: eglGetDisplay() failed: 0x%x!\n", (unsigned int) eglGetError() );
          close( egl->fb_fd );
          return DFB_INIT;
     }

     if (!eglInitialize( egl->eglDisplay, NULL, NULL )) {
          D_ERROR( "EGL/System: eglInitialize() failed: 0x%x!\n", (unsigned int) eglGetError() );
          close( egl->fb_fd );
          return DFB_INIT;
     }

     if (!eglChooseConfig( egl->eglDisplay, config_attr, &config, 1, &num_config ) || (num_config != 1)) {
          D_ERROR( "EGL/System: eglChooseConfig() failed: 0x%x!\n", (unsigned int) eglGetError() );
          eglTerminate( egl->eglDisplay );
          close( egl->fb_fd );
          return DFB_INIT;
     }

     /* Create EGL window surface using Mali fbdev native window. */
     egl->eglSurface = eglCreateWindowSurface( egl->eglDisplay, config,
                                                (EGLNativeWindowType) &egl->native_window, NULL );
     if (egl->eglSurface == EGL_NO_SURFACE) {
          D_ERROR( "EGL/System: eglCreateWindowSurface() failed: 0x%x!\n", (unsigned int) eglGetError() );
          eglTerminate( egl->eglDisplay );
          close( egl->fb_fd );
          return DFB_INIT;
     }

     /* Create EGL context. */
     egl->eglContext = eglCreateContext( egl->eglDisplay, config, EGL_NO_CONTEXT, context_attr );
     if (egl->eglContext == EGL_NO_CONTEXT) {
          D_ERROR( "EGL/System: eglCreateContext() failed: 0x%x!\n", (unsigned int) eglGetError() );
          eglDestroySurface( egl->eglDisplay, egl->eglSurface );
          eglTerminate( egl->eglDisplay );
          close( egl->fb_fd );
          return DFB_INIT;
     }

     /* Make current. */
     if (!eglMakeCurrent( egl->eglDisplay, egl->eglSurface, egl->eglSurface, egl->eglContext )) {
          D_ERROR( "EGL/System: eglMakeCurrent() failed: 0x%x!\n", (unsigned int) eglGetError() );
          eglDestroyContext( egl->eglDisplay, egl->eglContext );
          eglDestroySurface( egl->eglDisplay, egl->eglSurface );
          eglTerminate( egl->eglDisplay );
          close( egl->fb_fd );
          return DFB_INIT;
     }

     D_INFO( "EGL/System: EGL initialized successfully on Mali fbdev\n" );

     /* Store display size in shared memory for slave processes. */
     egl->shared->mode.w = egl->size.w;
     egl->shared->mode.h = egl->size.h;

     /* Register screen and layer. */
     screen = dfb_screens_register( egl, &eglScreenFuncs );

     dfb_layers_register( screen, egl, &eglPrimaryLayerFuncs );

     return DFB_OK;
}

static DFBResult
local_deinit( EGLMaliData *egl )
{
     if (egl->eglContext) {
          eglMakeCurrent( egl->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
          eglDestroyContext( egl->eglDisplay, egl->eglContext );
     }

     if (egl->eglSurface)
          eglDestroySurface( egl->eglDisplay, egl->eglSurface );

     if (egl->eglDisplay)
          eglTerminate( egl->eglDisplay );

     if (egl->fb_fd >= 0)
          close( egl->fb_fd );

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->version.major = 0;
     info->version.minor = 1;

     info->caps = CSCAPS_ACCELERATION | CSCAPS_ALWAYS_INDIRECT;

     snprintf( info->name,   DFB_CORE_SYSTEM_INFO_NAME_LENGTH,   "EGL Mali fbdev" );
     snprintf( info->vendor, DFB_CORE_SYSTEM_INFO_VENDOR_LENGTH, "zhMinUI" );
}

static DFBResult
system_initialize( CoreDFB  *core,
                   void    **ret_data )
{
     DFBResult            ret;
     EGLMaliData         *egl;
     EGLMaliDataShared   *shared;
     FusionSHMPoolShared *pool;

     D_DEBUG_AT( EGL_System, "%s()\n", __FUNCTION__ );

     egl = D_CALLOC( 1, sizeof(EGLMaliData) );
     if (!egl)
          return D_OOM();

     egl->core  = core;
     egl->fb_fd = -1;

     pool = dfb_core_shmpool( core );

     shared = SHCALLOC( pool, 1, sizeof(EGLMaliDataShared) );
     if (!shared) {
          D_FREE( egl );
          return D_OOSHM();
     }

     shared->shmpool = pool;

     egl->shared = shared;

     ret = local_init( egl );
     if (ret)
          goto error;

     *ret_data = egl;

     ret = dfb_surface_pool_initialize( core, &eglSurfacePoolFuncs, &shared->pool );
     if (ret)
          goto error;

     ret = core_arena_add_shared_field( core, "egl_mali", shared );
     if (ret)
          goto error;

     return DFB_OK;

error:
     local_deinit( egl );

     SHFREE( pool, shared );

     D_FREE( egl );

     return ret;
}

static DFBResult
system_join( CoreDFB  *core,
             void    **ret_data )
{
     DFBResult          ret;
     EGLMaliData       *egl;
     EGLMaliDataShared *shared;

     D_DEBUG_AT( EGL_System, "%s()\n", __FUNCTION__ );

     egl = D_CALLOC( 1, sizeof(EGLMaliData) );
     if (!egl)
          return D_OOM();

     egl->core  = core;
     egl->fb_fd = -1;

     ret = core_arena_get_shared_field( core, "egl_mali", (void**) &shared );
     if (ret) {
          D_FREE( egl );
          return ret;
     }

     egl->shared = shared;

     /* Slave process does NOT init EGL — with CSCAPS_ALWAYS_INDIRECT,
        all rendering goes through the master process via Fusion IPC.
        We only need the shared data for display dimensions. */
     egl->size.w = shared->mode.w;
     egl->size.h = shared->mode.h;

     /* Register screen and layer — DFB2 core requires matching registrations
        in joining processes even though rendering is indirect. */
     {
          CoreScreen *screen;
          screen = dfb_screens_register( egl, &eglScreenFuncs );
          dfb_layers_register( screen, egl, &eglPrimaryLayerFuncs );
     }

     *ret_data = egl;

     ret = dfb_surface_pool_join( core, shared->pool, &eglSurfacePoolFuncs );
     if (ret)
          goto error;

     return DFB_OK;

error:
     D_FREE( egl );

     return ret;
}

static DFBResult
system_shutdown( bool emergency )
{
     EGLMaliData       *egl = dfb_system_data();
     EGLMaliDataShared *shared;

     D_DEBUG_AT( EGL_System, "%s()\n", __FUNCTION__ );

     D_ASSERT( egl != NULL );
     D_ASSERT( egl->shared != NULL );

     shared = egl->shared;

     dfb_surface_pool_destroy( shared->pool );

     local_deinit( egl );

     SHFREE( shared->shmpool, shared );

     D_FREE( egl );

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     EGLMaliData       *egl = dfb_system_data();
     EGLMaliDataShared *shared;

     D_DEBUG_AT( EGL_System, "%s()\n", __FUNCTION__ );

     D_ASSERT( egl != NULL );
     D_ASSERT( egl->shared != NULL );

     shared = egl->shared;

     dfb_surface_pool_leave( shared->pool );

     /* Slave process did not init EGL, so nothing to deinit. */

     D_FREE( egl );

     return DFB_OK;
}

static DFBResult
system_suspend()
{
     return DFB_OK;
}

static DFBResult
system_resume()
{
     return DFB_OK;
}

static VideoMode *
system_get_modes()
{
     return NULL;
}

static VideoMode *
system_get_current_mode()
{
     return NULL;
}

static DFBResult
system_thread_init()
{
     return DFB_OK;
}

static bool
system_input_filter( CoreInputDevice *device,
                     DFBInputEvent   *event )
{
     return false;
}

static volatile void *
system_map_mmio( unsigned int offset,
                 int          length )
{
     return NULL;
}

static void
system_unmap_mmio( volatile void *addr,
                   int            length )
{
}

static unsigned int
system_get_accelerator()
{
     return 0xffffffff;
}

static unsigned long
system_video_memory_physical( unsigned int offset )
{
     return 0;
}

static void *
system_video_memory_virtual( unsigned int offset )
{
     return NULL;
}

static unsigned int
system_videoram_length()
{
     return 0;
}

static void
system_get_busid( int *ret_bus,
                  int *ret_dev,
                  int *ret_func )
{
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
}
