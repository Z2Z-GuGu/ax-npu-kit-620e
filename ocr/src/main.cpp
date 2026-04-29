#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include "letterbox_component.h"
#include "ocr_det_component.h"

void print_usage(const char* program_name)
{
    printf("Usage: %s <model_path> <input_image> [width] [height]\n", program_name);
    printf("\n");
    printf("Arguments:\n");
    printf("  model_path  - Path to .axmodel file (OCR detection model)\n");
    printf("  input_image - Input image file path (JPG, PNG, BMP, TIFF)\n");
    printf("  width       - Target width for letterbox resize (default: 640)\n");
    printf("  height      - Target height for letterbox resize (default: 480)\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s /root/models/pp_ocr/ch_PP_OCRv3_det_npu.axmodel test.png\n", program_name);
    printf("  %s model.axmodel input.jpg 640 480\n", program_name);
    printf("\n");
    printf("Output (auto-generated in same directory as input image):\n");
    printf("  - <image_name>_ocr/ folder\n");
    printf("  - <image_name>_out_640_480.png (heatmap visualization with letterbox)\n");
    printf("  - <image_name>_out.png (original image with red boxes)\n");
    printf("  - <image_name>_out.txt (detection results)\n");
    printf("  - text_001.png, text_002.png, ... (cropped text regions, 320x40 each)\n");
    printf("\n");
    printf("Workflow:\n");
    printf("  1. Resize input image to target size with letterbox (aspect ratio preserved)\n");
    printf("  2. Run OCR detection on resized image using NPU\n");
    printf("  3. Restore bounding boxes to original image coordinates\n");
    printf("  4. Output heatmap visualization and original image with boxes\n");
    printf("\n");
    printf("Features:\n");
    printf("  - Preserves aspect ratio with letterbox padding\n");
    printf("  - Detects text regions using OCR detection model\n");
    printf("  - Outputs bounding boxes with confidence scores\n");
    printf("  - Generates heatmap visualization with overlay\n");
    printf("  - Restores box coordinates to original image size\n");
}

// Create directory if it doesn't exist
bool createDirectory(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        // Directory doesn't exist, create it
        if (mkdir(path.c_str(), 0755) != 0) {
            printf("Error: Failed to create directory: %s\n", path.c_str());
            return false;
        }
        return true;
    }
    return true; // Directory already exists
}

// Get filename without extension
std::string getFilenameWithoutExtension(const std::string& filepath)
{
    size_t lastSlash = filepath.find_last_of("/\\");
    size_t lastDot = filepath.find_last_of('.');
    
    if (lastDot == std::string::npos || lastDot < lastSlash) {
        return filepath;
    }
    
    return filepath.substr(lastSlash + 1, lastDot - lastSlash - 1);
}

// Get directory from filepath
std::string getDirectory(const std::string& filepath)
{
    size_t lastSlash = filepath.find_last_of("/\\");
    if (lastSlash == std::string::npos) {
        return ".";
    }
    return filepath.substr(0, lastSlash);
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        print_usage(argv[0]);
        return -1;
    }

    const char* modelPath = argv[1];
    const char* inputPath = argv[2];
    int targetWidth = 640;
    int targetHeight = 480;
    
    if (argc >= 5) {
        targetWidth = atoi(argv[3]);
        targetHeight = atoi(argv[4]);
        if (targetWidth <= 0 || targetHeight <= 0) {
            printf("Invalid dimensions: %dx%d\n", targetWidth, targetHeight);
            return -1;
        }
    }
    
    // Check input files
    if (!OcrDetComponent::fileExists(modelPath)) {
        printf("Error: Model file not found: %s\n", modelPath);
        return -1;
    }
    
    if (!LetterboxComponent::isImageFile(inputPath)) {
        printf("Error: Input file must be an image format (JPG, PNG, BMP, TIFF)\n");
        return -1;
    }
    
    // Load input image to get dimensions
    cv::Mat inputImage = cv::imread(inputPath);
    if (inputImage.empty()) {
        printf("Error: Failed to load image: %s\n", inputPath);
        return -1;
    }
    
    int originalWidth = inputImage.cols;
    int originalHeight = inputImage.rows;
    
    printf("========================================\n");
    printf("OCR Text Detection Tool\n");
    printf("========================================\n");
    printf("Model: %s\n", modelPath);
    printf("Input: %s\n", inputPath);
    printf("Original image size: %dx%d\n", originalWidth, originalHeight);
    printf("Target size: %dx%d\n", targetWidth, targetHeight);
    printf("\n");

    // Calculate letterbox parameters
    LetterboxComponent letterbox(targetWidth, targetHeight);
    LetterboxParams params = letterbox.calculateParams(originalWidth, originalHeight);
    
    printf("Letterbox parameters:\n");
    printf("  Resize to: %dx%d\n", params.resizeWidth, params.resizeHeight);
    printf("  Offset: (%d, %d)\n", params.offsetX, params.offsetY);
    printf("  Scale: %.4f\n", params.scale);
    printf("\n");

    // Prepare output paths
    std::string inputDir = getDirectory(inputPath);
    std::string baseName = getFilenameWithoutExtension(inputPath);
    std::string outputDir = inputDir + "/" + baseName + "_ocr";
    
    std::string outputHeatmapPath = outputDir + "/" + baseName + "_out_" + 
                                    std::to_string(targetWidth) + "_" + 
                                    std::to_string(targetHeight) + ".png";
    std::string outputOriginalPath = outputDir + "/" + baseName + "_out.png";
    std::string outputTxtPath = outputDir + "/" + baseName + "_out.txt";
    
    // Create output directory
    if (!createDirectory(outputDir)) {
        return -1;
    }
    
    printf("Output directory: %s\n", outputDir.c_str());
    printf("Output files:\n");
    printf("  - %s\n", baseName + "_out_" + std::to_string(targetWidth) + "_" + std::to_string(targetHeight) + ".png");
    printf("  - %s\n", baseName + "_out.png");
    printf("  - %s\n", baseName + "_out.txt");
    printf("  - text_001.png, text_002.png, ... (cropped text regions)\n");
    printf("\n");

    // Run OCR detection with letterbox parameters
    printf("Running OCR detection...\n");
    OcrDetComponent ocrDet;
    
    if (!ocrDet.loadModel(modelPath)) {
        printf("Error: Failed to load model\n");
        return -1;
    }
    
    OCRDetResult detResult = ocrDet.detect(inputImage, targetWidth, targetHeight,
                                           params.offsetX, params.offsetY,
                                           params.resizeWidth, params.resizeHeight);
    
    if (!detResult.success) {
        printf("Error: %s\n", detResult.errorMessage.c_str());
        return -1;
    }
    
    // Save results
    printf("\nSaving results...\n");
    if (!ocrDet.saveResult(detResult, outputTxtPath, outputHeatmapPath, outputOriginalPath, outputDir)) {
        printf("Error: Failed to save results\n");
        return -1;
    }
    
    printf("\n");
    printf("========================================\n");
    printf("Done!\n");
    printf("========================================\n");
    printf("Summary:\n");
    printf("  - Detected %zu text regions\n", detResult.boxes.size());
    printf("  - Heatmap visualization: %s\n", outputHeatmapPath.c_str());
    printf("  - Original image with boxes: %s\n", outputOriginalPath.c_str());
    printf("  - Results text: %s\n", outputTxtPath.c_str());
    printf("\n");

    return 0;
}
