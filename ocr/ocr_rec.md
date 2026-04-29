root@kvm-cccd:~/ocr# ./test_ocr_recognition /root/models/pp_ocr/ch_PP_OCRv4_rec_npu.axmodel ./test.png_ocr/text_058.png ./result.txt /root/models/pp_ocr/ppocr_keys_v1.txt 
========================================
OCR Text Recognition Model Tester
========================================
Model: /root/models/pp_ocr/ch_PP_OCRv4_rec_npu.axmodel
Input: ./test.png_ocr/text_058.png
Output: ./result.txt
Dictionary: /root/models/pp_ocr/ppocr_keys_v1.txt

Dictionary loaded: /root/models/pp_ocr/ppocr_keys_v1.txt (6623 characters)
Model loaded: /root/models/pp_ocr/ch_PP_OCRv4_rec_npu.axmodel (3152722 bytes)

Model IO Info:
  Inputs: 1
  Outputs: 1

Input Tensor:
  Name: x
  Shape: [1, 48, 320, 3]

Output Tensor:
  Name: softmax_11.tmp_0
  Shape: [1, 40, 6625]

Loading input image: ./test.png_ocr/text_058.png
Original image size: 320x48
Input shape parsed: height=48, width=320, channels=3
Preprocessing image (resize to 320x48, channels=3)...

Allocating IO buffers...
Input buffer 0: phy=0x73c42000, vir=0xffffadf54000, size=46080
Output buffer 0: phy=0x73c4e000, vir=0xffffacc56000, size=1060000
Copying image data to input buffer...
Input data size: 46080 bytes (48x320x3 uint8)

Running inference...
Inference completed!

Output shape: [1, 40, 6625]
Total output elements: 265000

Decoding CTC output (sequence_length=40, num_classes=6625)...

Debug: First 10 character positions:
  t=0: model_index=0, dict_index=-1, value=0.9926 -> (blank)
  t=1: model_index=0, dict_index=-1, value=0.9926 -> (blank)
  t=2: model_index=0, dict_index=-1, value=0.9794 -> (blank)
  t=3: model_index=3589, dict_index=3588, value=0.9794 -> char='N'
  t=4: model_index=0, dict_index=-1, value=0.9971 -> (blank)
  t=5: model_index=0, dict_index=-1, value=0.9971 -> (blank)
  t=6: model_index=4544, dict_index=4543, value=0.9971 -> char='a'
  t=7: model_index=0, dict_index=-1, value=0.9971 -> (blank)
  t=8: model_index=0, dict_index=-1, value=0.9926 -> (blank)
  t=9: model_index=4547, dict_index=4546, value=0.9882 -> char='n'

Character confidences: 0.9794 0.9971 0.9882 0.9971 0.7279 0.5382 0.9441 0.8691 0.9618 0.9926 0.9971 
Sum: 9.9926, Count: 11, Average: 0.9084

========================================
Recognition Result:
========================================
Text: NanokVMcube
Confidence: 90.84%
Characters: 11
========================================

Result saved to: ./result.txt

Cleaning up...

========================================
Done!
========================================
root@kvm-cccd:~/ocr# 