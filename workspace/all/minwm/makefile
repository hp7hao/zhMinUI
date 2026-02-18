###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
	$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

ifeq (,$(CROSS_COMPILE))
	$(error missing CROSS_COMPILE for this toolchain)
endif

###########################################################

include ../../$(PLATFORM)/platform/makefile.env

###########################################################

TARGET = minwm
INCDIR = -I. -I../common/ -I../../$(PLATFORM)/platform/ -I$(PREFIX)/include/directfb -I$(PREFIX)/include
SOURCE = $(TARGET).c ../common/lockscreen.c ../common/api.c ../common/theme.c ../common/font.c ../common/config.c ../common/utils.c ../common/scaler.c ../common/i18n.c ../common/perf.c ../../$(PLATFORM)/platform/platform.c

CC = $(CROSS_COMPILE)gcc
CFLAGS   = $(ARCH) -fomit-frame-pointer
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -DUSE_$(SDL) -Ofast -std=gnu99
CFLAGS	+= -Os -flto
LDFLAGS	 = -ldl $(LIBS) -L$(PREFIX)/lib -Wl,-rpath-link,$(PREFIX)/lib -l$(SDL) -l$(SDL)_image -l$(SDL)_ttf -ldirectfb-2.0 -ldirect-2.0 -lfusion-2.0 -lpthread -lm -lrt -lmsettings

PRODUCT= build/$(PLATFORM)/$(TARGET).elf

all: $(PREFIX)/include/msettings.h
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(PRODUCT)

$(PREFIX)/include/msettings.h:
	cd /root/workspace/$(PLATFORM)/libmsettings && make
