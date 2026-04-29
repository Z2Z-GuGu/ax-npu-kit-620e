#ifndef OCR_DET_COMPONENT_H
#define OCR_DET_COMPONENT_H

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

// AXERA SDK headers
#include "ax_engine_api.h"
#include "ax_sys_api.h"

#define OCR_DET_CMM_ALIGN_SIZE 128
#define OCR_DET_CMM_SESSION_NAME "npu"

// Bounding box expansion ratio (15% by default)
const float OCR_DET_BBOX_EXPANSION_RATIO = 0.15f;

struct TextBox {
    float x1, y1, x2, y2;
    float confidence;
    
    TextBox() : x1(0), y1(0), x2(0), y2(0), confidence(0) {}
    TextBox(float _x1, float _y1, float _x2, float _y2, float _conf)
        : x1(_x1), y1(_y1), x2(_x2), y2(_y2), confidence(_conf) {}
};

struct TextImageInfo {
    cv::Mat image;
    int boxIndex;      // Original box index (1-based)
    int segmentIndex;  // Segment index for this box (1-based, 1 means single segment)
    
    TextImageInfo() : boxIndex(0), segmentIndex(1) {}
    TextImageInfo(const cv::Mat& img, int idx, int seg) 
        : image(img), boxIndex(idx), segmentIndex(seg) {}
};

struct OCRDetResult {
    std::vector<TextBox> boxes;            // Boxes in 640x480 space
    std::vector<TextBox> boxesOriginal;    // Boxes in original image space
    std::vector<TextBox> boxesSorted;      // Boxes sorted by height (descending)
    cv::Mat heatmapData;                   // Raw heatmap data (480x640 float)
    cv::Mat visualizationImg;              // Visualization with overlay and boxes (640x480)
    cv::Mat visualizationOriginalImg;      // Original image with boxes
    std::vector<TextImageInfo> textImages; // Cropped text images with metadata
    bool success;
    std::string errorMessage;
    
    OCRDetResult() : success(false) {}
};

class OcrDetComponent {
public:
    OcrDetComponent();
    ~OcrDetComponent();
    
    bool loadModel(const std::string& modelPath);
    
    OCRDetResult detect(const cv::Mat& inputImage);
    
    // Detect with letterbox parameters for coordinate restoration
    OCRDetResult detect(const cv::Mat& inputImage, int targetWidth, int targetHeight,
                        int offsetX, int offsetY, int resizeWidth, int resizeHeight);
    
    bool saveResult(const OCRDetResult& result, 
                    const std::string& outputTxtPath,
                    const std::string& outputImagePath = "",
                    const std::string& outputOriginalImagePath = "",
                    const std::string& outputTextDir = "");
    
    static bool fileExists(const std::string& filename);
    
    // Sort boxes by height (descending) and assign indices
    void sortBoxesByHeight(std::vector<TextBox>& boxesSorted);
    
    // Crop and resize text regions from original image
    void cropAndResizeTextRegions(const cv::Mat& originalImage,
                                  const std::vector<TextBox>& boxesOriginal,
                                  std::vector<TextImageInfo>& textImages,
                                  int targetWidth = 320, int targetHeight = 48);
    
    // Save individual text images
    bool saveTextImages(const std::vector<TextImageInfo>& textImages,
                        const std::string& outputDir);

private:
    AX_ENGINE_HANDLE handle;
    bool modelLoaded;
    std::vector<char> modelBuffer;
    
    bool prepareIO(AX_ENGINE_IO_INFO_T* ioInfo, AX_ENGINE_IO_T* ioData);
    void freeIO(AX_ENGINE_IO_T* io);
    
    bool heatmapToBoundingBoxes(const float* data, int height, int width,
                                std::vector<TextBox>& boxes,
                                float threshold = 0.15f, float minArea = 50.0f);
    
    cv::Mat drawHeatmapOverlay(const cv::Mat& originalImage, 
                               const float* heatmapData, int height, int width,
                               const std::vector<TextBox>& boxes);
    
    cv::Mat drawBoxesOnOriginalImage(const cv::Mat& originalImage,
                                     const std::vector<TextBox>& boxesOriginal,
                                     const std::vector<TextBox>& boxesSorted);
    
    // Restore boxes from 640x480 space to original image space
    void restoreBoxesToOriginal(const std::vector<TextBox>& boxes,
                                std::vector<TextBox>& boxesOriginal,
                                int originalWidth, int originalHeight,
                                int targetWidth, int targetHeight,
                                int offsetX, int offsetY,
                                int resizeWidth, int resizeHeight);
};

#endif // OCR_DET_COMPONENT_H
