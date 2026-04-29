# test_ocr_detection - OCR 文字检测模型测试工具

# 测试方式：
`./test_ocr_detection /root/models/pp_ocr/ch_PP_OCRv3_det_npu.axmodel ./test2_out.png ./ocr_det.txt ./ocr_det.png`

## 功能概述

测试 PaddleOCR 文字检测模型（ch_PP_OCRv3_det_npu.axmodel）的推理工具。

**主要功能**：
- 加载 OCR 检测模型
- 输入 640x480 图像进行推理
- 输出模型原始结果到 TXT 文件
- 可选保存热力图可视化

## 编译

```bash
cd test
./build.sh AX630C
```

编译后的可执行文件：`build_ax630c/install/bin/test_ocr_detection`

## 使用方法

### 基本用法

```bash
./build_ax630c/install/bin/test_ocr_detection <模型路径> <输入图像> <输出 TXT> [输出热力图]
```

### 参数说明

| 参数 | 说明 | 必需 |
|------|------|------|
| 模型路径 | ch_PP_OCRv3_det_npu.axmodel 文件路径 | 是 |
| 输入图像 | 输入图像（任意尺寸，会自动缩放到 640x480） | 是 |
| 输出 TXT | 输出文本文件路径 | 是 |
| 输出热力图 | 热力图可视化图像（可选） | 否 |

### 示例

```bash
# 基本使用
./build_ax630c/install/bin/test_ocr_detection \
    ../models/pp_ocr/ch_PP_OCRv3_det_npu.axmodel \
    input.jpg \
    output.txt

# 带热力图可视化
./build_ax630c/install/bin/test_ocr_detection \
    ../models/pp_ocr/ch_PP_OCRv3_det_npu.axmodel \
    input.jpg \
    output.txt \
    heatmap.png
```

## 模型信息

### 输入
- **Shape**: [1, 480, 640, 3]
- **Layout**: NHWC
- **数据类型**: uint8
- **颜色空间**: BGR

### 输出
- **Shape**: [1, 1, 480, 640]
- **数据类型**: float32
- **含义**: 每个像素位置的文字置信度（heatmap）

## 输出文件

程序生成三个输出文件：

### 1. 热力图数据 (TXT)

- **文件名**: `ocr_det.txt`
- **内容**: 完整的 40x30 热力图数据
- **格式**: 每行 40 个浮点数，共 30 行

### 2. 边界框数据 (TXT)

- **文件名**: `ocr_det.txt.boxes.txt`
- **内容**: 检测到的文字区域边界框
- **格式**:
```
# OCR Detection Bounding Boxes
# Format: x1, y1, x2, y2, confidence
# Image size: 640x480

Box 1: 120.00, 180.00, 320.00, 240.00 (conf: 0.856)
Box 2: 350.00, 190.00, 520.00, 235.00 (conf: 0.742)
```

### 3. 热力图可视化 (PNG)

- **文件名**: `ocr_det.png`
- **内容**: 
  - **绿色半透明层**: 表示文字置信度区域
  - **红色方框**: 检测到的文字边界框
  - **红色标签**: 每个方框的编号和置信度
- **用途**: 直观查看检测结果和边界框位置

## 工作原理

### 1. 模型加载

从文件加载 .axmodel 模型到内存，创建 NPU 推理上下文。

### 2. 图像预处理

- 加载输入图像
- 自动缩放到 640x480
- 保持 BGR 格式

### 3. NPU 推理

使用 AX_ENGINE_RunSyncV2 执行推理。

### 4. 结果分析

输出包括：
- 整体统计（min/max/mean）
- 逐行统计
- 高置信度点定位

## 输出示例

### TXT 文件片段

```
OCR Detection Model Output
==========================

Output Shape: [1, 1, 480, 640]
Data Type: float32
Total Elements: 307200

Statistics:
  Min: 0.000123
  Max: 0.987654
  Mean: 0.045678

Sample Values (first 100 elements):
-----------------------------------
[    0] = 0.001234
[    1] = 0.002345
...

Heatmap Data (480 rows x 640 cols):
-----------------------------------
Format: Each row shows min/max/mean for that row

Row   0: Min=0.0001, Max=0.5432, Mean=0.0234
Row   1: Min=0.0002, Max=0.6543, Mean=0.0345
...

High-Confidence Regions (value > 0.5):
--------------------------------------
  ( 123,  45): 0.7654
  ( 124,  45): 0.8123
  ( 125,  45): 0.7890
  ...
```

## 结果解读

### 输出含义

PaddleOCR 检测模型输出的是**概率热力图**：
- 每个像素值 (0-1) 表示该位置属于文字区域的概率
- 高概率区域（> 0.3）会以绿色半透明层显示
- 绿色越深，表示置信度越高
- 连续的高概率区域形成文字框

### 热力图可视化

**绿色叠加层**：
- 绿色区域 = 可能是文字的区域
- 透明度 50%，可以看到底下的原图
- 便于直观定位文字位置

**红色边界框**：
- 红色方框 = 算法检测到的文字区域
- 红色标签 = 方框编号和平均置信度
- 清晰显示每个文字区域的位置和范围

### 后续处理

这个工具只输出原始模型结果。完整的 OCR 流程还需要：

1. **二值化** - 将 heatmap 转为二值图
2. **连通域分析** - 找出连续的文字区域
3. **框提取** - 从连通域生成文字框坐标
4. **框筛选** - 去除太小/太长的框
5. **识别** - 将文字框送入识别模型

## 性能参考

在 AX630C 上的典型性能：
- 模型加载：~100ms
- 推理时间：~50-100ms
- 结果保存：~10ms

## 注意事项

1. **输入尺寸**: 图像会自动缩放到 640x480
2. **输出格式**: TXT 文件包含详细统计，便于调试
3. **热力图**: 可选，用于可视化文字区域
4. **内存使用**: 约 2MB 用于输入输出 buffer

## 常见问题

**Q: 为什么输出是热力图而不是文字框？**  
A: 这是检测模型的原始输出。需要后处理才能提取文字框。

**Q: 如何确定文字位置？**  
A: 查看 TXT 文件中的"High-Confidence Regions"部分，或使用热力图可视化。

**Q: 置信度阈值设多少合适？**  
A: 默认 0.5。可以根据实际情况调整（0.3-0.7 之间）。

**Q: 为什么有些文字区域置信度很低？**  
A: 可能是模糊、倾斜、特殊字体或背景复杂导致。

## 相关文件

- 模型文件：`../models/pp_ocr/ch_PP_OCRv3_det_npu.axmodel`
- 识别模型：`../models/pp_ocr/ch_PP_OCRv4_rec_npu.axmodel`

## 下一步

要完成完整 OCR，还需要：
1. 文字检测（本工具）
2. 文字框提取（后处理）
3. 文字识别（使用 rec 模型）
