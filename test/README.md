# Model Information Tool / 模型信息工具

这个工具用于从 axmodel 文件中提取模型的输入输出尺寸信息。
This tool extracts model input/output dimensions from axmodel files.

## C++ 程序（需要完整 BSP SDK，在目标板上运行）
## C++ Program (Requires complete BSP SDK, run on target board)

### 编译方法 / Build

使用构建脚本（推荐）/ Using build script (recommended):

```bash
cd test

# AX630C 编译 / Build for AX630C
./build.sh AX630C

# 或者 AX620Q / Or for AX620Q
./build.sh AX620Q

# 或者 AX650 / Or for AX650
./build.sh AX650
```

手动编译 / Manual build:

```bash
cd test
mkdir -p build_ax630c && cd build_ax630c

# AX630C 示例 / Example for AX630C
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../../toolchains/aarch64-none-linux-gnu.toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCHIP_AX630C=ON

make -j4
make install
```

### 使用方法 / Usage

编译后的可执行文件位于：`build_ax630c/install/bin/get_model_info`

```bash
./get_model_info <模型路径>

# 示例 / Examples
./get_model_info ../models/pp_ocr/ch_PP_OCRv3_det_npu.axmodel
./get_model_info ../models/pp_ocr/ch_PP_OCRv4_rec_npu.axmodel
./get_model_info ../models/ax_ax620E_bv_algo_model_V1.1.6.axmodel
```

## 输出信息 / Output Information

程序会输出以下信息：
Program outputs:

- ✅ 模型文件路径和大小 / Model file path and size
- ✅ 最大 Batch Size / Max Batch Size
- ✅ 是否支持动态 Batch / Dynamic Batch support
- ✅ 输入/输出张量数量 / Number of input/output tensors
- ✅ 每个张量的详细信息 / Details for each tensor:
  - 张量名称 / Tensor name
  - Shape（维度）/ Shape dimensions
  - 数据布局 / Data layout (NHWC/NCHW)
  - 数据类型 / Data type (uint8/float32, etc.)
  - 颜色空间 / Color space (for images)
  - 内存大小 / Memory size
  - 量化参数 / Quantization value
- ✅ **图像输入尺寸** / **Image input dimensions** (Height, Width, Channels)

## 注意事项 / Notes

1. **交叉编译程序** 需要在目标板（AX620E/AX630C/AX650 等）上运行
   **Cross-compiled program** requires target board (AX620E/AX630C/AX650, etc.)

2. 需要完整的 BSP SDK（包含 ax_engine, ax_sys 等库）
   Requires complete BSP SDK (including ax_engine, ax_sys, etc.)

3. 确保模型文件路径正确
   Ensure model file path is correct

4. 需要访问 NPU 硬件，可能需要 root 权限
   Requires NPU hardware access, may need root privileges

5. 不能在 PC 上直接运行（ARM64 架构）
   Cannot run directly on PC (ARM64 architecture)
