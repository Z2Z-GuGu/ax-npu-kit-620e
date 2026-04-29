# OCR 识别功能使用说明

## 概述

新增了 OCR 文字识别功能，支持输入 40x320 的 PNG/JPG 图像，加载 PaddleOCR 识别模型和字典文件，输出识别的文字结果。

## 新增文件

### 1. OCR 识别组件
```
ocr/components/ocr_rec/
├── CMakeLists.txt              # 组件编译配置
├── ocr_rec_component.h         # 组件头文件
└── ocr_rec_component.cpp       # 组件实现
```

### 2. 测试程序
```
ocr/
├── test_ocr_recognition.cpp    # 主测试程序
├── test_ocr_recognition.sh     # 测试脚本
└── README_OCR_RECOGNITION.md   # 详细文档
```

## 编译方法

```bash
cd ocr
./build.sh AX630C
```

编译后的可执行文件：
- `build_ax630c/install/bin/test_ocr_recognition` (4.7MB)

## 使用方法

### 命令行参数

```bash
./build_ax630c/install/bin/test_ocr_recognition \
    <模型路径> \
    <字典路径> \
    <输入图像> \
    <输出结果文件>
```

### 参数说明

| 参数 | 说明 | 示例 |
|------|------|------|
| 模型路径 | OCR 识别模型文件 | `ch_PP_OCRv4_rec_npu.axmodel` |
| 字典路径 | 字符字典文件 | `ppocr_keys_v1.txt` |
| 输入图像 | 40x320 的 PNG/JPG 图像 | `text.png` |
| 输出结果 | 识别结果文本文件 | `result.txt` |

### 使用示例

```bash
# 基本使用
./build_ax630c/install/bin/test_ocr_recognition \
    ../models/pp_ocr/ch_PP_OCRv4_rec_npu.axmodel \
    ../models/pp_ocr/ppocr_keys_v1.txt \
    text_image.png \
    recognition_result.txt

# 运行测试脚本
./test_ocr_recognition.sh
```

## 输入要求

### 图像规格
- **尺寸**: 40x320 像素（推荐）
- **格式**: PNG, JPG, JPEG, BMP
- **颜色**: 灰度或彩色（自动转换）

### 图像内容
- 单行文字
- 文字水平排列
- 文字清晰，对比度高
- 背景简洁

## 输出结果

### 输出文件格式

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
...

Summary:
  Text length: 11 characters
  Confidence: 92.34%
  Status: Success
```

## 技术细节

### 模型信息
- **输入**: [1, 40, 320, 1] NHWC, float32, 灰度归一化
- **输出**: [1, 6624, 1, 1] float32, CTC 输出

### 解码算法
使用 CTC（Connectionist Temporal Classification）解码：
1. 对每个时间步应用 softmax
2. 跳过空白字符（索引 0）
3. 去除连续重复字符
4. 拼接成最终文本

### 置信度
- 字符级置信度：softmax 概率
- 平均置信度：所有字符置信度的平均值

## 完整 OCR 流程

完整的 OCR 系统包括检测和识别两个阶段：

```
1. 文字检测 (test_ocr_detection)
   输入：原图（任意尺寸）
   输出：文字区域边界框

2. 文字裁剪
   输入：原图 + 边界框
   输出：40x320 文字图像

3. 文字识别 (test_ocr_recognition)
   输入：40x320 文字图像
   输出：识别的文字内容
```

## 相关文件位置

```
/home/bugu/NanoKVM_Pro/ax-npu-kit-620e/
├── ocr/
│   ├── components/ocr_rec/          # OCR 识别组件
│   ├── test_ocr_recognition.cpp     # 测试程序
│   ├── test_ocr_recognition.sh      # 测试脚本
│   └── README_OCR_RECOGNITION.md    # 详细文档
└── models/pp_ocr/
    ├── ch_PP_OCRv4_rec_npu.axmodel  # 识别模型
    └── ppocr_keys_v1.txt            # 字典文件
```

## 与检测模型配合使用

```bash
# 1. 检测文字区域
./test_ocr_detection \
    ch_PP_OCRv3_det_npu.axmodel \
    input.jpg \
    det_result.txt \
    det_heatmap.png

# 2. 从 det_result.txt.boxes.txt 获取边界框
# 3. 裁剪文字区域为 40x320 图像

# 4. 识别文字
./test_ocr_recognition \
    ch_PP_OCRv4_rec_npu.axmodel \
    ppocr_keys_v1.txt \
    cropped_text.png \
    rec_result.txt
```

## 性能参考

在 AX630C 芯片上：
- 推理时间：~50-100ms
- 内存占用：~10MB

## 参考资料

- 详细文档：`README_OCR_RECOGNITION.md`
- 检测模型文档：`../test/README_OCR_DETECTION.md`
- PaddleOCR: https://github.com/PaddlePaddle/PaddleOCR
