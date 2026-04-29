/**************************************************************************************************
 *
 * Letterbox Resize Tool - Pure Software Version
 * 
 * This tool resizes an image to 640x480 with aspect ratio preservation using OpenCV.
 * If the aspect ratio differs, black borders are added (letterbox padding).
 *
 * Supports: JPG, PNG, BMP, TIFF formats for both input and output
 *
 **************************************************************************************************/

#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <string>
#include <opencv2/opencv.hpp>

// Check if file is a supported image format
bool IsImageFile(const char* filename)
{
    std::string fname(filename);
    size_t pos = fname.find_last_of(".");
    if (pos == std::string::npos) return false;
    
    std::string ext = fname.substr(pos);
    for (auto& c : ext) c = tolower(c);
    
    return (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || 
            ext == ".bmp" || ext == ".tiff" || ext == ".tif");
}

// Get output format from filename
bool IsImageOutput(const char* filename)
{
    return IsImageFile(filename);
}

void print_usage(const char* program_name)
{
    printf("Usage: %s <input_image> <output_image> [width] [height]\n", program_name);
    printf("\n");
    printf("Arguments:\n");
    printf("  input_image   - Input image file path (JPG, PNG, BMP, TIFF)\n");
    printf("  output_image  - Output image file path (JPG, PNG, BMP, TIFF)\n");
    printf("  width         - Target width (default: 640)\n");
    printf("  height        - Target height (default: 480)\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s input.jpg output.png\n", program_name);
    printf("  %s input.png output.jpg 640 480\n", program_name);
    printf("  %s input.jpg output.png 800 600\n", program_name);
    printf("\n");
    printf("Features:\n");
    printf("  - Preserves aspect ratio\n");
    printf("  - Adds black borders (letterbox) if needed\n");
    printf("  - Supports JPG, PNG, BMP, TIFF formats\n");
    printf("  - Pure software implementation (no hardware dependencies)\n");
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        print_usage(argv[0]);
        return -1;
    }

    const char* inputPath = argv[1];
    const char* outputPath = argv[2];
    int targetWidth = 640;
    int targetHeight = 480;
    
    // Parse optional width and height
    if (argc >= 5) {
        targetWidth = atoi(argv[3]);
        targetHeight = atoi(argv[4]);
        if (targetWidth <= 0 || targetHeight <= 0) {
            printf("Invalid dimensions: %dx%d\n", targetWidth, targetHeight);
            return -1;
        }
    }
    
    printf("========================================\n");
    printf("Letterbox Resize Tool (Pure Software)\n");
    printf("========================================\n");
    printf("Input: %s\n", inputPath);
    printf("Output: %s\n", outputPath);
    printf("Target size: %dx%d\n", targetWidth, targetHeight);
    printf("\n");

    // Check input file
    if (!IsImageFile(inputPath)) {
        printf("Error: Input file must be an image format (JPG, PNG, BMP, TIFF)\n");
        printf("For NV12 raw format, please use the hardware version of this tool.\n");
        return -1;
    }

    // Load input image
    printf("Loading image: %s\n", inputPath);
    cv::Mat srcImage = cv::imread(inputPath, cv::IMREAD_COLOR);
    if (srcImage.empty()) {
        printf("Error: Failed to load image: %s\n", inputPath);
        return -1;
    }
    
    printf("Image loaded: %dx%d (BGR)\n", srcImage.cols, srcImage.rows);

    // Calculate letterbox parameters
    float srcAspect = (float)srcImage.cols / (float)srcImage.rows;
    float dstAspect = (float)targetWidth / (float)targetHeight;
    
    int resizeWidth, resizeHeight;
    int offsetX, offsetY;
    
    // Calculate resize dimensions while preserving aspect ratio
    if (srcAspect > dstAspect) {
        // Source is wider - fit to width, add black bars top/bottom
        resizeWidth = targetWidth;
        resizeHeight = (int)(targetWidth / srcAspect);
        offsetX = 0;
        offsetY = (targetHeight - resizeHeight) / 2;
    } else {
        // Source is taller - fit to height, add black bars left/right
        resizeHeight = targetHeight;
        resizeWidth = (int)(targetHeight * srcAspect);
        offsetX = (targetWidth - resizeWidth) / 2;
        offsetY = 0;
    }
    
    printf("Source aspect ratio: %.2f\n", srcAspect);
    printf("Target aspect ratio: %.2f\n", dstAspect);
    printf("Resize to: %dx%d\n", resizeWidth, resizeHeight);
    printf("Offset: (%d, %d)\n", offsetX, offsetY);
    printf("\n");

    // Resize image
    printf("Resizing image...\n");
    cv::Mat resizedImage;
    cv::resize(srcImage, resizedImage, cv::Size(resizeWidth, resizeHeight), 0, 0, cv::INTER_AREA);
    
    // Create output image with black background
    printf("Creating letterbox output...\n");
    cv::Mat outputImage(targetHeight, targetWidth, CV_8UC3, cv::Scalar(0, 0, 0));
    
    // Copy resized image to center
    cv::Rect roi(offsetX, offsetY, resizeWidth, resizeHeight);
    resizedImage.copyTo(outputImage(roi));
    
    printf("Output image size: %dx%d\n", outputImage.cols, outputImage.rows);
    printf("\n");

    // Save output image
    printf("Saving output to: %s\n", outputPath);
    
    // Set compression parameters for different formats
    std::string outPath(outputPath);
    size_t pos = outPath.find_last_of(".");
    std::string ext = outPath.substr(pos);
    for (auto& c : ext) c = tolower(ext[pos]);
    
    std::vector<int> compression_params;
    if (ext == ".jpg" || ext == ".jpeg") {
        compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
        compression_params.push_back(95); // High quality JPEG
    } else if (ext == ".png") {
        compression_params.push_back(cv::IMWRITE_PNG_COMPRESSION);
        compression_params.push_back(3); // Low compression (fast, good quality)
    }
    
    bool success = cv::imwrite(outputPath, outputImage, compression_params);
    if (!success) {
        printf("Error: Failed to save image: %s\n", outputPath);
        return -1;
    }
    
    printf("\n");
    printf("========================================\n");
    printf("Done!\n");
    printf("========================================\n");

    return 0;
}
