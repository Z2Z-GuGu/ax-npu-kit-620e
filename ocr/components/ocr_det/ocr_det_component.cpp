#include "ocr_det_component.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

OcrDetComponent::OcrDetComponent() : handle(0), modelLoaded(false)
{
}

OcrDetComponent::~OcrDetComponent()
{
    if (handle) {
        AX_ENGINE_DestroyHandle(handle);
    }
    AX_ENGINE_Deinit();
    AX_SYS_Deinit();
}

bool OcrDetComponent::fileExists(const std::string& filename)
{
    FILE* fp = fopen(filename.c_str(), "r");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

bool OcrDetComponent::loadModel(const std::string& modelPath)
{
    if (!fileExists(modelPath)) {
        printf("Error: Model file not found: %s\n", modelPath.c_str());
        return false;
    }
    
    FILE* fp = fopen(modelPath.c_str(), "rb");
    if (!fp) {
        printf("Error: Cannot open model file: %s\n", modelPath.c_str());
        return false;
    }
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    modelBuffer.resize(size);
    
    if (fread(modelBuffer.data(), 1, size, fp) != (size_t)size) {
        printf("Error: Cannot read model file\n");
        fclose(fp);
        return false;
    }
    
    fclose(fp);
    printf("Model loaded: %s (%ld bytes)\n", modelPath.c_str(), size);
    
    // Initialize AXERA SDK
    AX_S32 ret = AX_SYS_Init();
    if (ret != 0) {
        printf("Error: AX_SYS_Init failed: 0x%x\n", ret);
        return false;
    }
    
    // Initialize ENGINE
    AX_ENGINE_NPU_ATTR_T npuAttr;
    memset(&npuAttr, 0, sizeof(AX_ENGINE_NPU_ATTR_T));
    npuAttr.eHardMode = AX_ENGINE_VIRTUAL_NPU_DISABLE;
    ret = AX_ENGINE_Init(&npuAttr);
    if (ret != 0) {
        printf("Error: AX_ENGINE_Init failed: 0x%x\n", ret);
        AX_SYS_Deinit();
        return false;
    }
    
    // Create handle with model buffer
    ret = AX_ENGINE_CreateHandle(&handle, modelBuffer.data(), modelBuffer.size());
    if (ret != 0 || !handle) {
        printf("Error: AX_ENGINE_CreateHandle failed: 0x%x\n", ret);
        AX_ENGINE_Deinit();
        AX_SYS_Deinit();
        return false;
    }
    
    // Create context
    ret = AX_ENGINE_CreateContext(handle);
    if (ret != 0) {
        printf("Error: AX_ENGINE_CreateContext failed: 0x%x\n", ret);
        AX_ENGINE_DestroyHandle(handle);
        handle = 0;
        AX_ENGINE_Deinit();
        AX_SYS_Deinit();
        return false;
    }
    
    modelLoaded = true;
    return true;
}

void OcrDetComponent::freeIO(AX_ENGINE_IO_T* io)
{
    if (!io) return;
    
    for (AX_U32 i = 0; i < io->nInputSize; i++) {
        if (io->pInputs[i].pVirAddr) {
            AX_SYS_MemFree(io->pInputs[i].phyAddr, io->pInputs[i].pVirAddr);
        }
    }
    
    for (AX_U32 i = 0; i < io->nOutputSize; i++) {
        if (io->pOutputs[i].pVirAddr) {
            AX_SYS_MemFree(io->pOutputs[i].phyAddr, io->pOutputs[i].pVirAddr);
        }
    }
    
    delete[] io->pInputs;
    delete[] io->pOutputs;
}

bool OcrDetComponent::prepareIO(AX_ENGINE_IO_INFO_T* ioInfo, AX_ENGINE_IO_T* ioData)
{
    memset(ioData, 0, sizeof(*ioData));
    
    // Allocate input buffers
    ioData->pInputs = new AX_ENGINE_IO_BUFFER_T[ioInfo->nInputSize];
    memset(ioData->pInputs, 0, sizeof(AX_ENGINE_IO_BUFFER_T) * ioInfo->nInputSize);
    ioData->nInputSize = ioInfo->nInputSize;
    
    for (AX_U32 i = 0; i < ioInfo->nInputSize; i++) {
        auto& meta = ioInfo->pInputs[i];
        auto& buffer = ioData->pInputs[i];
        
        AX_S32 ret = AX_SYS_MemAlloc(
            (AX_U64*)(&buffer.phyAddr),
            &buffer.pVirAddr,
            meta.nSize,
            OCR_DET_CMM_ALIGN_SIZE,
            (const AX_S8*)OCR_DET_CMM_SESSION_NAME
        );
        
        if (ret != 0) {
            printf("Error: Failed to allocate input buffer %d (size: %u)\n", i, meta.nSize);
            freeIO(ioData);
            return false;
        }
    }
    
    // Allocate output buffers
    ioData->pOutputs = new AX_ENGINE_IO_BUFFER_T[ioInfo->nOutputSize];
    memset(ioData->pOutputs, 0, sizeof(AX_ENGINE_IO_BUFFER_T) * ioInfo->nOutputSize);
    ioData->nOutputSize = ioInfo->nOutputSize;
    
    for (AX_U32 i = 0; i < ioInfo->nOutputSize; i++) {
        auto& meta = ioInfo->pOutputs[i];
        auto& buffer = ioData->pOutputs[i];
        buffer.nSize = meta.nSize;
        
        AX_S32 ret = AX_SYS_MemAlloc(
            (AX_U64*)(&buffer.phyAddr),
            &buffer.pVirAddr,
            meta.nSize,
            OCR_DET_CMM_ALIGN_SIZE,
            (const AX_S8*)OCR_DET_CMM_SESSION_NAME
        );
        
        if (ret != 0) {
            printf("Error: Failed to allocate output buffer %d (size: %u)\n", i, meta.nSize);
            freeIO(ioData);
            return false;
        }
    }
    
    return true;
}

bool OcrDetComponent::heatmapToBoundingBoxes(const float* data, int height, int width,
                                             std::vector<TextBox>& boxes,
                                             float threshold, float minArea)
{
    // Normalize heatmap to 0-255
    float minVal = data[0];
    float maxVal = data[0];
    
    for (int i = 0; i < height * width; i++) {
        if (data[i] < minVal) minVal = data[i];
        if (data[i] > maxVal) maxVal = data[i];
    }
    
    float range = maxVal - minVal;
    if (range < 0.0001f) range = 1.0f;
    
    // Create binary image
    cv::Mat binary(height, width, CV_8UC1);
    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            float confidence = (data[h * width + w] - minVal) / range;
            binary.at<uchar>(h, w) = (confidence > threshold) ? 255 : 0;
        }
    }
    
    // Morphological operations to connect nearby regions
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::Mat dilated;
    cv::dilate(binary, dilated, kernel, cv::Point(-1, -1), 2);
    cv::Mat eroded;
    cv::erode(dilated, eroded, kernel, cv::Point(-1, -1), 1);
    
    // Find contours
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(eroded, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    // Convert contours to bounding boxes
    boxes.clear();
    for (size_t i = 0; i < contours.size(); i++) {
        cv::Rect rect = cv::boundingRect(contours[i]);
        
        // Filter small boxes
        if (rect.area() >= minArea) {
            TextBox box;
            box.x1 = (float)rect.x;
            box.y1 = (float)rect.y;
            box.x2 = (float)(rect.x + rect.width);
            box.y2 = (float)(rect.y + rect.height);
            
            // Calculate average confidence in the box region
            float sumConf = 0.0f;
            int count = 0;
            for (int y = rect.y; y < rect.y + rect.height; y++) {
                for (int x = rect.x; x < rect.x + rect.width; x++) {
                    if (binary.at<uchar>(y, x) > 0) {
                        sumConf += (data[y * width + x] - minVal) / range;
                        count++;
                    }
                }
            }
            box.confidence = (count > 0) ? (sumConf / count) : 0.0f;
            
            boxes.push_back(box);
            
            printf("Box %zu: [%d, %d, %d, %d] (confidence: %.3f)\n", 
                   i + 1, rect.x, rect.y, rect.x + rect.width, rect.y + rect.height, box.confidence);
        }
    }
    
    printf("Total boxes detected: %zu\n", boxes.size());
    return (boxes.size() > 0);
}

cv::Mat OcrDetComponent::drawHeatmapOverlay(const cv::Mat& originalImage, 
                                            const float* heatmapData, int height, int width,
                                            const std::vector<TextBox>& boxes)
{
    cv::Mat image = originalImage.clone();
    
    // Resize original image to match heatmap size if needed
    if (image.cols != width || image.rows != height) {
        cv::resize(image, image, cv::Size(width, height), 0, 0, cv::INTER_AREA);
    }
    
    // Normalize heatmap to 0-1
    float minVal = heatmapData[0];
    float maxVal = heatmapData[0];
    
    for (int i = 0; i < height * width; i++) {
        if (heatmapData[i] < minVal) minVal = heatmapData[i];
        if (heatmapData[i] > maxVal) maxVal = heatmapData[i];
    }
    
    float range = maxVal - minVal;
    if (range < 0.0001f) range = 1.0f;
    
    // Create green overlay mask
    cv::Mat overlay = image.clone();
    
    // Draw green semi-transparent overlay based on confidence
    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            float confidence = (heatmapData[h * width + w] - minVal) / range;
            
            // Only draw overlay for high confidence areas (> 0.15)
            if (confidence > 0.15f) {
                uchar intensity = (uchar)(255 * confidence);
                overlay.at<cv::Vec3b>(h, w) = cv::Vec3b(0, intensity, 0);
            }
        }
    }
    
    // Blend original image with overlay (alpha = 0.5 for semi-transparent)
    float alpha = 0.5f;
    cv::Mat blended;
    cv::addWeighted(overlay, alpha, image, 1.0f - alpha, 0.0f, blended);
    
    // Draw red bounding boxes (expanded)
    if (!boxes.empty()) {
        for (size_t i = 0; i < boxes.size(); i++) {
            // Expand shorter side by 15%
            float boxWidth = boxes[i].x2 - boxes[i].x1;
            float boxHeight = boxes[i].y2 - boxes[i].y1;
            
            float expandAmount = (boxWidth < boxHeight) ? 
                (boxWidth * OCR_DET_BBOX_EXPANSION_RATIO) : 
                (boxHeight * OCR_DET_BBOX_EXPANSION_RATIO);
            
            cv::Point pt1(boxes[i].x1 - expandAmount, boxes[i].y1 - expandAmount);
            cv::Point pt2(boxes[i].x2 + expandAmount, boxes[i].y2 + expandAmount);
            
            // Draw red rectangle (thickness: 1px)
            cv::rectangle(blended, pt1, pt2, cv::Scalar(0, 0, 255), 1);
        }
    }
    
    return blended;
}

cv::Mat OcrDetComponent::drawBoxesOnOriginalImage(const cv::Mat& originalImage,
                                                   const std::vector<TextBox>& boxesOriginal,
                                                   const std::vector<TextBox>& boxesSorted)
{
    cv::Mat result = originalImage.clone();
    
    // Draw red bounding boxes on original image
    if (!boxesOriginal.empty()) {
        for (size_t i = 0; i < boxesOriginal.size(); i++) {
            // Expand shorter side by 15%
            float boxWidth = boxesOriginal[i].x2 - boxesOriginal[i].x1;
            float boxHeight = boxesOriginal[i].y2 - boxesOriginal[i].y1;
            
            float expandAmount = (boxWidth < boxHeight) ? 
                (boxWidth * OCR_DET_BBOX_EXPANSION_RATIO) : 
                (boxHeight * OCR_DET_BBOX_EXPANSION_RATIO);
            
            cv::Point pt1(boxesOriginal[i].x1 - expandAmount, boxesOriginal[i].y1 - expandAmount);
            cv::Point pt2(boxesOriginal[i].x2 + expandAmount, boxesOriginal[i].y2 + expandAmount);
            
            // Draw red rectangle (thickness: 2px for original image)
            cv::rectangle(result, pt1, pt2, cv::Scalar(0, 0, 255), 2);
        }
    }
    
    // Draw index numbers next to sorted boxes
    if (!boxesSorted.empty()) {
        for (size_t i = 0; i < boxesSorted.size(); i++) {
            // Get box coordinates
            int x1 = std::max(0, (int)boxesSorted[i].x1);
            int y1 = std::max(0, (int)boxesSorted[i].y1);
            int x2 = std::min(originalImage.cols, (int)boxesSorted[i].x2);
            int y2 = std::min(originalImage.rows, (int)boxesSorted[i].y2);
            
            // Skip invalid boxes
            if (x2 <= x1 || y2 <= y1) {
                continue;
            }
            
            // Prepare label text
            char label[16];
            snprintf(label, sizeof(label), "#%03zu", i + 1);
            
            // Get text size
            int fontFace = cv::FONT_HERSHEY_SIMPLEX;
            double fontScale = 0.4;
            int thickness = 1;
            cv::Size textSize = cv::getTextSize(label, fontFace, fontScale, thickness, nullptr);
            
            // Position: top-right corner of the box
            int textX = x2 - textSize.width - 2;
            int textY = y1 + textSize.height + 2;
            
            // Ensure text is within image bounds
            if (textX < 0) textX = 0;
            if (textY > originalImage.rows - 1) textY = originalImage.rows - 1;
            
            // Draw text background (white rectangle)
            cv::Point bgTL(textX - 1, textY - textSize.height - 1);
            cv::Point bgBR(textX + textSize.width + 1, textY + 1);
            cv::rectangle(result, bgTL, bgBR, cv::Scalar(255, 255, 255), cv::FILLED);
            
            // Draw text (black)
            cv::putText(result, label, cv::Point(textX, textY), 
                       fontFace, fontScale, cv::Scalar(0, 0, 0), thickness);
        }
    }
    
    return result;
}

void OcrDetComponent::restoreBoxesToOriginal(const std::vector<TextBox>& boxes,
                                              std::vector<TextBox>& boxesOriginal,
                                              int originalWidth, int originalHeight,
                                              int targetWidth, int targetHeight,
                                              int offsetX, int offsetY,
                                              int resizeWidth, int resizeHeight)
{
    boxesOriginal.clear();
    
    // Calculate scale factors (from resized image to original image)
    float scaleX = (float)originalWidth / resizeWidth;
    float scaleY = (float)originalHeight / resizeHeight;
    
    for (size_t i = 0; i < boxes.size(); i++) {
        TextBox boxOrig;
        
        // Step 1: Remove offset (letterbox padding)
        float x1 = boxes[i].x1 - offsetX;
        float y1 = boxes[i].y1 - offsetY;
        float x2 = boxes[i].x2 - offsetX;
        float y2 = boxes[i].y2 - offsetY;
        
        // Step 2: Clip to valid region (in case boxes extend beyond letterbox area)
        x1 = std::max(0.0f, std::min(x1, (float)resizeWidth));
        y1 = std::max(0.0f, std::min(y1, (float)resizeHeight));
        x2 = std::max(0.0f, std::min(x2, (float)resizeWidth));
        y2 = std::max(0.0f, std::min(y2, (float)resizeHeight));
        
        // Step 3: Scale to original image size
        boxOrig.x1 = x1 * scaleX;
        boxOrig.y1 = y1 * scaleY;
        boxOrig.x2 = x2 * scaleX;
        boxOrig.y2 = y2 * scaleY;
        boxOrig.confidence = boxes[i].confidence;
        
        boxesOriginal.push_back(boxOrig);
    }
}

OCRDetResult OcrDetComponent::detect(const cv::Mat& inputImage, int targetWidth, int targetHeight,
                                      int offsetX, int offsetY, int resizeWidth, int resizeHeight)
{
    OCRDetResult result;
    result.success = false;
    
    if (!modelLoaded) {
        result.errorMessage = "Model not loaded";
        return result;
    }
    
    if (inputImage.empty()) {
        result.errorMessage = "Input image is empty";
        return result;
    }
    
    // Create letterbox image (same as letterbox component does)
    cv::Mat letterboxImage(targetHeight, targetWidth, CV_8UC3, cv::Scalar(0, 0, 0));
    
    // Resize input image
    cv::Mat resizedImage;
    cv::resize(inputImage, resizedImage, cv::Size(resizeWidth, resizeHeight), 0, 0, cv::INTER_AREA);
    
    // Copy to center of letterbox
    cv::Rect roi(offsetX, offsetY, resizeWidth, resizeHeight);
    resizedImage.copyTo(letterboxImage(roi));
    
    // Get IO info
    AX_ENGINE_IO_INFO_T* ioInfo = nullptr;
    AX_S32 ret = AX_ENGINE_GetIOInfo(handle, &ioInfo);
    if (ret != 0 || !ioInfo) {
        result.errorMessage = "AX_ENGINE_GetIOInfo failed";
        return result;
    }
    
    printf("\nModel IO Info:\n");
    printf("  Inputs: %u\n", ioInfo->nInputSize);
    printf("  Outputs: %u\n", ioInfo->nOutputSize);
    
    // Prepare IO buffers
    printf("\nAllocating IO buffers...\n");
    AX_ENGINE_IO_T ioData;
    if (!prepareIO(ioInfo, &ioData)) {
        result.errorMessage = "Failed to prepare IO";
        return result;
    }
    
    // Copy image data to input buffer
    printf("Copying image data to input buffer...\n");
    AX_U8* inputData = (AX_U8*)letterboxImage.data;
    AX_U32 inputSize = targetWidth * targetHeight * 3;
    memcpy(ioData.pInputs[0].pVirAddr, inputData, inputSize);
    
    // Run inference
    printf("\nRunning inference...\n");
    ret = AX_ENGINE_RunSync(handle, &ioData);
    if (ret != 0) {
        printf("Error: AX_ENGINE_RunSync failed: 0x%x\n", ret);
        freeIO(&ioData);
        result.errorMessage = "Inference failed";
        return result;
    }
    printf("Inference completed!\n\n");
    
    // Get output data
    float* outputData = (float*)ioData.pOutputs[0].pVirAddr;
    AX_U32 outputHeight = ioInfo->pOutputs[0].pShape[2]; // 480
    AX_U32 outputWidth = ioInfo->pOutputs[0].pShape[3];  // 640
    
    printf("Output shape: [1, 1, %d, %d]\n", outputHeight, outputWidth);
    
    // Store heatmap data
    result.heatmapData = cv::Mat(outputHeight, outputWidth, CV_32FC1, (void*)outputData).clone();
    
    // Convert heatmap to bounding boxes (in 640x480 space)
    if (heatmapToBoundingBoxes(outputData, outputHeight, outputWidth, result.boxes)) {
        printf("\n=== Bounding Boxes Summary ===\n");
        printf("Detected %zu text regions\n", result.boxes.size());
    }
    
    // Restore boxes to original image space
    restoreBoxesToOriginal(result.boxes, result.boxesOriginal,
                           inputImage.cols, inputImage.rows,
                           targetWidth, targetHeight,
                           offsetX, offsetY,
                           resizeWidth, resizeHeight);
    
    // Sort boxes by height
    result.boxesSorted = result.boxesOriginal;
    sortBoxesByHeight(result.boxesSorted);
    
    // Crop and resize text regions
    cropAndResizeTextRegions(inputImage, result.boxesOriginal, result.textImages, 320, 48);
    
    // Draw visualization on 640x480 image
    result.visualizationImg = drawHeatmapOverlay(letterboxImage, outputData, 
                                                  outputHeight, outputWidth, result.boxes);
    
    // Draw boxes on original image with index numbers
    result.visualizationOriginalImg = drawBoxesOnOriginalImage(inputImage, result.boxesOriginal, 
                                                               result.boxesSorted);
    
    // Cleanup
    freeIO(&ioData);
    
    result.success = true;
    return result;
}

OCRDetResult OcrDetComponent::detect(const cv::Mat& inputImage)
{
    // Default: detect directly without letterbox
    return detect(inputImage, 640, 480, 0, 0, 640, 480);
}

bool OcrDetComponent::saveResult(const OCRDetResult& result, 
                                  const std::string& outputTxtPath,
                                  const std::string& outputImagePath,
                                  const std::string& outputOriginalImagePath,
                                  const std::string& outputTextDir)
{
    if (!result.success) {
        return false;
    }
    
    // Save text output
    FILE* fp = fopen(outputTxtPath.c_str(), "w");
    if (!fp) {
        printf("Error: Cannot create output file: %s\n", outputTxtPath.c_str());
        return false;
    }
    
    // Write header
    fprintf(fp, "OCR Detection Model Output\n");
    fprintf(fp, "==========================\n\n");
    fprintf(fp, "Output Shape: [1, 1, 480, 640]\n");
    fprintf(fp, "Data Type: float32\n");
    fprintf(fp, "Total Elements: %d\n\n", 480 * 640);
    
    // Write statistics
    const float* data = (const float*)result.heatmapData.data;
    int totalElements = 480 * 640;
    
    float minVal = data[0];
    float maxVal = data[0];
    float sumVal = 0.0f;
    
    for (int i = 0; i < totalElements; i++) {
        if (data[i] < minVal) minVal = data[i];
        if (data[i] > maxVal) maxVal = data[i];
        sumVal += data[i];
    }
    
    float meanVal = sumVal / totalElements;
    
    fprintf(fp, "Statistics:\n");
    fprintf(fp, "  Min: %.6f\n", minVal);
    fprintf(fp, "  Max: %.6f\n", maxVal);
    fprintf(fp, "  Mean: %.6f\n", meanVal);
    fprintf(fp, "\n");
    
    // Write bounding boxes (640x480 space)
    fprintf(fp, "Bounding Boxes (640x480 space):\n");
    fprintf(fp, "---------------\n");
    fprintf(fp, "Format: x1, y1, x2, y2, confidence\n\n");
    
    for (size_t i = 0; i < result.boxes.size(); i++) {
        fprintf(fp, "Box %zu: %.2f, %.2f, %.2f, %.2f (conf: %.3f)\n",
                i + 1, result.boxes[i].x1, result.boxes[i].y1, 
                result.boxes[i].x2, result.boxes[i].y2, result.boxes[i].confidence);
    }
    
    // Write bounding boxes (original image space)
    if (!result.boxesOriginal.empty()) {
        fprintf(fp, "\nBounding Boxes (Original image space):\n");
        fprintf(fp, "---------------\n");
        fprintf(fp, "Format: x1, y1, x2, y2, confidence\n\n");
        
        for (size_t i = 0; i < result.boxesOriginal.size(); i++) {
            fprintf(fp, "Box %zu: %.2f, %.2f, %.2f, %.2f (conf: %.3f)\n",
                    i + 1, result.boxesOriginal[i].x1, result.boxesOriginal[i].y1, 
                    result.boxesOriginal[i].x2, result.boxesOriginal[i].y2, 
                    result.boxesOriginal[i].confidence);
        }
    }
    
    // Write sorted bounding boxes (by height)
    if (!result.boxesSorted.empty()) {
        fprintf(fp, "\nBounding Boxes (Sorted by height, descending):\n");
        fprintf(fp, "---------------\n");
        fprintf(fp, "Format: index, x1, y1, x2, y2, height, confidence\n\n");
        
        for (size_t i = 0; i < result.boxesSorted.size(); i++) {
            float height = result.boxesSorted[i].y2 - result.boxesSorted[i].y1;
            fprintf(fp, "#%03zu: %.2f, %.2f, %.2f, %.2f (height: %.2f, conf: %.3f)\n",
                    i + 1, result.boxesSorted[i].x1, result.boxesSorted[i].y1, 
                    result.boxesSorted[i].x2, result.boxesSorted[i].y2,
                    height, result.boxesSorted[i].confidence);
        }
    }
    
    fprintf(fp, "\nTotal boxes: %zu\n", result.boxes.size());
    
    fclose(fp);
    printf("Output saved to: %s\n", outputTxtPath.c_str());
    
    // Save visualization image (640x480 with heatmap)
    if (!outputImagePath.empty() && !result.visualizationImg.empty()) {
        bool success = cv::imwrite(outputImagePath, result.visualizationImg);
        if (success) {
            printf("Visualization saved to: %s\n", outputImagePath.c_str());
        } else {
            printf("Error: Failed to save visualization\n");
        }
    }
    
    // Save original image with boxes
    if (!outputOriginalImagePath.empty() && !result.visualizationOriginalImg.empty()) {
        bool success = cv::imwrite(outputOriginalImagePath, result.visualizationOriginalImg);
        if (success) {
            printf("Original image with boxes saved to: %s\n", outputOriginalImagePath.c_str());
        } else {
            printf("Error: Failed to save original image with boxes\n");
        }
    }
    
    // Save individual text images
    if (!outputTextDir.empty() && !result.textImages.empty()) {
        saveTextImages(result.textImages, outputTextDir);
    }
    
    return true;
}

void OcrDetComponent::sortBoxesByHeight(std::vector<TextBox>& boxesSorted)
{
    // Sort boxes by height in descending order
    std::sort(boxesSorted.begin(), boxesSorted.end(), 
        [](const TextBox& a, const TextBox& b) {
            float heightA = a.y2 - a.y1;
            float heightB = b.y2 - b.y1;
            return heightA > heightB;  // Descending order
        });
}

void OcrDetComponent::cropAndResizeTextRegions(const cv::Mat& originalImage,
                                                const std::vector<TextBox>& boxesOriginal,
                                                std::vector<TextImageInfo>& textImages,
                                                int targetWidth, int targetHeight)
{
    textImages.clear();
    
    // First, sort boxes by height
    std::vector<TextBox> sortedBoxes = boxesOriginal;
    sortBoxesByHeight(sortedBoxes);
    
    printf("\nCropping and resizing text regions...\n");
    printf("Input boxes: %zu\n", boxesOriginal.size());
    printf("Target size: %dx%d (letterbox scaling, no vertical black borders)\n", targetWidth, targetHeight);
    
    int validCount = 0;
    int invalidCount = 0;
    int totalImages = 0;
    
    for (size_t i = 0; i < sortedBoxes.size(); i++) {
        // Get box coordinates (ensure they are within image bounds)
        int x1 = std::max(0, (int)sortedBoxes[i].x1);
        int y1 = std::max(0, (int)sortedBoxes[i].y1);
        int x2 = std::min(originalImage.cols, (int)sortedBoxes[i].x2);
        int y2 = std::min(originalImage.rows, (int)sortedBoxes[i].y2);
        
        // Skip invalid boxes
        if (x2 <= x1 || y2 <= y1) {
            printf("  [SKIP] Box %zu: [%d,%d,%d,%d] - Invalid size (w:%d, h:%d)\n", 
                   i + 1, x1, y1, x2, y2, x2 - x1, y2 - y1);
            invalidCount++;
            continue;
        }
        
        validCount++;
        
        // Crop the region
        cv::Rect roi(x1, y1, x2 - x1, y2 - y1);
        cv::Mat cropped = originalImage(roi).clone();
        
        int cropWidth = cropped.cols;
        int cropHeight = cropped.rows;
        
        // Calculate scaling: always fit to height (no vertical black borders)
        float scale = (float)targetHeight / (float)cropHeight;
        int scaledWidth = (int)(cropWidth * scale);
        int scaledHeight = targetHeight;
        
        // Resize cropped image
        cv::Mat resized;
        cv::resize(cropped, resized, cv::Size(scaledWidth, scaledHeight), 0, 0, cv::INTER_AREA);
        
        printf("  Box %03zu: [%d,%d,%d,%d] (%dx%d) -> scaled:%dx%d\n", 
               validCount, x1, y1, x2, y2, cropWidth, cropHeight, scaledWidth, scaledHeight);
        
        // If scaled width <= targetWidth, add horizontal black borders and save as one image
        if (scaledWidth <= targetWidth) {
            cv::Mat letterboxImage(targetHeight, targetWidth, CV_8UC3, cv::Scalar(0, 0, 0));
            int offsetX = (targetWidth - scaledWidth) / 2;
            cv::Rect letterboxRoi(offsetX, 0, scaledWidth, scaledHeight);
            resized.copyTo(letterboxImage(letterboxRoi));
            
            textImages.push_back(TextImageInfo(letterboxImage, validCount, 1));
            totalImages++;
            
            printf("    -> Single image with left/right borders (offset: %d)\n", offsetX);
        } 
        // If scaled width > targetWidth, split into multiple segments with overlap
        else {
            int overlap = 96;  // Overlap 96 pixels (2x height) to avoid cutting characters
            int segmentWidth = targetWidth;
            int step = segmentWidth - overlap;
            
            int segmentCount = 0;
            for (int startX = 0; startX < scaledWidth; startX += step) {
                int endX = std::min(startX + segmentWidth, scaledWidth);
                int actualWidth = endX - startX;
                
                // Extract segment
                cv::Rect segmentRoi(startX, 0, actualWidth, targetHeight);
                cv::Mat segment = resized(segmentRoi).clone();
                
                // If segment is narrower than targetWidth, pad with black on the right
                cv::Mat segmentPadded;
                if (actualWidth < targetWidth) {
                    cv::Mat blackPad(targetHeight, targetWidth - actualWidth, CV_8UC3, cv::Scalar(0, 0, 0));
                    cv::hconcat(segment, blackPad, segmentPadded);
                } else {
                    segmentPadded = segment;
                }
                
                segmentCount++;
                totalImages++;
                textImages.push_back(TextImageInfo(segmentPadded, validCount, segmentCount));
                
                printf("    -> Segment %d: [%d-%d] (width: %d)\n", 
                       segmentCount, startX, endX, actualWidth);
                
                // If we've reached the end, break
                if (endX >= scaledWidth) {
                    break;
                }
            }
            
            printf("    -> Total segments: %d\n", segmentCount);
        }
    }
    
    printf("Valid boxes: %d, Invalid boxes: %d, Total output images: %d\n", 
           validCount, invalidCount, totalImages);
}

bool OcrDetComponent::saveTextImages(const std::vector<TextImageInfo>& textImages,
                                      const std::string& outputDir)
{
    if (textImages.empty()) {
        printf("Warning: No text images to save\n");
        return true;
    }
    
    printf("\nSaving %zu text images to %s...\n", textImages.size(), outputDir.c_str());
    
    for (size_t i = 0; i < textImages.size(); i++) {
        char filename[256];
        
        // Format: text_XXX_Y.png where XXX is box index, Y is segment index
        if (textImages[i].segmentIndex == 1 && i + 1 < textImages.size() && 
            textImages[i].boxIndex != textImages[i+1].boxIndex) {
            // Single segment for this box
            snprintf(filename, sizeof(filename), "%s/text_%03d.png", 
                    outputDir.c_str(), textImages[i].boxIndex);
        } else {
            // Multiple segments or part of multi-segment box
            snprintf(filename, sizeof(filename), "%s/text_%03d_%d.png", 
                    outputDir.c_str(), textImages[i].boxIndex, textImages[i].segmentIndex);
        }
        
        bool success = cv::imwrite(filename, textImages[i].image);
        if (!success) {
            printf("Error: Failed to save %s\n", filename);
            return false;
        }
    }
    
    printf("All text images saved successfully!\n");
    return true;
}
