#rg35xx

###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_UNZIP60=other/unzip60
REQUIRES_SDL2=other/sdl2
REQUIRES_DTC=other/dtc
REQUIRES_FBSET=other/fbset
REQUIRES_DIRECTFB2=other/directfb2
REQUIRES_DFB2_MEDIA=other/directfb2-media
REQUIRES_STB=other/stb
REQUIRES_LODEPNG=other/lodepng
REQUIRES_FLUX=other/flux
REQUIRES_DFB2_EGL_MALI=other/dfb2-egl-mali
REQUIRES_DFB2_GLES2=other/dfb2-gles2

all: readmes
	cd boot && ./build.sh
	cd init && make
	cd show && make

early: $(REQUIRES_UNZIP60) $(REQUIRES_SDL2) $(REQUIRES_DTC) $(REQUIRES_FBSET) $(REQUIRES_FLUX) $(REQUIRES_DIRECTFB2) $(REQUIRES_DFB2_MEDIA) $(REQUIRES_STB) $(REQUIRES_LODEPNG)
	mkdir -p other
	cd $(REQUIRES_DTC) && CC=${CROSS_COMPILE}gcc make NO_PYTHON=1 dtc
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart unzip
	cd $(REQUIRES_FBSET) && make

	# Build DirectFB2 first (SDL2 needs its headers for --enable-video-directfb)
	cp /usr/bin/automake /opt/rg35xxplus-toolchain/usr/bin/automake
	cp /usr/bin/autoreconf /opt/rg35xxplus-toolchain/usr/bin/autoreconf
	cp /usr/bin/aclocal /opt/rg35xxplus-toolchain/usr/bin/aclocal
	cp /usr/bin/libtoolize /opt/rg35xxplus-toolchain/usr/bin/libtoolize
	# build flux
	cd $(REQUIRES_FLUX) && \
		./autogen.sh && ./configure && make && make install

	# Create /bin/env symlink for fluxcomp.py shebang
	ln -sf /usr/bin/env /bin/env

	# Create minimal cross file to tell meson this is cross-compilation
	mkdir -p $(REQUIRES_DIRECTFB2)/cross
	printf "[binaries]\n" > $(REQUIRES_DIRECTFB2)/cross/rg35xxplus.ini
	printf "c = '%sgcc'\n" "${CROSS_COMPILE}" >> $(REQUIRES_DIRECTFB2)/cross/rg35xxplus.ini
	printf "pkgconfig = 'pkg-config'\n" >> $(REQUIRES_DIRECTFB2)/cross/rg35xxplus.ini
	printf "[properties]\n" >> $(REQUIRES_DIRECTFB2)/cross/rg35xxplus.ini
	printf "sys_root = '/'\n" >> $(REQUIRES_DIRECTFB2)/cross/rg35xxplus.ini
	printf "[host_machine]\n" >> $(REQUIRES_DIRECTFB2)/cross/rg35xxplus.ini
	printf "system = 'linux'\n" >> $(REQUIRES_DIRECTFB2)/cross/rg35xxplus.ini
	printf "cpu_family = 'arm'\n" >> $(REQUIRES_DIRECTFB2)/cross/rg35xxplus.ini
	printf "cpu = 'armv8-a'\n" >> $(REQUIRES_DIRECTFB2)/cross/rg35xxplus.ini
	printf "endian = 'little'\n" >> $(REQUIRES_DIRECTFB2)/cross/rg35xxplus.ini

	# Patch DirectFB2 linux_input driver to remap gamepad buttons to keyboard codes
	cd $(REQUIRES_DIRECTFB2) && patch -p1 -N < ../../platform/gamepad-remap.patch || true

	cd $(REQUIRES_DIRECTFB2) && rm -rf build && mkdir -p build && \
		CC=${CROSS_COMPILE}gcc \
		PKG_CONFIG_LIBDIR=$(PREFIX)/lib/pkgconfig /opt/meson/meson \
		  --prefix $(PREFIX) \
		  --cross-file cross/rg35xxplus.ini \
		  -Dmulti=true -Dos=linux -Dfbdev=true -Ddrmkms=false -Dlinux_input=true build/ && \
		PKG_CONFIG_LIBDIR=$(PREFIX)/lib/pkgconfig ninja -C build && \
		PKG_CONFIG_LIBDIR=$(PREFIX)/lib/pkgconfig ninja -C build install

	# copy DirectFB2 libs from installed location (toolchain prefix)
	# Core libraries - these are installed to $(PREFIX)/lib/
	cp $(PREFIX)/lib/libdirectfb-2.0.so.0.0.0 ../libdirectfb-2.0.so.0
	cp $(PREFIX)/lib/libdirect-2.0.so.0.0.0 ../libdirect-2.0.so.0
	cp $(PREFIX)/lib/libfusion-2.0.so.0.0.0 ../libfusion-2.0.so.0
	# System modules - these are installed to $(PREFIX)/lib/directfb-2.0-0/
	cp $(PREFIX)/lib/directfb-2.0-0/systems/libdirectfb_fbdev.so ../libdirectfb_fbdev.so
	cp $(PREFIX)/lib/directfb-2.0-0/systems/libdirectfb_dummy.so ../libdirectfb_dummy.so
	# Window manager
	cp $(PREFIX)/lib/directfb-2.0-0/wm/libdirectfbwm_default.so ../libdirectfbwm_default.so
	# Input drivers
	cp $(PREFIX)/lib/directfb-2.0-0/inputdrivers/libdirectfb_linux_input.so ../libdirectfb_linux_input.so
	# Interface modules
	cp $(PREFIX)/lib/directfb-2.0-0/interfaces/ICoreResourceManager/libicoreresourcemanager_default.so ../libicoreresourcemanager_default.so
	cp $(PREFIX)/lib/directfb-2.0-0/interfaces/IDirectFBFont/libidirectfbfont_dgiff.so ../libidirectfbfont_dgiff.so
	cp $(PREFIX)/lib/directfb-2.0-0/interfaces/IDirectFBImageProvider/libidirectfbimageprovider_dfiff.so ../libidirectfbimageprovider_dfiff.so
	cp $(PREFIX)/lib/directfb-2.0-0/interfaces/IDirectFBVideoProvider/libidirectfbvideoprovider_dfvff.so ../libidirectfbvideoprovider_dfvff.so
	cp $(PREFIX)/lib/directfb-2.0-0/interfaces/IDirectFBWindows/libidirectfbwindows_default.so ../libidirectfbwindows_default.so

	# Build DirectFB2-media (STB TrueType font + LodePNG image providers)
	# Install stb and lodepng headers into toolchain prefix
	mkdir -p $(PREFIX)/include/stb
	cp $(REQUIRES_STB)/stb_truetype.h $(PREFIX)/include/stb/
	mkdir -p $(PREFIX)/include/lodepng
	cp $(REQUIRES_LODEPNG)/lodepng.h $(PREFIX)/include/lodepng/
	cp $(REQUIRES_LODEPNG)/lodepng.cpp $(PREFIX)/include/lodepng/
	cd $(REQUIRES_DFB2_MEDIA) && rm -rf build && mkdir -p build && \
		CC=${CROSS_COMPILE}gcc \
		PKG_CONFIG_LIBDIR=$(PREFIX)/lib/pkgconfig /opt/meson/meson \
		  --prefix $(PREFIX) \
		  --cross-file ../directfb2/cross/rg35xxplus.ini \
		  -Dstb_truetype=$(PREFIX)/include/stb \
		  -Dlodepng=$(PREFIX)/include/lodepng \
		  -Dft2=false -Dsft=false \
		  -Dpng=false -Dspng=false -Dstb_image=false \
		  -Djpeg=false -Dgif=false -Dbmp=false -Dsvg=false -Dtiff=false -Dwebp=false \
		  -Davif=false -Dheif=false -Djxl=false -Dopenexr=false -Dimlib2=false -Djasper=false -Dgdkpixbuf=false -Dyuv=false \
		  -Dnanosvg=false \
		  -Dffmpeg=false -Dgstreamer=false -Dlibmpeg3=false -Dmng=false -Dplm=false -Dswfdec=false -Dv4l=false \
		  -Dmad=false -Dplaylist=false -Dtremor=false -Dvorbis=false \
		  -Dfusionsound=false \
		  build/ && \
		PKG_CONFIG_LIBDIR=$(PREFIX)/lib/pkgconfig ninja -C build && \
		PKG_CONFIG_LIBDIR=$(PREFIX)/lib/pkgconfig ninja -C build install
	# copy DirectFB2-media providers
	cp $(PREFIX)/lib/directfb-2.0-0/interfaces/IDirectFBFont/libidirectfbfont_stb.so ../libidirectfbfont_stb.so
	cp $(PREFIX)/lib/directfb-2.0-0/interfaces/IDirectFBImageProvider/libidirectfbimageprovider_lodepng.so ../libidirectfbimageprovider_lodepng.so

	# Install EGL/GLES2 Khronos headers and create pkg-config files for Mali GPU cross-compilation
	# Download standard Khronos EGL/GLES2 headers only if not already present
	mkdir -p $(PREFIX)/include/EGL $(PREFIX)/include/GLES2 $(PREFIX)/include/KHR
	test -f $(PREFIX)/include/EGL/egl.h || curl -sL -o $(PREFIX)/include/EGL/egl.h https://registry.khronos.org/EGL/api/EGL/egl.h
	test -f $(PREFIX)/include/EGL/eglext.h || curl -sL -o $(PREFIX)/include/EGL/eglext.h https://registry.khronos.org/EGL/api/EGL/eglext.h
	test -f $(PREFIX)/include/EGL/eglplatform.h || curl -sL -o $(PREFIX)/include/EGL/eglplatform.h https://registry.khronos.org/EGL/api/EGL/eglplatform.h
	test -f $(PREFIX)/include/GLES2/gl2.h || curl -sL -o $(PREFIX)/include/GLES2/gl2.h https://registry.khronos.org/OpenGL/api/GLES2/gl2.h
	test -f $(PREFIX)/include/GLES2/gl2ext.h || curl -sL -o $(PREFIX)/include/GLES2/gl2ext.h https://registry.khronos.org/OpenGL/api/GLES2/gl2ext.h
	test -f $(PREFIX)/include/GLES2/gl2platform.h || curl -sL -o $(PREFIX)/include/GLES2/gl2platform.h https://registry.khronos.org/OpenGL/api/GLES2/gl2platform.h
	test -f $(PREFIX)/include/KHR/khrplatform.h || curl -sL -o $(PREFIX)/include/KHR/khrplatform.h https://registry.khronos.org/EGL/api/KHR/khrplatform.h
	# Create stub shared libraries for linking (real Mali libs deployed to device separately)
	echo 'void eglGetDisplay() {} void eglInitialize() {} void eglChooseConfig() {} void eglCreateWindowSurface() {} void eglCreateContext() {} void eglMakeCurrent() {} void eglSwapBuffers() {} void eglDestroySurface() {} void eglDestroyContext() {} void eglTerminate() {} void eglGetError() {}' | \
		${CROSS_COMPILE}gcc -shared -x c - -o $(PREFIX)/lib/libEGL.so -Wl,-soname,libEGL.so
	echo 'void glViewport() {} void glScissor() {} void glEnable() {} void glDisable() {} void glBlendFunc() {} void glClear() {} void glClearColor() {} void glGenTextures() {} void glDeleteTextures() {} void glBindTexture() {} void glTexImage2D() {} void glTexSubImage2D() {} void glTexParameterf() {} void glTexParameteri() {} void glGenFramebuffers() {} void glDeleteFramebuffers() {} void glBindFramebuffer() {} void glFramebufferTexture2D() {} void glCheckFramebufferStatus() {} void glDrawArrays() {} void glVertexAttribPointer() {} void glEnableVertexAttribArray() {} void glDisableVertexAttribArray() {} void glCreateShader() {} void glShaderSource() {} void glCompileShader() {} void glGetShaderiv() {} void glCreateProgram() {} void glAttachShader() {} void glLinkProgram() {} void glGetProgramiv() {} void glUseProgram() {} void glGetUniformLocation() {} void glGetAttribLocation() {} void glBindAttribLocation() {} void glUniform1i() {} void glUniform1f() {} void glUniform2f() {} void glUniform3f() {} void glUniform3i() {} void glUniform4f() {} void glUniformMatrix3fv() {} void glDeleteShader() {} void glDeleteProgram() {} void glGetShaderInfoLog() {} void glGetProgramInfoLog() {} void glGetIntegerv() {} void glReadPixels() {} void glPixelStorei() {} void glFlush() {} void glFinish() {} void glGetAttachedShaders() {} void glDetachShader() {} void *glGetString() { return (void*)0; }' | \
		${CROSS_COMPILE}gcc -shared -x c - -o $(PREFIX)/lib/libGLESv2.so -Wl,-soname,libGLESv2.so
	# Create pkg-config files
	mkdir -p $(PREFIX)/lib/pkgconfig
	printf 'prefix=%s\nlibdir=$${prefix}/lib\nincludedir=$${prefix}/include\n\nName: egl\nDescription: EGL library\nVersion: 1.5\nLibs: -L$${libdir} -lEGL\nCflags: -I$${includedir}\n' "$(PREFIX)" > $(PREFIX)/lib/pkgconfig/egl.pc
	printf 'prefix=%s\nlibdir=$${prefix}/lib\nincludedir=$${prefix}/include\n\nName: glesv2\nDescription: OpenGL ES 2.0 library\nVersion: 2.0\nLibs: -L$${libdir} -lGLESv2\nCflags: -I$${includedir}\n' "$(PREFIX)" > $(PREFIX)/lib/pkgconfig/glesv2.pc

	# Copy EGL Mali system module sources from platform/ into other/ (platform/ is git-tracked)
	rm -rf $(REQUIRES_DFB2_EGL_MALI)
	cp -r platform/dfb2-egl-mali $(REQUIRES_DFB2_EGL_MALI)
	# Build EGL Mali fbdev system module
	cd $(REQUIRES_DFB2_EGL_MALI) && rm -rf build && mkdir -p build && \
		CC=${CROSS_COMPILE}gcc \
		PKG_CONFIG_LIBDIR=$(PREFIX)/lib/pkgconfig /opt/meson/meson \
		  --prefix $(PREFIX) \
		  --cross-file ../directfb2/cross/rg35xxplus.ini \
		  build/ && \
		PKG_CONFIG_LIBDIR=$(PREFIX)/lib/pkgconfig ninja -C build && \
		PKG_CONFIG_LIBDIR=$(PREFIX)/lib/pkgconfig ninja -C build install
	cp $(PREFIX)/lib/directfb-2.0-0/systems/libdirectfb_egl_mali_fbdev.so ../libdirectfb_egl_mali_fbdev.so

	# Copy GLES2 gfxdriver sources from platform/ into other/
	rm -rf $(REQUIRES_DFB2_GLES2)
	cp -r platform/dfb2-gles2 $(REQUIRES_DFB2_GLES2)
	# Build GLES2 gfxdriver
	cd $(REQUIRES_DFB2_GLES2) && rm -rf build && mkdir -p build && \
		CC=${CROSS_COMPILE}gcc \
		PKG_CONFIG_LIBDIR=$(PREFIX)/lib/pkgconfig /opt/meson/meson \
		  --prefix $(PREFIX) \
		  --cross-file ../directfb2/cross/rg35xxplus.ini \
		  build/ && \
		PKG_CONFIG_LIBDIR=$(PREFIX)/lib/pkgconfig ninja -C build && \
		PKG_CONFIG_LIBDIR=$(PREFIX)/lib/pkgconfig ninja -C build install
	cp $(PREFIX)/lib/directfb-2.0-0/gfxdrivers/libdirectfb_gles2.so ../libdirectfb_gles2.so

	# Build SDL2 without DirectFB backend (display goes through direct DFB2 calls in platform.c)
	cd $(REQUIRES_SDL2) && git checkout -- .
	cd $(REQUIRES_SDL2) && CFLAGS="-O2 -march=armv8-a+crc -mtune=cortex-a53 -mcpu=cortex-a53 -ffast-math -ftree-vectorize" \
		LDFLAGS="-L$(PREFIX)/lib" \
		PKG_CONFIG_LIBDIR="$(PREFIX)/lib/pkgconfig" \
		./configure CC=${CROSS_COMPILE}gcc --host=arm-buildroot-linux-gnueabihf \
		  --enable-arm-neon \
		  --disable-oss \
		  --disable-rpath \
		  --disable-arts \
		  --disable-esd \
		  --disable-dbus \
		  --disable-diskaudio \
		  --disable-dummyaudio \
		  --disable-pulseaudio \
		  --disable-video-kmsdrm \
		  --disable-video-vivante \
		  --disable-video-cocoa \
		  --disable-video-metal \
		  --disable-video-dummy \
		  --disable-video-offscreen \
		  --disable-video-x11 \
		  --disable-video-vulkan \
		  --disable-ime \
		  --disable-ibus \
		  --disable-fcitx \
		  --disable-joystick-mfi \
		  --disable-directx \
		  --disable-xinput \
		  --disable-wasapi \
		  --disable-hidapi-joystick \
		  --disable-hidapi-libusb \
		  --disable-joystick-virtual \
		  --enable-video-mali \
		  --disable-video-directfb \
		  --disable-libsamplerate && make
	# copy libs from toolchain (later moved to build)
	cp $(PREFIX)/lib/libSDL2_image-2.0.so.0.0.1 ../libSDL2_image-2.0.so.0
	cp $(PREFIX)/lib/libSDL2_ttf-2.0.so.0.14.0 ../libSDL2_ttf-2.0.so.0
	
clean:
	cd boot && rm -rf output
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart clean
	cd $(REQUIRES_FBSET) && make clean
	cd $(REQUIRES_FLUX) && make clean
	cd $(REQUIRES_DIRECTFB2) && rm -rf build
	cd $(REQUIRES_DFB2_MEDIA) && rm -rf build
	
$(REQUIRES_UNZIP60):
	git clone --depth 1 https://github.com/shauninman/unzip60.git $(REQUIRES_UNZIP60)

$(REQUIRES_SDL2):
	git clone https://github.com/JohnnyonFlame/SDL-malifbdev-rot $(REQUIRES_SDL2)

$(REQUIRES_DTC):
	git clone https://github.com/dgibson/dtc $(REQUIRES_DTC)

$(REQUIRES_FBSET):
	git clone https://github.com/shauninman/union-fbset $(REQUIRES_FBSET)

$(REQUIRES_DIRECTFB2):
	git clone https://github.com/directfb2/DirectFB2.git $(REQUIRES_DIRECTFB2)

$(REQUIRES_DFB2_MEDIA):
	git clone https://github.com/directfb2/DirectFB2-media.git $(REQUIRES_DFB2_MEDIA)

$(REQUIRES_STB):
	mkdir -p $(REQUIRES_STB)
	curl -L -o $(REQUIRES_STB)/stb_truetype.h https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h

$(REQUIRES_LODEPNG):
	mkdir -p $(REQUIRES_LODEPNG)
	curl -L -o $(REQUIRES_LODEPNG)/lodepng.h https://raw.githubusercontent.com/lvandeve/lodepng/master/lodepng.h
	curl -L -o $(REQUIRES_LODEPNG)/lodepng.cpp https://raw.githubusercontent.com/lvandeve/lodepng/master/lodepng.cpp

$(REQUIRES_FLUX):
	git clone https://github.com/deniskropp/flux $(REQUIRES_FLUX)

include ../all/readmes/makefile
