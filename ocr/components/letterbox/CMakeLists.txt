# Letterbox Component CMakeLists.txt

# Collect source files
set(LETTERBOX_SRCS
    letterbox_component.cpp
)

set(LETTERBOX_HDRS
    letterbox_component.h
)

# Create component library (STATIC - compiled into executable)
add_library(letterbox_component STATIC ${LETTERBOX_SRCS})

# Set include directories for this component
target_include_directories(letterbox_component PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link dependencies
target_link_libraries(letterbox_component PUBLIC ${OpenCV_LIBS})
