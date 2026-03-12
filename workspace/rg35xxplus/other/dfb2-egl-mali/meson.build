#  EGL Mali fbdev system module for DirectFB2.
#
#  Based on DirectFB2-eglrpi.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.

project('DirectFB2-egl-mali-fbdev', 'c',
        version: '2.0.0')

directfb_dep = dependency('directfb-internal')

moduledir = directfb_dep.get_variable(pkgconfig: 'moduledir')

egl_dep = dependency('egl')
glesv2_dep = dependency('glesv2')

egl_mali_sources = [
  'egl_layer.c',
  'egl_screen.c',
  'egl_surface_pool.c',
  'egl_system.c'
]

library('directfb_egl_mali_fbdev',
        egl_mali_sources,
        dependencies: [directfb_dep, egl_dep, glesv2_dep],
        install: true,
        install_dir: join_paths(moduledir, 'systems'))
