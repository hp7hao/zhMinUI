/*
   EGL Mali fbdev surface pool for DirectFB2.

   Based on DirectFB2-eglrpi.

   GPU-only surface pool — surfaces are GL textures + FBOs.
   CPU access is provided via shadow buffers: Lock with CSAID_CPU
   allocates a temp buffer, reads GPU content into it (glReadPixels),
   and Unlock writes modified data back (glTexSubImage2D).

   - AllocateBuffer/DeallocateBuffer: master-only (GL texture + FBO lifecycle)
   - Lock/Unlock: GPU access (bind texture/FBO) or CPU access (shadow buffer)
   - Read: glReadPixels — GPU→CPU transfer (called by DFB2 core)
   - Write: glTexSubImage2D — CPU→GPU transfer (called by DFB2 core)

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.
*/

#include <stdlib.h>
#include <core/core.h>
#include <core/surface_allocation.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

/* GL_EXT_texture_format_BGRA8888 — matches DFB2's DSPF_ARGB byte order. */
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

#include "egl_system.h"

D_DEBUG_DOMAIN( EGL_Surfaces, "EGL/Surfaces", "EGL Mali Surface Pool" );
D_DEBUG_DOMAIN( EGL_SurfLock, "EGL/SurfLock", "EGL Mali Surface Pool Locks" );

/**********************************************************************************************************************/

typedef struct {
     int    magic;

     int    pitch;
     int    size;
     int    w;
     int    h;

     GLuint tex;
     GLuint fbo;

     void  *shadow;   /* CPU shadow buffer — stashed here so Unlock can free
                         the original malloc pointer even if core adjusts lock->addr. */
} EGLAllocationData;

/**********************************************************************************************************************/

static int
eglAllocationDataSize( void )
{
     return sizeof(EGLAllocationData);
}

static DFBResult
eglInitPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data,
             CoreSurfacePoolDescription *ret_desc )
{
     D_DEBUG_AT( EGL_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps              = CSPCAPS_VIRTUAL;
     ret_desc->access[CSAID_GPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->types             = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority          = CSPP_PREFERED;

     ret_desc->access[CSAID_LAYER0] = CSAF_READ | CSAF_SHARED;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "EGL Mali Surface Pool" );

     return DFB_OK;
}

static DFBResult
eglJoinPool( CoreDFB         *core,
             CoreSurfacePool *pool,
             void            *pool_data,
             void            *pool_local,
             void            *system_data )
{
     D_DEBUG_AT( EGL_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
eglDestroyPool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     D_DEBUG_AT( EGL_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
eglLeavePool( CoreSurfacePool *pool,
              void            *pool_data,
              void            *pool_local )
{
     D_DEBUG_AT( EGL_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
eglTestConfig( CoreSurfacePool         *pool,
               void                    *pool_data,
               void                    *pool_local,
               CoreSurfaceBuffer       *buffer,
               const CoreSurfaceConfig *config )
{
     D_DEBUG_AT( EGL_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );

     /* Only accept 32-bit ARGB surfaces — the pool stores everything as
        GL_RGBA textures, so Read/Write assume 4 bytes per pixel.
        Non-ARGB surfaces (e.g. RGB16) fall through to shared_surface_pool.
        The CSTF_LAYER surface is always ARGB (set by egl_layer). */
     switch (config->format) {
          case DSPF_ARGB:
          case DSPF_ABGR:
          case DSPF_AiRGB:
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

/* AllocateBuffer is only called in the master process. */
static DFBResult
eglAllocateBuffer( CoreSurfacePool       *pool,
                   void                  *pool_data,
                   void                  *pool_local,
                   CoreSurfaceBuffer     *buffer,
                   CoreSurfaceAllocation *allocation,
                   void                  *alloc_data )
{
     CoreSurface       *surface;
     EGLAllocationData *alloc = alloc_data;
     GLint              tex;
     GLint              fbo;

     D_DEBUG_AT( EGL_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );

     surface = buffer->surface;

     alloc->w = surface->config.size.w;
     alloc->h = surface->config.size.h;
     alloc->shadow = NULL;

     dfb_surface_calc_buffer_size( surface, 8, 1, &alloc->pitch, &alloc->size );

     D_DEBUG_AT( EGL_Surfaces, "  -> pitch %d, size %d\n", alloc->pitch, alloc->size );

     allocation->size   = alloc->size;
     allocation->offset = -1;

     /* Save current GL state. */
     glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo );
     glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex );

     /* Create texture.
        Use GL_BGRA_EXT so the byte order matches DFB2's DSPF_ARGB
        (little-endian: B,G,R,A bytes = 0xAARRGGBB as u32).
        GL_EXT_texture_format_BGRA8888 is supported by Mali. */
     glGenTextures( 1, &alloc->tex );
     glBindTexture( GL_TEXTURE_2D, alloc->tex );
     glTexImage2D( GL_TEXTURE_2D, 0, GL_BGRA_EXT, alloc->w, alloc->h, 0,
                   GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL );

     /* Create FBO. */
     glGenFramebuffers( 1, &alloc->fbo );
     glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );
     glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, alloc->tex, 0 );

     /* Restore previous GL state. */
     glBindTexture( GL_TEXTURE_2D, tex );
     glBindFramebuffer( GL_FRAMEBUFFER, fbo );

     D_DEBUG_AT( EGL_Surfaces, "  -> tex %u, fbo %u\n", alloc->tex, alloc->fbo );

     D_MAGIC_SET( alloc, EGLAllocationData );

     return DFB_OK;
}

/* DeallocateBuffer is only called in the master process. */
static DFBResult
eglDeallocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     EGLAllocationData *alloc = alloc_data;

     D_DEBUG_AT( EGL_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( alloc, EGLAllocationData );

     if (alloc->shadow) {
          free( alloc->shadow );
          alloc->shadow = NULL;
     }

     glDeleteFramebuffers( 1, &alloc->fbo );
     glDeleteTextures( 1, &alloc->tex );

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

/* Lock — GPU or CPU access.
   GPU: bind texture/FBO as before.
   CPU: allocate shadow buffer, optionally read current GPU content into it.
   Called in the master process (CSCAPS_ALWAYS_INDIRECT ensures all
   GPU operations from slaves are dispatched to master via Fusion IPC). */
static DFBResult
eglLock( CoreSurfacePool       *pool,
         void                  *pool_data,
         void                  *pool_local,
         CoreSurfaceAllocation *allocation,
         void                  *alloc_data,
         CoreSurfaceBufferLock *lock )
{
     EGLAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, EGLAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( EGL_SurfLock, "%s( %p, accessor=0x%x, access=0x%x )\n",
                 __FUNCTION__, lock->buffer, lock->accessor, lock->access );

     lock->pitch  = alloc->pitch;
     lock->offset = ~0;
     lock->addr   = NULL;
     lock->phys   = 0;
     lock->handle = (void*)(long) alloc->tex;

     if (lock->accessor == CSAID_CPU) {
          /* CPU access — allocate shadow buffer and read GPU content if needed. */
          void *shadow = malloc( alloc->size );
          if (!shadow) {
               D_ERROR( "EGL/SurfLock: Failed to allocate %d byte shadow buffer!\n", alloc->size );
               return DFB_NOSYSTEMMEMORY;
          }

          if (lock->access & CSAF_READ) {
               GLint prev_fbo;
               glGetIntegerv( GL_FRAMEBUFFER_BINDING, &prev_fbo );
               glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );
               glReadPixels( 0, 0, alloc->w, alloc->h,
                             GL_BGRA_EXT, GL_UNSIGNED_BYTE, shadow );
               glBindFramebuffer( GL_FRAMEBUFFER, prev_fbo );
          }

          alloc->shadow = shadow;
          lock->addr    = shadow;
          return DFB_OK;
     }

     /* GPU access path — always render into the allocation's own FBO/texture.
        The layer's UpdateRegion will blit the texture to FBO 0 for display. */
     if (lock->access & CSAF_WRITE)
          glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );

     if (lock->access & CSAF_READ) {
          glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );
          glBindTexture( GL_TEXTURE_2D, alloc->tex );
     }

     return DFB_OK;
}

static DFBResult
eglUnlock( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock )
{
     EGLAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, EGLAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( EGL_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     if (lock->accessor == CSAID_CPU && alloc->shadow) {
          /* Write shadow buffer back to GPU texture if it was a write lock. */
          if (lock->access & CSAF_WRITE) {
               GLint prev_tex;
               glGetIntegerv( GL_TEXTURE_BINDING_2D, &prev_tex );
               glBindTexture( GL_TEXTURE_2D, alloc->tex );
               glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, alloc->w, alloc->h,
                                GL_BGRA_EXT, GL_UNSIGNED_BYTE, alloc->shadow );
               glBindTexture( GL_TEXTURE_2D, prev_tex );
          }

          free( alloc->shadow );
          alloc->shadow = NULL;
          lock->addr    = NULL;
     }

     return DFB_OK;
}

/* Read — GPU→CPU transfer via glReadPixels.
   Called by DFB2 core (in master process) when it needs to transfer
   data from this GPU pool to the shared_surface_pool for CPU access. */
static DFBResult
eglRead( CoreSurfacePool       *pool,
         void                  *pool_data,
         void                  *pool_local,
         CoreSurfaceAllocation *allocation,
         void                  *alloc_data,
         void                  *destination,
         int                    pitch,
         const DFBRectangle    *rect )
{
     EGLAllocationData *alloc = alloc_data;
     GLint              prev_fbo;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, EGLAllocationData );

     D_DEBUG_AT( EGL_SurfLock, "%s( %p, %d,%d-%dx%d )\n", __FUNCTION__,
                 allocation->buffer, rect ? rect->x : 0, rect ? rect->y : 0,
                 rect ? rect->w : alloc->w, rect ? rect->h : alloc->h );

     glGetIntegerv( GL_FRAMEBUFFER_BINDING, &prev_fbo );

     glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );

     if (rect) {
          if (pitch == alloc->pitch) {
               glReadPixels( rect->x, rect->y, rect->w, rect->h,
                             GL_BGRA_EXT, GL_UNSIGNED_BYTE, destination );
          }
          else {
               /* Row-by-row read when pitches differ. */
               int    y;
               char  *dst = destination;
               for (y = 0; y < rect->h; y++) {
                    glReadPixels( rect->x, rect->y + y, rect->w, 1,
                                  GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst );
                    dst += pitch;
               }
          }
     }
     else {
          glReadPixels( 0, 0, alloc->w, alloc->h,
                        GL_BGRA_EXT, GL_UNSIGNED_BYTE, destination );
     }

     glBindFramebuffer( GL_FRAMEBUFFER, prev_fbo );

     return DFB_OK;
}

/* Write — CPU→GPU transfer via glTexSubImage2D.
   Called by DFB2 core (in master process) when it needs to transfer
   data from the shared_surface_pool to this GPU pool. */
static DFBResult
eglWrite( CoreSurfacePool       *pool,
          void                  *pool_data,
          void                  *pool_local,
          CoreSurfaceAllocation *allocation,
          void                  *alloc_data,
          const void            *source,
          int                    pitch,
          const DFBRectangle    *rect )
{
     EGLAllocationData *alloc = alloc_data;
     GLint              tex;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, EGLAllocationData );

     D_DEBUG_AT( EGL_SurfLock, "%s( %p, %d,%d-%dx%d )\n", __FUNCTION__,
                 allocation->buffer, rect ? rect->x : 0, rect ? rect->y : 0,
                 rect ? rect->w : alloc->w, rect ? rect->h : alloc->h );

     glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex );

     glBindTexture( GL_TEXTURE_2D, alloc->tex );

     if (rect) {
          if (pitch == alloc->pitch) {
               glTexSubImage2D( GL_TEXTURE_2D, 0, rect->x, rect->y, rect->w, rect->h,
                                GL_BGRA_EXT, GL_UNSIGNED_BYTE, source );
          }
          else {
               /* Row-by-row upload when pitches differ. */
               int         y;
               const char *src = source;
               for (y = 0; y < rect->h; y++) {
                    glTexSubImage2D( GL_TEXTURE_2D, 0, rect->x, rect->y + y, rect->w, 1,
                                     GL_BGRA_EXT, GL_UNSIGNED_BYTE, src );
                    src += pitch;
               }
          }
     }
     else {
          glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, alloc->w, alloc->h,
                           GL_BGRA_EXT, GL_UNSIGNED_BYTE, source );
     }

     glBindTexture( GL_TEXTURE_2D, tex );

     return DFB_OK;
}

const SurfacePoolFuncs eglSurfacePoolFuncs = {
     .AllocationDataSize = eglAllocationDataSize,
     .InitPool           = eglInitPool,
     .JoinPool           = eglJoinPool,
     .DestroyPool        = eglDestroyPool,
     .LeavePool          = eglLeavePool,
     .TestConfig         = eglTestConfig,
     .AllocateBuffer     = eglAllocateBuffer,
     .DeallocateBuffer   = eglDeallocateBuffer,
     .Lock               = eglLock,
     .Unlock             = eglUnlock,
     .Read               = eglRead,
     .Write              = eglWrite
};
