#!/bin/bash

# Test script for letterbox_resize_sw (pure software version)

BUILD_DIR="build_ax630c"
TOOL="$BUILD_DIR/install/bin/letterbox_resize_sw"

echo "========================================"
echo "Letterbox Resize SW - Test Script"
echo "========================================"
echo ""

# Check if tool exists
if [ ! -f "$TOOL" ]; then
    echo "Error: Tool not found at $TOOL"
    echo "Please run ./build.sh AX630C first"
    exit 1
fi

echo "Tool found: $TOOL"
echo ""

# Create test images
echo "Creating test images..."
echo "----------------------------------------"

python3 << 'EOF'
import numpy as np
import cv2

# Create a colorful test image (1920x1080)
img1 = np.zeros((1080, 1920, 3), dtype=np.uint8)
cv2.rectangle(img1, (100, 100), (500, 400), (0, 255, 0), -1)
cv2.circle(img1, (960, 540), 200, (255, 0, 0), -1)
cv2.line(img1, (0, 0), (1920, 1080), (0, 0, 255), 5)
cv2.imwrite('test_1920x1080.jpg', img1)
print('Created: test_1920x1080.jpg (1920x1080)')

# Create a square image (640x640)
img2 = np.zeros((640, 640, 3), dtype=np.uint8)
cv2.rectangle(img2, (50, 50), (590, 590), (0, 255, 255), -1)
cv2.circle(img2, (320, 320), 150, (255, 0, 255), -1)
cv2.imwrite('test_640x640.png', img2)
print('Created: test_640x640.png (640x640)')

# Create a portrait image (480x640)
img3 = np.zeros((640, 480, 3), dtype=np.uint8)
cv2.rectangle(img3, (40, 40), (440, 600), (128, 255, 128), -1)
cv2.ellipse(img3, (240, 320), (100, 150), 0, 0, 360, (0, 128, 255), -1)
cv2.imwrite('test_480x640.jpg', img3)
print('Created: test_480x640.jpg (480x640)')

# Create a small odd-sized image (73x54)
img4 = np.zeros((54, 73, 3), dtype=np.uint8)
cv2.rectangle(img4, (10, 10), (60, 40), (255, 128, 0), -1)
cv2.circle(img4, (36, 27), 15, (128, 0, 255), -1)
cv2.imwrite('test_73x54.png', img4)
print('Created: test_73x54.png (73x54) - odd dimensions')

print('Test images created successfully!')
EOF

if [ $? -ne 0 ]; then
    echo "Python3 or OpenCV not available, skipping image creation"
    exit 1
fi

echo ""

# Test 1: JPG 1920x1080 -> PNG 640x480
echo "Test 1: JPG 1920x1080 -> PNG 640x480"
echo "----------------------------------------"
$TOOL test_1920x1080.jpg output_640x480.png 640 480
if [ $? -eq 0 ]; then
    echo "✓ Test 1 passed"
    ls -lh output_640x480.png
    file output_640x480.png
else
    echo "✗ Test 1 failed"
fi
echo ""

# Test 2: PNG 640x640 -> JPG 480x480
echo "Test 2: PNG 640x640 -> JPG 480x480"
echo "----------------------------------------"
$TOOL test_640x640.png output_480x480.jpg 480 480
if [ $? -eq 0 ]; then
    echo "✓ Test 2 passed"
    ls -lh output_480x480.jpg
    file output_480x480.jpg
else
    echo "✗ Test 2 failed"
fi
echo ""

# Test 3: JPG 480x640 (portrait) -> PNG 640x480
echo "Test 3: JPG 480x640 (portrait) -> PNG 640x480"
echo "----------------------------------------"
$TOOL test_480x640.jpg output_portrait.png 640 480
if [ $? -eq 0 ]; then
    echo "✓ Test 3 passed"
    ls -lh output_portrait.png
    file output_portrait.png
else
    echo "✗ Test 3 failed"
fi
echo ""

# Test 4: PNG 73x54 (odd size) -> JPG 640x480
echo "Test 4: PNG 73x54 (odd size) -> JPG 640x480"
echo "----------------------------------------"
$TOOL test_73x54.png output_odd_size.jpg 640 480
if [ $? -eq 0 ]; then
    echo "✓ Test 4 passed"
    ls -lh output_odd_size.jpg
    file output_odd_size.jpg
else
    echo "✗ Test 4 failed"
fi
echo ""

# Test 5: Custom size
echo "Test 5: JPG 1920x1080 -> PNG 800x600 (custom size)"
echo "----------------------------------------"
$TOOL test_1920x1080.jpg output_800x600.png 800 600
if [ $? -eq 0 ]; then
    echo "✓ Test 5 passed"
    ls -lh output_800x600.png
    file output_800x600.png
else
    echo "✗ Test 5 failed"
fi
echo ""

# Cleanup
echo "Cleanup..."
rm -f test_*.jpg test_*.png output_*.jpg output_*.png
echo "Test files removed"
echo ""

echo "========================================"
echo "All tests completed!"
echo "========================================"
