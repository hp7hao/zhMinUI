#  This file is part of DirectFB.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.

project('DirectFB2-gles2', 'c',
        version: '2.0.0')

directfb_dep = dependency('directfb-internal')

moduledir = directfb_dep.get_variable(pkgconfig: 'moduledir')

gles2_dep = dependency('glesv2')

gles2_sources = [
  'gles2_2d.c',
  'gles2_gfxdriver.c'
]

library('directfb_gles2',
        gles2_sources,
        dependencies: [directfb_dep, gles2_dep],
        install: true,
        install_dir: join_paths(moduledir, 'gfxdrivers'))
