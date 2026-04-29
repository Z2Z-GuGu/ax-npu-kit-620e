#!/bin/bash

# Build script for OCR letterbox tool
# Supports AX620Q, AX630C, and AX650 chips

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Default chip type
CHIP_TYPE="${1:-AX630C}"

echo "========================================"
echo "Building OCR Letterbox for $CHIP_TYPE"
echo "========================================"

# Set chip flag and toolchain
case "$CHIP_TYPE" in
    AX620Q)
        CHIP_FLAG="-DCHIP_AX620Q=ON"
        TOOLCHAIN="../toolchains/arm-linux-uclibc-gnueabihf.toolchain.cmake"
        ;;
    AX630C)
        CHIP_FLAG="-DCHIP_AX630C=ON"
        TOOLCHAIN="../toolchains/aarch64-none-linux-gnu.toolchain.cmake"
        ;;
    AX650)
        CHIP_FLAG="-DCHIP_AX650=ON"
        TOOLCHAIN="../toolchains/aarch64-none-linux-gnu.toolchain.cmake"
        ;;
    *)
        echo "Error: Unknown chip type: $CHIP_TYPE"
        echo "Supported types: AX620Q, AX630C, AX650"
        exit 1
        ;;
esac

# Create build directory
BUILD_DIR="build_${CHIP_TYPE,,}"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Build directory: $(pwd)"
echo "Toolchain: $TOOLCHAIN"
echo "Chip flag: $CHIP_FLAG"
echo ""

# Configure
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=./install \
    $CHIP_FLAG

if [ $? -ne 0 ]; then
    echo "CMake configuration failed!"
    exit 1
fi

# Build
echo ""
echo "Building..."
make -j4

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

# Install
echo ""
echo "Installing..."
make install

echo ""
echo "========================================"
echo "Build completed successfully!"
echo "Executable: $BUILD_DIR/install/bin/ocr_letterbox"
echo "========================================"
