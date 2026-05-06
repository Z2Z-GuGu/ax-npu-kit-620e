add_library(ocr_det STATIC
    ocr_det.cpp
)

target_include_directories(ocr_det PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/components/ocr_image
)

# 链接必要的库（根据实际模型推理库调整）
# target_link_libraries(ocr_det PUBLIC ...)
