ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = lockscreen
PRODUCT = $(TARGET).elf

CC = $(CROSS_COMPILE)gcc
FLAGS	= -Os -lSDL2 -lSDL2_ttf -lrt -ldl -Wl,--gc-sections -s

all:
	$(CC) $(TARGET).c -o $(PRODUCT) $(FLAGS)
clean:
	rm -rf $(PRODUCT)
