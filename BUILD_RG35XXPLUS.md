# Quick Guide: Build RG35XXPLUS Only

## Prerequisites

1. Docker installed and running
2. Git repository cloned
3. Toolchains for both rg35xx and rg35xxplus (rg35xxplus reuses rg35xx cores)

## Quick Start (Recommended)

The easiest way to build only rg35xxplus:

```bash
cd /home/hp/indie2/hp7hao/zhMinUI

# Build only rg35xx and rg35xxplus platforms
PLATFORMS="rg35xx rg35xxplus" make all
```

This single command handles everything: setup, building both platforms, special configs, and packaging.

## Build Steps

### 1. Build RG35XX First (for cores)
```bash
cd /home/hp/indie2/hp7hao/zhMinUI

# Build rg35xx to generate libretro cores
# rg35xxplus copies these cores instead of rebuilding them
make setup
make rg35xx
```

### 2. Build RG35XXPLUS
```bash
# Build rg35xxplus platform
make rg35xxplus
```

### 3. Package the Release (Optional)
```bash
# Create release ZIP files
make special package

# Your release files will be in:
# ./releases/MinUI-YYYYMMDD-N-base.zip
# ./releases/MinUI-YYYYMMDD-N-extras.zip
```

## One-Line Build (All Steps)

### Using PLATFORMS variable (recommended)
```bash
# Build only rg35xx and rg35xxplus with full packaging
PLATFORMS="rg35xx rg35xxplus" make all
```

### Manual step-by-step
```bash
make setup rg35xx rg35xxplus special package
```

## Incremental Builds

If you modify code and want to rebuild:

```bash
# Rebuild just rg35xxplus without setup
make build PLATFORM=rg35xxplus
make system PLATFORM=rg35xxplus
make cores PLATFORM=rg35xxplus
```

## Clean and Rebuild

```bash
# Clean build directory
make clean

# Clean toolchain (forces rebuild of Docker image)
cd toolchains/rg35xxplus-toolchain
make clean
cd ../..

# Rebuild everything
make setup rg35xx rg35xxplus
```

## Troubleshooting

### Missing build directory error
```
cp: cannot create regular file './build/SYSTEM/rg35xxplus/bin/': No such file or directory
```
**Solution**: Run `make setup` first

### Missing cores error
```
cp: cannot stat '../../rg35xx/cores/output': No such file or directory
```
**Solution**: Build `rg35xx` first (it compiles the cores)

### Docker build 404 errors (Debian Buster)
The Dockerfiles have been updated to use `archive.debian.org` since Buster is archived.

## Development Workflow

```bash
# 1. Enter toolchain shell for development
make shell PLATFORM=rg35xxplus

# 2. Inside container, build your changes
cd /root/workspace
make

# 3. Exit and copy binaries to build
exit
make system PLATFORM=rg35xxplus
```

## Output Structure

After building, your files are in:
```
./build/
├── SYSTEM/
│   └── rg35xxplus/
│       ├── bin/         # Executables (minui.elf, minarch.elf, etc.)
│       ├── cores/       # Libretro cores (.so files)
│       └── lib/         # Libraries
└── EXTRAS/
    └── Tools/
        └── rg35xxplus/  # Extra tools
```

## Supported Devices (rg35xxplus platform)

- RG35XX Plus, H, 2024, SP
- RG28XX
- RG40XX H/V
- RG CubeXX
- RG34XX (and SP)

All use the same `rg35xxplus` build!

