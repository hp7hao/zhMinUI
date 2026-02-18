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
REQUIRES_FLUX=other/flux

all: readmes
	cd boot && ./build.sh
	cd init && make
	cd show && make

early: $(REQUIRES_UNZIP60) $(REQUIRES_SDL2) $(REQUIRES_DTC) $(REQUIRES_FBSET) $(REQUIRES_FLUX) $(REQUIRES_DIRECTFB2)
	mkdir -p other
	cd $(REQUIRES_DTC) && CC=${CROSS_COMPILE}gcc make NO_PYTHON=1 dtc
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart unzip
	cd $(REQUIRES_FBSET) && make
	cd $(REQUIRES_SDL2) && CFLAGS="-O2 -march=armv8-a+crc -mtune=cortex-a53 -mcpu=cortex-a53 -ffast-math -ftree-vectorize" \
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
		  --disable-libsamplerate && make
	# copy libs from toolchain (later moved to build)
	cp $(PREFIX)/lib/libSDL2_image-2.0.so.0.0.1 ../libSDL2_image-2.0.so.0
	cp $(PREFIX)/lib/libSDL2_ttf-2.0.so.0.14.0 ../libSDL2_ttf-2.0.so.0

	# directfb2
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
	printf "[host_machine]\n" >> $(REQUIRES_DIRECTFB2)/cross/rg35xxplus.ini
	printf "system = 'linux'\n" >> $(REQUIRES_DIRECTFB2)/cross/rg35xxplus.ini
	printf "cpu_family = 'arm'\n" >> $(REQUIRES_DIRECTFB2)/cross/rg35xxplus.ini
	printf "cpu = 'armv8-a'\n" >> $(REQUIRES_DIRECTFB2)/cross/rg35xxplus.ini
	printf "endian = 'little'\n" >> $(REQUIRES_DIRECTFB2)/cross/rg35xxplus.ini

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
	
clean:
	cd boot && rm -rf output
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart clean
	cd $(REQUIRES_FBSET) && make clean
	cd $(REQUIRES_FLUX) && make clean
	cd $(REQUIRES_DIRECTFB2) && rm -rf build
	
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

$(REQUIRES_FLUX):
	git clone https://github.com/deniskropp/flux $(REQUIRES_FLUX)

include ../all/readmes/makefile
