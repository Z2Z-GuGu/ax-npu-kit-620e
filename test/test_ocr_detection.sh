#!/bin/bash

# Test script for OCR detection model

BUILD_DIR="build_ax630c"
TOOL="$BUILD_DIR/install/bin/test_ocr_detection"
MODEL="../models/pp_ocr/ch_PP_OCRv3_det_npu.axmodel"

echo "========================================"
echo "OCR Detection Model Test"
echo "========================================"
echo ""

# Check if tool exists
if [ ! -f "$TOOL" ]; then
    echo "Error: Tool not found at $TOOL"
    echo "Please run ./build.sh AX630C first"
    exit 1
fi

# Check if model exists
if [ ! -f "$MODEL" ]; then
    echo "Error: Model not found at $MODEL"
    exit 1
fi

echo "Tool: $TOOL"
echo "Model: $MODEL"
echo ""

# Create test image
echo "Creating test image with text..."
echo "----------------------------------------"

python3 << 'EOF'
import numpy as np
import cv2

# Create a white background image (640x480)
img = np.ones((480, 640, 3), dtype=np.uint8) * 255

# Draw some text-like rectangles
# Text line 1
cv2.rectangle(img, (50, 100), (300, 140), (0, 0, 0), -1)
# Text line 2
cv2.rectangle(img, (50, 180), (350, 220), (0, 0, 0), -1)
# Text line 3
cv2.rectangle(img, (50, 260), (280, 300), (0, 0, 0), -1)
# Small text box
cv2.rectangle(img, (400, 100), (580, 130), (0, 0, 0), -1)

# Add some noise to make it more realistic
noise = np.random.randint(0, 50, img.shape, dtype=np.uint8)
img = cv2.add(img, noise)

cv2.imwrite('test_text.png', img)
print('Created: test_text.png (640x480)')
EOF

if [ $? -ne 0 ]; then
    echo "Python3 or OpenCV not available, creating simple test image"
    # Create a simple test image using ImageMagick if available
    if command -v convert &> /dev/null; then
        convert -size 640x480 xc:white \
            -fill black \
            -draw "rectangle 50,100 300,140" \
            -draw "rectangle 50,180 350,220" \
            -draw "rectangle 50,260 280,300" \
            -draw "rectangle 400,100 580,130" \
            test_text.png
        echo 'Created: test_text.png (using ImageMagick)'
    else
        echo "Cannot create test image. Please provide your own image."
        exit 1
    fi
fi

echo ""

# Run test
echo "Running OCR detection model..."
echo "----------------------------------------"
$TOOL "$MODEL" test_text.png output.txt heatmap.png

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Test completed successfully!"
    echo ""
    echo "Output files:"
    ls -lh output.txt
    ls -lh heatmap.png 2>/dev/null || echo "Heatmap not created"
    echo ""
    
    # Show summary from output
    echo "Output Summary:"
    echo "----------------------------------------"
    head -20 output.txt
    echo "..."
    echo ""
    echo "Full results saved to: output.txt"
    echo "Heatmap saved to: heatmap.png"
else
    echo "✗ Test failed!"
fi

# Cleanup
echo ""
echo "Cleanup..."
rm -f test_text.png
echo "Test files removed (output.txt and heatmap.png kept)"
echo ""

echo "========================================"
echo "Test completed!"
echo "========================================"
