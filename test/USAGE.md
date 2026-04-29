# get_model_info 工具使用说明

## 概述
`get_model_info` 是一个用于从 axmodel 文件中提取模型输入输出尺寸信息的工具。它可以显示模型的详细张量信息，包括输入尺寸、数据类型、布局等。

## 编译成功信息
✅ 已成功为 AX630C 编译
- 编译目录：`test/build_ax630c/`
- 可执行文件：`test/build_ax630c/install/bin/get_model_info`
- 库文件：`test/build_ax630c/libax_skel.so`

## 使用方法

### 1. 在目标板上运行

将以下文件复制到目标板（AX630C）：
- `get_model_info` 可执行文件
- BSP SDK 中的库文件（libax_engine.so, libax_sys.so 等）
- 模型文件（.axmodel）

在目标板上执行：
```bash
./get_model_info /path/to/model.axmodel
```

### 2. 预期输出示例

```
Loading model: /path/to/ch_PP_OCRv3_det_npu.axmodel

Model file size: 1234567 bytes

========================================
Model Information
========================================
Max Batch Size: 1
Support Dynamic Batch: No
Number of Inputs: 1
Number of Outputs: 1

========================================
Input Tensors
========================================
Input: input
    Shape: [1, 3, 480, 480]
    Layout: NCHW
    Data Type: uint8
    Color Space: RGB
    Size: 691200 bytes
    Quantization Value: 128
    >>> Image Dimensions: Channel=3, Height=480, Width=480

========================================
Output Tensors
========================================
Output: output
    Shape: [1, 100, 4, 21]
    Layout: NCHW
    Data Type: float32
    Size: 33600 bytes

Done!
```

## 支持的芯片类型

编译时可以使用以下芯片类型：
- **AX620Q**: `./build.sh AX620Q`
- **AX630C**: `./build.sh AX630C`
- **AX650**: `./build.sh AX650`

## 文件结构

```
test/
├── get_model_info.cpp          # 源代码
├── CMakeLists.txt              # 编译配置
├── build.sh                    # 编译脚本
├── README.md                   # 使用说明
└── build_ax630c/               # AX630C 编译输出
    ├── CMakeFiles/
    ├── Makefile
    ├── libax_skel.so
    ├── get_model_info
    └── install/
        └── bin/
            └── get_model_info
```

## 技术细节

### 输入尺寸提取逻辑

工具通过以下步骤获取模型输入尺寸：

1. **加载模型文件**: 使用 `AX_ENGINE_CreateHandle` 加载 axmodel 文件
2. **创建上下文**: 调用 `AX_ENGINE_CreateContext` 初始化模型
3. **获取 IO 信息**: 通过 `AX_ENGINE_GetIOInfo` 获取张量信息
4. **解析形状**: 从 `AX_ENGINE_IO_INFO_T` 的 `pInputs[0].pShape` 读取维度
5. **判断布局**: 根据 `eLayout` 字段判断是 NHWC 还是 NCHW
   - NCHW: `[Batch, Channel, Height, Width]`
   - NHWC: `[Batch, Height, Width, Channel]`

### 关键 API

```cpp
AX_ENGINE_CreateHandle()    // 创建模型句柄
AX_ENGINE_CreateContext()   // 创建上下文
AX_ENGINE_GetIOInfo()       // 获取 IO 信息
AX_ENGINE_DestroyHandle()   // 销毁句柄
```

## 常见问题

### Q: 为什么不能在 PC 上运行？
A: 程序使用了 AXERA NPU SDK 的专有库（ax_engine, ax_sys），这些库依赖于 NPU 硬件，只能在目标板上运行。

### Q: 如何查看 PC 上的模型信息？
A: 如果没有目标板，可以：
1. 使用 Netron (https://netron.app/) 在线查看模型结构
2. 使用 Python 脚本解析模型文件头部信息（功能有限）

### Q: 编译时找不到 BSP SDK？
A: 确保 BSP SDK 已下载并位于正确目录：
- AX630C: `ax620q_bsp_sdk/msp/out/arm64_glibc/`
- AX620Q: `ax620q_bsp_sdk/msp/out/arm_uclibc/`
- AX650: `ax650n_bsp_sdk/msp/out/`

## 更新日志

- 2024-XX-XX: 初始版本，支持 AX630C/AX620Q/AX650
