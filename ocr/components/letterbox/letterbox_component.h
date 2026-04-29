#ifndef LETTERBOX_COMPONENT_H
#define LETTERBOX_COMPONENT_H

#include <string>
#include <opencv2/opencv.hpp>

struct LetterboxParams {
    int resizeWidth;
    int resizeHeight;
    int offsetX;
    int offsetY;
    float scale;
};

struct LetterboxResult {
    cv::Mat outputImage;
    int resizeWidth;
    int resizeHeight;
    int offsetX;
    int offsetY;
    float srcAspectRatio;
    float targetAspectRatio;
    bool success;
    std::string errorMessage;
};

class LetterboxComponent {
public:
    LetterboxComponent(int targetWidth = 640, int targetHeight = 480);
    
    bool setTargetSize(int width, int height);
    
    LetterboxResult process(const std::string& inputPath);
    
    bool saveResult(const LetterboxResult& result, const std::string& outputPath);
    
    static bool isImageFile(const std::string& filename);
    
    int getTargetWidth() const { return targetWidth; }
    int getTargetHeight() const { return targetHeight; }
    
    // Calculate letterbox parameters without processing image
    LetterboxParams calculateParams(int srcWidth, int srcHeight);

private:
    int targetWidth;
    int targetHeight;
    
    LetterboxResult createLetterbox(const cv::Mat& srcImage);
};

#endif // LETTERBOX_COMPONENT_H
