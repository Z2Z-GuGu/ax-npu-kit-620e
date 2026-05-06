#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <vector>
#include <string>

#include "ocr_image.h"
#include "ocr_det.h"
#include "ocr_rec.h"

// 时间戳数组，最多支持 50 个步骤
#define MAX_STEPS 50
std::chrono::high_resolution_clock::time_point timestamps[MAX_STEPS];
std::vector<std::string> stepNames;  // 改用 vector 存储字符串
int currentStep = 0;

// 记录时间戳
void recordTime(const char* stepName) {
    if (currentStep < MAX_STEPS) {
        timestamps[currentStep] = std::chrono::high_resolution_clock::now();
        stepNames.push_back(std::string(stepName));  // 存储为 std::string
        currentStep++;
    }
}

// 打印所有步骤的耗时
void printTiming() {
    printf("\n");
    printf("==================================================\n");
    printf("Timing Report\n");
    printf("==================================================\n");
    
    for (int i = 1; i < currentStep; i++) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamps[i] - timestamps[i-1]).count();
        printf("[%s] Time: %lld ms\n", stepNames[i].c_str(), duration);
    }
    
    // 总耗时
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamps[currentStep-1] - timestamps[0]).count();
    printf("--------------------------------------------------\n");
    printf("[Total] Time: %lld ms\n", totalDuration);
    printf("==================================================\n");
}


int main(int argc, char** argv)
{
    // 检查参数
    if (argc < 3) {
        printf("  %s /root/models/pp_ocr/ch_PP_OCRv3_det_npu.axmodel ./test.png ./out.jpg\n", argv[0]);
        return -1;
    }

    const char* modelPath = argv[1];    // model path to .axmodel file
    const char* inputPath = argv[2];    // input image path
    const char* outputPath = (argc >= 4) ? argv[3] : "/tmp/heatmap_vis.jpg"; // output image path pattern

    // 记录开始时间
    recordTime("Start");
    
    // 步骤 1: 加载 OcrDetNPU 模型
    printf("Step 1: Loading OCR detection model to NPU...\n");
    OcrDetNPU detector(modelPath);
    
    if (!detector.isModelLoaded()) {
        printf("Error: Failed to load model!\n");
        return -1;
    }
    printf("Model loaded successfully!\n");
    recordTime("Step 1 - Load Model");
    printf("\n");

    // 步骤 2: 加载图像并转换为 BGR
    printf("Step 2: Loading image and converting to BGR...\n");
    OCRImage image(inputPath);
    
    if (!image.isLoaded()) {
        printf("Error: Failed to load image!\n");
        return -1;
    }
    
    printf("Image loaded successfully!\n");
    printf("  Size: %dx%d\n", image.getWidth(), image.getHeight());
    recordTime("Step 2 - Load Image");
    printf("\n");

    // 步骤 3: 获取 BGR 图像
    printf("Step 3: Getting BGR image structure...\n");
    BGRImage bgrImage = image.getImage();
    printf("BGR image: %dx%d, %d channels\n", 
           bgrImage.width, bgrImage.height, bgrImage.channels);
    recordTime("Step 3 - Get BGR Image");
    printf("\n");

    // 步骤 4: 计算分块裁剪参数
    printf("Step 4: Calculating tile crop regions...\n");
    
    // 参数设置：缩小倍数 2，重叠区域 20 像素，输出尺寸 640x480
    float scaleFactor = 2.0f;
    int overlapPixels = 20;
    int targetWidth = 640;
    int targetHeight = 480;
    
    // 计算裁剪区域
    std::vector<CropRegion> cropRegions = calculateCropRegions(
        bgrImage.width, bgrImage.height,
        scaleFactor, overlapPixels,
        targetWidth, targetHeight
    );
    
    if (cropRegions.empty()) {
        printf("Error: No valid crop regions generated!\n");
        return -1;
    }
    
    printf("Total crop regions: %zu\n", cropRegions.size());
    recordTime("Step 4 - Calculate Crop Regions");
    printf("\n");

    // 步骤 5: 对每个裁剪区域运行 NPU 推理
    printf("Step 5: Running NPU inference on each crop region...\n");
    std::vector<cv::Mat> heatmaps;
    
    for (size_t i = 0; i < cropRegions.size(); i++) {
        const CropRegion& region = cropRegions[i];
        printf("\nProcessing region %zu/%zu [%d,%d,%d,%d]...\n", 
               i + 1, cropRegions.size(), 
               region.x1, region.y1, region.x2, region.y2);
        
        // 记录裁剪开始时间
        recordTime("Step 5 - Crop Start");
        
        // 裁剪图像
        BGRImage croppedImage = cropAndResize(
            bgrImage,
            region.x1, region.y1, region.x2, region.y2,
            targetWidth, targetHeight
        );
        
        if (!croppedImage.isValid()) {
            printf("  Warning: Failed to crop region %zu, skipping...\n", i);
            continue;
        }
        
        // 记录裁剪完成时间
        recordTime(("Step 5." + std::to_string(i + 1) + " - Crop").c_str());
        
        printf("  Cropped image: %dx%d\n", croppedImage.width, croppedImage.height);
        
        // 运行 NPU 推理
        cv::Mat heatmap = detector.detect(croppedImage, targetWidth, targetHeight);
        
        if (heatmap.empty()) {
            printf("  Warning: Failed to generate heatmap for region %zu, skipping...\n", i);
            continue;
        }
        
        // 记录 NPU 推理完成时间
        recordTime(("Step 5." + std::to_string(i + 1) + " - NPU Inference").c_str());
        
        heatmaps.push_back(heatmap);
        printf("  Heatmap generated: %dx%d, type=CV_32FC1\n", heatmap.cols, heatmap.rows);
    }
    
    printf("\nTotal heatmaps generated: %zu\n", heatmaps.size());
    recordTime("Step 5 - NPU Inference");
    printf("\n");

    // 步骤 6: 将所有热力图拼接为原图尺寸（获取热力图数据）
    printf("Step 6: Merging heatmaps to original size...\n");
    HeatmapData mergedHeatmap = mergeHeatmaps(bgrImage, heatmaps, cropRegions);
    
    if (!mergedHeatmap.checkValid()) {
        printf("Error: Failed to merge heatmaps!\n");
        return -1;
    }
    
    printf("Merged heatmap: %dx%d, channels=%d\n", 
           mergedHeatmap.width, mergedHeatmap.height, mergedHeatmap.channels);
    recordTime("Step 6 - Merge Heatmaps");
    printf("\n");
    
    // 步骤 7: 从热力图提取边界框
    printf("Step 7: Extracting bounding boxes from heatmap...\n");
    std::vector<BoundingBox> boxes = extractBoxesFromHeatmap(mergedHeatmap, 0.1f, 500, 70);
    
    // 打印所有方框信息
    printf("  Detected %zu bounding boxes:\n", boxes.size());
    for (size_t i = 0; i < boxes.size(); i++) {
        printf("    Box %zu: [%d, %d, %d, %d], score=%.3f\n",
               i, boxes[i].x1, boxes[i].y1, boxes[i].x2, boxes[i].y2, boxes[i].score);
    }
    recordTime("Step 7 - Extract Bounding Boxes");
    printf("\n");
    
    // 步骤 8: 将热力图可视化（带方框）
    printf("Step 8: Visualizing merged heatmap with bounding boxes...\n");
    BGRImage visImage = visualizeMergedHeatmap(bgrImage, mergedHeatmap, 0.5f, boxes);
    recordTime("Step 8 - Visualize");
    printf("\n");
    
    // 步骤 9: 保存可视化结果
    printf("Step 9: Saving visualization result...\n");
    if (saveImage(visImage, outputPath)) {
        printf("  Saved: %s\n", outputPath);
    } else {
        printf("  Warning: Failed to save %s\n", outputPath);
    }
    recordTime("Step 9 - Save Image");
    printf("\n");
    
    // 步骤 10: 对象离开作用域，自动释放 NPU 资源和热力图数据
    printf("Step 10: Destroying objects...\n");
    // detector, image, heatmaps, mergedHeatmap, boxes 等对象会自动调用析构函数
    printf("All objects destroyed, resources released automatically\n");
    recordTime("Step 10 - Cleanup");
    printf("\n");

    printf("========================================\n");
    printf("Test completed successfully!\n");
    printf("========================================\n");
    
    // 打印时间统计
    printTiming();

    return 0;
}
