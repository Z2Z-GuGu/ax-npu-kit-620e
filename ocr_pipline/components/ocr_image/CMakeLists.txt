add_library(ocr_image STATIC
    ocr_image.cpp
)

target_include_directories(ocr_image PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接 OpenCV
find_package(OpenCV REQUIRED)
target_link_libraries(ocr_image PUBLIC ${OpenCV_LIBS})
