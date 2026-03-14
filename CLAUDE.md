# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

zhMinUI is a fork of MinUI — a focused retro game launcher and libretro frontend for handheld gaming devices. This fork targets the **rg35xxplus** platform (Anbernic RG35xx Plus family including RG34xx, RG40xx, RGCubeXX) with DirectFB2 compositor support and Mali G31 GPU acceleration.

## Build System

Builds use Docker cross-compilation. The host makefile orchestrates Docker containers that run the actual compilation.

### Key Commands

```bash
# Enter Docker cross-compile shell (interactive)
make shell PLATFORM=rg35xxplus

# Full build for one platform (runs inside Docker via makefile.toolchain)
make build PLATFORM=rg35xxplus

# Full release build for all platforms
make  # runs: setup → convert-artworks → build all platforms → package

# Inside Docker shell, build everything:
make  # runs workspace/makefile which builds all components in order

# Build individual components (inside Docker shell):
cd rg35xxplus/libmsettings && make    # settings library (must be first)
cd rg35xxplus && make early            # SDL2, DirectFB2, deps (cloned on demand)
cd rg35xxplus/keymon && make           # input daemon
cd all/minui && make                   # launcher UI
cd all/minarch && make                 # libretro frontend
cd all/clock && make                   # clock tool
cd all/minput && make                  # input config tool
cd all/syncsettings && make            # settings sync

# Copy built binaries to build/ tree (runs on host)
make system PLATFORM=rg35xxplus
make cores PLATFORM=rg35xxplus
```

### Build Order (workspace/makefile)

The build must follow this sequence — later steps depend on earlier ones:
1. `$(PLATFORM)/libmsettings` — shared settings library
2. `$(PLATFORM)` early — external deps (SDL2, DirectFB2, unzip60, dtc, fbset)
3. `$(PLATFORM)/keymon` — input event daemon
4. `all/minui` — launcher
5. `all/minarch` — emulation frontend
6. `all/clock`, `all/minput`, `all/syncsettings` — utilities
7. `$(PLATFORM)/cores` — libretro cores
8. `$(PLATFORM)` — platform-specific final targets (init, show, boot)

### Toolchain

- Cross-compiler: `arm-buildroot-linux-gnueabihf-` (32-bit ARM, Cortex-A53)
- Toolchain auto-cloned from `github.com/shauninman/union-$(PLATFORM)-toolchain/`
- Docker image built from `toolchains/$(PLATFORM)-toolchain/Dockerfile`
- Arch flags: `-marm -mtune=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard`

## Architecture

### Component Hierarchy

```
minwm (master process — DirectFB2 compositor, window manager)
  ├── lockscreen overlay (DWSC_UPPER layer)
  ├── spawns → minui.elf (game launcher UI)
  └── spawns → minarch.elf (via pak launch.sh, libretro frontend)

keymon (daemon — reads /dev/input/event*, manages volume/brightness)
  └── sends SIGUSR1/SIGUSR2 to minwm (lock/unlock)
```

- **minwm**: Always-running compositor. Owns the DFB2 display (DLSCL_ADMINISTRATIVE), manages window stack, lockscreen, app lifecycle. Ignores SIGTERM/SIGINT/SIGHUP (killed only via SIGKILL at shutdown).
- **minui**: Game browser/launcher. SDL2 app that runs under minwm. Writes `/tmp/next` when a game is selected.
- **minarch**: Libretro emulation frontend. Loads `.so` cores, handles save states, in-game menu, frame pacing, audio.
- **keymon**: System daemon. Reads raw evdev input, controls volume/brightness via msettings, monitors HDMI/jack, sends lock/unlock signals.
- **libmsettings**: POSIX shared memory IPC (`/SharedSettings`). Keymon writes; minui/minarch/lockscreen read.

### Source Layout

```
workspace/
  all/                    # Platform-agnostic code
    common/               # Shared library: api.h, defines.h, theme, config, scaler, i18n, lockscreen
    minui/minui.c         # Launcher (single large C file, ~65KB)
    minarch/minarch.c     # Libretro frontend (single large C file, ~138KB)
    minwm/                # DirectFB2 window manager (built binary only in build/)
    clock/, minput/, syncsettings/  # Small utilities
    cores/                # Libretro core compilation + patches
  rg35xxplus/             # Platform-specific code
    platform/platform.c   # Input mapping, video init, battery, power, LED, scaling
    platform/platform.h   # Button codes, fixed dimensions (640x480), HDMI (1280x720)
    platform/makefile.env # ARCH, LIBS, SDL=SDL2
    platform/makefile.copy # Copies platform binaries/libs to build/
    keymon/keymon.c       # Evdev input reader, volume/brightness, HDMI watcher
    libmsettings/         # Shared memory settings (brightness 0-10, volume 0-20)
    init/, show/, boot/   # Boot sequence utilities
    other/                # Git-cloned deps (SDL2, DirectFB2, unzip60, dtc, fbset) — not version controlled
```

### Display Pipeline (rg35xxplus)

- **DirectFB2** with `egl_mali_fbdev` system module (GPU-accelerated, Mali G31 GLES2)
- Custom GLES2 gfxdriver for FillRect/Blit/StretchBlit operations
- SDL2 compiled with `--enable-video-mali` (draws directly, `--disable-video-directfb`)
- Double-buffering via DLBM_BACKVIDEO (kernel fb ypanstep), fallback DLBM_BACKSYSTEM
- DFBARGS: `system=egl_mali_fbdev,inputdrivers=linux_input,no-banner,no-cursor,no-core-sighandler`

### Input Chain

```
/dev/input/event{0,1,3} → platform.c (maps vendor-specific evdev codes)
                        → SDL2 event loop (minui/minarch)
                        → keymon reads independently for volume/brightness/power
```

Button codes vary by device (Anbernic native vs RG P01 controller vs Xbox/8BitDo). All abstracted in `platform.c` via `CODE_*` and `JOY_*` constants.

### IPC

- **Shared memory** (`/SharedSettings`): msettings API for brightness, volume, jack, HDMI, mute state
- **Signals**: SIGUSR1 (lock), SIGUSR2 (unlock) from keymon → minwm
- **File sentinels**: `/tmp/minui_exec` (minwm checks to continue app loop), `/tmp/next` (game selection)

### Build Output

```
build/
  SYSTEM/$(PLATFORM)/bin/   # minui.elf, minarch.elf, keymon.elf, syncsettings.elf
  SYSTEM/$(PLATFORM)/lib/   # libmsettings.so, libSDL2*.so
  SYSTEM/$(PLATFORM)/cores/ # *.so libretro cores
  SYSTEM/res/               # Fonts, artworks (640x480), effects
  SYSTEM/locale/            # i18n .mo files
  BASE/                     # ROMs, BIOS, saves structure
  EXTRAS/                   # Extra emulators and tools (.pak bundles)
```

### Pak Format

Games and tools are packaged as `.pak` directories containing a `launch.sh` script. Emulator paks invoke `minarch.elf` with the appropriate libretro core `.so`.

## Platform Support

Supported platforms defined in top-level makefile:
`miyoomini trimuismart rg35xx rg35xxplus my355 tg5040 zero28 rgb30 m17 gkdpixel my282 magicmini`

Each platform has its own `workspace/$(PLATFORM)/` directory with platform-specific keymon, libmsettings, platform.c, boot scripts, and core build configs. The `workspace/all/` components compile per-platform via `#include` of the platform's `platform.c`.

## Conventions

- Main programs (minui, minarch) are single large C files — not split into modules
- Platform abstraction is via `PLAT_*` functions defined in each platform's `platform.c`
- Common API uses `GFX_*`, `SND_*`, `PAD_*`, `PWR_*` prefixes (defined in `common/api.h`)
- Compiled binaries use `.elf` extension
- External deps in `workspace/$(PLATFORM)/other/` are git-cloned on first build — not checked into the repo
