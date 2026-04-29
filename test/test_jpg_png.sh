#!/bin/bash

# Test script for letterbox_resize with JPG/PNG support

BUILD_DIR="build_ax630c"
TOOL="$BUILD_DIR/install/bin/letterbox_resize"

echo "========================================"
echo "Letterbox Resize Tool - JPG/PNG Test"
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

# Create a test image using Python
echo "Creating test images..."
echo "----------------------------------------"

python3 << 'EOF'
import numpy as np
import cv2

# Create a colorful test image (1920x1080)
img1 = np.zeros((1080, 1920, 3), dtype=np.uint8)
# Draw some patterns
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

print('Test images created successfully!')
EOF

if [ $? -ne 0 ]; then
    echo "Python3 or OpenCV not available, skipping image creation"
    echo "Please provide your own test images"
    exit 1
fi

echo ""

# Test 1: JPG 1920x1080 -> 640x480
echo "Test 1: JPG 1920x1080 -> 640x480"
echo "----------------------------------------"
$TOOL test_1920x1080.jpg output_1920x1080.nv12 640 480
if [ $? -eq 0 ]; then
    echo "✓ Test 1 passed"
    ls -lh output_1920x1080.nv12
else
    echo "✗ Test 1 failed"
fi
echo ""

# Test 2: PNG 640x640 -> 640x480
echo "Test 2: PNG 640x640 -> 640x480"
echo "----------------------------------------"
$TOOL test_640x640.png output_640x640.nv12 640 480
if [ $? -eq 0 ]; then
    echo "✓ Test 2 passed"
    ls -lh output_640x640.nv12
else
    echo "✗ Test 2 failed"
fi
echo ""

# Test 3: JPG 480x640 (portrait) -> 640x480
echo "Test 3: JPG 480x640 (portrait) -> 640x480"
echo "----------------------------------------"
$TOOL test_480x640.jpg output_480x640.nv12 640 480
if [ $? -eq 0 ]; then
    echo "✓ Test 3 passed"
    ls -lh output_480x640.nv12
else
    echo "✗ Test 3 failed"
fi
echo ""

# Test 4: Custom size
echo "Test 4: JPG 1920x1080 -> 800x600 (custom size)"
echo "----------------------------------------"
$TOOL test_1920x1080.jpg output_800x600.nv12 800 600
if [ $? -eq 0 ]; then
    echo "✓ Test 4 passed"
    ls -lh output_800x600.nv12
else
    echo "✗ Test 4 failed"
fi
echo ""

# Test 5: JPG to JPG output
echo "Test 5: JPG 1920x1080 -> JPG 640x480 (image output)"
echo "----------------------------------------"
$TOOL test_1920x1080.jpg output_640x480.jpg 640 480
if [ $? -eq 0 ]; then
    echo "✓ Test 5 passed"
    ls -lh output_640x480.jpg
    file output_640x480.jpg
else
    echo "✗ Test 5 failed"
fi
echo ""

# Test 6: PNG to PNG output
echo "Test 6: PNG 640x640 -> PNG 480x480 (image output)"
echo "----------------------------------------"
$TOOL test_640x640.png output_480x480.png 480 480
if [ $? -eq 0 ]; then
    echo "✓ Test 6 passed"
    ls -lh output_480x480.png
    file output_480x480.png
else
    echo "✗ Test 6 failed"
fi
echo ""

# Test 7: JPG to PNG output
echo "Test 7: JPG 1920x1080 -> PNG 800x600 (format conversion)"
echo "----------------------------------------"
$TOOL test_1920x1080.jpg output_800x600.png 800 600
if [ $? -eq 0 ]; then
    echo "✓ Test 7 passed"
    ls -lh output_800x600.png
    file output_800x600.png
else
    echo "✗ Test 7 failed"
fi
echo ""

# Cleanup
echo "Cleanup..."
rm -f test_*.jpg test_*.png output_*.nv12 output_*.jpg output_*.png
echo "Test files removed"
echo ""

echo "========================================"
echo "All tests completed!"
echo "========================================"
