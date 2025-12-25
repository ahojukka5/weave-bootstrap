# Automatic LLVM Download and Build

The Weave bootstrap project can automatically download and build LLVM from source if it's not found on your system.

## Quick Start

To enable automatic LLVM building:

```bash
cmake -DWEAVE_AUTO_BUILD_LLVM=ON ..
cmake --build . --target llvm_external
cmake ..  # Reconfigure to find the newly built LLVM
cmake --build .  # Build the project
```

## Requirements

Building LLVM from source requires:

- **C++ compiler** with C++17 support (clang++ or g++)
- **~10GB disk space** (for source, build, and install)
- **4+ GB RAM** (8GB+ recommended)
- **30-60+ minutes** build time (depending on your system)

## Build Process

1. **Configure with auto-build enabled:**
   ```bash
   cmake -DWEAVE_AUTO_BUILD_LLVM=ON ..
   ```
   This will:
   - Check if LLVM is already installed
   - If not found, set up ExternalProject to download and build LLVM
   - Configure LLVM build with minimal components (C API only)

2. **Build LLVM:**
   ```bash
   cmake --build . --target llvm_external
   ```
   This downloads the LLVM source code and builds it. This step takes 30-60+ minutes.

3. **Reconfigure CMake:**
   ```bash
   cmake ..
   ```
   After LLVM is built, reconfigure so CMake can find the newly built LLVM.

4. **Build the project:**
   ```bash
   cmake --build .
   ```

## What Gets Built

The automatic build configures LLVM with:
- **Minimal components**: Only the core LLVM libraries needed for the C API
- **Release build**: Optimized for performance
- **Host target only**: Only builds codegen for your current architecture
- **No tests/docs**: Faster build, smaller footprint

## Build Location

LLVM is built in your CMake build directory:
- **Source**: `build/llvm-src/`
- **Build**: `build/llvm-build/`
- **Install**: `build/llvm-install/`

You can clean the LLVM build by removing these directories.

## Alternative: Use System LLVM

If you prefer to use a system-installed LLVM:

```bash
# Install LLVM development packages
# macOS:
brew install llvm

# Ubuntu/Debian:
sudo apt-get install llvm-dev

# Then configure normally
cmake ..
```

The build system will automatically detect and use system LLVM if available.

## Troubleshooting

### Build Fails

- **Check C++ compiler**: Ensure you have a C++17 compatible compiler
- **Check disk space**: Ensure you have at least 10GB free
- **Check RAM**: Build may fail if system runs out of memory
- **Check logs**: See `build/llvm-build/llvm_external-build.log`

### CMake Can't Find Built LLVM

After building LLVM, you **must** reconfigure CMake:
```bash
cmake ..
```

### Build Takes Too Long

- Use more parallel jobs: `cmake --build . --target llvm_external -j8`
- Consider using a pre-built LLVM instead
- The first build is always slowest; subsequent builds are incremental

### Want to Use a Different LLVM Version

Edit `stage0/CMakeLists.txt` and change the `LLVM_VERSION` variable:
```cmake
set(LLVM_VERSION "18.1.0")  # Change to desired version
```

## Disabling Auto-Build

To disable automatic LLVM building:
```bash
cmake -DWEAVE_AUTO_BUILD_LLVM=OFF ..
```

Or simply don't set the option - it defaults to OFF.

## Benefits of Auto-Build

- **No manual installation**: Everything is handled automatically
- **Reproducible**: Same LLVM version for all developers
- **Isolated**: Doesn't interfere with system LLVM
- **Version control**: Easy to pin to specific LLVM version

## Drawbacks

- **Slow first build**: Takes 30-60+ minutes
- **Large download**: ~500MB source code
- **Disk space**: Requires ~10GB
- **Requires C++ compiler**: Must have C++ toolchain installed

