#!/bin/bash

# Test script for OCR recognition model

BUILD_DIR="build_ax630c"
TOOL="$BUILD_DIR/install/bin/test_ocr_recognition"
MODEL="../models/pp_ocr/ch_PP_OCRv4_rec_npu.axmodel"
DICT="../models/pp_ocr/ppocr_keys_v1.txt"

echo "========================================"
echo "OCR Recognition Model Test"
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

# Check if dictionary exists
if [ ! -f "$DICT" ]; then
    echo "Error: Dictionary not found at $DICT"
    exit 1
fi

echo "Tool: $TOOL"
echo "Model: $MODEL"
echo "Dictionary: $DICT"
echo ""

# Create test image (40x320 with text-like pattern)
echo "Creating test image (40x320)..."
echo "----------------------------------------"

python3 << 'EOF'
import numpy as np
import cv2

# Create a white background image (320x40)
img = np.ones((40, 320, 3), dtype=np.uint8) * 255

# Draw some text-like patterns (horizontal lines to simulate text)
# Line 1
cv2.rectangle(img, (10, 10), (80, 30), (0, 0, 0), -1)
# Line 2
cv2.rectangle(img, (90, 10), (150, 30), (0, 0, 0), -1)
# Line 3
cv2.rectangle(img, (160, 10), (240, 30), (0, 0, 0), -1)
# Small gap
cv2.rectangle(img, (250, 10), (300, 30), (0, 0, 0), -1)

# Save image
cv2.imwrite('test_text.png', img)
print("Test image created: test_text.png (320x40)")
EOF

if [ $? -ne 0 ]; then
    echo "Error: Failed to create test image"
    exit 1
fi

echo "----------------------------------------"
echo ""

# Run OCR recognition
echo "Running OCR recognition..."
echo "----------------------------------------"

"$TOOL" "$MODEL" "$DICT" "test_text.png" "test_recognition_result.txt"

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "Test completed successfully!"
    echo "========================================"
    echo ""
    echo "Results:"
    echo "  Output: test_recognition_result.txt"
    echo ""
    
    # Show result
    if [ -f "test_recognition_result.txt" ]; then
        echo "Result content:"
        echo "----------------------------------------"
        cat test_recognition_result.txt
        echo "----------------------------------------"
    fi
else
    echo ""
    echo "========================================"
    echo "Test failed!"
    echo "========================================"
    exit 1
fi
