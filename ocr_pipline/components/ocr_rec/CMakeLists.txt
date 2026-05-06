add_library(ocr_rec STATIC
    ocr_rec.cpp
)

target_include_directories(ocr_rec PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/components/ocr_image
)

# 链接必要的库（根据实际模型推理库调整）
# target_link_libraries(ocr_rec PUBLIC ...)
