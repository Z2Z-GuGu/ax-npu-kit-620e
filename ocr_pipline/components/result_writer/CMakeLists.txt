cmake_minimum_required(VERSION 3.10)
project(result_writer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_ROOT_DIR ${CMAKE_SOURCE_DIR}/../..)

# 配置芯片类型和 BSP 路径
if (CHIP_AX650)
    add_definitions(-DCHIP_AX650)
    if(NOT BSP_MSP_DIR)
        set(BSP_MSP_DIR ${PROJECT_ROOT_DIR}/ax650n_bsp_sdk/msp/out)
    endif()
elseif (CHIP_AX630C)
    add_definitions(-DCHIP_AX630C)
    if(NOT BSP_MSP_DIR)
        set(BSP_MSP_DIR ${PROJECT_ROOT_DIR}/ax620q_bsp_sdk/msp/out/arm64_glibc)
    endif()
elseif (CHIP_AX620Q)
    add_definitions(-DCHIP_AX620Q)
    if(NOT BSP_MSP_DIR)
        set(BSP_MSP_DIR ${PROJECT_ROOT_DIR}/ax620q_bsp_sdk/msp/out/arm_uclibc)
    endif()
elseif (CHIP_AX620QE)
    add_definitions(-DCHIP_AX620QE)
    if(NOT BSP_MSP_DIR)
        set(BSP_MSP_DIR ${PROJECT_ROOT_DIR}/ax620q_bsp_sdk/msp/out/arm_glibc)
    endif()
endif()

if(NOT EXISTS ${BSP_MSP_DIR})
    message(FATAL_ERROR "FATAL: BSP_MSP_DIR ${BSP_MSP_DIR} does not exist")
endif()

message(STATUS "BSP_MSP_DIR = ${BSP_MSP_DIR}")

set(MSP_INC_DIR ${BSP_MSP_DIR}/include)
set(MSP_LIB_DIR ${BSP_MSP_DIR}/lib)

# 添加库
add_library(result_writer STATIC result_writer.cpp)

# 包含目录
target_include_directories(result_writer PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_ROOT_DIR}/components/ocr_det
    ${PROJECT_ROOT_DIR}/components/ocr_rec
)

# 链接依赖库
target_link_libraries(result_writer
    ocr_det
    ocr_rec
)
