/**************************************************************************************************
 *
 * OCR Text Recognition Model Tester
 * 
 * Load PaddleOCR recognition model (ch_PP_OCRv4_rec_npu.axmodel)
 * Input: 40x320 grayscale image (PNG/JPG)
 * Output: Recognized text
 *
 * Model Info:
 *   Input:  [1, 40, 320, 1] NHWC, float32, grayscale normalized to [0,1]
 *   Output: [1, 6624, 1, 1] float32 (CTC output: [batch, sequence_length, num_classes, 1])
 *
 * Usage:
 *   ./test_ocr_recognition <model_path> <dict_path> <input_image> <output_txt>
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

// Load dictionary file
AX_BOOL LoadDictionary(const char* dictPath, std::vector<std::string>& dictionary)
{
    FILE* fp = fopen(dictPath, "r");
    if (!fp) {
        printf("Error: Cannot open dictionary file: %s\n", dictPath);
        return AX_FALSE;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
            len--;
        }
        if (len > 0 && line[len-1] == '\r') {
            line[len-1] = '\0';
            len--;
        }
        
        if (len > 0) {
            dictionary.push_back(std::string(line));
        }
    }
    
    fclose(fp);
    printf("Dictionary loaded: %s (%zu characters)\n", dictPath, dictionary.size());
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

// Preprocess image for recognition
// Keep as uint8 - model expects uint8 input
cv::Mat PreprocessImage(const cv::Mat& inputImage, int targetWidth, int targetHeight, int targetChannels = 3)
{
    cv::Mat output;
    
    if (targetChannels == 1) {
        // Convert to grayscale if needed
        if (inputImage.channels() == 3) {
            cv::cvtColor(inputImage, output, cv::COLOR_BGR2GRAY);
        } else if (inputImage.channels() == 4) {
            cv::cvtColor(inputImage, output, cv::COLOR_BGRA2GRAY);
        } else {
            output = inputImage.clone();
        }
        
        // Resize to target size
        if (output.cols != targetWidth || output.rows != targetHeight) {
            cv::resize(output, output, cv::Size(targetWidth, targetHeight), 0, 0, cv::INTER_AREA);
        }
    } else if (targetChannels == 3) {
        // Convert to BGR if needed
        if (inputImage.channels() == 1) {
            cv::cvtColor(inputImage, output, cv::COLOR_GRAY2BGR);
        } else if (inputImage.channels() == 4) {
            cv::cvtColor(inputImage, output, cv::COLOR_BGRA2BGR);
        } else {
            output = inputImage.clone();
        }
        
        // Resize to target size
        if (output.cols != targetWidth || output.rows != targetHeight) {
            cv::resize(output, output, cv::Size(targetWidth, targetHeight), 0, 0, cv::INTER_AREA);
        }
    }
    
    return output;
}

// CTC decoding
std::string DecodeCTC(const float* outputData, int sequenceLength, int numClasses,
                      const std::vector<std::string>& dictionary,
                      std::vector<float>& charConfidences,
                      std::vector<int>& charIndices)
{
    std::string result;
    charConfidences.clear();
    charIndices.clear();
    
    int lastIndex = -1;
    float totalConfidence = 0.0f;
    int charCount = 0;
    
    // CTC decoding: for each time step, find the character with highest probability
    for (int t = 0; t < sequenceLength; t++) {
        int maxIndex = 0;
        float maxValue = outputData[t * numClasses];
        
        // Find max in this time step (across all characters)
        for (int c = 1; c < numClasses; c++) {
            float value = outputData[t * numClasses + c];
            if (value > maxValue) {
                maxValue = value;
                maxIndex = c;
            }
        }
        
        // Skip blank (index 0) and duplicates
        if (maxIndex > 0 && maxIndex != lastIndex) {
            // Model output is already softmax probability (from layer "softmax_11.tmp_0")
            // So we can directly use maxValue as the confidence
            float confidence = maxValue;
            
            // Fix index: model output includes blank at index 0, but dictionary starts from actual character
            // So we need to subtract 1 to match dictionary index
            int dictIndex = maxIndex - 1;
            
            if (dictIndex >= 0 && dictIndex < (int)dictionary.size()) {
                result += dictionary[dictIndex];
                charConfidences.push_back(confidence);
                charIndices.push_back(dictIndex);
                totalConfidence += confidence;
                charCount++;
            }
        }
        
        lastIndex = maxIndex;
    }
    
    return result;
}

// Save result to text file
AX_BOOL SaveResult(const char* outputPath, const std::string& text, float confidence,
                   const std::vector<float>& charConfidences, 
                   const std::vector<int>& charIndices,
                   const std::vector<std::string>& dictionary)
{
    FILE* fp = fopen(outputPath, "w");
    if (!fp) {
        printf("Error: Cannot create output file: %s\n", outputPath);
        return AX_FALSE;
    }
    
    // Write header
    fprintf(fp, "OCR Recognition Result\n");
    fprintf(fp, "======================\n\n");
    
    // Write recognized text
    fprintf(fp, "Recognized Text:\n");
    fprintf(fp, "%s\n\n", text.c_str());
    
    // Write confidence
    fprintf(fp, "Average Confidence: %.4f\n\n", confidence);
    
    // Write character-level details
    if (!charConfidences.empty()) {
        fprintf(fp, "Character Details:\n");
        fprintf(fp, "------------------\n");
        fprintf(fp, "Index\tChar\tConfidence\n");
        
        for (size_t i = 0; i < charConfidences.size(); i++) {
            std::string ch = (charIndices[i] < (int)dictionary.size()) ? 
                             dictionary[charIndices[i]] : "<invalid>";
            fprintf(fp, "%zu\t%s\t%.4f\n", i, ch.c_str(), charConfidences[i]);
        }
    }
    
    // Write summary
    fprintf(fp, "\nSummary:\n");
    fprintf(fp, "  Text length: %zu characters\n", text.length());
    fprintf(fp, "  Confidence: %.2f%%\n", confidence * 100.0f);
    fprintf(fp, "  Status: Success\n");
    
    fclose(fp);
    printf("Result saved to: %s\n", outputPath);
    return AX_TRUE;
}

void print_usage(const char* program_name)
{
    printf("Usage: %s <model_path> <input_image> <output_txt> [dict_path]\n", program_name);
    printf("\n");
    printf("Arguments:\n");
    printf("  model_path    - Path to .axmodel file (OCR recognition model)\n");
    printf("  input_image   - Input image (48x320, PNG/JPG, will be resized if needed)\n");
    printf("  output_txt    - Output text file for recognition result\n");
    printf("  dict_path     - Path to dictionary txt file (optional)\n");
    printf("\n");
    printf("Example:\n");
    printf("  Without dictionary: %s model.axmodel text.png result.txt\n", program_name);
    printf("  With dictionary:    %s model.axmodel text.png result.txt dict.txt\n", program_name);
    printf("\n");
    printf("Note: This tool performs OCR text recognition on cropped text images.\n");
    printf("      Without dictionary: outputs raw indices and confidences\n");
    printf("      With dictionary: decodes indices to actual characters\n");
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
    const char* dictPath = (argc >= 5) ? argv[4] : nullptr;
    
    printf("========================================\n");
    printf("OCR Text Recognition Model Tester\n");
    printf("========================================\n");
    printf("Model: %s\n", modelPath);
    printf("Input: %s\n", inputPath);
    printf("Output: %s\n", outputTxtPath);
    if (dictPath) {
        printf("Dictionary: %s\n", dictPath);
    } else {
        printf("Dictionary: (none, will output raw indices)\n");
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
    
    // Load dictionary if provided
    std::vector<std::string> dictionary;
    int dictSize = 0;
    if (dictPath) {
        if (!FileExists(dictPath)) {
            printf("Error: Dictionary file not found: %s\n", dictPath);
            return -1;
        }
        if (!LoadDictionary(dictPath, dictionary)) {
            return -1;
        }
        dictSize = dictionary.size();
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
    
    // Get input shape from model
    // For NHWC format: [1, height, width, channels]
    int inputShapeSize = ioInfo->pInputs[0].nShapeSize;
    std::vector<int> inputShape(inputShapeSize);
    int inputHeight = 40;
    int inputWidth = 320;
    int inputChannels = 3;
    
    for (int i = 0; i < inputShapeSize; i++) {
        inputShape[i] = ioInfo->pInputs[0].pShape[i];
    }
    
    // Parse input shape based on layout
    if (inputShapeSize >= 4) {
        // NHWC: [batch, height, width, channels]
        inputHeight = inputShape[1];
        inputWidth = inputShape[2];
        inputChannels = inputShape[3];
    } else if (inputShapeSize >= 3) {
        // HWC: [height, width, channels]
        inputHeight = inputShape[0];
        inputWidth = inputShape[1];
        inputChannels = inputShape[2];
    }
    
    printf("Input shape parsed: height=%d, width=%d, channels=%d\n", 
           inputHeight, inputWidth, inputChannels);
    
    // Preprocess image based on model input requirements
    printf("Preprocessing image (resize to %dx%d, channels=%d)...\n", 
           inputWidth, inputHeight, inputChannels);
    cv::Mat processedImage = PreprocessImage(srcImage, inputWidth, inputHeight, inputChannels);
    
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
    AX_U8* inputData = (AX_U8*)ioData.pInputs[0].pVirAddr;
    
    // Copy data with correct layout (NHWC format)
    if (inputChannels == 3) {
        // For 3-channel image, OpenCV stores as BGR interleaved
        // We need NHWC format: [N][H][W][C]
        AX_U8* srcData = processedImage.ptr<AX_U8>(0);
        memcpy(inputData, srcData, inputHeight * inputWidth * inputChannels * sizeof(AX_U8));
    } else if (inputChannels == 1) {
        // For single channel, direct copy
        AX_U8* srcData = processedImage.ptr<AX_U8>(0);
        memcpy(inputData, srcData, inputHeight * inputWidth * sizeof(AX_U8));
    }
    
    AX_U32 inputSize = inputHeight * inputWidth * inputChannels * sizeof(AX_U8);
    printf("Input data size: %u bytes (%dx%dx%d uint8)\n", 
           inputSize, inputHeight, inputWidth, inputChannels);

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
    
    // Get output dimensions
    int outputShapeSize = ioInfo->pOutputs[0].nShapeSize;
    std::vector<int> outputShape(outputShapeSize);
    int totalOutputSize = 1;
    for (int i = 0; i < outputShapeSize; i++) {
        outputShape[i] = ioInfo->pOutputs[0].pShape[i];
        totalOutputSize *= outputShape[i];
    }
    
    printf("Output shape: [");
    for (int i = 0; i < outputShapeSize; i++) {
        printf("%d", outputShape[i]);
        if (i < outputShapeSize - 1) printf(", ");
    }
    printf("]\n");
    printf("Total output elements: %d\n\n", totalOutputSize);
    
    // Decode CTC output
    // Output shape is [1, 40, 6625] in NCHW format: [batch, sequence_length, num_classes]
    // Or could be [1, 6625, 40] in NHWC: [batch, num_classes, sequence_length]
    // We need to determine the correct layout
    int sequenceLength, numClasses;
    
    if (outputShapeSize >= 3) {
        // For 3D output, assume NCHW: [batch, seq_len, num_classes]
        sequenceLength = outputShape[1];
        numClasses = outputShape[2];
    } else if (outputShapeSize == 2) {
        // For 2D output: [seq_len, num_classes]
        sequenceLength = outputShape[0];
        numClasses = outputShape[1];
    } else {
        // Fallback
        sequenceLength = 40;
        numClasses = 6625;
    }
    
    printf("Decoding CTC output (sequence_length=%d, num_classes=%d)...\n", 
           sequenceLength, numClasses);
    
    std::vector<float> charConfidences;
    std::vector<int> charIndices;
    std::string recognizedText = DecodeCTC(outputData, sequenceLength, numClasses, 
                                           dictionary, charConfidences, charIndices);
    
    // Debug: print first few character indices and their dictionary mappings
    printf("\nDebug: First 10 character positions:\n");
    for (int t = 0; t < sequenceLength && t < 10; t++) {
        int maxIndex = 0;
        float maxValue = outputData[t * numClasses];
        for (int c = 1; c < numClasses; c++) {
            float value = outputData[t * numClasses + c];
            if (value > maxValue) {
                maxValue = value;
                maxIndex = c;
            }
        }
        
        // Apply index correction for dictionary lookup
        int dictIndex = maxIndex - 1;
        
        printf("  t=%d: model_index=%d, dict_index=%d, value=%.4f", 
               t, maxIndex, dictIndex, maxValue);
        if (maxIndex > 0 && dictIndex >= 0 && dictIndex < (int)dictionary.size()) {
            printf(" -> char='%s'\n", dictionary[dictIndex].c_str());
        } else if (maxIndex == 0) {
            printf(" -> (blank)\n");
        } else {
            printf(" -> (out of range)\n");
        }
    }
    printf("\n");
    
    // Calculate average confidence
    float avgConfidence = 0.0f;
    if (!charConfidences.empty()) {
        float sum = 0.0f;
        printf("Character confidences: ");
        for (float conf : charConfidences) {
            printf("%.4f ", conf);
            sum += conf;
        }
        printf("\n");
        avgConfidence = sum / charConfidences.size();
        printf("Sum: %.4f, Count: %zu, Average: %.4f\n", sum, charConfidences.size(), avgConfidence);
    }
    
    // Print result
    printf("\n========================================\n");
    printf("Recognition Result:\n");
    printf("========================================\n");
    if (dictPath) {
        printf("Text: %s\n", recognizedText.c_str());
        printf("Confidence: %.2f%%\n", avgConfidence * 100.0f);
    } else {
        printf("Raw CTC Output (no dictionary provided)\n");
        printf("Character Indices: ");
        for (size_t i = 0; i < charIndices.size() && i < 20; i++) {
            printf("%d ", charIndices[i]);
        }
        if (charIndices.size() > 20) printf("...");
        printf("\n");
    }
    printf("Characters: %zu\n", charConfidences.size());
    printf("========================================\n\n");

    // Save result to text file
    if (!SaveResult(outputTxtPath, recognizedText, avgConfidence, 
                    charConfidences, charIndices, dictionary)) {
        FreeIO(&ioData);
        AX_ENGINE_DestroyHandle(handle);
        AX_ENGINE_Deinit();
        AX_SYS_Deinit();
        return -1;
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
