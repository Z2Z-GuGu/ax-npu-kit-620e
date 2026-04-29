/**************************************************************************************************
 *
 * OCR Text Detection Model Tester
 * 
 * Load PaddleOCR detection model (ch_PP_OCRv3_det_npu.axmodel)
 * Input: 640x480 BGR image
 * Output: Raw model output to text file for analysis
 *
 * Model Info:
 *   Input:  [1, 480, 640, 3] NHWC, uint8, BGR
 *   Output: [1, 1, 480, 640] float32 (heatmap/sigmoid output)
 *
 **************************************************************************************************/

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>

// AXERA SDK headers
#include "ax_engine_api.h"
#include "ax_sys_api.h"

#define AX_CMM_ALIGN_SIZE 128
const char* AX_CMM_SESSION_NAME = "npu";

// Bounding box expansion ratio (15% by default)
const float BBOX_EXPANSION_RATIO = 0.15f;

// Check if file exists
bool FileExists(const char* filename)
{
    FILE* fp = fopen(filename, "r");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

// Load model file to buffer
AX_BOOL LoadModel(const char* modelPath, std::vector<char>& modelBuffer)
{
    FILE* fp = fopen(modelPath, "rb");
    if (!fp) {
        printf("Error: Cannot open model file: %s\n", modelPath);
        return AX_FALSE;
    }
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    modelBuffer.resize(size);
    
    if (fread(modelBuffer.data(), 1, size, fp) != (size_t)size) {
        printf("Error: Cannot read model file\n");
        fclose(fp);
        return AX_FALSE;
    }
    
    fclose(fp);
    printf("Model loaded: %s (%ld bytes)\n", modelPath, size);
    return AX_TRUE;
}

// Free IO buffers
void FreeIO(AX_ENGINE_IO_T* io)
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

// Prepare IO buffers
AX_BOOL PrepareIO(AX_ENGINE_IO_INFO_T* ioInfo, AX_ENGINE_IO_T* ioData)
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
            AX_CMM_ALIGN_SIZE,
            (const AX_S8*)AX_CMM_SESSION_NAME
        );
        
        if (ret != 0) {
            printf("Error: Failed to allocate input buffer %d (size: %u)\n", i, meta.nSize);
            FreeIO(ioData);
            return AX_FALSE;
        }
        
        printf("Input buffer %d: phy=0x%lx, vir=%p, size=%u\n", 
               i, (unsigned long)buffer.phyAddr, buffer.pVirAddr, meta.nSize);
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
            AX_CMM_ALIGN_SIZE,
            (const AX_S8*)AX_CMM_SESSION_NAME
        );
        
        if (ret != 0) {
            printf("Error: Failed to allocate output buffer %d (size: %u)\n", i, meta.nSize);
            FreeIO(ioData);
            return AX_FALSE;
        }
        
        printf("Output buffer %d: phy=0x%lx, vir=%p, size=%u\n", 
               i, (unsigned long)buffer.phyAddr, buffer.pVirAddr, meta.nSize);
    }
    
    return AX_TRUE;
}

// Save raw output to text file
AX_BOOL SaveOutputToTxt(const char* outputPath, const AX_VOID* outputData, 
                        const AX_U32* shape, AX_U32 shapeSize)
{
    FILE* fp = fopen(outputPath, "w");
    if (!fp) {
        printf("Error: Cannot create output file: %s\n", outputPath);
        return AX_FALSE;
    }
    
    // Write header
    fprintf(fp, "OCR Detection Model Output\n");
    fprintf(fp, "==========================\n\n");
    fprintf(fp, "Output Shape: [");
    for (AX_U32 i = 0; i < shapeSize; i++) {
        fprintf(fp, "%d", (int)shape[i]);
        if (i < shapeSize - 1) fprintf(fp, ", ");
    }
    fprintf(fp, "]\n");
    fprintf(fp, "Data Type: float32\n");
    fprintf(fp, "Total Elements: %d\n\n", shape[0] * shape[1] * shape[2] * shape[3]);
    
    // Cast to float
    const float* data = (const float*)outputData;
    AX_U32 totalElements = shape[0] * shape[1] * shape[2] * shape[3];
    
    // Write statistics
    float minVal = data[0];
    float maxVal = data[0];
    float sumVal = 0.0f;
    
    for (AX_U32 i = 0; i < totalElements; i++) {
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
    
    // Write sample values (first 100 elements)
    fprintf(fp, "Sample Values (first 100 elements):\n");
    fprintf(fp, "-----------------------------------\n");
    for (AX_U32 i = 0; i < std::min((AX_U32)100, totalElements); i++) {
        fprintf(fp, "[%5d] = %.6f\n", i, data[i]);
    }
    fprintf(fp, "\n");
    
    // Write 2D slice (for visualization)
    // Output shape is [1, 1, 480, 640], so we write the 480x640 heatmap
    fprintf(fp, "Heatmap Data (480 rows x 640 cols):\n");
    fprintf(fp, "-----------------------------------\n");
    fprintf(fp, "Format: Each row shows min/max/mean for that row\n\n");
    
    const AX_U32 height = shape[2];
    const AX_U32 width = shape[3];
    
    for (AX_U32 h = 0; h < height; h++) {
        float rowMin = data[h * width];
        float rowMax = data[h * width];
        float rowSum = 0.0f;
        
        for (AX_U32 w = 0; w < width; w++) {
            float val = data[h * width + w];
            if (val < rowMin) rowMin = val;
            if (val > rowMax) rowMax = val;
            rowSum += val;
        }
        
        float rowMean = rowSum / width;
        fprintf(fp, "Row %3d: Min=%.4f, Max=%.4f, Mean=%.4f\n", 
                h, rowMin, rowMax, rowMean);
    }
    fprintf(fp, "\n");
    
    // Find high-confidence regions (potential text areas)
    fprintf(fp, "High-Confidence Regions (value > 0.5):\n");
    fprintf(fp, "--------------------------------------\n");
    
    int count = 0;
    const float threshold = 0.5f;
    
    for (AX_U32 h = 0; h < height; h++) {
        for (AX_U32 w = 0; w < width; w++) {
            float val = data[h * width + w];
            if (val > threshold) {
                fprintf(fp, "  (%4d, %4d): %.4f\n", w, h, val);
                count++;
                if (count >= 100) { // Limit to first 100 points
                    fprintf(fp, "  ... (more points omitted)\n");
                    goto done_points;
                }
            }
        }
    }
    
    if (count == 0) {
        fprintf(fp, "  No points with confidence > %.2f\n", threshold);
    } else {
        fprintf(fp, "\nTotal high-confidence points: %d\n", count);
    }
    
done_points:
    fclose(fp);
    printf("Output saved to: %s\n", outputPath);
    return AX_TRUE;
}

// Save heatmap as image for visualization (overlay on original image with green semi-transparent + red bounding boxes)
AX_BOOL SaveHeatmapAsImage(const char* imagePath, const float* data, 
                           AX_U32 height, AX_U32 width, const char* originalImagePath,
                           const std::vector<std::vector<float>>& boxes = std::vector<std::vector<float>>())
{
    // Load original image
    cv::Mat originalImage = cv::imread(originalImagePath);
    if (originalImage.empty()) {
        printf("Error: Cannot load original image: %s\n", originalImagePath);
        return AX_FALSE;
    }
    
    // Resize original image to match heatmap size if needed
    if (originalImage.cols != (int)width || originalImage.rows != (int)height) {
        cv::resize(originalImage, originalImage, cv::Size(width, height), 0, 0, cv::INTER_AREA);
    }
    
    // Normalize heatmap to 0-1
    float minVal = data[0];
    float maxVal = data[0];
    
    for (AX_U32 i = 0; i < height * width; i++) {
        if (data[i] < minVal) minVal = data[i];
        if (data[i] > maxVal) maxVal = data[i];
    }
    
    float range = maxVal - minVal;
    if (range < 0.0001f) range = 1.0f;
    
    // Create green overlay mask
    cv::Mat overlay = originalImage.clone();
    
    // Draw green semi-transparent overlay based on confidence
    for (AX_U32 h = 0; h < height; h++) {
        for (AX_U32 w = 0; w < width; w++) {
            float confidence = (data[h * width + w] - minVal) / range;
            
            // Only draw overlay for high confidence areas (> 0.15)
            if (confidence > 0.15f) {
                // Green color with intensity based on confidence
                // BGR format: (0, 255 * confidence, 0)
                uchar intensity = (uchar)(255 * confidence);
                overlay.at<cv::Vec3b>(h, w) = cv::Vec3b(0, intensity, 0);
            }
        }
    }
    
    // Blend original image with overlay (alpha = 0.5 for semi-transparent)
    float alpha = 0.5f;
    cv::Mat blended;
    cv::addWeighted(overlay, alpha, originalImage, 1.0f - alpha, 0.0f, blended);
    
    // Draw red bounding boxes (expanded)
    if (!boxes.empty()) {
        for (size_t i = 0; i < boxes.size(); i++) {
            // Expand shorter side by 10%, longer side expansion equals shorter side expansion
            float boxWidth = boxes[i][2] - boxes[i][0];
            float boxHeight = boxes[i][3] - boxes[i][1];
            
            // Determine which side is shorter
            float expandAmount = (boxWidth < boxHeight) ? (boxWidth * BBOX_EXPANSION_RATIO) : (boxHeight * BBOX_EXPANSION_RATIO);
            float expandX = expandAmount;
            float expandY = expandAmount;
            
            cv::Point pt1(boxes[i][0] - expandX, boxes[i][1] - expandY);
            cv::Point pt2(boxes[i][2] + expandX, boxes[i][3] + expandY);
            
            // Draw red rectangle (thickness: 1px)
            cv::rectangle(blended, pt1, pt2, cv::Scalar(0, 0, 255), 1);
        }
        printf("  - Red boxes: %zu text regions detected\n", boxes.size());
    }
    
    // Save result
    bool success = cv::imwrite(imagePath, blended);
    if (success) {
        printf("Heatmap overlay saved to: %s\n", imagePath);
        printf("  - Green areas indicate text confidence\n");
        printf("  - Darker green = higher confidence\n");
        printf("  - Overlay transparency: 50%%\n");
        printf("  - Confidence threshold: 0.15 (showing more area)\n");
    } else {
        printf("Error: Failed to save heatmap overlay image\n");
    }
    
    return success ? AX_TRUE : AX_FALSE;
}

// Convert heatmap to bounding boxes using threshold + contour detection
AX_BOOL HeatmapToBoundingBoxes(const float* data, AX_U32 height, AX_U32 width,
                               std::vector<std::vector<float>>& boxes,
                               float threshold = 0.15f, float minArea = 50.0f)
{
    // Normalize heatmap to 0-255
    float minVal = data[0];
    float maxVal = data[0];
    
    for (AX_U32 i = 0; i < height * width; i++) {
        if (data[i] < minVal) minVal = data[i];
        if (data[i] > maxVal) maxVal = data[i];
    }
    
    float range = maxVal - minVal;
    if (range < 0.0001f) range = 1.0f;
    
    // Create binary image
    cv::Mat binary(height, width, CV_8UC1);
    for (AX_U32 h = 0; h < height; h++) {
        for (AX_U32 w = 0; w < width; w++) {
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
            // Format: [x1, y1, x2, y2, confidence]
            std::vector<float> box(5);
            box[0] = (float)rect.x;
            box[1] = (float)rect.y;
            box[2] = (float)(rect.x + rect.width);
            box[3] = (float)(rect.y + rect.height);
            
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
            box[4] = (count > 0) ? (sumConf / count) : 0.0f;
            
            boxes.push_back(box);
            
            printf("Box %zu: [%d, %d, %d, %d] (confidence: %.3f)\n", 
                   i + 1, rect.x, rect.y, rect.x + rect.width, rect.y + rect.height, box[4]);
        }
    }
    
    printf("Total boxes detected: %zu\n", boxes.size());
    return (boxes.size() > 0) ? AX_TRUE : AX_FALSE;
}

void print_usage(const char* program_name)
{
    printf("Usage: %s <model_path> <input_image> <output_txt> [output_heatmap_image]\n", program_name);
    printf("\n");
    printf("Arguments:\n");
    printf("  model_path          - Path to .axmodel file\n");
    printf("  input_image         - Input image (640x480, will be resized if needed)\n");
    printf("  output_txt          - Output text file for model output\n");
    printf("  output_heatmap_img  - (Optional) Output heatmap image for visualization\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s model.axmodel input.jpg output.txt heatmap.png\n", program_name);
    printf("\n");
    printf("Note: This tool analyzes the raw output of OCR detection models.\n");
    printf("      The output is a heatmap (confidence map) showing text regions.\n");
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        print_usage(argv[0]);
        return -1;
    }

    const char* modelPath = argv[1];
    const char* inputPath = argv[2];
    const char* outputTxtPath = argv[3];
    const char* outputHeatmapPath = (argc >= 5) ? argv[4] : nullptr;
    
    printf("========================================\n");
    printf("OCR Text Detection Model Tester\n");
    printf("========================================\n");
    printf("Model: %s\n", modelPath);
    printf("Input: %s\n", inputPath);
    printf("Output TXT: %s\n", outputTxtPath);
    if (outputHeatmapPath) {
        printf("Output Heatmap: %s\n", outputHeatmapPath);
    }
    printf("\n");

    // Check files
    if (!FileExists(modelPath)) {
        printf("Error: Model file not found: %s\n", modelPath);
        return -1;
    }
    
    if (!FileExists(inputPath)) {
        printf("Error: Input image not found: %s\n", inputPath);
        return -1;
    }

    // Load model
    std::vector<char> modelBuffer;
    if (!LoadModel(modelPath, modelBuffer)) {
        return -1;
    }

    // Initialize AXERA SDK
    AX_S32 ret = AX_SYS_Init();
    if (ret != 0) {
        printf("Error: AX_SYS_Init failed: 0x%x\n", ret);
        return -1;
    }
    
    // Initialize ENGINE
    AX_ENGINE_NPU_ATTR_T npuAttr;
    memset(&npuAttr, 0, sizeof(AX_ENGINE_NPU_ATTR_T));
    npuAttr.eHardMode = AX_ENGINE_VIRTUAL_NPU_DISABLE;
    ret = AX_ENGINE_Init(&npuAttr);
    if (ret != 0) {
        printf("Error: AX_ENGINE_Init failed: 0x%x\n", ret);
        AX_SYS_Deinit();
        return -1;
    }
    
    // Create handle with model buffer
    AX_ENGINE_HANDLE handle = 0;
    ret = AX_ENGINE_CreateHandle(&handle, modelBuffer.data(), modelBuffer.size());
    if (ret != 0 || !handle) {
        printf("Error: AX_ENGINE_CreateHandle failed: 0x%x\n", ret);
        AX_ENGINE_Deinit();
        AX_SYS_Deinit();
        return -1;
    }
    
    // Create context
    ret = AX_ENGINE_CreateContext(handle);
    if (ret != 0) {
        printf("Error: AX_ENGINE_CreateContext failed: 0x%x\n", ret);
        AX_ENGINE_DestroyHandle(handle);
        AX_ENGINE_Deinit();
        AX_SYS_Deinit();
        return -1;
    }

    // Get IO info
    AX_ENGINE_IO_INFO_T* ioInfo = nullptr;
    ret = AX_ENGINE_GetIOInfo(handle, &ioInfo);
    if (ret != 0 || !ioInfo) {
        printf("Error: AX_ENGINE_GetIOInfo failed: 0x%x\n", ret);
        AX_ENGINE_DestroyHandle(handle);
        AX_ENGINE_Deinit();
        AX_SYS_Deinit();
        return -1;
    }

    printf("\nModel IO Info:\n");
    printf("  Inputs: %u\n", ioInfo->nInputSize);
    printf("  Outputs: %u\n", ioInfo->nOutputSize);
    
    // Print input info
    printf("\nInput Tensor:\n");
    printf("  Name: %s\n", ioInfo->pInputs[0].pName);
    printf("  Shape: [");
    for (AX_U32 i = 0; i < ioInfo->pInputs[0].nShapeSize; i++) {
        printf("%d", (int)ioInfo->pInputs[0].pShape[i]);
        if (i < ioInfo->pInputs[0].nShapeSize - 1) printf(", ");
    }
    printf("]\n");
    
    // Print output info
    printf("\nOutput Tensor:\n");
    printf("  Name: %s\n", ioInfo->pOutputs[0].pName);
    printf("  Shape: [");
    for (AX_U32 i = 0; i < ioInfo->pOutputs[0].nShapeSize; i++) {
        printf("%d", (int)ioInfo->pOutputs[0].pShape[i]);
        if (i < ioInfo->pOutputs[0].nShapeSize - 1) printf(", ");
    }
    printf("]\n");
    printf("\n");

    // Load and prepare input image
    printf("Loading input image: %s\n", inputPath);
    cv::Mat srcImage = cv::imread(inputPath, cv::IMREAD_COLOR);
    if (srcImage.empty()) {
        printf("Error: Failed to load image\n");
        AX_ENGINE_DestroyHandle(handle);
        AX_ENGINE_Deinit();
        AX_SYS_Deinit();
        return -1;
    }
    
    printf("Original image size: %dx%d\n", srcImage.cols, srcImage.rows);
    
    // Resize to 640x480 if needed
    if (srcImage.cols != 640 || srcImage.rows != 480) {
        printf("Resizing to 640x480...\n");
        cv::resize(srcImage, srcImage, cv::Size(640, 480), 0, 0, cv::INTER_AREA);
    }
    
    // Prepare IO buffers (allocate physical memory)
    printf("\nAllocating IO buffers...\n");
    AX_ENGINE_IO_T ioData;
    if (!PrepareIO(ioInfo, &ioData)) {
        AX_ENGINE_DestroyHandle(handle);
        AX_ENGINE_Deinit();
        AX_SYS_Deinit();
        return -1;
    }
    
    // Copy image data to input buffer
    printf("Copying image data to input buffer...\n");
    AX_U8* inputData = (AX_U8*)srcImage.data;
    AX_U32 inputSize = 640 * 480 * 3;
    memcpy(ioData.pInputs[0].pVirAddr, inputData, inputSize);
    
    printf("Input data size: %u bytes\n", inputSize);

    // Run inference
    printf("\nRunning inference...\n");
    ret = AX_ENGINE_RunSync(handle, &ioData);
    if (ret != 0) {
        printf("Error: AX_ENGINE_RunSync failed: 0x%x\n", ret);
        FreeIO(&ioData);
        AX_ENGINE_DestroyHandle(handle);
        AX_ENGINE_Deinit();
        AX_SYS_Deinit();
        return -1;
    }
    printf("Inference completed!\n\n");

    // Get output data
    float* outputData = (float*)ioData.pOutputs[0].pVirAddr;
    AX_U32 outputHeight = ioInfo->pOutputs[0].pShape[2]; // 480
    AX_U32 outputWidth = ioInfo->pOutputs[0].pShape[3];  // 640
    
    // Save output to text file
    AX_U32 outputShape[4] = {1, 1, outputHeight, outputWidth};
    if (!SaveOutputToTxt(outputTxtPath, outputData, outputShape, 4)) {
        FreeIO(&ioData);
        AX_ENGINE_DestroyHandle(handle);
        AX_ENGINE_Deinit();
        AX_SYS_Deinit();
        return -1;
    }
    
    // Convert heatmap to bounding boxes
    std::vector<std::vector<float>> boxes;
    if (HeatmapToBoundingBoxes(outputData, outputHeight, outputWidth, boxes)) {
        printf("\n=== Bounding Boxes Summary ===\n");
        printf("Detected %zu text regions\n", boxes.size());
        
        // Save boxes to a separate file
        char boxPath[512];
        snprintf(boxPath, sizeof(boxPath), "%s.boxes.txt", outputTxtPath);
        FILE* boxFile = fopen(boxPath, "w");
        if (boxFile) {
            fprintf(boxFile, "# OCR Detection Bounding Boxes\n");
            fprintf(boxFile, "# Format: x1, y1, x2, y2, confidence\n");
            fprintf(boxFile, "# Image size: 640x480\n\n");
            
            for (size_t i = 0; i < boxes.size(); i++) {
                fprintf(boxFile, "Box %zu: %.2f, %.2f, %.2f, %.2f (conf: %.3f)\n",
                        i + 1, boxes[i][0], boxes[i][1], boxes[i][2], boxes[i][3], boxes[i][4]);
            }
            fclose(boxFile);
            printf("Boxes saved to: %s\n", boxPath);
        }
        
        // Optionally save heatmap visualization with bounding boxes
        if (outputHeatmapPath) {
            SaveHeatmapAsImage(outputHeatmapPath, outputData, outputHeight, outputWidth, inputPath, boxes);
        }
    } else {
        // No boxes detected, save heatmap without boxes
        if (outputHeatmapPath) {
            SaveHeatmapAsImage(outputHeatmapPath, outputData, outputHeight, outputWidth, inputPath);
        }
    }
    
    // Cleanup
    printf("\nCleaning up...\n");
    FreeIO(&ioData);
    AX_ENGINE_DestroyHandle(handle);
    AX_ENGINE_Deinit();
    AX_SYS_Deinit();
    
    printf("\n========================================\n");
    printf("Done!\n");
    printf("========================================\n");

    return 0;
}
