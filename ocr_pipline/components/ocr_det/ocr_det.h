#ifndef OCR_DET_H
#define OCR_DET_H

#include <string>
#include <vector>
#include <memory>
#include "ocr_image.h"

// AXERA SDK headers
#include "ax_engine_api.h"
#include "ax_sys_api.h"

#define OCR_DET_CMM_ALIGN_SIZE 128
#define OCR_DET_CMM_SESSION_NAME "npu"

/**
 * @brief 图像裁剪区域结构体
 * 
 * 存储裁剪区域的坐标和索引信息
 */
struct CropRegion {
    int index;      ///< 裁剪块索引（从 0 开始）
    int x1;         ///< 左上角 x 坐标
    int y1;         ///< 左上角 y 坐标
    int x2;         ///< 右下角 x 坐标
    int y2;         ///< 右下角 y 坐标
    int width;      ///< 裁剪区域宽度
    int height;     ///< 裁剪区域高度
    
    /**
     * @brief 默认构造函数
     */
    CropRegion() : index(0), x1(0), y1(0), x2(0), y2(0), width(0), height(0) {}
    
    /**
     * @brief 构造函数
     */
    CropRegion(int idx, int x1, int y1, int x2, int y2)
        : index(idx), x1(x1), y1(y1), x2(x2), y2(y2), 
          width(x2 - x1), height(y2 - y1) {}
    
    /**
     * @brief 检查区域是否有效
     */
    bool isValid() const {
        return x1 < x2 && y1 < y2 && width > 0 && height > 0;
    }
};

/**
 * @brief 热力图数据结构体
 * 
 * 封装热力图数据，包含尺寸、通道信息和数据指针
 */
struct HeatmapData {
    int width;          ///< 热力图宽度
    int height;         ///< 热力图高度
    int channels;       ///< 通道数（固定为 1）
    float* data;        ///< 数据指针（float32 类型）
    bool isValid;       ///< 数据是否有效
    
    /**
     * @brief 默认构造函数
     */
    HeatmapData() : width(0), height(0), channels(1), data(nullptr), isValid(false) {}
    
    /**
     * @brief 构造函数
     */
    HeatmapData(int w, int h, float* d) 
        : width(w), height(h), channels(1), data(d), isValid(d != nullptr) {}
    
    /**
     * @brief 析构函数
     */
    ~HeatmapData() {
        if (data && isValid) {
            delete[] data;
            data = nullptr;
        }
    }
    
    /**
     * @brief 检查数据是否有效
     */
    bool checkValid() const {
        return isValid && data != nullptr && width > 0 && height > 0;
    }
};

/**
 * @brief 边界框结构体
 * 
 * 存储检测到的文本区域的边界框坐标
 */
struct BoundingBox {
    int x1;         ///< 左上角 x 坐标
    int y1;         ///< 左上角 y 坐标
    int x2;         ///< 右下角 x 坐标
    int y2;         ///< 右下角 y 坐标
    float score;    ///< 置信度得分（可选）
    
    /**
     * @brief 默认构造函数
     */
    BoundingBox() : x1(0), y1(0), x2(0), y2(0), score(0.0f) {}
    
    /**
     * @brief 构造函数
     */
    BoundingBox(int x1, int y1, int x2, int y2, float score = 0.0f)
        : x1(x1), y1(y1), x2(x2), y2(y2), score(score) {}
    
    /**
     * @brief 检查方框是否有效
     */
    bool isValid() const {
        return x1 < x2 && y1 < y2;
    }
    
    /**
     * @brief 计算方框面积
     */
    int area() const {
        return (x2 - x1) * (y2 - y1);
    }
};

/**
 * @brief 计算图像分块裁剪参数
 * 
 * 将图像按指定倍数缩小后，分割为多个固定尺寸的重叠块。
 * 返回每个块在原图中的裁剪区域坐标。
 * 
 * @param imageWidth 原图宽度
 * @param imageHeight 原图高度
 * @param scaleFactor 缩小倍数（>1 表示缩小）
 * @param overlapPixels 重叠区域像素
 * @param targetWidth 目标输出宽度
 * @param targetHeight 目标输出高度
 * @return std::vector<CropRegion> 裁剪区域参数列表
 */
std::vector<CropRegion> calculateCropRegions(
    int imageWidth,
    int imageHeight,
    float scaleFactor,
    int overlapPixels,
    int targetWidth,
    int targetHeight
);

/**
 * @brief 将热力图可视化并叠加到原图上
 * 
 * 将热力图转换为伪彩色图，然后与原图叠加，生成可视化结果。
 * 
 * @param originalImage 原始 BGR 图像
 * @param heatmap 热力图（CV_32FC1，单通道浮点型）
 * @param cropRegion 裁剪区域信息（用于定位热力图在原图中的位置）
 * @param alpha 叠加透明度（0.0-1.0，默认 0.5）
 * @return BGRImage 叠加后的 BGR 图像
 */
BGRImage visualizeHeatmap(
    const BGRImage& originalImage,
    const cv::Mat& heatmap,
    const CropRegion& cropRegion,
    float alpha = 0.5f
);

/**
 * @brief 将多个热力图拼接为原图尺寸
 * 
 * 将多个裁剪区域的热力图拼接回原图尺寸，返回完整的合并热力图。
 * 
 * @param originalImage 原始 BGR 图像（用于获取尺寸信息）
 * @param heatmaps 热力图列表（CV_32FC1，单通道浮点型）
 * @param cropRegions 裁剪区域列表（与 heatmaps 一一对应）
 * @return HeatmapData 合并后的热力图数据（原图尺寸）
 */
HeatmapData mergeHeatmaps(
    const BGRImage& originalImage,
    const std::vector<cv::Mat>& heatmaps,
    const std::vector<CropRegion>& cropRegions
);

/**
 * @brief 将热力图可视化并叠加到原图上
 * 
 * 将合并后的热力图转换为伪彩色图，然后与原图叠加，生成可视化结果。
 * 
 * @param originalImage 原始 BGR 图像
 * @param mergedHeatmap 合并后的热力图数据
 * @param alpha 叠加透明度（0.0-1.0，默认 0.5）
 * @param boxes 边界框列表（可选，用于绘制绿色方框）
 * @return BGRImage 叠加后的 BGR 图像（原图尺寸）
 */
BGRImage visualizeMergedHeatmap(
    const BGRImage& originalImage,
    const HeatmapData& mergedHeatmap,
    float alpha = 0.5f,
    const std::vector<BoundingBox>& boxes = std::vector<BoundingBox>()
);

/**
 * @brief 从热力图中提取边界框
 * 
 * 使用连通域分析，从热力图中提取所有高亮区域，并为每个区域生成一个边界框。
 * 
 * @param heatmap 热力图数据
 * @param threshold 二值化阈值（大于此值的像素被认为是前景）
 * @param minArea 最小面积（小于此面积的框会被过滤掉）
 * @param expandPercent 扩展比例（0-100，表示在垂直方向扩展的百分比，例如 30 表示扩展到 130%）
 * @return std::vector<BoundingBox> 边界框列表
 */
std::vector<BoundingBox> extractBoxesFromHeatmap(
    const HeatmapData& heatmap,
    float threshold = 0.1f,
    int minArea = 50,
    int expandPercent = 0
);

/**
 * @brief OCR 检测 NPU 类
 * 
 * 使用 AXERA NPU 进行 OCR 文本检测
 * 功能：
 * 1. 构造函数加载模型到 NPU
 * 2. detect 函数输入 BGRImage，输出热力图
 * 3. 析构函数释放 NPU 资源
 */
class OcrDetNPU {
public:
    /**
     * @brief 构造函数 - 加载模型到 NPU
     * @param modelPath 模型文件路径（.axmodel）
     */
    explicit OcrDetNPU(const std::string& modelPath);
    
    /**
     * @brief 析构函数 - 释放 NPU 资源
     */
    ~OcrDetNPU();
    
    /**
     * @brief 检测文本区域，输出热力图
     * @param image 输入 BGR 图像
     * @param targetWidth 目标宽度（默认 640）
     * @param targetHeight 目标高度（默认 480）
     * @return cv::Mat 热力图（CV_32FC1，单通道浮点型，值范围 0-1）
     */
    cv::Mat detect(const BGRImage& image, int targetWidth = 640, int targetHeight = 480);
    
    /**
     * @brief 检查模型是否成功加载
     * @return true 模型已加载，false 加载失败
     */
    bool isModelLoaded() const;
    
    /**
     * @brief 检查文件是否存在
     * @param filepath 文件路径
     * @return true 文件存在，false 文件不存在
     */
    static bool fileExists(const std::string& filepath);

private:
    bool modelLoaded;
    std::string modelPath;
    AX_ENGINE_HANDLE handle;
    
    /**
     * @brief 准备 IO 缓冲区
     */
    bool prepareIO(AX_ENGINE_IO_INFO_T* ioInfo, AX_ENGINE_IO_T* ioData);
    
    /**
     * @brief 释放 IO 缓冲区
     */
    void freeIO(AX_ENGINE_IO_T* ioData);
    
    /**
     * @brief 预处理图像（letterbox 缩放到目标尺寸）
     */
    cv::Mat preprocessImage(const BGRImage& image, int targetWidth, int targetHeight);
};

#endif // OCR_DET_H
