/*
   EGL Mali fbdev system module for DirectFB2.

   Based on DirectFB2-eglrpi, adapted for Mali fbdev EGL.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.
*/

#ifndef __EGL_SYSTEM_H__
#define __EGL_SYSTEM_H__

#include <core/coretypes.h>
#include <fusion/types.h>
#include <EGL/egl.h>

/**********************************************************************************************************************/

/* Mali fbdev native window type — matches mali.h from Mali driver */
typedef struct {
     unsigned short width;
     unsigned short height;
} fbdev_window_s;

typedef struct {
     FusionSHMPoolShared *shmpool;

     CoreSurfacePool     *pool;

     DFBDimension         mode;
} EGLMaliDataShared;

typedef struct {
     EGLMaliDataShared *shared;

     CoreDFB           *core;

     int                fb_fd;

     fbdev_window_s     native_window;

     EGLDisplay         eglDisplay;
     EGLSurface         eglSurface;
     EGLContext         eglContext;

     DFBDimension       size;

     /* Blit-to-screen shader (initialized in egl_layer.c). */
     unsigned int       blit_program;
     int                blit_attr_pos;
     int                blit_attr_tex;
} EGLMaliData;

#endif
