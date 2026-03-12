/*
   EGL Mali fbdev display layer for DirectFB2.

   Based on DirectFB2-eglrpi.

   UpdateRegion blits the layer's texture to FBO 0 (the EGL window surface)
   using a minimal GLES2 fullscreen-quad shader, then calls eglSwapBuffers.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.
*/

#include <stdio.h>
#include <core/layers.h>
#include <GLES2/gl2.h>

#include "egl_system.h"

D_DEBUG_DOMAIN( EGL_Layer, "EGL/Layer", "EGL Mali fbdev Layer" );

/**********************************************************************************************************************/

/* Minimal blit shader — draws a fullscreen textured quad. */

static const char *blit_vert_src =
     "attribute vec2 aPos;\n"
     "attribute vec2 aTex;\n"
     "varying vec2 vTex;\n"
     "void main() {\n"
     "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
     "    vTex = aTex;\n"
     "}\n";

static const char *blit_frag_src =
     "precision mediump float;\n"
     "varying vec2 vTex;\n"
     "uniform sampler2D uSampler;\n"
     "void main() {\n"
     "    gl_FragColor = texture2D(uSampler, vTex);\n"
     "}\n";

static GLuint
compile_shader( GLenum type, const char *src )
{
     GLuint shader = glCreateShader( type );
     GLint  ok;

     glShaderSource( shader, 1, &src, NULL );
     glCompileShader( shader );

     glGetShaderiv( shader, GL_COMPILE_STATUS, &ok );
     if (!ok) {
          char log[512];
          glGetShaderInfoLog( shader, sizeof(log), NULL, log );
          D_ERROR( "EGL/Layer: Shader compile error: %s\n", log );
          glDeleteShader( shader );
          return 0;
     }

     return shader;
}

static void
init_blit_program( EGLMaliData *egl )
{
     GLuint vert, frag, prog;
     GLint  ok;

     vert = compile_shader( GL_VERTEX_SHADER, blit_vert_src );
     frag = compile_shader( GL_FRAGMENT_SHADER, blit_frag_src );

     if (!vert || !frag) {
          D_ERROR( "EGL/Layer: Failed to compile blit shaders!\n" );
          return;
     }

     prog = glCreateProgram();
     glAttachShader( prog, vert );
     glAttachShader( prog, frag );
     glLinkProgram( prog );

     glGetProgramiv( prog, GL_LINK_STATUS, &ok );
     if (!ok) {
          char log[512];
          glGetProgramInfoLog( prog, sizeof(log), NULL, log );
          D_ERROR( "EGL/Layer: Program link error: %s\n", log );
          glDeleteProgram( prog );
          glDeleteShader( vert );
          glDeleteShader( frag );
          return;
     }

     /* Shaders can be detached after linking. */
     glDeleteShader( vert );
     glDeleteShader( frag );

     egl->blit_program = prog;
     egl->blit_attr_pos = glGetAttribLocation( prog, "aPos" );
     egl->blit_attr_tex = glGetAttribLocation( prog, "aTex" );

     D_INFO( "EGL/Layer: Blit shader program %u initialized (pos=%d, tex=%d)\n",
             prog, egl->blit_attr_pos, egl->blit_attr_tex );
}

/**********************************************************************************************************************/

static DFBResult
eglPrimaryInitLayer( CoreLayer                  *layer,
                     void                       *driver_data,
                     void                       *layer_data,
                     DFBDisplayLayerDescription *description,
                     DFBDisplayLayerConfig      *config,
                     DFBColorAdjustment         *adjustment )
{
     EGLMaliData       *egl = driver_data;
     EGLMaliDataShared *shared;

     D_DEBUG_AT( EGL_Layer, "%s()\n", __FUNCTION__ );

     D_ASSERT( egl != NULL );
     D_ASSERT( egl->shared != NULL );

     shared = egl->shared;

     description->caps = DLCAPS_SURFACE;
     description->type = DLTF_GRAPHICS;

     snprintf( description->name, DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "EGL Mali Primary Layer" );

     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
     config->width       = dfb_config->mode.width  ?: shared->mode.w;
     config->height      = dfb_config->mode.height ?: shared->mode.h;
     config->pixelformat = dfb_config->mode.format ?: DSPF_ARGB;
     config->buffermode  = DLBM_FRONTONLY;

     /* Initialize the blit-to-screen shader. */
     init_blit_program( egl );

     return DFB_OK;
}

static DFBResult
eglPrimaryTestRegion( CoreLayer                  *layer,
                      void                       *driver_data,
                      void                       *layer_data,
                      CoreLayerRegionConfig      *config,
                      CoreLayerRegionConfigFlags *ret_failed )
{
     CoreLayerRegionConfigFlags failed = CLRCF_NONE;

     D_DEBUG_AT( EGL_Layer, "%s( %dx%d, %s )\n", __FUNCTION__,
                 config->source.w, config->source.h, dfb_pixelformat_name( config->format ) );

     switch (config->buffermode) {
          case DLBM_FRONTONLY:
          case DLBM_BACKVIDEO:
          case DLBM_BACKSYSTEM:
          case DLBM_TRIPLE:
               break;

          default:
               failed |= CLRCF_BUFFERMODE;
               break;
     }

     switch (config->format) {
          case DSPF_ARGB:
          case DSPF_RGB16:
               break;

          default:
               failed |= CLRCF_FORMAT;
               break;
     }

     if (config->options)
          failed |= CLRCF_OPTIONS;

     if (ret_failed)
          *ret_failed = failed;

     if (failed)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
eglPrimarySetRegion( CoreLayer                  *layer,
                     void                       *driver_data,
                     void                       *layer_data,
                     void                       *region_data,
                     CoreLayerRegionConfig      *config,
                     CoreLayerRegionConfigFlags  updated,
                     CoreSurface                *surface,
                     CorePalette                *palette,
                     CoreSurfaceBufferLock      *left_lock,
                     CoreSurfaceBufferLock      *right_lock )
{
     D_DEBUG_AT( EGL_Layer, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
eglPrimaryUpdateRegion( CoreLayer             *layer,
                        void                  *driver_data,
                        void                  *layer_data,
                        void                  *region_data,
                        CoreSurface           *surface,
                        const DFBRegion       *left_update,
                        CoreSurfaceBufferLock *left_lock,
                        const DFBRegion       *right_update,
                        CoreSurfaceBufferLock *right_lock )
{
     EGLMaliData *egl    = driver_data;
     DFBRegion    region = DFB_REGION_INIT_FROM_DIMENSION( &surface->config.size );

     D_DEBUG_AT( EGL_Layer, "%s()\n", __FUNCTION__ );

     D_ASSERT( egl != NULL );

     if (left_update && !dfb_region_region_intersect( &region, left_update ))
          return DFB_OK;

     {
          static int update_count = 0;
          update_count++;
          if (update_count <= 5 || update_count % 60 == 0)
               fprintf( stderr, "EGL/Layer: UpdateRegion #%d left_lock=%p handle=%p program=%u\n",
                        update_count, left_lock, left_lock ? left_lock->handle : NULL, egl->blit_program );
     }

     /* Blit the layer texture to FBO 0 (EGL window surface), then swap.
        The layer surface lives in the EGL pool as a GL texture + private FBO.
        Both the gles2 gfxdriver (GPU path) and Genefx (CPU shadow path)
        render into alloc->tex.  We draw it to FBO 0 for presentation. */
     if (egl->blit_program && left_lock && left_lock->handle) {
          GLuint tex = (GLuint)(unsigned long) left_lock->handle;

          /* Fullscreen quad: two triangles covering NDC [-1,1].
             Texture coords [0,1] with Y flipped (OpenGL origin is bottom-left,
             but DFB2 textures are top-down). */
          static const GLfloat verts[] = {
               /* aPos        aTex    */
               -1.0f, -1.0f,  0.0f, 1.0f,
                1.0f, -1.0f,  1.0f, 1.0f,
               -1.0f,  1.0f,  0.0f, 0.0f,
                1.0f,  1.0f,  1.0f, 0.0f,
          };

          /* Save GL state we'll modify. */
          GLint prev_prog, prev_fbo, prev_tex, prev_vp[4];
          glGetIntegerv( GL_CURRENT_PROGRAM, &prev_prog );
          glGetIntegerv( GL_FRAMEBUFFER_BINDING, &prev_fbo );
          glGetIntegerv( GL_TEXTURE_BINDING_2D, &prev_tex );
          glGetIntegerv( GL_VIEWPORT, prev_vp );

          /* Target FBO 0 — the EGL window surface. */
          glBindFramebuffer( GL_FRAMEBUFFER, 0 );
          glViewport( 0, 0, egl->size.w, egl->size.h );

          /* Disable blending — layer surface is the final composited image. */
          glDisable( GL_BLEND );
          glDisable( GL_DEPTH_TEST );
          glDisable( GL_CULL_FACE );
          glDisable( GL_SCISSOR_TEST );

          glUseProgram( egl->blit_program );

          /* Texture unit 0 is already active by default in GLES2. */
          glBindTexture( GL_TEXTURE_2D, tex );
          glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
          glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

          glVertexAttribPointer( egl->blit_attr_pos, 2, GL_FLOAT, GL_FALSE,
                                 4 * sizeof(GLfloat), verts );
          glEnableVertexAttribArray( egl->blit_attr_pos );

          glVertexAttribPointer( egl->blit_attr_tex, 2, GL_FLOAT, GL_FALSE,
                                 4 * sizeof(GLfloat), verts + 2 );
          glEnableVertexAttribArray( egl->blit_attr_tex );

          glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );

          /* Restore GL state. */
          glDisableVertexAttribArray( egl->blit_attr_pos );
          glDisableVertexAttribArray( egl->blit_attr_tex );

          glUseProgram( prev_prog );
          glBindTexture( GL_TEXTURE_2D, prev_tex );
          glBindFramebuffer( GL_FRAMEBUFFER, prev_fbo );
          glViewport( prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3] );
     }

     eglSwapBuffers( egl->eglDisplay, egl->eglSurface );

     return DFB_OK;
}

const DisplayLayerFuncs eglPrimaryLayerFuncs = {
     .InitLayer    = eglPrimaryInitLayer,
     .TestRegion   = eglPrimaryTestRegion,
     .SetRegion    = eglPrimarySetRegion,
     .UpdateRegion = eglPrimaryUpdateRegion
};
