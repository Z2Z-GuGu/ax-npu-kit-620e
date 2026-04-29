#ifndef OCR_REC_COMPONENT_H
#define OCR_REC_COMPONENT_H

#include <string>
#include <vector>
#include <map>
#include <opencv2/opencv.hpp>

// AXERA SDK headers
#include "ax_engine_api.h"
#include "ax_sys_api.h"

#define OCR_REC_CMM_ALIGN_SIZE 128
#define OCR_REC_CMM_SESSION_NAME "npu"

struct OCRRecResult {
    std::string text;              // Recognized text
    float confidence;              // Average confidence
    std::vector<float> charConfidences;  // Per-character confidences
    std::vector<int> charIndices;        // Character indices in dictionary
    bool success;
    std::string errorMessage;
    
    OCRRecResult() : confidence(0), success(false) {}
};

class OcrRecComponent {
public:
    OcrRecComponent();
    ~OcrRecComponent();
    
    bool loadModel(const std::string& modelPath);
    bool loadDictionary(const std::string& dictPath);
    
    OCRRecResult recognize(const cv::Mat& inputImage);
    
    bool saveResult(const OCRRecResult& result, 
                    const std::string& outputTxtPath);
    
    static bool fileExists(const std::string& filename);
    
    // Get dictionary size
    int getDictSize() const { return dictSize; }
    
    // Get character from index
    std::string getChar(int index) const;

private:
    AX_ENGINE_HANDLE handle;
    bool modelLoaded;
    std::vector<char> modelBuffer;
    
    std::vector<std::string> dictionary;
    int dictSize;
    
    // Cached IO info and buffers for faster inference
    AX_ENGINE_IO_INFO_T* cachedIOInfo;
    AX_ENGINE_IO_T cachedIOData;
    bool ioBuffersAllocated;
    
    bool prepareIO(AX_ENGINE_IO_INFO_T* ioInfo, AX_ENGINE_IO_T* ioData);
    void freeIO(AX_ENGINE_IO_T* io);
    
    // Decode CTC output to text
    std::string decodeCTC(const float* outputData, int width, int height,
                          std::vector<float>& charConfidences,
                          std::vector<int>& charIndices);
    
    // Preprocess image for recognition (40x320 input)
    cv::Mat preprocessImage(const cv::Mat& inputImage, int targetWidth = 320, int targetHeight = 40);
};

#endif // OCR_REC_COMPONENT_H
