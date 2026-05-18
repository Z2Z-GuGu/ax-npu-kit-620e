#include "result_writer.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

// 转义 JSON 字符串中的特殊字符
static std::string escapeJsonString(const std::string& input) {
    std::ostringstream oss;
    for (char c : input) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

// 获取当前时间的 RFC3339 格式
static std::string getCurrentTimeRFC3339() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    gmtime_r(&time_t_now, &tm_now);  // 使用 UTC 时间
    
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%dT%H:%M:%SZ");  // Z 表示 UTC
    
    return oss.str();
}

std::string formatResultsToJson(
    const std::string& inputImagePath,
    int imageWidth,
    int imageHeight,
    const std::vector<RecognitionResultWithIndex>& results,
    const std::vector<BoundingBox>& boxes)
{
    std::ostringstream jsonStream;
    
    // 计算整体置信度
    float totalConfidence = 0.0f;
    int validCount = 0;
    for (const auto& result : results) {
        if (result.result.success && !result.result.text.empty()) {
            totalConfidence += result.result.confidence;
            validCount++;
        }
    }
    float avgConfidence = (validCount > 0) ? (totalConfidence / validCount) : 0.0f;
    
    // 获取当前时间
    std::string currentTime = getCurrentTimeRFC3339();
    
    // 写入 JSON 内容
    jsonStream << "{\n";
    jsonStream << "  \"schema_version\": 1,\n";
    jsonStream << "  \"type\": \"ocr_observation\",\n";
    jsonStream << "  \"captured_at\": \"" << currentTime << "\",\n";
    jsonStream << "  \"source\": {\n";
    jsonStream << "    \"kind\": \"screen_ocr\",\n";
    jsonStream << "    \"device\": \"nanoagent\",\n";
    jsonStream << "    \"engine\": \"ocr_process_npu\"\n";
    jsonStream << "  },\n";
    jsonStream << "  \"screen\": {\n";
    jsonStream << "    \"image_path\": \"" << escapeJsonString(inputImagePath) << "\",\n";
    jsonStream << "    \"width\": " << imageWidth << ",\n";
    jsonStream << "    \"height\": " << imageHeight << "\n";
    jsonStream << "  },\n";
    jsonStream << "  \"context\": {\n";
    jsonStream << "    \"app_name\": \"ScreenOCR\",\n";
    jsonStream << "    \"title\": \"OCR Process Result\"\n";
    jsonStream << "  },\n";
    jsonStream << "  \"event\": {\n";
    jsonStream << "    \"type\": \"ocr_capture\",\n";
    jsonStream << "    \"reason\": \"manual\"\n";
    jsonStream << "  },\n";
    jsonStream << "  \"ocr\": {\n";
    jsonStream << "    \"language\": \"zh\",\n";
    
    // 拼接所有文本
    std::string fullText;
    for (size_t i = 0; i < results.size(); i++) {
        if (i > 0) fullText += " ";
        fullText += results[i].result.text;
    }
    jsonStream << "    \"text\": \"" << escapeJsonString(fullText) << "\",\n";
    jsonStream << "    \"confidence\": " << std::fixed << std::setprecision(4) << avgConfidence << ",\n";
    jsonStream << "    \"block_count\": " << results.size() << ",\n";
    jsonStream << "    \"blocks\": [\n";
    
    // 写入每个 block
    for (size_t i = 0; i < results.size(); i++) {
        const auto& result = results[i];
        jsonStream << "      {\n";
        jsonStream << "        \"id\": " << (i + 1) << ",\n";
        jsonStream << "        \"text\": \"" << escapeJsonString(result.result.text) << "\",\n";
        jsonStream << "        \"bbox\": [" << result.box.x1 << ", " << result.box.y1 << ", " 
                 << result.box.x2 << ", " << result.box.y2 << "],\n";
        jsonStream << "        \"confidence\": " << std::fixed << std::setprecision(4) 
                 << result.result.confidence << "\n";
        jsonStream << "      }";
        if (i < results.size() - 1) {
            jsonStream << ",";
        }
        jsonStream << "\n";
    }
    
    jsonStream << "    ]\n";
    jsonStream << "  },\n";
    jsonStream << "  \"tags\": [\"external\", \"ocr\"]\n";
    jsonStream << "}\n";
    
    return jsonStream.str();
}
