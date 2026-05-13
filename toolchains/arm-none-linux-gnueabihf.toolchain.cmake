# Copyright (c) 2019-2023 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
#
# This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
# may not be copied or distributed in any isomorphic form without the prior
# written consent of Axera Semiconductor (Ningbo) Co., Ltd.
#
# Author: wanglusheng@axera-tech.com
#

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN_PREFIX "/usr/local/ARM-toolchain/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf")
set(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}/bin/arm-none-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PREFIX}/bin/arm-none-linux-gnueabihf-g++")
set(CMAKE_LINKER "${TOOLCHAIN_PREFIX}/bin/arm-none-linux-gnueabihf-ld")
set(CMAKE_AR "${TOOLCHAIN_PREFIX}/bin/arm-none-linux-gnueabihf-ar")
set(CMAKE_STRIP "${TOOLCHAIN_PREFIX}/bin/arm-none-linux-gnueabihf-strip")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
