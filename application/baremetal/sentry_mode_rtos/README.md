# NA900 车载哨兵模式视觉监控系统

## 项目概述

基于 Nuclei NA900 RISC-V 处理器的车载停车监控系统（哨兵模式），实现：
- **Level 1**: 5秒/帧低功耗监控，检测人员靠近
- **Level 2**: 5 FPS高帧率识别，检测危险动作（砸车、撬门、划车、攀爬）
- **自动录制**: 检测到危险时自动录制30秒视频证据

## 硬件要求

- **CPU**: NA900 core1（rv32imac / ilp32，带 DSP 扩展，无 F/D 浮点）
- **内存**: 
  - ILM: 64KB（指令内存）
  - DLM: 64KB（数据内存，静态分配）
  - DDR: 512MB+（L2 模型、视频缓冲、Tensor Arena 1MB）
- **详细规格**: 见 [doc/design/哨兵模式详细设计文档.md](doc/design/哨兵模式详细设计文档.md) §1.2
- **外设**: 摄像头、SD卡、蜂鸣器、4G/WiFi模块(可选)

## 软件架构

```
src/
├── main.c              # 主程序入口，任务创建
├── tasks.c             # FreeRTOS任务实现
├── memory_manager.c    # DDR内存管理和模型加载
├── ai_engine.cpp       # TFLite Micro封装
├── hal.c              # 硬件抽象层
├── storage.c          # FatFs文件系统操作
└── inc/
    └── sentry_mode.h  # 主头文件
```

### 任务设计

| 任务 | 优先级 | 职责 |
|------|--------|------|
| AI主任务 | 3 | 状态机管理、推理 |
| 视频录制 | 2 | 高清视频采集 |
| 文件写入 | 1 | SD卡写入（后台） |
| 警报处理 | 4 | 声光警报触发 |
| 系统监控 | 1 | 统计信息打印 |

## 构建方法

```bash
cd application/baremetal/sentry_mode_rtos

# 编译
make CORE=core1_na900 ARCH_EXT=_xxldsp all

# 烧录
make CORE=core1_na900 upload

# 调试
make CORE=core1_na900 debug
```

## 模型部署

### 模型格式

- **Level 1模型**: 人形检测（~1.5MB，int8量化）
- **Level 2模型**: 危险动作识别（~101KB，动态量化）

### 镜像分区（建议）

| 区域 | 建议大小 | 说明 |
|------|---------|------|
| 应用程序代码 | ~128KB | FreeRTOS + TFLM 运行时 |
| Level 1 模型 | ~1.5MB | SSD MobileNet v2，常驻 |
| Level 2 模型 | ~101KB | CNN+LSTM，按需加载 |

> 具体烧录地址以实际 Flash 分区方案为准。当前构建使用 `gcc_evalsoc_ddr.ld`，固件镜像位于 DDR ROM 区（`0xA0000000`）。

### 模型转换

```python
# TensorFlow Lite量化示例
import tensorflow as tf

converter = tf.lite.TFLiteConverter.from_saved_model(model_path)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_data_gen
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8

tflite_model = converter.convert()

# 保存为C数组
with open('model.h', 'w') as f:
    f.write('const unsigned char model[] = {')
    f.write(','.join(str(b) for b in tflite_model))
    f.write('};')
```

## 运行流程

1. **系统启动**: 初始化硬件，加载Level 1模型
2. **Level 1监控**: 5秒周期检测人员
3. **Level 2触发**: 检测到人后加载大模型，5 FPS识别危险动作
4. **Level 3持续监控**: 检测到危险后进入L3，持续5 FPS监控+录像
5. **状态回退**:
   - 危险解除但人还在 → **退回L2**继续监控
   - 人离开超过15秒 → **退回L1**
   - L3运行超过60秒 → **退回L1**
6. **自动恢复**: 系统根据场景自动在各等级间切换

## 配置选项

在`sentry_mode.h`中修改：

```c
#define LEVEL1_INTERVAL_MS          5000    // Level 1周期 (5秒)
#define LEVEL2_FPS                  5       // Level 2帧率 (5 FPS)
#define LEVEL2_MAX_DURATION_MS      10000   // Level 2超时 (10秒)
#define LEVEL3_FPS                  5       // Level 3帧率 (5 FPS)
#define LEVEL3_MAX_DURATION_MS      60000   // Level 3最大持续时间 (60秒)
#define LEVEL3_PERSON_LOST_TIMEOUT_MS 15000 // L3人员离开超时 (15秒)
#define LEVEL3_DANGER_CLEARED_FRAMES 15     // 危险解除帧数 (3秒)
```

## 三级状态机说明

### Level 1: 低功耗监控
- **周期**: 5秒/帧
- **任务**: 人形检测
- **触发**: 连续2帧检测到人 → 激活L2

### Level 2: 高帧率危险识别
- **周期**: 200ms/帧 (5 FPS)
- **任务**: 危险动作识别（砸车/撬门/划车/攀爬）
- **退出条件**:
  1. 检测到危险 → 进入L3
  2. 10秒无危险且人离开 → 返回L1

### Level 3: 持续监控+录像
- **周期**: 200ms/帧 (5 FPS)
- **任务**: 持续危险检测+视频录制
- **退出条件**:
  1. **危险解除3秒+人还在** → **退回L2**（继续监控）
  2. 人员离开超过15秒 → 返回L1
  3. 达到最大持续时间60秒 → 返回L1

## 调试信息

系统每10秒输出统计信息：
```
[I] Level1 frames: 123
[I] Level2 activations: 5
[I] Danger detections: 1
[I] Videos: 1
```

## 注意事项

1. DDR初始化: 确保BootROM已初始化DDR，或手动初始化
2. SD卡格式: 建议使用FAT32或exFAT
3. 模型路径: 根据实际Flash地址修改`sentry_mode.h`
4. 摄像头驱动: 根据实际硬件实现`hal.c`中的函数

## 性能指标

- Level 1推理: ~100ms (NA900 core1 + DSP)
- Level 2推理: ~200ms (DDR模型)
- 系统功耗: 5mW(Level1) / 100mW(Level2)
- 响应延迟: <1秒 (检测到人→启动Level 2)

## 许可证

Apache-2.0

## 参考文档

- [`doc/design/哨兵模式详细设计文档.md`](doc/design/哨兵模式详细设计文档.md): 详细设计文档
- [`doc/README.md`](doc/README.md): 项目文档索引
- Nuclei SDK文档: https://doc.nucleisys.com/nuclei_sdk
- NMSIS-NN文档: https://doc.nucleisys.com/nmsis/nn
