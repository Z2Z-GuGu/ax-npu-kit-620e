cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(get_model_info)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_CXX_FLAGS "-fvisibility=hidden -g -O0")
    add_definitions(-D__AX_SKEL_DEBUG__)
elseif (CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_CXX_FLAGS "-fvisibility=hidden -O2 -fdata-sections -ffunction-sections")
endif()

# Define project root as absolute path
set(PROJECT_ROOT_DIR ${CMAKE_SOURCE_DIR}/..)

# Setup BSP paths based on chip type (same logic as msp_dependencies.cmake)
if (CHIP_AX650)
    message(STATUS "Chip type: AX650")
    add_definitions(-DCHIP_AX650)
    if(NOT BSP_MSP_DIR)
        set(BSP_MSP_DIR ${PROJECT_ROOT_DIR}/ax650n_bsp_sdk/msp/out)
    endif()
elseif (CHIP_AX630C)
    message(STATUS "Chip type: AX630C")
    add_definitions(-DCHIP_AX630C)
    if(NOT BSP_MSP_DIR)
        set(BSP_MSP_DIR ${PROJECT_ROOT_DIR}/ax620q_bsp_sdk/msp/out/arm64_glibc)
    endif()
elseif (CHIP_AX620Q)
    message(STATUS "Chip type: AX620Q")
    add_definitions(-DCHIP_AX620Q)
    if(NOT BSP_MSP_DIR)
        set(BSP_MSP_DIR ${PROJECT_ROOT_DIR}/ax620q_bsp_sdk/msp/out/arm_uclibc)
    endif()
else()
    message(FATAL_ERROR "FATAL: Chip type not set! Use -DCHIP_AX620Q=ON or -DCHIP_AX630C=ON or -DCHIP_AX650=ON")
endif()

# Check BSP directory exists
if(NOT EXISTS ${BSP_MSP_DIR})
    message(FATAL_ERROR "FATAL: BSP_MSP_DIR ${BSP_MSP_DIR} does not exist")
endif()

message(STATUS "BSP_MSP_DIR = ${BSP_MSP_DIR}")

set(MSP_INC_DIR ${BSP_MSP_DIR}/include)
set(MSP_LIB_DIR ${BSP_MSP_DIR}/lib)

list(APPEND MSP_LIBS
        ax_sys
        ax_proton
        ax_ivps
        ax_venc
        ax_engine
        ax_interpreter)

include_directories(${MSP_INC_DIR})
link_directories(${MSP_LIB_DIR})

include_directories(${PROJECT_ROOT_DIR}/inc)
include_directories(${PROJECT_ROOT_DIR}/src)
include_directories(${PROJECT_ROOT_DIR}/src/3rdparty/eigen3)

# Find OpenCV for letterbox_resize
if (CHIP_AX630C)
    set(OpenCV_DIR ${PROJECT_ROOT_DIR}/third-party/opencv/lib/arm64/glibc/cmake/opencv4)
elseif (CHIP_AX620Q)
    set(OpenCV_DIR ${PROJECT_ROOT_DIR}/third-party/opencv/lib/arm/glibc/cmake/opencv4)
elseif (CHIP_AX650)
    set(OpenCV_DIR ${PROJECT_ROOT_DIR}/third-party/opencv/lib/arm/glibc/cmake/opencv4)
endif()
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Build ax_skel library first
aux_source_directory(${PROJECT_ROOT_DIR}/src/api SRCS)
aux_source_directory(${PROJECT_ROOT_DIR}/src/mgr SRCS)
aux_source_directory(${PROJECT_ROOT_DIR}/src/utils SRCS)
aux_source_directory(${PROJECT_ROOT_DIR}/src/inference SRCS)
aux_source_directory(${PROJECT_ROOT_DIR}/src/pipeline SRCS)
aux_source_directory(${PROJECT_ROOT_DIR}/src/pipeline/hvcfp SRCS)
aux_source_directory(${PROJECT_ROOT_DIR}/src/tracker SRCS)

add_library(ax_skel SHARED ${SRCS})
target_link_libraries(ax_skel ${MSP_LIBS})

# Executables
add_executable(get_model_info get_model_info.cpp)
add_executable(letterbox_resize_sw letterbox_resize_sw.cpp)
add_executable(test_ocr_detection test_ocr_detection.cpp)

# Link libraries
target_link_libraries(get_model_info
    ax_skel
    ${MSP_LIBS}
    pthread
)

target_link_libraries(letterbox_resize_sw
    ${OpenCV_LIBS}
)

target_link_libraries(test_ocr_detection
    ax_skel
    ${MSP_LIBS}
    pthread
    ${OpenCV_LIBS}
)

# Set RPATH
set_target_properties(get_model_info PROPERTIES
    BUILD_RPATH "${MSP_LIB_DIR}"
    INSTALL_RPATH "${MSP_LIB_DIR}"
)

set_target_properties(letterbox_resize_sw PROPERTIES
    BUILD_RPATH "${MSP_LIB_DIR}"
    INSTALL_RPATH "${MSP_LIB_DIR}"
)

set_target_properties(test_ocr_detection PROPERTIES
    BUILD_RPATH "${MSP_LIB_DIR}"
    INSTALL_RPATH "${MSP_LIB_DIR}"
)

install(TARGETS get_model_info letterbox_resize_sw test_ocr_detection
    RUNTIME DESTINATION bin)
