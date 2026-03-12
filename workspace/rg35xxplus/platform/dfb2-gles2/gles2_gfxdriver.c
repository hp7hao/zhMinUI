/*
   This file is part of DirectFB.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

#include <core/graphics_driver.h>
#include <misc/conf.h>

#include "gles2_gfxdriver.h"
#include "gles2_shaders.h"

D_DEBUG_DOMAIN( GLES2_Driver, "GLES2/Driver", "OpenGL ES 2.0 Driver" );

DFB_GRAPHICS_DRIVER( gles2 )

/**********************************************************************************************************************/

extern const GraphicsDeviceFuncs gles2GraphicsDeviceFuncs;

static DFBResult
init_shader( GLuint      prog_obj,
             const char *prog_src,
             GLenum      type )
{
     GLuint  shader;
     GLint   status;
     GLint   log_length;
     GLchar *log;

     shader = glCreateShader( type );
     if (!shader)
          return DFB_FAILURE;

     glShaderSource( shader, 1, &prog_src, NULL );

     glCompileShader( shader );

     glGetShaderiv( shader, GL_COMPILE_STATUS, &status );
     if (!status) {
          glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &log_length );
          log = D_MALLOC( log_length );
          if (!log)
               return D_OOM();

          glGetShaderInfoLog( shader, log_length, NULL, log );
          D_ERROR( "GLES2/Driver: Failed to compile shader:\n%s\n", log );

          D_FREE( log );
          glDeleteShader( shader );

          return DFB_FAILURE;
     }

     glAttachShader( prog_obj, shader );

     glDeleteShader( shader );

     return DFB_OK;
}

static GLuint
init_program( const char *vert_prog_src,
              const char *frag_prog_src,
              DFBBoolean  texcoords )
{
     GLuint  prog_obj;
     GLuint  shaders[2];
     GLint   status;
     GLint   log_length;
     char   *log;

     prog_obj = glCreateProgram();
     if (!prog_obj)
          return 0;

     if (init_shader( prog_obj, vert_prog_src, GL_VERTEX_SHADER )) {
          D_ERROR( "GLES2/Driver: Failed to create vertex shader!\n" );
          return 0;
     }

     if (init_shader( prog_obj, frag_prog_src, GL_FRAGMENT_SHADER )) {
          D_ERROR( "GLES2/Driver: Failed to create fragment shader!\n" );
          return 0;
     }

     glBindAttribLocation( prog_obj, GLES2VA_POSITIONS, "dfbPos" );

     if (texcoords)
          glBindAttribLocation( prog_obj, GLES2VA_TEXCOORDS, "dfbUV" );

     glLinkProgram( prog_obj );

     glGetProgramiv( prog_obj, GL_LINK_STATUS, &status );
     if (!status) {
          glGetProgramiv( prog_obj, GL_INFO_LOG_LENGTH, &log_length );
          log = D_MALLOC( log_length );
          if (!log)
               return 0;

          glGetProgramInfoLog( prog_obj, log_length, NULL, log );
          D_ERROR( "GLES2/Driver: Failed to link program:\n%s\n", log );

          D_FREE( log );
          return 0;
     }

     glGetAttachedShaders( prog_obj, 2, NULL, shaders );
     glDetachShader( prog_obj, shaders[0] );
     glDetachShader( prog_obj, shaders[1] );

     return prog_obj;
}

/**********************************************************************************************************************/

static int
driver_probe()
{
     return dfb_gfxcard_get_accelerator() > 0x10000 ? 1 : 0;
}

static void
driver_get_info( GraphicsDriverInfo *driver_info )
{
     driver_info->version.major = 0;
     driver_info->version.minor = 1;

     snprintf( driver_info->name,   DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,   "GLES2" );
     snprintf( driver_info->vendor, DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH, "DirectFB" );

     driver_info->driver_data_size = sizeof(GLES2DriverData);
     driver_info->device_data_size = sizeof(GLES2DeviceData);
}

static DFBResult
driver_init_driver( GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     D_DEBUG_AT( GLES2_Driver, "%s()\n", __FUNCTION__ );

     dfb_config->font_format = DSPF_ARGB;

     *funcs = gles2GraphicsDeviceFuncs;

     return DFB_OK;
}

static DFBResult
driver_init_device( GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     GLES2DeviceData *dev = device_data;
     GLuint           prog_obj;
     int              i;

     D_DEBUG_AT( GLES2_Driver, "%s()\n", __FUNCTION__ );

     const char *renderer = (const char*) glGetString( GL_RENDERER );
     snprintf( device_info->name,   DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,   "%s", renderer ? renderer : "Mali" );
     snprintf( device_info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "OpenGL ES" );
     device_info->caps.flags    = CCF_CLIPPING | CCF_RENDEROPTS;
     device_info->caps.accel    = DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | DFXL_DRAWLINE | DFXL_FILLTRIANGLE |
                                  DFXL_BLIT          | DFXL_STRETCHBLIT;
     device_info->caps.blitting = DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA | DSBLIT_COLORIZE         |
                                  DSBLIT_SRC_COLORKEY       | DSBLIT_SRC_PREMULTIPLY  | DSBLIT_SRC_PREMULTCOLOR |
                                  DSBLIT_ROTATE180          | DSBLIT_ROTATE90         | DSBLIT_ROTATE270;
     device_info->caps.drawing  = DSDRAW_BLEND | DSDRAW_SRC_PREMULTIPLY;

     for (i = 0; i < NUM_PROGRAMS; i++) {
          dev->progs[i].obj          =  0;
          dev->progs[i].dfbScale     = -1;
          dev->progs[i].dfbRotMatrix = -1;
          dev->progs[i].dfbROMatrix  = -1;
          dev->progs[i].dfbMVPMatrix = -1;
          dev->progs[i].dfbColor     = -1;
          dev->progs[i].dfbColorkey  = -1;
          dev->progs[i].dfbTexScale  = -1;
          dev->progs[i].flags        =  NONE;
          dev->progs[i].name         = "invalid";
     }

     prog_obj = init_program( draw_vert_src, draw_frag_src, DFB_FALSE );
     if (!prog_obj) {
          D_ERROR( "GLES2/Driver: Failed to create draw program!\n" );
          goto fail;
     }

     dev->progs[DRAW].obj          = prog_obj;
     dev->progs[DRAW].name         = "draw";
     dev->progs[DRAW].dfbColor     = glGetUniformLocation( dev->progs[DRAW].obj, "dfbColor" );
     dev->progs[DRAW].dfbScale     = glGetUniformLocation( dev->progs[DRAW].obj, "dfbScale" );
     dev->progs[DRAW].dfbRotMatrix = glGetUniformLocation( dev->progs[DRAW].obj, "dfbRotMatrix" );

     prog_obj = init_program( draw_mat_vert_src, draw_frag_src, DFB_FALSE );
     if (!prog_obj) {
          D_ERROR( "GLES2/Driver: Failed to create draw_mat program!\n" );
          goto fail;
     }

     dev->progs[DRAW_MAT].obj          = prog_obj;
     dev->progs[DRAW_MAT].name         = "draw_mat";
     dev->progs[DRAW_MAT].dfbColor     = glGetUniformLocation( dev->progs[DRAW_MAT].obj, "dfbColor" );
     dev->progs[DRAW_MAT].dfbROMatrix  = glGetUniformLocation( dev->progs[DRAW_MAT].obj, "dfbROMatrix" );
     dev->progs[DRAW_MAT].dfbMVPMatrix = glGetUniformLocation( dev->progs[DRAW_MAT].obj, "dfbMVPMatrix" );

     prog_obj = init_program( blit_vert_src, blit_frag_src, DFB_TRUE );
     if (!prog_obj) {
          D_ERROR( "GLES2/Driver: Failed to create blit program!\n" );
          goto fail;
     }

     dev->progs[BLIT].obj          = prog_obj;
     dev->progs[BLIT].name         = "blit";
     dev->progs[BLIT].dfbScale     = glGetUniformLocation( dev->progs[BLIT].obj, "dfbScale" );
     dev->progs[BLIT].dfbRotMatrix = glGetUniformLocation( dev->progs[BLIT].obj, "dfbRotMatrix" );
     dev->progs[BLIT].dfbTexScale  = glGetUniformLocation( dev->progs[BLIT].obj, "dfbTexScale" );

     prog_obj = init_program( blit_mat_vert_src, blit_frag_src, DFB_TRUE );
     if (!prog_obj) {
          D_ERROR( "GLES2/Driver: Failed to create blit_mat program!\n" );
          goto fail;
     }

     dev->progs[BLIT_MAT].obj          = prog_obj;
     dev->progs[BLIT_MAT].name         = "blit_mat";
     dev->progs[BLIT_MAT].dfbROMatrix  = glGetUniformLocation( dev->progs[BLIT_MAT].obj, "dfbROMatrix" );
     dev->progs[BLIT_MAT].dfbMVPMatrix = glGetUniformLocation( dev->progs[BLIT_MAT].obj, "dfbMVPMatrix" );
     dev->progs[BLIT_MAT].dfbTexScale  = glGetUniformLocation( dev->progs[BLIT_MAT].obj, "dfbTexScale" );

     prog_obj = init_program( blit_vert_src, blit_color_frag_src, DFB_TRUE );
     if (!prog_obj) {
          D_ERROR( "GLES2/Driver: Failed to create blit_color program!\n" );
          goto fail;
     }

     dev->progs[BLIT_COLOR].obj          = prog_obj;
     dev->progs[BLIT_COLOR].name         = "blit_color";
     dev->progs[BLIT_COLOR].dfbColor     = glGetUniformLocation( dev->progs[BLIT_COLOR].obj, "dfbColor" );
     dev->progs[BLIT_COLOR].dfbScale     = glGetUniformLocation( dev->progs[BLIT_COLOR].obj, "dfbScale" );
     dev->progs[BLIT_COLOR].dfbRotMatrix = glGetUniformLocation( dev->progs[BLIT_COLOR].obj, "dfbRotMatrix" );
     dev->progs[BLIT_COLOR].dfbTexScale  = glGetUniformLocation( dev->progs[BLIT_COLOR].obj, "dfbTexScale" );

     prog_obj = init_program( blit_mat_vert_src, blit_color_frag_src, DFB_TRUE );
     if (!prog_obj) {
          D_ERROR( "GLES2/Driver: Failed to create blit_color_mat program!\n" );
          goto fail;
     }

     dev->progs[BLIT_COLOR_MAT].obj          = prog_obj;
     dev->progs[BLIT_COLOR_MAT].name         = "blit_color_mat";
     dev->progs[BLIT_COLOR_MAT].dfbColor     = glGetUniformLocation( dev->progs[BLIT_COLOR_MAT].obj, "dfbColor" );
     dev->progs[BLIT_COLOR_MAT].dfbROMatrix  = glGetUniformLocation( dev->progs[BLIT_COLOR_MAT].obj, "dfbROMatrix" );
     dev->progs[BLIT_COLOR_MAT].dfbMVPMatrix = glGetUniformLocation( dev->progs[BLIT_COLOR_MAT].obj, "dfbMVPMatrix" );
     dev->progs[BLIT_COLOR_MAT].dfbTexScale  = glGetUniformLocation( dev->progs[BLIT_COLOR_MAT].obj, "dfbTexScale" );

     prog_obj = init_program( blit_vert_src, blit_colorkey_frag_src, DFB_TRUE );
     if (!prog_obj) {
          D_ERROR( "GLES2/Driver: Failed to create blit_colorkey program!\n" );
          goto fail;
     }

     dev->progs[BLIT_COLORKEY].obj          = prog_obj;
     dev->progs[BLIT_COLORKEY].name         = "blit_colorkey";
     dev->progs[BLIT_COLORKEY].dfbColor     = glGetUniformLocation( dev->progs[BLIT_COLORKEY].obj, "dfbColor" );
     dev->progs[BLIT_COLORKEY].dfbScale     = glGetUniformLocation( dev->progs[BLIT_COLORKEY].obj, "dfbScale" );
     dev->progs[BLIT_COLORKEY].dfbRotMatrix = glGetUniformLocation( dev->progs[BLIT_COLORKEY].obj, "dfbRotMatrix" );
     dev->progs[BLIT_COLORKEY].dfbTexScale  = glGetUniformLocation( dev->progs[BLIT_COLORKEY].obj, "dfbTexScale" );
     dev->progs[BLIT_COLORKEY].dfbColorkey  = glGetUniformLocation( dev->progs[BLIT_COLORKEY].obj, "dfbColorkey" );

     prog_obj = init_program( blit_mat_vert_src, blit_colorkey_frag_src, DFB_TRUE );
     if (!prog_obj) {
          D_ERROR( "GLES2/Driver: Failed to create blit_colorkey_mat program!\n" );
          goto fail;
     }

     dev->progs[BLIT_COLORKEY_MAT].obj          = prog_obj;
     dev->progs[BLIT_COLORKEY_MAT].name         = "blit_colorkey_mat";
     dev->progs[BLIT_COLORKEY_MAT].dfbColor     = glGetUniformLocation( dev->progs[BLIT_COLORKEY_MAT].obj, "dfbColor" );
     dev->progs[BLIT_COLORKEY_MAT].dfbROMatrix  = glGetUniformLocation( dev->progs[BLIT_COLORKEY_MAT].obj, "dfbROMatrix" );
     dev->progs[BLIT_COLORKEY_MAT].dfbMVPMatrix = glGetUniformLocation( dev->progs[BLIT_COLORKEY_MAT].obj, "dfbMVPMatrix" );
     dev->progs[BLIT_COLORKEY_MAT].dfbTexScale  = glGetUniformLocation( dev->progs[BLIT_COLORKEY_MAT].obj, "dfbTexScale" );
     dev->progs[BLIT_COLORKEY_MAT].dfbColorkey  = glGetUniformLocation( dev->progs[BLIT_COLORKEY_MAT].obj, "dfbColorkey" );

     prog_obj = init_program( blit_vert_src, blit_premultiply_frag_src, DFB_TRUE );
     if (!prog_obj) {
          D_ERROR( "GLES2/Driver: Failed to create blit_premultiply program!\n" );
          goto fail;
     }

     dev->progs[BLIT_PREMULTIPLY].obj          = prog_obj;
     dev->progs[BLIT_PREMULTIPLY].name         = "blit_premultiply";
     dev->progs[BLIT_PREMULTIPLY].dfbColor     = glGetUniformLocation( dev->progs[BLIT_PREMULTIPLY].obj, "dfbColor" );
     dev->progs[BLIT_PREMULTIPLY].dfbScale     = glGetUniformLocation( dev->progs[BLIT_PREMULTIPLY].obj, "dfbScale" );
     dev->progs[BLIT_PREMULTIPLY].dfbRotMatrix = glGetUniformLocation( dev->progs[BLIT_PREMULTIPLY].obj, "dfbRotMatrix" );
     dev->progs[BLIT_PREMULTIPLY].dfbTexScale  = glGetUniformLocation( dev->progs[BLIT_PREMULTIPLY].obj, "dfbTexScale" );

     prog_obj = init_program( blit_mat_vert_src, blit_premultiply_frag_src, DFB_TRUE );
     if (!prog_obj) {
          D_ERROR( "GLES2/Driver: Failed to create blit_premultiply_mat program!\n" );
          goto fail;
     }

     dev->progs[BLIT_PREMULTIPLY_MAT].obj          = prog_obj;
     dev->progs[BLIT_PREMULTIPLY_MAT].name         = "blit_premultiply_mat";
     dev->progs[BLIT_PREMULTIPLY_MAT].dfbColor     = glGetUniformLocation( dev->progs[BLIT_PREMULTIPLY_MAT].obj, "dfbColor" );
     dev->progs[BLIT_PREMULTIPLY_MAT].dfbROMatrix  = glGetUniformLocation( dev->progs[BLIT_PREMULTIPLY_MAT].obj, "dfbROMatrix" );
     dev->progs[BLIT_PREMULTIPLY_MAT].dfbMVPMatrix = glGetUniformLocation( dev->progs[BLIT_PREMULTIPLY_MAT].obj, "dfbMVPMatrix" );
     dev->progs[BLIT_PREMULTIPLY_MAT].dfbTexScale  = glGetUniformLocation( dev->progs[BLIT_PREMULTIPLY_MAT].obj, "dfbTexScale" );

     dev->prog_index = INVALID_PROGRAM;

     return DFB_OK;

fail:
     for (i = 0; i < NUM_PROGRAMS; i++)
          glDeleteProgram(dev->progs[i].obj);

     return DFB_INIT;
}

static void
driver_close_device( void *driver_data,
                     void *device_data )
{
     D_DEBUG_AT( GLES2_Driver, "%s()\n", __FUNCTION__ );
}

static void
driver_close_driver( void *driver_data )
{
     D_DEBUG_AT( GLES2_Driver, "%s()\n", __FUNCTION__ );
}
