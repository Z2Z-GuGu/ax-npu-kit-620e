#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <numeric>
#include "letterbox_component.h"
#include "ocr_det_component.h"
#include "ocr_rec_component.h"

// Global variables for signal handling
static volatile bool g_should_stop = false;
static std::vector<double> g_timing_results;
static std::string g_timing_file_path;
static int g_run_count = 0;
static double g_total_download_time = 0.0;
static double g_total_detection_time = 0.0;
static double g_total_recognition_time = 0.0;

void saveTimingResults() {
    if (g_timing_results.empty() || g_timing_file_path.empty()) {
        return;
    }
    
    std::ofstream timingFile(g_timing_file_path, std::ios::app);
    if (timingFile.is_open()) {
        double total = 0.0;
        for (double t : g_timing_results) {
            total += t;
        }
        double avg = total / g_timing_results.size();
        
        timingFile << "\n========================================\n";
        timingFile << "Session Summary (Interrupted by Ctrl+C)\n";
        timingFile << "========================================\n";
        timingFile << "Total runs: " << g_timing_results.size() << "\n";
        timingFile << "Average time per screenshot: " << avg << " ms (" << (avg / 1000.0) << " seconds)\n";
        timingFile << "Total time: " << total << " ms (" << (total / 1000.0) << " seconds)\n";
        timingFile << "\nBreakdown:\n";
        timingFile << "  - Avg download time: " << (g_total_download_time / g_timing_results.size()) << " ms\n";
        timingFile << "  - Avg detection time: " << (g_total_detection_time / g_timing_results.size()) << " ms\n";
        timingFile << "  - Avg recognition time: " << (g_total_recognition_time / g_timing_results.size()) << " ms\n";
        timingFile.close();
        
        printf("\n\nTiming results saved to: %s\n", g_timing_file_path.c_str());
        printf("Total runs: %zu\n", g_timing_results.size());
        printf("Average time per screenshot: %.3f ms (%.3f seconds)\n", avg, avg / 1000.0);
    }
}

void signalHandler(int signum) {
    printf("\n\nReceived signal %d (Ctrl+C), stopping...\n", signum);
    g_should_stop = true;
    
    // Save timing results immediately
    saveTimingResults();
    
    exit(signum);
}

struct OCRResult {
    int index;
    int width;
    int height;
    int x;
    int y;
    float confidence;
    std::string text;
};

// Merge overlapping text segments
// When a long text region is cropped with overlap, we need to merge the recognized results
std::vector<OCRResult> mergeOverlappingSegments(const std::vector<OCRResult>& results)
{
    if (results.size() <= 1) {
        return results;
    }
    
    std::vector<OCRResult> merged;
    OCRResult current = results[0];
    
    for (size_t i = 1; i < results.size(); i++) {
        const OCRResult& next = results[i];
        
        // Check if this is a continuation (same index or consecutive indices)
        bool isContinuation = (next.index == current.index) || 
                              (next.index == current.index + 1);
        
        if (isContinuation && !current.text.empty() && !next.text.empty()) {
            // Try to merge overlapping parts with multiple strategies
            bool merged_flag = false;
            
            // Strategy 1: Check 2-char overlap (most common)
            if (current.text.length() >= 2 && next.text.length() >= 2) {
                std::string currentEnd = current.text.substr(current.text.length() - 2);
                std::string nextStart = next.text.substr(0, 2);
                
                if (currentEnd == nextStart) {
                    current.text += next.text.substr(2);
                    current.confidence = (current.confidence + next.confidence) / 2.0f;
                    merged_flag = true;
                    printf("  Merged segment %d: 2-char overlap=\"%s\" -> \"%s\"\n", 
                           current.index, currentEnd.c_str(), current.text.c_str());
                }
            }
            
            // Strategy 2: Check 3-char overlap
            if (!merged_flag && current.text.length() >= 3 && next.text.length() >= 3) {
                std::string currentEnd = current.text.substr(current.text.length() - 3);
                std::string nextStart = next.text.substr(0, 3);
                
                if (currentEnd == nextStart) {
                    current.text += next.text.substr(3);
                    current.confidence = (current.confidence + next.confidence) / 2.0f;
                    merged_flag = true;
                    printf("  Merged segment %d: 3-char overlap=\"%s\" -> \"%s\"\n", 
                           current.index, currentEnd.c_str(), current.text.c_str());
                }
            }
            
            // Strategy 3: Check 4-char overlap (for longer repeated sequences)
            if (!merged_flag && current.text.length() >= 4 && next.text.length() >= 4) {
                std::string currentEnd = current.text.substr(current.text.length() - 4);
                std::string nextStart = next.text.substr(0, 4);
                
                if (currentEnd == nextStart) {
                    current.text += next.text.substr(4);
                    current.confidence = (current.confidence + next.confidence) / 2.0f;
                    merged_flag = true;
                    printf("  Merged segment %d: 4-char overlap=\"%s\" -> \"%s\"\n", 
                           current.index, currentEnd.c_str(), current.text.c_str());
                }
            }
            
            // Strategy 4: Check 1-char overlap
            if (!merged_flag && !current.text.empty() && !next.text.empty()) {
                char currentLast = current.text.back();
                char nextFirst = next.text[0];
                
                if (currentLast == nextFirst) {
                    current.text += next.text.substr(1);
                    current.confidence = (current.confidence + next.confidence) / 2.0f;
                    merged_flag = true;
                    printf("  Merged segment %d: 1-char overlap=\"%c\" -> \"%s\"\n", 
                           current.index, currentLast, current.text.c_str());
                }
            }
            
            // Strategy 5: Remove last char from current, then check 2-char overlap
            if (!merged_flag && current.text.length() >= 3 && next.text.length() >= 2) {
                std::string currentWithoutLast = current.text.substr(0, current.text.length() - 1);
                std::string currentEnd2 = currentWithoutLast.substr(currentWithoutLast.length() - 2);
                std::string nextStart = next.text.substr(0, 2);
                
                if (currentEnd2 == nextStart) {
                    current.text = currentWithoutLast + next.text.substr(2);
                    current.confidence = (current.confidence + next.confidence) / 2.0f;
                    merged_flag = true;
                    printf("  Merged segment %d: removed last char+2-char overlap -> \"%s\"\n", 
                           current.index, current.text.c_str());
                }
            }
            
            // Strategy 6: Remove first char from next, then check 2-char overlap
            if (!merged_flag && current.text.length() >= 2 && next.text.length() >= 3) {
                std::string nextWithoutFirst = next.text.substr(1);
                std::string currentEnd = current.text.substr(current.text.length() - 2);
                std::string nextStart2 = nextWithoutFirst.substr(0, 2);
                
                if (currentEnd == nextStart2) {
                    current.text += nextWithoutFirst.substr(2);
                    current.confidence = (current.confidence + next.confidence) / 2.0f;
                    merged_flag = true;
                    printf("  Merged segment %d: removed first char+2-char overlap -> \"%s\"\n", 
                           current.index, current.text.c_str());
                }
            }
            
            // Strategy 7: Smart merge - find longest common substring at boundary
            if (!merged_flag && current.text.length() >= 2 && next.text.length() >= 2) {
                // Try to find overlapping region (2 to min(10, min_len) chars)
                size_t maxOverlap = std::min((size_t)10, std::min(current.text.length(), next.text.length()));
                
                for (size_t overlapLen = maxOverlap; overlapLen >= 2; overlapLen--) {
                    std::string currentEnd = current.text.substr(current.text.length() - overlapLen);
                    std::string nextStart = next.text.substr(0, overlapLen);
                    
                    if (currentEnd == nextStart) {
                        current.text += next.text.substr(overlapLen);
                        current.confidence = (current.confidence + next.confidence) / 2.0f;
                        merged_flag = true;
                        printf("  Merged segment %d: %zu-char smart overlap=\"%s\" -> \"%s\"\n", 
                               current.index, overlapLen, currentEnd.c_str(), current.text.c_str());
                        break;
                    }
                }
            }
            
            // Strategy 8: Detect and remove duplicated words/phrases in the middle
            // This handles cases like "点击点击这里" -> "点击这里"
            if (!merged_flag) {
                // Concatenate first, then look for internal duplicates
                std::string combined = current.text + next.text;
                std::string optimized = combined;
                
                // Look for repeated patterns (2-10 chars)
                for (size_t patternLen = 2; patternLen <= 10 && patternLen * 2 <= combined.length(); patternLen++) {
                    for (size_t pos = 0; pos + patternLen * 2 <= combined.length(); pos++) {
                        std::string pattern1 = combined.substr(pos, patternLen);
                        std::string pattern2 = combined.substr(pos + patternLen, patternLen);
                        
                        // If two consecutive patterns are identical
                        if (pattern1 == pattern2) {
                            // Check if removing one gives a better result
                            std::string withoutDup = combined.substr(0, pos + patternLen) + 
                                                     combined.substr(pos + patternLen * 2);
                            
                            // Simple heuristic: shorter is better (removed duplicate)
                            if (withoutDup.length() < optimized.length()) {
                                optimized = withoutDup;
                                printf("  Merged segment %d: removed duplicate \"%s\" at pos %zu -> \"%s\"\n", 
                                       current.index, pattern1.c_str(), pos, optimized.c_str());
                            }
                        }
                    }
                }
                
                if (optimized.length() < combined.length()) {
                    current.text = optimized;
                    current.confidence = (current.confidence + next.confidence) / 2.0f;
                    merged_flag = true;
                } else {
                    // Fallback: just concatenate
                    current.text += next.text;
                    current.confidence = (current.confidence + next.confidence) / 2.0f;
                    printf("  Concatenated segment %d (no overlap found): \"%s\"\n", 
                           current.index, current.text.c_str());
                }
            }
        } else {
            // Not a continuation, push current and start new
            merged.push_back(current);
            current = next;
        }
    }
    
    // Push the last segment
    merged.push_back(current);
    
    return merged;
}

void print_usage(const char* program_name)
{
    printf("Usage: %s <det_model> <rec_model> <dict_path>\n", program_name);
    printf("\n");
    printf("Arguments:\n");
    printf("  det_model   - Path to OCR detection model (.axmodel)\n");
    printf("  rec_model   - Path to OCR recognition model (.axmodel)\n");
    printf("  dict_path   - Path to dictionary file (.txt)\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s det.axmodel rec.axmodel dict.txt\n", program_name);
    printf("\n");
    printf("Description:\n");
    printf("  Continuously fetches screenshots from http://192.168.1.12:5002/screenshot\n");
    printf("  and processes them with OCR. Press Ctrl+C to stop.\n");
    printf("  Timing results are saved to /root/ocr/timing_results.txt\n");
    printf("\n");
}

std::string getFilenameWithoutExtension(const std::string& filepath)
{
    size_t lastSlash = filepath.find_last_of("/\\");
    size_t lastDot = filepath.find_last_of('.');
    
    if (lastDot == std::string::npos || lastDot < lastSlash) {
        return filepath;
    }
    
    return filepath.substr(lastSlash + 1, lastDot - lastSlash - 1);
}

std::string getDirectory(const std::string& filepath)
{
    size_t lastSlash = filepath.find_last_of("/\\");
    if (lastSlash == std::string::npos) {
        return ".";
    }
    return filepath.substr(0, lastSlash);
}

std::string escapeJsonString(const std::string& str)
{
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    return result;
}

bool saveToJson(const std::string& outputPath, const std::vector<OCRResult>& results)
{
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        printf("Error: Cannot create JSON file: %s\n", outputPath.c_str());
        return false;
    }
    
    file << "{\n";
    file << "  \"total_regions\": " << results.size() << ",\n";
    file << "  \"results\": [\n";
    
    for (size_t i = 0; i < results.size(); i++) {
        const auto& result = results[i];
        file << "    {\n";
        file << "      \"index\": " << result.index << ",\n";
        file << "      \"size\": {\n";
        file << "        \"width\": " << result.width << ",\n";
        file << "        \"height\": " << result.height << "\n";
        file << "      },\n";
        file << "      \"position\": {\n";
        file << "        \"x\": " << result.x << ",\n";
        file << "        \"y\": " << result.y << "\n";
        file << "      },\n";
        file << "      \"confidence\": " << result.confidence << ",\n";
        file << "      \"text\": \"" << escapeJsonString(result.text) << "\"\n";
        file << "    }";
        
        if (i < results.size() - 1) {
            file << ",";
        }
        file << "\n";
    }
    
    file << "  ]\n";
    file << "}\n";
    
    file.close();
    printf("JSON result saved to: %s\n", outputPath.c_str());
    return true;
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        print_usage(argv[0]);
        return -1;
    }

    const char* detModelPath = argv[1];
    const char* recModelPath = argv[2];
    const char* dictPath = argv[3];
    
    // Setup timing file path
    g_timing_file_path = "/root/ocr/timing_results.txt";
    
    // Setup signal handler for Ctrl+C
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    printf("========================================\n");
    printf("OCR Full Pipeline - Continuous Screenshot Mode\n");
    printf("========================================\n");
    printf("Detection Model: %s\n", detModelPath);
    printf("Recognition Model: %s\n", recModelPath);
    printf("Dictionary: %s\n", dictPath);
    printf("Screenshot URL: http://192.168.1.12:5002/screenshot\n");
    printf("Timing results will be saved to: %s\n", g_timing_file_path.c_str());
    printf("Press Ctrl+C to stop and save results\n");
    printf("\n");

    // Validate model files
    if (!OcrDetComponent::fileExists(detModelPath)) {
        printf("Error: Detection model file not found: %s\n", detModelPath);
        return -1;
    }
    
    if (!OcrRecComponent::fileExists(recModelPath)) {
        printf("Error: Recognition model file not found: %s\n", recModelPath);
        return -1;
    }
    
    if (!OcrRecComponent::fileExists(dictPath)) {
        printf("Error: Dictionary file not found: %s\n", dictPath);
        return -1;
    }
    
    const char* screenshotPath = "/tmp/screenshot.jpg";
    int targetWidth = 640;
    int targetHeight = 480;
    
    // Main loop: continuously fetch screenshots and process them
    int runCount = 0;
    double totalDownloadTime = 0.0;
    double totalDetectionTime = 0.0;
    double totalRecognitionTime = 0.0;
    
    while (!g_should_stop) {
        runCount++;
        printf("\n========================================\n");
        printf("Run #%d\n", runCount);
        printf("========================================\n");
        
        // Step 1: Fetch screenshot via curl
        auto downloadStartTime = std::chrono::high_resolution_clock::now();
        
        printf("Fetching screenshot from http://192.168.1.12:5002/screenshot...\n");
        std::string curlCmd = "curl -s http://192.168.1.12:5002/screenshot -o " + std::string(screenshotPath);
        int curlRet = system(curlCmd.c_str());
        
        auto downloadEndTime = std::chrono::high_resolution_clock::now();
        double downloadTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(downloadEndTime - downloadStartTime).count();
        totalDownloadTime += downloadTimeMs;
        
        if (curlRet != 0) {
            printf("Warning: curl command failed with return code %d, retrying...\n", curlRet);
            usleep(500000); // Wait 500ms before retry
            continue;
        }
        
        // Check if screenshot was downloaded successfully
        if (!OcrDetComponent::fileExists(screenshotPath)) {
            printf("Warning: Screenshot file not created, retrying...\n");
            usleep(500000);
            continue;
        }
        
        printf("Screenshot downloaded in %.3f ms\n", downloadTimeMs);
        
        // Step 2: Load and process the screenshot
        auto loadStartTime = std::chrono::high_resolution_clock::now();
        cv::Mat inputImage = cv::imread(screenshotPath);
        auto loadEndTime = std::chrono::high_resolution_clock::now();
        double loadTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(loadEndTime - loadStartTime).count();
        
        if (inputImage.empty()) {
            printf("Warning: Failed to load screenshot, retrying...\n");
            usleep(500000);
            continue;
        }
        
        int originalWidth = inputImage.cols;
        int originalHeight = inputImage.rows;
        
        printf("Screenshot size: %dx%d (loaded in %.3f ms)\n", originalWidth, originalHeight, loadTimeMs);
        
        LetterboxComponent letterbox(targetWidth, targetHeight);
        LetterboxParams params = letterbox.calculateParams(originalWidth, originalHeight);
        
        OCRDetResult detResult;
        
        bool skipDetection = (originalWidth == 320 && originalHeight == 48);
        
        double totalDetTime = 0.0; // Initialize to 0 for both cases
        
        if (skipDetection) {
            printf(">>> Screenshot is 320x48, skipping detection <<<\n");
            
            TextBox box;
            box.x1 = 0;
            box.y1 = 0;
            box.x2 = 320;
            box.y2 = 48;
            box.confidence = 1.0f;
            
            detResult.success = true;
            detResult.boxes.push_back(box);
            detResult.boxesOriginal.push_back(box);
            detResult.boxesSorted.push_back(box);
            detResult.textImages.push_back(TextImageInfo(inputImage, 1, 1));
        } else {
            auto detStartTime = std::chrono::high_resolution_clock::now();
            
            {
                OcrDetComponent ocrDet;
                
                auto loadDetStartTime = std::chrono::high_resolution_clock::now();
                if (!ocrDet.loadModel(detModelPath)) {
                    printf("Error: Failed to load detection model\n");
                    continue;
                }
                auto loadDetEndTime = std::chrono::high_resolution_clock::now();
                double loadDetTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(loadDetEndTime - loadDetStartTime).count();
                printf("Detection model loaded in %.3f ms\n", loadDetTimeMs);
                
                printf("Running OCR detection...\n");
                auto inferStartTime = std::chrono::high_resolution_clock::now();
                detResult = ocrDet.detect(inputImage, targetWidth, targetHeight,
                                               params.offsetX, params.offsetY,
                                               params.resizeWidth, params.resizeHeight);
                auto inferEndTime = std::chrono::high_resolution_clock::now();
                double inferTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(inferEndTime - inferStartTime).count();
                printf("Detection inference completed in %.3f ms\n", inferTimeMs);
                totalDetectionTime += inferTimeMs;
                
                if (!detResult.success) {
                    printf("Error: %s\n", detResult.errorMessage.c_str());
                    continue;
                }
            
            printf("Detection completed: %zu text regions\n", detResult.boxes.size());
            }
            
            auto detEndTime = std::chrono::high_resolution_clock::now();
            totalDetTime = std::chrono::duration_cast<std::chrono::milliseconds>(detEndTime - detStartTime).count();
            printf("Total detection phase time: %.3f ms\n", totalDetTime);
            
            usleep(100000);
            
            if (detResult.boxes.empty()) {
                printf("No text regions detected in this screenshot\n");
            }
        }

        // Step 3: Recognition
        auto recPhaseStartTime = std::chrono::high_resolution_clock::now();
        
        OcrRecComponent ocrRec;
        
        auto loadRecStartTime = std::chrono::high_resolution_clock::now();
        if (!ocrRec.loadModel(recModelPath)) {
            printf("Error: Failed to load recognition model\n");
            continue;
        }
        auto loadRecEndTime = std::chrono::high_resolution_clock::now();
        double loadRecTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(loadRecEndTime - loadRecStartTime).count();
        printf("Recognition model loaded in %.3f ms\n", loadRecTimeMs);
        
        if (!ocrRec.loadDictionary(dictPath)) {
            printf("Error: Failed to load dictionary\n");
            continue;
        }
        
        std::vector<OCRResult> results;
        
        auto recInferStartTime = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < detResult.textImages.size(); i++) {
            const auto& textInfo = detResult.textImages[i];
            
            if (textInfo.image.empty()) {
                continue;
            }
            
            OCRRecResult recResult = ocrRec.recognize(textInfo.image);
            
            if (!recResult.success) {
                continue;
            }
            
            if (recResult.confidence <= 0.0f || recResult.text.empty()) {
                continue;
            }
            
            OCRResult result;
            result.index = textInfo.boxIndex;
            result.width = textInfo.image.cols;
            result.height = textInfo.image.rows;
            
            if (textInfo.boxIndex - 1 < (int)detResult.boxesOriginal.size()) {
                const auto& box = detResult.boxesOriginal[textInfo.boxIndex - 1];
                result.x = static_cast<int>(box.x1);
                result.y = static_cast<int>(box.y1);
            } else {
                result.x = 0;
                result.y = 0;
            }
            
            result.confidence = recResult.confidence;
            result.text = recResult.text;
            
            results.push_back(result);
        }
        auto recInferEndTime = std::chrono::high_resolution_clock::now();
        double recInferTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(recInferEndTime - recInferStartTime).count();
        totalRecognitionTime += recInferTimeMs;
        
        auto recPhaseEndTime = std::chrono::high_resolution_clock::now();
        double recPhaseTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(recPhaseEndTime - recPhaseStartTime).count();
        printf("Recognition phase time: %.3f ms (inference: %.3f ms)\n", recPhaseTimeMs, recInferTimeMs);
        
        // Merge overlapping segments
        std::vector<OCRResult> mergedResults = mergeOverlappingSegments(results);
        
        // Calculate total processing time
        auto loopEndTime = std::chrono::high_resolution_clock::now();
        auto loopDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loopEndTime - downloadStartTime);
        double loopTimeMs = loopDuration.count();
        
        // Record timing
        g_timing_results.push_back(loopTimeMs);
        g_run_count = runCount;
        g_total_download_time += downloadTimeMs;
        g_total_detection_time += totalDetectionTime;
        g_total_recognition_time += recInferTimeMs;
        
        // Save to timing file immediately
        std::ofstream timingFile(g_timing_file_path, std::ios::app);
        if (timingFile.is_open()) {
            timingFile << "\n========================================\n";
            timingFile << "Run #" << runCount << ": " << loopTimeMs << " ms (download: " << downloadTimeMs 
                      << " ms, det: " << totalDetTime << " ms, rec: " << recPhaseTimeMs << " ms)\n";
            timingFile << "========================================\n";
            timingFile << "Recognized text (" << mergedResults.size() << " regions):\n";
            for (const auto& result : mergedResults) {
                timingFile << "  [" << result.index << "] conf=" << result.confidence 
                          << " text=\"" << result.text << "\"\n";
            }
            timingFile << "========================================\n";
            timingFile.close();
        }
        
        // Print results
        printf("\n========================================\n");
        printf("Timing Breakdown for Run #%d\n", runCount);
        printf("========================================\n");
        printf("Download time: %.3f ms\n", downloadTimeMs);
        printf("Image load time: %.3f ms\n", loadTimeMs);
        if (!skipDetection) {
            printf("Detection phase time: %.3f ms\n", totalDetTime);
        }
        printf("Recognition phase time: %.3f ms\n", recPhaseTimeMs);
        printf("TOTAL TIME: %.3f ms (%.3f seconds)\n", loopTimeMs, loopTimeMs / 1000.0);
        printf("========================================\n");
        
        printf("Text regions recognized: %zu\n", mergedResults.size());
        
        if (!mergedResults.empty()) {
            printf("Results:\n");
            for (const auto& result : mergedResults) {
                printf("  [%d] conf=%.4f text=\"%s\"\n",
                       result.index, result.confidence, result.text.c_str());
            }
        }
        
        // Calculate running average
        double total = 0.0;
        for (double t : g_timing_results) {
            total += t;
        }
        double avg = total / g_timing_results.size();
        printf("\nRunning average: %.3f ms (%.3f seconds) over %zu runs\n", avg, avg / 1000.0, g_timing_results.size());
        printf("  - Avg download time: %.3f ms\n", totalDownloadTime / g_timing_results.size());
        printf("  - Avg detection time: %.3f ms\n", totalDetectionTime / g_timing_results.size());
        printf("  - Avg recognition time: %.3f ms\n", totalRecognitionTime / g_timing_results.size());
        
        // Small delay before next screenshot
        usleep(200000); // 200ms
    }
    
    // Final summary (if we exit normally, though signal handler should catch Ctrl+C)
    saveTimingResults();
    
    printf("\n========================================\n");
    printf("Final Summary\n");
    printf("========================================\n");
    printf("Total runs: %d\n", runCount);
    double finalAvg = g_timing_results.empty() ? 0.0 : 
                     (std::accumulate(g_timing_results.begin(), g_timing_results.end(), 0.0) / g_timing_results.size());
    printf("Average time per screenshot: %.3f ms (%.3f seconds)\n", finalAvg, finalAvg / 1000.0);
    printf("  - Total download time: %.3f ms (avg: %.3f ms)\n", 
           totalDownloadTime, runCount > 0 ? totalDownloadTime / runCount : 0.0);
    printf("  - Total detection time: %.3f ms (avg: %.3f ms)\n", 
           totalDetectionTime, runCount > 0 ? totalDetectionTime / runCount : 0.0);
    printf("  - Total recognition time: %.3f ms (avg: %.3f ms)\n", 
           totalRecognitionTime, runCount > 0 ? totalRecognitionTime / runCount : 0.0);
    printf("Timing results saved to: %s\n", g_timing_file_path.c_str());
    printf("\n");

    return 0;
}
