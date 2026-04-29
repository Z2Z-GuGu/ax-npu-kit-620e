#include "ocr_rec_component.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

OcrRecComponent::OcrRecComponent() : handle(0), modelLoaded(false), dictSize(0), 
                                      cachedIOInfo(nullptr), ioBuffersAllocated(false)
{
    memset(&cachedIOData, 0, sizeof(cachedIOData));
}

OcrRecComponent::~OcrRecComponent()
{
    // Free cached IO buffers
    if (ioBuffersAllocated) {
        freeIO(&cachedIOData);
        ioBuffersAllocated = false;
    }
    
    if (handle) {
        AX_ENGINE_DestroyHandle(handle);
    }
    AX_ENGINE_Deinit();
    AX_SYS_Deinit();
}

bool OcrRecComponent::fileExists(const std::string& filename)
{
    FILE* fp = fopen(filename.c_str(), "r");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

bool OcrRecComponent::loadModel(const std::string& modelPath)
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
    
    // Pre-allocate IO buffers for faster inference
    printf("Pre-allocating IO buffers for faster inference...\n");
    cachedIOInfo = new AX_ENGINE_IO_INFO_T;
    memset(cachedIOInfo, 0, sizeof(AX_ENGINE_IO_INFO_T));
    
    AX_S32 retIO = AX_ENGINE_GetIOInfo(handle, &cachedIOInfo);
    if (retIO != 0) {
        printf("Warning: AX_ENGINE_GetIOInfo failed, will allocate per inference\n");
        delete cachedIOInfo;
        cachedIOInfo = nullptr;
    } else {
        if (prepareIO(cachedIOInfo, &cachedIOData)) {
            ioBuffersAllocated = true;
            printf("IO buffers allocated successfully!\n");
        } else {
            printf("Warning: prepareIO failed, will allocate per inference\n");
            delete cachedIOInfo;
            cachedIOInfo = nullptr;
        }
    }
    
    return true;
}

bool OcrRecComponent::loadDictionary(const std::string& dictPath)
{
    if (!fileExists(dictPath)) {
        printf("Error: Dictionary file not found: %s\n", dictPath.c_str());
        return false;
    }
    
    FILE* fp = fopen(dictPath.c_str(), "r");
    if (!fp) {
        printf("Error: Cannot open dictionary file: %s\n", dictPath.c_str());
        return false;
    }
    
    dictionary.clear();
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
    
    dictSize = dictionary.size();
    printf("Dictionary loaded: %s (%d characters)\n", dictPath.c_str(), dictSize);
    
    return true;
}

std::string OcrRecComponent::getChar(int index) const
{
    if (index >= 0 && index < (int)dictionary.size()) {
        return dictionary[index];
    }
    return "";
}

void OcrRecComponent::freeIO(AX_ENGINE_IO_T* io)
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

bool OcrRecComponent::prepareIO(AX_ENGINE_IO_INFO_T* ioInfo, AX_ENGINE_IO_T* ioData)
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
            OCR_REC_CMM_ALIGN_SIZE,
            (const AX_S8*)OCR_REC_CMM_SESSION_NAME
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
            OCR_REC_CMM_ALIGN_SIZE,
            (const AX_S8*)OCR_REC_CMM_SESSION_NAME
        );
        
        if (ret != 0) {
            printf("Error: Failed to allocate output buffer %d (size: %u)\n", i, meta.nSize);
            freeIO(ioData);
            return false;
        }
    }
    
    return true;
}

cv::Mat OcrRecComponent::preprocessImage(const cv::Mat& inputImage, int targetWidth, int targetHeight)
{
    // Fast path: if image already has correct size and 3 channels, just clone and return
    if (inputImage.cols == targetWidth && inputImage.rows == targetHeight && inputImage.channels() == 3) {
        // printf("  [FAST PATH] Image already %dx%d with 3 channels, skipping conversion\n", targetWidth, targetHeight);
        return inputImage.clone();
    }
    
    printf("  [SLOW PATH] Converting image: %dx%d channels=%d -> %dx%d\n", 
           inputImage.cols, inputImage.rows, inputImage.channels(), targetWidth, targetHeight);
    
    cv::Mat output;
    
    // Model expects 3-channel BGR image (not grayscale!)
    // Convert to BGR if needed
    if (inputImage.channels() == 1) {
        // Grayscale to BGR
        printf("    -> Converting grayscale to BGR\n");
        cv::cvtColor(inputImage, output, cv::COLOR_GRAY2BGR);
    } else if (inputImage.channels() == 4) {
        // BGRA to BGR
        printf("    -> Converting BGRA to BGR\n");
        cv::cvtColor(inputImage, output, cv::COLOR_BGRA2BGR);
    } else if (inputImage.channels() == 3) {
        // Already BGR, just clone
        output = inputImage.clone();
    } else {
        // Fallback
        output = inputImage.clone();
    }
    
    // Resize to target size if needed
    if (output.cols != targetWidth || output.rows != targetHeight) {
        printf("    -> Resizing from %dx%d to %dx%d\n", output.cols, output.rows, targetWidth, targetHeight);
        cv::resize(output, output, cv::Size(targetWidth, targetHeight), 0, 0, cv::INTER_AREA);
    }
    
    // Keep as uint8 - model expects uint8 input (0-255 range)
    // Do NOT normalize to [0,1] or convert to float
    
    return output;
}

std::string OcrRecComponent::decodeCTC(const float* outputData, int width, int height,
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
    for (int t = 0; t < width; t++) {
        int maxIndex = 0;
        float maxValue = outputData[t * height];
        
        // Find max in this time step (across all characters)
        for (int c = 1; c < height; c++) {
            float value = outputData[t * height + c];
            if (value > maxValue) {
                maxValue = value;
                maxIndex = c;
            }
        }
        
        // Skip blank (index 0) and duplicates
        if (maxIndex > 0 && maxIndex != lastIndex) {
            // Model output is already softmax probability, directly use maxValue
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

OCRRecResult OcrRecComponent::recognize(const cv::Mat& inputImage)
{
    OCRRecResult result;
    
    if (!modelLoaded) {
        result.errorMessage = "Model not loaded";
        return result;
    }
    
    if (inputImage.empty()) {
        result.errorMessage = "Input image is empty";
        return result;
    }
    
    // Preprocess image
    cv::Mat processedImage = preprocessImage(inputImage, 320, 40);
    
    if (processedImage.empty()) {
        result.errorMessage = "Failed to preprocess image";
        return result;
    }
    
    // Use cached IO info and buffers if available (much faster!)
    AX_ENGINE_IO_INFO_T* ioInfo = nullptr;
    AX_ENGINE_IO_T ioData;
    bool useCachedIO = false;
    
    if (cachedIOInfo && ioBuffersAllocated) {
        ioInfo = cachedIOInfo;
        ioData = cachedIOData;
        useCachedIO = true;
    } else {
        // Fallback: allocate IO buffers on-the-fly (slow)
        ioInfo = new AX_ENGINE_IO_INFO_T;
        memset(ioInfo, 0, sizeof(AX_ENGINE_IO_INFO_T));
        
        AX_S32 ret2 = AX_ENGINE_GetIOInfo(handle, &ioInfo);
        if (ret2 != 0 || !ioInfo) {
            result.errorMessage = "AX_ENGINE_GetIOInfo failed";
            delete ioInfo;
            return result;
        }
        
        if (!prepareIO(ioInfo, &ioData)) {
            result.errorMessage = "Failed to prepare IO";
            delete ioInfo;
            return result;
        }
    }
    
    // Copy image data to input buffer
    // Input shape: [1, 40, 320, 3] for BGR NHWC format
    AX_U8* inputData = (AX_U8*)ioData.pInputs[0].pVirAddr;
    
    // Calculate expected size based on model input shape
    // Model expects 40x320x3 = 38400 bytes for BGR image
    size_t expectedSize = 40 * 320 * 3 * sizeof(AX_U8);
    
    printf("  Preprocessed image: %dx%d, channels=%d, size=%zu bytes\n", 
           processedImage.cols, processedImage.rows, processedImage.channels(), 
           processedImage.total() * processedImage.elemSize());
    printf("  Expected input size: %zu bytes\n", expectedSize);
    
    // Ensure image is continuous in memory
    if (processedImage.isContinuous()) {
        memcpy(inputData, processedImage.data, processedImage.total() * processedImage.elemSize());
    } else {
        // Copy row by row
        size_t rowSize = processedImage.cols * processedImage.elemSize();
        for (int r = 0; r < processedImage.rows; r++) {
            memcpy(inputData + r * processedImage.cols * 3, processedImage.ptr<AX_U8>(r), rowSize);
        }
    }
    
    // Run inference
    AX_S32 ret = AX_ENGINE_RunSync(handle, &ioData);
    if (ret != 0) {
        printf("Error: AX_ENGINE_RunSync failed: 0x%x\n", ret);
        freeIO(&ioData);
        result.errorMessage = "Inference failed";
        return result;
    }
    
    // Get output data
    // Output shape: [1, 6624, 1, 1] or similar (depends on model)
    // For PaddleOCR: [1, num_classes, sequence_length, 1]
    float* outputData = (float*)ioData.pOutputs[0].pVirAddr;
    
    // Get output dimensions
    int outputSize = 1;
    for (AX_U32 i = 0; i < ioInfo->pOutputs[0].nShapeSize; i++) {
        outputSize *= ioInfo->pOutputs[0].pShape[i];
    }
    
    printf("  Output shape: ");
    for (AX_U32 i = 0; i < ioInfo->pOutputs[0].nShapeSize; i++) {
        printf("%d ", ioInfo->pOutputs[0].pShape[i]);
    }
    printf("\n");
    printf("  Total output size: %d floats\n", outputSize);
    
    int sequenceLength = ioInfo->pOutputs[0].pShape[1]; // Usually the second dimension
    int numClasses = ioInfo->pOutputs[0].pShape[2];     // Number of character classes
    
    printf("  Parsed: sequenceLength=%d, numClasses=%d\n", sequenceLength, numClasses);
    
    // For PaddleOCR rec model, output is typically [1, 6624, 1, 1] or [1, T, C, 1]
    // where T is sequence length and C is number of classes
    // We need to reshape to [T, C] for CTC decoding
    
    // Decode CTC output
    std::vector<float> charConfidences;
    std::vector<int> charIndices;
    
    // Assuming output is [1, sequenceLength, numClasses, 1] in NHWC format
    // Reshape to [sequenceLength, numClasses]
    result.text = decodeCTC(outputData, sequenceLength, numClasses, charConfidences, charIndices);
    result.charConfidences = charConfidences;
    result.charIndices = charIndices;
    
    printf("  Decoded text: \"%s\" (length=%zu, chars=%d)\n", result.text.c_str(), result.text.length(), (int)charIndices.size());
    
    // Calculate average confidence
    if (!charConfidences.empty()) {
        float sum = 0.0f;
        for (float conf : charConfidences) {
            sum += conf;
        }
        result.confidence = sum / charConfidences.size();
    }
    
    result.success = true;
    
    // Cleanup: only free if we allocated on-the-fly (not using cached buffers)
    if (!useCachedIO) {
        freeIO(&ioData);
        delete ioInfo;
    }
    // If using cached IO, buffers are kept for next inference (freed in destructor)
    
    return result;
}

bool OcrRecComponent::saveResult(const OCRRecResult& result, 
                                  const std::string& outputTxtPath)
{
    FILE* fp = fopen(outputTxtPath.c_str(), "w");
    if (!fp) {
        printf("Error: Cannot create output file: %s\n", outputTxtPath.c_str());
        return false;
    }
    
    // Write header
    fprintf(fp, "OCR Recognition Result\n");
    fprintf(fp, "======================\n\n");
    
    // Write recognized text
    fprintf(fp, "Recognized Text:\n");
    fprintf(fp, "%s\n\n", result.text.c_str());
    
    // Write confidence
    fprintf(fp, "Average Confidence: %.4f\n\n", result.confidence);
    
    // Write character-level details
    if (!result.charConfidences.empty()) {
        fprintf(fp, "Character Details:\n");
        fprintf(fp, "------------------\n");
        fprintf(fp, "Index\tChar\tConfidence\n");
        
        for (size_t i = 0; i < result.charConfidences.size(); i++) {
            std::string ch = getChar(result.charIndices[i]);
            if (ch.empty()) ch = "<blank>";
            fprintf(fp, "%zu\t%s\t%.4f\n", i, ch.c_str(), result.charConfidences[i]);
        }
    }
    
    // Write status
    fprintf(fp, "\nStatus: %s\n", result.success ? "Success" : "Failed");
    if (!result.errorMessage.empty()) {
        fprintf(fp, "Error: %s\n", result.errorMessage.c_str());
    }
    
    fclose(fp);
    printf("Result saved to: %s\n", outputTxtPath.c_str());
    
    return true;
}
