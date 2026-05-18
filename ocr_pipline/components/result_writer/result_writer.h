#ifndef RESULT_WRITER_H
#define RESULT_WRITER_H

#include <string>
#include <vector>
#include "ocr_det.h"
#include "ocr_rec.h"

/**
 * @brief 将 OCR 识别结果格式化为 JSON 字符串
 * 
 * @param inputImagePath 输入图像路径
 * @param imageWidth 图像宽度
 * @param imageHeight 图像高度
 * @param results 识别结果数组
 * @param boxes 检测框数组
 * @return std::string JSON 格式的字符串
 */
std::string formatResultsToJson(
    const std::string& inputImagePath,
    int imageWidth,
    int imageHeight,
    const std::vector<RecognitionResultWithIndex>& results,
    const std::vector<BoundingBox>& boxes);

#endif // RESULT_WRITER_H
