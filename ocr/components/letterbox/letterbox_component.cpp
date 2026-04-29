#include "letterbox_component.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

LetterboxComponent::LetterboxComponent(int targetWidth, int targetHeight)
    : targetWidth(targetWidth), targetHeight(targetHeight)
{
}

bool LetterboxComponent::setTargetSize(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return false;
    }
    targetWidth = width;
    targetHeight = height;
    return true;
}

bool LetterboxComponent::isImageFile(const std::string& filename)
{
    std::string fname(filename);
    size_t pos = fname.find_last_of(".");
    if (pos == std::string::npos) return false;
    
    std::string ext = fname.substr(pos);
    for (auto& c : ext) c = tolower(c);
    
    return (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || 
            ext == ".bmp" || ext == ".tiff" || ext == ".tif");
}

LetterboxResult LetterboxComponent::createLetterbox(const cv::Mat& srcImage)
{
    LetterboxResult result;
    result.success = false;
    
    float srcAspect = (float)srcImage.cols / (float)srcImage.rows;
    float dstAspect = (float)targetWidth / (float)targetHeight;
    
    result.srcAspectRatio = srcAspect;
    result.targetAspectRatio = dstAspect;
    
    int resizeWidth, resizeHeight;
    
    if (srcAspect > dstAspect) {
        resizeWidth = targetWidth;
        resizeHeight = (int)(targetWidth / srcAspect);
        result.offsetX = 0;
        result.offsetY = (targetHeight - resizeHeight) / 2;
    } else {
        resizeHeight = targetHeight;
        resizeWidth = (int)(targetHeight * srcAspect);
        result.offsetX = (targetWidth - resizeWidth) / 2;
        result.offsetY = 0;
    }
    
    result.resizeWidth = resizeWidth;
    result.resizeHeight = resizeHeight;
    
    cv::Mat resizedImage;
    cv::resize(srcImage, resizedImage, cv::Size(resizeWidth, resizeHeight), 0, 0, cv::INTER_AREA);
    
    cv::Mat outputImage(targetHeight, targetWidth, CV_8UC3, cv::Scalar(0, 0, 0));
    
    cv::Rect roi(result.offsetX, result.offsetY, resizeWidth, resizeHeight);
    resizedImage.copyTo(outputImage(roi));
    
    result.outputImage = outputImage;
    result.success = true;
    
    return result;
}

LetterboxParams LetterboxComponent::calculateParams(int srcWidth, int srcHeight)
{
    LetterboxParams params;
    
    float srcAspect = (float)srcWidth / (float)srcHeight;
    float dstAspect = (float)targetWidth / (float)targetHeight;
    
    params.scale = srcAspect > dstAspect ? 
        (float)targetWidth / (float)srcWidth : 
        (float)targetHeight / (float)srcHeight;
    
    if (srcAspect > dstAspect) {
        params.resizeWidth = targetWidth;
        params.resizeHeight = (int)(targetWidth / srcAspect);
        params.offsetX = 0;
        params.offsetY = (targetHeight - params.resizeHeight) / 2;
    } else {
        params.resizeHeight = targetHeight;
        params.resizeWidth = (int)(targetHeight * srcAspect);
        params.offsetX = (targetWidth - params.resizeWidth) / 2;
        params.offsetY = 0;
    }
    
    return params;
}

LetterboxResult LetterboxComponent::process(const std::string& inputPath)
{
    LetterboxResult result;
    result.success = false;
    
    if (!isImageFile(inputPath)) {
        result.errorMessage = "Input file must be an image format (JPG, PNG, BMP, TIFF)";
        return result;
    }
    
    cv::Mat srcImage = cv::imread(inputPath, cv::IMREAD_COLOR);
    if (srcImage.empty()) {
        result.errorMessage = "Failed to load image: " + inputPath;
        return result;
    }
    
    return createLetterbox(srcImage);
}

bool LetterboxComponent::saveResult(const LetterboxResult& result, const std::string& outputPath)
{
    if (!result.success || result.outputImage.empty()) {
        return false;
    }
    
    std::string outPath(outputPath);
    size_t pos = outPath.find_last_of(".");
    std::string ext = (pos != std::string::npos) ? outPath.substr(pos) : "";
    for (auto& c : ext) c = tolower(c);
    
    std::vector<int> compression_params;
    if (ext == ".jpg" || ext == ".jpeg") {
        compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
        compression_params.push_back(95);
    } else if (ext == ".png") {
        compression_params.push_back(cv::IMWRITE_PNG_COMPRESSION);
        compression_params.push_back(3);
    }
    
    return cv::imwrite(outputPath, result.outputImage, compression_params);
}
