# OCR Recognition Component CMakeLists.txt

# Create library
add_library(ocr_rec_component STATIC
    ocr_rec_component.cpp
    ocr_rec_component.h
)

target_link_libraries(ocr_rec_component
    ${OpenCV_LIBS}
)

# Set include directories
target_include_directories(ocr_rec_component PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
