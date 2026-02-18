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
SOURCE = $(TARGET).c

CC = $(CROSS_COMPILE)gcc
CFLAGS   = $(ARCH) -fomit-frame-pointer
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -DUSE_$(SDL) -DDATADIR=\"./data\" -Ofast -std=gnu99
CFLAGS	+= -Os -flto
LDFLAGS	 = -L$(PREFIX)/lib -Wl,-rpath-link,$(PREFIX)/lib -ldirectfb-2.0 -ldirect-2.0 -lfusion-2.0 -lm

PRODUCT= build/$(PLATFORM)/$(TARGET).elf

all:
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(PRODUCT)

