# letterbox_resize_sw - 图像缩放工具

## 功能概述

纯软件实现的图像等比缩放工具，使用 OpenCV 库，不依赖任何硬件加速器。

**主要功能**：
- 等比缩放到目标尺寸（默认 640x480）
- 自动添加黑边保持宽高比
- 支持 JPG/PNG/BMP/TIFF 格式

## 编译

```bash
cd test
./build.sh AX630C
```

编译后的可执行文件：`build_ax630c/install/bin/letterbox_resize_sw`

## 使用方法

### 基本用法（默认 640x480）

```bash
./build_ax630c/install/bin/letterbox_resize_sw input.jpg output.png
```

### 指定输出尺寸

```bash
./build_ax630c/install/bin/letterbox_resize_sw input.png output.jpg 640 480
```

### 自定义尺寸

```bash
./build_ax630c/install/bin/letterbox_resize_sw input.jpg output.png 800 600
```

## 参数说明

| 参数 | 说明 | 必需 | 默认值 |
|------|------|------|--------|
| input_image | 输入图像路径 | 是 | - |
| output_image | 输出图像路径 | 是 | - |
| width | 目标宽度 | 否 | 640 |
| height | 目标高度 | 否 | 480 |

## 支持格式

**输入**: JPG, JPEG, PNG, BMP, TIFF  
**输出**: JPG, JPEG, PNG, BMP, TIFF（由扩展名自动决定）

## 工作原理

### 等比缩放

- **源图像更宽** → 按宽度缩放，上下加黑边
- **源图像更高** → 按高度缩放，左右加黑边

### 示例

```
输入：1920x1080 (16:9)
目标：640x480 (4:3)

16:9 > 4:3，按宽度缩放：
缩放后：640x360
黑边：上下各 60 像素
输出：640x480（含黑边）
```

## 应用示例

### 批量转换 JPG 为 PNG

```bash
for img in *.jpg; do
    ./build_ax630c/install/bin/letterbox_resize_sw "$img" "${img%.jpg}.png" 640 480
done
```

### 批量转换 PNG 为 JPG

```bash
for img in *.png; do
    ./build_ax630c/install/bin/letterbox_resize_sw "$img" "${img%.png}.jpg" 640 480
done
```

### 处理不同尺寸的图片

```bash
# 横屏照片
./build_ax630c/install/bin/letterbox_resize_sw landscape.jpg landscape_640x480.png

# 竖屏照片
./build_ax630c/install/bin/letterbox_resize_sw portrait.jpg portrait_480x640.png 480 640

# 正方形照片
./build_ax630c/install/bin/letterbox_resize_sw square.png square_640x640.jpg 640 640
```

## 技术细节

- **插值算法**: OpenCV INTER_AREA（高质量缩小）
- **颜色空间**: BGR
- **JPEG 质量**: 95%
- **PNG 压缩**: 级别 3（快速、高质量）

## 性能参考

在 AX630C 上的典型性能：
- 1920x1080 → 640x480: ~50-100ms
- 1280x720 → 640x480: ~20-50ms
- 640x480 → 640x480: ~5-10ms

## 注意事项

1. **奇数尺寸处理**: 如果图片尺寸是奇数，会自动调整为偶数（因为某些图像处理算法要求）
2. **黑边**: 为保持宽高比，比例不同时会添加黑边，这是正常现象
3. **格式选择**: 输出格式由文件扩展名决定，与输入格式无关

## 常见问题

**Q: 为什么输出有黑边？**  
A: 为保持原始宽高比不变形，比例不同时会添加黑边。

**Q: 可以去除黑边吗？**  
A: 可以调整目标尺寸匹配输入比例，或修改代码使用拉伸模式（不推荐）。

**Q: 支持视频处理吗？**  
A: 不支持，仅处理单张图片。

**Q: 可以在 PC 上运行吗？**  
A: 可以，只需安装 OpenCV 即可跨平台运行。

## 相关工具

- `get_model_info` - 查看 axmodel 模型的输入输出尺寸
