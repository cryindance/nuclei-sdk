/*
 * L2 危险动作识别模型 (CNN + LSTM)
 * 输入: (1, 8, 64, 64, 3) INT8
 * 输出: (1, 5) INT8 — 5类危险动作
 * 模型大小: 101KB
 * 生成: python3 train.py && python3 quantize.py
 */

#ifndef __L2_MODEL_H__
#define __L2_MODEL_H__

extern const unsigned char _home_jingo_work_l2_training_l2_model_quant_tflite[];
extern const unsigned int _home_jingo_work_l2_training_l2_model_quant_tflite_len;

#endif
