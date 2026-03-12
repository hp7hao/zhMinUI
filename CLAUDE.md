# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

zhMinUI is a fork of [MinUI](https://github.com/shauninman/MinUI) — a custom launcher and libretro frontend for retro gaming handhelds. This fork adds internationalization (Chinese support), theming, lockscreen, and firmware switching capabilities.

Target devices include Anbernic RG35xx family, Trimui Smart/Pro, Miyoo Mini, GKD Pixel, and others. All are ARM-based, resource-constrained Linux devices.

## Build System

Builds run on the host and use Docker containers for cross-compilation. Each platform has its own toolchain image (`union-<platform>-toolchain`).

### Key Commands

```bash
make setup                    # Initialize build directory from skeleton/
make convert-artworks         # Resize artwork PNGs (requires ImageMagick)
make <PLATFORM>               # Build for one platform (e.g., make rg35xxplus)
make all                      # Build all platforms, package releases
make shell PLATFORM=<name>    # Enter Docker shell for interactive dev
make build PLATFORM=<name>    # Build binaries only (runs inside Docker)
make system PLATFORM=<name>   # Assemble system files into build/
make cores PLATFORM=<name>    # Copy emulator cores into build/
make common PLATFORM=<name>   # Build + system + cores combined
make package                  # Create release ZIPs in releases/
make clean                    # Remove build/
```

**Supported platforms:** `miyoomini trimuismart rg35xx rg35xxplus my355 tg5040 zero28 rgb30 m17 gkdpixel my282 magicmini`

### Build Flow

1. `make setup` — copies `skeleton/` to `build/`, records git hash to `workspace/hash.txt`
2. `make convert-artworks` — converts PNG assets to 640x480
3. `make <PLATFORM>` — triggers `make common PLATFORM=<PLATFORM>` which runs Docker cross-compilation via `makefile.toolchain`, then assembles system files and copies cores
4. `make package` — zips into `releases/`

### Inside Docker

The Docker container maps `workspace/` to `/root/workspace/`. Inside, the platform's `workspace/<platform>/makefile` runs first (building platform deps), then each component in `workspace/all/` is built via its own makefile. Component makefiles include `../../<platform>/platform/makefile.env` for arch-specific `ARCH`, `LIBS`, and `SDL` version.

## Architecture

### Core Binaries (workspace/all/)

| Component | Path | Purpose |
|-----------|------|---------|
| **minui** | `workspace/all/minui/` | Main launcher UI — file browser, ROM launcher |
| **minarch** | `workspace/all/minarch/` | Libretro emulator frontend — loads `.so` cores via dlopen |
| **minwm** | `workspace/all/minwm/` | Window manager — lockscreen, power management |
| **syncsettings** | `workspace/all/syncsettings/` | Settings synchronization daemon |
| **clock** | `workspace/all/clock/` | Clock utility (tool pak) |
| **minput** | `workspace/all/minput/` | Input mapper (tool pak) |

### Shared Library (workspace/all/common/)

All binaries link against the same set of common source files (compiled per-binary, not a shared library):

- **api.c/h** — Graphics rendering (SDL), event handling, logging
- **scaler.c/h** — Image scaling/filtering
- **font.c/h** — Font loading and text rendering
- **i18n.c/h** — Internationalization (gettext-style .po/.mo files)
- **config.c/h** — Persistent configuration
- **theme.c/h** — Dynamic theming with color triads
- **lockscreen.c/h** — Lock screen UI
- **perf.c/h** — Performance monitoring
- **defines.h** — Global constants and macros
- **sdl.h** — SDL1/SDL2 compatibility shim (`USE_SDL` vs `USE_SDL2`)

### Platform Layer (workspace/<platform>/)

Each platform provides:
- `platform/platform.c` + `platform/platform.h` — Hardware-specific code, button mappings, display dimensions
- `platform/makefile.env` — Cross-compile flags (`ARCH`, `LIBS`, `SDL` version)
- `platform/makefile.copy` — File copy rules for system assembly
- `libmsettings/` — Platform-specific settings library
- `keymon/` — Input/keyboard monitor daemon
- `cores/` — Pre-compiled libretro emulator cores

### Key Patterns

- **Platform macro**: `PLATFORM` is defined at compile time; platform headers use `#ifdef` for device variants
- **SDL abstraction**: Code uses `USE_SDL` or `USE_SDL2` preprocessor flag; `sdl.h` provides a compatibility layer
- **Device detection**: Runtime checks like `is_cubexx()`, `is_rg34xx()` handle variant hardware within a platform
- **PAK system**: Emulators and tools are packaged as `.pak` folders containing `launch.sh` scripts (see `PAKS.md`)
- **Locale**: Translation files in `workspace/all/minui/locale/` (`.pot` template, `.po` per language, built via `locale/build.sh`)

### Directory Layout

```
skeleton/           # Template copied to build/ — SYSTEM/, BASE/, EXTRAS/, BOOT/
workspace/all/      # Cross-platform source code
workspace/<plat>/   # Platform-specific code, deps, cores
toolchains/         # Docker toolchain repos (cloned on first build)
build/              # Generated output (gitignored)
releases/           # Release ZIPs
```

### Dependencies

- **SDL 1.2 or SDL2** (varies by platform), SDL_image, SDL_ttf
- **libretro** API for emulator cores
- **msettings** — custom per-platform settings library
- **zlib**, **pthread**, **dl** (dlopen for cores)
- **ImageMagick** on host for artwork conversion

## Fork-Specific Notes

- The `rg35xxplus` toolchain clones from `hp7hao/union-rg35xxplus-toolchain` (custom fork), all others from `shauninman/`
- Current development branch (`dev_dfb2`) is experimenting with DirectFB2 support for rg35xxplus
- Chinese font support added via `wqy-zenhei.ttc`
