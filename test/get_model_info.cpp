#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>

#include "ax_engine_api.h"
#include "ax_sys_api.h"

void print_usage(const char* program_name) {
    printf("Usage: %s <model_path>\n", program_name);
    printf("Example: %s /path/to/model.axmodel\n", program_name);
}

const char* describe_layout(AX_ENGINE_TENSOR_LAYOUT_T type) {
    switch (type) {
        case AX_ENGINE_TENSOR_LAYOUT_NHWC:
            return "NHWC";
        case AX_ENGINE_TENSOR_LAYOUT_NCHW:
            return "NCHW";
        default:
            return "unknown";
    }
}

const char* describe_data_type(AX_ENGINE_DATA_TYPE_T type) {
    switch (type) {
        case AX_ENGINE_DT_UINT8:
            return "uint8";
        case AX_ENGINE_DT_UINT16:
            return "uint16";
        case AX_ENGINE_DT_FLOAT32:
            return "float32";
        case AX_ENGINE_DT_SINT16:
            return "sint16";
        case AX_ENGINE_DT_SINT8:
            return "sint8";
        case AX_ENGINE_DT_SINT32:
            return "sint32";
        case AX_ENGINE_DT_UINT32:
            return "uint32";
        case AX_ENGINE_DT_FLOAT64:
            return "float64";
        default:
            return "unknown";
    }
}

const char* describe_color_space(AX_ENGINE_COLOR_SPACE_T cs) {
    switch (cs) {
        case AX_ENGINE_CS_FEATUREMAP:
            return "FeatureMap";
        case AX_ENGINE_CS_BGR:
            return "BGR";
        case AX_ENGINE_CS_RGB:
            return "RGB";
        case AX_ENGINE_CS_RGBA:
            return "RGBA";
        case AX_ENGINE_CS_GRAY:
            return "GRAY";
        case AX_ENGINE_CS_NV12:
            return "NV12";
        case AX_ENGINE_CS_NV21:
            return "NV21";
        case AX_ENGINE_CS_YUV444:
            return "YUV444";
        case AX_ENGINE_CS_RAW8:
            return "RAW8";
        case AX_ENGINE_CS_RAW10:
            return "RAW10";
        case AX_ENGINE_CS_RAW12:
            return "RAW12";
        case AX_ENGINE_CS_RAW14:
            return "RAW14";
        case AX_ENGINE_CS_RAW16:
            return "RAW16";
        default:
            return "unknown";
    }
}

void print_tensor_info(const char* prefix, const AX_ENGINE_IOMETA_T* tensor) {
    printf("%s: %s\n", prefix, tensor->pName);
    
    printf("    Shape: [");
    for (uint32_t j = 0; j < tensor->nShapeSize; ++j) {
        printf("%d", (int)tensor->pShape[j]);
        if (j + 1 < tensor->nShapeSize) printf(", ");
    }
    printf("]\n");
    
    printf("    Layout: %s\n", describe_layout(tensor->eLayout));
    printf("    Data Type: %s\n", describe_data_type(tensor->eDataType));
    
    if (tensor->pExtraMeta) {
        printf("    Color Space: %s\n", describe_color_space(tensor->pExtraMeta->eColorSpace));
    }
    
    printf("    Size: %u bytes\n", tensor->nSize);
    printf("    Quantization Value: %u\n", tensor->nQuantizationValue);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return -1;
    }

    std::string model_path = argv[1];
    printf("Loading model: %s\n\n", model_path.c_str());

    // 1. Load model file
    FILE* fp = fopen(model_path.c_str(), "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open model file: %s\n", model_path.c_str());
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    size_t model_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    void* model_buffer = malloc(model_size);
    if (!model_buffer) {
        fprintf(stderr, "Error: Cannot allocate memory for model\n");
        fclose(fp);
        return -1;
    }

    size_t read_size = fread(model_buffer, 1, model_size, fp);
    fclose(fp);

    if (read_size != model_size) {
        fprintf(stderr, "Error: Failed to read model file\n");
        free(model_buffer);
        return -1;
    }

    printf("Model file size: %zu bytes\n\n", model_size);

    // 2. Initialize SYS module
    AX_S32 ret = AX_SYS_Init();
    if (ret != 0) {
        fprintf(stderr, "Error: AX_SYS_Init failed with code: 0x%X\n", ret);
        free(model_buffer);
        return -1;
    }

    // 3. Initialize ENGINE module (required before creating handle)
    AX_ENGINE_NPU_ATTR_T npu_attr;
    memset(&npu_attr, 0, sizeof(AX_ENGINE_NPU_ATTR_T));
    npu_attr.eHardMode = AX_ENGINE_VIRTUAL_NPU_DISABLE;
    ret = AX_ENGINE_Init(&npu_attr);
    if (ret != 0) {
        fprintf(stderr, "Error: AX_ENGINE_Init failed with code: 0x%X\n", ret);
        AX_SYS_Deinit();
        free(model_buffer);
        return -1;
    }

    // 4. Create engine handle
    AX_ENGINE_HANDLE handle = nullptr;
    ret = AX_ENGINE_CreateHandle(&handle, model_buffer, model_size);
    if (ret != 0 || !handle) {
        fprintf(stderr, "Error: AX_ENGINE_CreateHandle failed with code: 0x%X\n", ret);
        AX_SYS_Deinit();
        free(model_buffer);
        return -1;
    }

    // 5. Create context
    ret = AX_ENGINE_CreateContext(handle);
    if (ret != 0) {
        fprintf(stderr, "Error: AX_ENGINE_CreateContext failed with code: 0x%X\n", ret);
        AX_ENGINE_DestroyHandle(handle);
        AX_ENGINE_Deinit();
        AX_SYS_Deinit();
        free(model_buffer);
        return -1;
    }

    // 6. Get IO info
    AX_ENGINE_IO_INFO_T* io_info = nullptr;
    ret = AX_ENGINE_GetIOInfo(handle, &io_info);
    if (ret != 0 || !io_info) {
        fprintf(stderr, "Error: AX_ENGINE_GetIOInfo failed with code: 0x%X\n", ret);
        AX_ENGINE_DestroyHandle(handle);
        AX_SYS_Deinit();
        free(model_buffer);
        return -1;
    }

    // 6. Print model information
    printf("========================================\n");
    printf("Model Information\n");
    printf("========================================\n");
    printf("Max Batch Size: %d\n", io_info->nMaxBatchSize);
    printf("Support Dynamic Batch: %s\n", io_info->bDynamicBatchSize == AX_TRUE ? "Yes" : "No");
    printf("Number of Inputs: %u\n", io_info->nInputSize);
    printf("Number of Outputs: %u\n", io_info->nOutputSize);
    printf("\n");

    // 7. Print input tensor details
    printf("========================================\n");
    printf("Input Tensors\n");
    printf("========================================\n");
    for (uint32_t i = 0; i < io_info->nInputSize; ++i) {
        print_tensor_info("Input", &io_info->pInputs[i]);
        
        // Print image input dimensions specifically
        if (io_info->pInputs[i].nShapeSize >= 4) {
            AX_ENGINE_TENSOR_LAYOUT_T layout = io_info->pInputs[i].eLayout;
            if (layout == AX_ENGINE_TENSOR_LAYOUT_NCHW) {
                printf("    >>> Image Dimensions: Channel=%d, Height=%d, Width=%d\n",
                       (int)io_info->pInputs[i].pShape[1],
                       (int)io_info->pInputs[i].pShape[2],
                       (int)io_info->pInputs[i].pShape[3]);
            } else { // NHWC
                printf("    >>> Image Dimensions: Height=%d, Width=%d, Channel=%d\n",
                       (int)io_info->pInputs[i].pShape[1],
                       (int)io_info->pInputs[i].pShape[2],
                       (int)io_info->pInputs[i].pShape[3]);
            }
        }
        printf("\n");
    }

    // 8. Print output tensor details
    printf("========================================\n");
    printf("Output Tensors\n");
    printf("========================================\n");
    for (uint32_t i = 0; i < io_info->nOutputSize; ++i) {
        print_tensor_info("Output", &io_info->pOutputs[i]);
        printf("\n");
    }

    // 9. Cleanup
    AX_ENGINE_DestroyHandle(handle);
    AX_ENGINE_Deinit();
    AX_SYS_Deinit();
    free(model_buffer);

    printf("Done!\n");
    return 0;
}
