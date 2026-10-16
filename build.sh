#!/bin/sh
# Build script for the DualShock tester (PSn00bSDK + mipsel-none-elf toolchain)
set -e

# Directory containing the installed PSn00bSDK release (bin/, lib/, include/).
# The release bundles the mipsel-none-elf GCC toolchain and mkpsxiso as well.
PSN00BSDK_ROOT="${PSN00BSDK_ROOT:-$HOME/opt/PSn00bSDK-0.24-Linux}"

export PATH="$PSN00BSDK_ROOT/bin:$PATH"

cmake -S . -B build \
	-DCMAKE_TOOLCHAIN_FILE="$PSN00BSDK_ROOT/lib/libpsn00b/cmake/sdk.cmake" \
	-DCMAKE_BUILD_TYPE=Release

cmake --build build -j"$(nproc)"

echo
echo "Output files:"
echo "  build/dstest.exe  - PS-EXE executable (EMU/PSX-Serial compatible)"
echo "  build/dstest.iso  - bootable CD image"
echo "  build/dstest.bin  - CD image data track"
echo "  build/dstest.cue  - cue sheet (rename to UNIROM.cue for PicoStation)"
