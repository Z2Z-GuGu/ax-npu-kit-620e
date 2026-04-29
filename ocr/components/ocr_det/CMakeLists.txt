# OCR Detection Component CMakeLists.txt

# Collect source files
set(OCR_DET_SRCS
    ocr_det_component.cpp
)

set(OCR_DET_HDRS
    ocr_det_component.h
)

# Create component library (STATIC - compiled into executable)
add_library(ocr_det_component STATIC ${OCR_DET_SRCS})

# Set include directories for this component
target_include_directories(ocr_det_component PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link dependencies (OpenCV and AXERA SDK)
target_link_libraries(ocr_det_component PUBLIC 
    ${OpenCV_LIBS}
    ${MSP_LIBS}
    pthread
)
