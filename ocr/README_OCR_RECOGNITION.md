# test_ocr_recognition - OCR 文字识别模型测试工具

# 测试方式：
`./test_ocr_recognition /root/models/pp_ocr/ch_PP_OCRv4_rec_npu.axmodel /root/models/pp_ocr/ppocr_keys_v1.txt ./text.png ./ocr_rec_result.txt`

## 功能概述

测试 PaddleOCR 文字识别模型（ch_PP_OCRv4_rec_npu.axmodel）的推理工具。

**主要功能**：
- 加载 OCR 识别模型和字典文件
- 输入 40x320 图像进行推理（支持 PNG/JPG 格式）
- 使用 CTC 解码输出识别文字
- 输出识别结果和置信度到 TXT 文件

## 编译

```bash
cd ocr
./build.sh AX630C
```

编译后的可执行文件：`build_ax630c/install/bin/test_ocr_recognition`

## 使用方法

### 基本用法

```bash
./build_ax630c/install/bin/test_ocr_recognition <模型路径> <字典路径> <输入图像> <输出 TXT>
```

### 参数说明

| 参数 | 说明 | 必需 |
|------|------|------|
| 模型路径 | ch_PP_OCRv4_rec_npu.axmodel 文件路径 | 是 |
| 字典路径 | ppocr_keys_v1.txt 字典文件路径 | 是 |
| 输入图像 | 输入图像（40x320，PNG/JPG，会自动缩放） | 是 |
| 输出 TXT | 输出文本文件路径 | 是 |

### 示例

```bash
# 基本使用
./build_ax630c/install/bin/test_ocr_recognition \
    ../models/pp_ocr/ch_PP_OCRv4_rec_npu.axmodel \
    ../models/pp_ocr/ppocr_keys_v1.txt \
    text_image.png \
    recognition_result.txt
```

## 模型信息

### 输入
- **Shape**: [1, 40, 320, 1]
- **Layout**: NHWC
- **数据类型**: float32
- **颜色空间**: 灰度图，归一化到 [0, 1]

### 输出
- **Shape**: [1, 6624, 1, 1] 或 [1, sequence_length, num_classes, 1]
- **数据类型**: float32
- **含义**: CTC 输出，每个时间步的字符合义概率分布

## 输出文件

### 识别结果 (TXT)

- **文件名**: `ocr_rec_result.txt`
- **内容**: 
  - 识别的文字内容
  - 平均置信度
  - 每个字符的详细信息（索引、字符、置信度）
  - 执行状态

- **格式**:
```
OCR Recognition Result
======================

Recognized Text:
Hello World

Average Confidence: 0.9234

Character Details:
------------------
Index	Char	Confidence
0	H	0.9523
1	e	0.9412
2	l	0.9345
3	l	0.9387
4	o	0.9256
...

Summary:
  Text length: 11 characters
  Confidence: 92.34%
  Status: Success
```

## 输入图像要求

### 尺寸要求
- **推荐尺寸**: 40x320 像素
- **最小尺寸**: 10x80 像素
- **最大尺寸**: 160x1280 像素（会自动缩放到 40x320）

### 格式要求
- **支持格式**: PNG, JPG, JPEG, BMP
- **颜色模式**: 灰度图或彩色图（会自动转换为灰度）
- **内容**: 单行文字图像，文字应水平排列

### 图像预处理建议
为了获得最佳识别效果，建议输入图像满足：
1. 文字清晰，对比度高
2. 文字大小适中，高度占图像的 50%-80%
3. 文字水平排列，无明显倾斜
4. 背景简洁，无干扰元素

## 测试脚本

提供了自动化测试脚本：

```bash
# 运行测试
./test_ocr_recognition.sh
```

脚本会自动：
1. 检查编译产物和模型文件
2. 创建测试图像（40x320）
3. 运行识别测试
4. 显示识别结果

## 与检测模型配合使用

OCR 识别通常与检测模型配合使用，完整的 OCR 流程为：

1. **文字检测**: 使用 `test_ocr_detection` 检测图像中的文字区域
2. **文字裁剪**: 从原图中裁剪出文字区域
3. **文字识别**: 使用 `test_ocr_recognition` 识别裁剪出的文字

示例流程：
```bash
# 1. 检测文字区域
./test_ocr_detection \
    ch_PP_OCRv3_det_npu.axmodel \
    input.jpg \
    det_result.txt \
    det_heatmap.png

# 2. 裁剪文字区域（需要额外工具）
# 从 det_result.txt.boxes.txt 获取边界框
# 从原图裁剪出 40x320 的文字图像

# 3. 识别文字
./test_ocr_recognition \
    ch_PP_OCRv4_rec_npu.axmodel \
    ppocr_keys_v1.txt \
    cropped_text.png \
    rec_result.txt
```

## 技术细节

### CTC 解码

本工具使用 CTC（Connectionist Temporal Classification）解码算法：

1. **输出处理**: 对每个时间步的输出应用 softmax，得到字符概率
2. **去重**: 跳过连续重复的字符
3. **去空白**: 跳过空白字符（索引 0）
4. **拼接**: 将剩余字符拼接成最终文本

### 置信度计算

- **字符置信度**: 每个字符的 softmax 概率
- **平均置信度**: 所有字符置信度的算术平均值

### 字典文件

字典文件（ppocr_keys_v1.txt）包含所有可识别的字符，每行一个字符：
- 第 0 行：空白字符（用于 CTC）
- 第 1-6623 行：中英文字符、标点符号等

## 常见问题

### Q: 识别结果为空？
A: 可能原因：
- 输入图像质量差，文字不清晰
- 文字尺寸过小或过大
- 文字倾斜角度过大
- 模型输出置信度低，CTC 解码过滤掉了所有字符

### Q: 识别结果不准确？
A: 建议：
- 提高输入图像质量，确保文字清晰
- 调整文字大小，使其高度占图像的 50%-80%
- 确保文字水平排列
- 检查字典文件是否正确加载

### Q: 如何支持更多字符？
A: 需要：
1. 重新训练模型，扩展字符集
2. 更新字典文件，添加新字符
3. 确保模型和字典的字符集一致

## 性能参考

在 AX630C 芯片上的性能：
- **推理时间**: ~50-100ms（取决于输入尺寸）
- **内存占用**: ~10MB

## 相关文件

- **模型文件**: `../models/pp_ocr/ch_PP_OCRv4_rec_npu.axmodel`
- **字典文件**: `../models/pp_ocr/ppocr_keys_v1.txt`
- **检测模型**: `../models/pp_ocr/ch_PP_OCRv3_det_npu.axmodel`

## 参考资料

- [PaddleOCR 官方文档](https://github.com/PaddlePaddle/PaddleOCR)
- [CTC 解码算法详解](https://distill.pub/2017/ctc/)
