/******************************************************************************
 * @file    sentry_mode.h
 * @brief   车载哨兵模式视觉监控系统 - 主头文件
 * @version 1.0
 * @date    2025-03-26
 ******************************************************************************/

#ifndef __SENTRY_MODE_H__
#define __SENTRY_MODE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "nuclei_sdk_soc.h"

// FreeRTOS头文件
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "event_groups.h"

// FatFs头文件（条件编译，SDK未提供时禁用）
#ifndef NO_STORAGE
#include "ff.h"
#else
/* FatFs stubs for compilation without storage */
typedef unsigned int UINT;
typedef struct { char dummy[64]; } FIL;
#define FA_READ         0x01
#define FA_WRITE        0x02
#define FA_OPEN_EXISTING 0x00
#define FA_CREATE_NEW   0x04
#define FA_CREATE_ALWAYS 0x08
#define FA_OPEN_ALWAYS  0x10
#define FR_OK           0
#define FRESULT         int
#endif

// TensorFlow Lite Micro（条件编译，SDK未提供时禁用）
#ifndef NO_AI_ENGINE
typedef struct tflite_MicroInterpreter { void* impl; } tflite_MicroInterpreter;
extern tflite_MicroInterpreter *g_interpreter;
#else
/* TFLite stubs */
typedef struct tflite_MicroInterpreter { void* impl; } tflite_MicroInterpreter;
#define g_interpreter NULL
#endif

/******************************************************************************
 * 系统配置常量
 ******************************************************************************/

#define LEVEL1_INTERVAL_MS          5000
#define LEVEL1_INPUT_WIDTH          224
#define LEVEL1_INPUT_HEIGHT         224
#define LEVEL1_INPUT_CHANNELS       3
#define LEVEL1_MODEL_SIZE           (1573512)
#define NMS_CONFIDENCE_THRESHOLD  0.25f

// Level 2配置（高帧率危险识别）
#define LEVEL2_FPS                  5       // 5帧/秒
#define LEVEL2_FRAME_PERIOD_MS      200     // 200ms周期
#define LEVEL2_HISTORY_FRAMES       8       // 时序帧数
#define LEVEL2_MAX_DURATION_MS      10000   // 最大运行10秒（无危险时）
#define LEVEL2_INPUT_WIDTH          64
#define LEVEL2_INPUT_HEIGHT         64
#define LEVEL2_INPUT_CHANNELS       3
#define LEVEL2_MODEL_SIZE           (300 * 1024)  // 300KB
#define LEVEL2_ACTION_THRESHOLD     30      // 动作识别阈值

// Level 3配置（持续监控+录像）
#define LEVEL3_FPS                  5       // 5帧/秒（持续）
#define LEVEL3_FRAME_PERIOD_MS      200     // 200ms周期
#define LEVEL3_MAX_DURATION_MS      60000   // 最长持续60秒（安全考虑）
#define LEVEL3_PERSON_LOST_TIMEOUT_MS 15000 // 人员离开15秒后停止
#define LEVEL3_INPUT_WIDTH          64
#define LEVEL3_INPUT_HEIGHT         64
#define LEVEL3_INPUT_CHANNELS       3

// 视频录制配置
#define VIDEO_FPS                   30      // 录制帧率
#define VIDEO_FRAME_PERIOD_MS       33      // 33ms周期
#define VIDEO_RECORD_DURATION_S     30      // 录制30秒
#define VIDEO_QUEUE_LENGTH          30      // 1秒缓冲（30帧）
#define VIDEO_FRAME_SIZE            (320 * 240 * 2)  // 320x240 YUV422

// 内存配置
#define DDR_BASE_ADDR               0xA0000000
#define DDR_MODEL_L1_ADDR           0xA0100000
#define DDR_MODEL_L2_ADDR           0xA0200000
#define DDR_TENSOR_ARENA_SIZE       (1024 * 1024)
#define DDR_FRAME_BUFFER_SIZE       (LEVEL2_HISTORY_FRAMES * \
                                     LEVEL2_INPUT_WIDTH * LEVEL2_INPUT_HEIGHT * LEVEL2_INPUT_CHANNELS)

// Flash模型存储地址
#define FLASH_MODEL_L1_ADDR         0x20020000
#define FLASH_MODEL_L2_ADDR         0x20040000

// AI模式常量
#define MODE_LEVEL1                 1   // Level 1: 人形检测
#define MODE_LEVEL2                 2   // Level 2: 危险动作识别

/******************************************************************************
 * 数据结构定义
 ******************************************************************************/

// 系统状态枚举
typedef enum {
    STATE_SLEEP,                // 休眠状态
    STATE_LEVEL1_MONITORING,    // Level 1: 低帧率监控
    STATE_LEVEL2_TRACKING,      // Level 2: 高帧率危险识别
    STATE_LEVEL3_CONTINUOUS,    // Level 3: 持续监控+录像
    STATE_ALARM_TRIGGERED,      // 警报触发状态
    STATE_ERROR                 // 错误状态
} SystemState;

// 命令类型（AI任务与录制任务通信）
typedef enum {
    CMD_START_RECORDING,        // 开始录制
    CMD_STOP_RECORDING,         // 停止录制
    CMD_LEVEL2_TO_L3,           // Level 2 -> Level 3（检测到危险）
    CMD_LEVEL3_TO_L2,           // Level 3 -> Level 2（危险解除，人还在）
    CMD_LEVEL3_TO_L1,           // Level 3 -> Level 1（人员离开超时）
    CMD_LEVEL2_TIMEOUT,         // Level 2超时（无危险，人离开）
    CMD_LEVEL3_TIMEOUT,         // Level 3超时（最大持续时间）
    CMD_PERSON_LOST,            // 人员离开检测
    CMD_DANGER_CLEARED,         // 危险解除（但人还在）
    CMD_SYSTEM_ERROR            // 系统错误
} CommandType;

typedef struct {
    CommandType type;
    uint32_t param;
} SystemCommand;

typedef struct {
    int x_min, y_min;
    int x_max, y_max;
    float score;
} DetectionBox;

// 危险动作类别
#define ACTION_NORMAL           0   // 正常
#define ACTION_SMASH            1   // 砸车/砸窗
#define ACTION_BREAK_IN         2   // 撬门/撬锁
#define ACTION_SCRATCH          3   // 划车/破坏
#define ACTION_CLIMB            4   // 攀爬/尝试进入
#define ACTION_NUM_CLASSES      5

static const char* action_names[ACTION_NUM_CLASSES] = {
    "正常", "砸车", "撬门", "划车", "攀爬"
};

// 系统统计信息
typedef struct {
    uint32_t level1_frames;             // Level 1检测次数
    uint32_t level1_detections;         // 人员检测次数
    uint32_t level2_activations;        // Level 2激活次数
    uint32_t level2_frames;             // Level 2推理次数
    uint32_t danger_detections;         // 危险动作检测次数
    uint32_t alarms_triggered;          // 警报触发次数
    uint32_t video_files_created;       // 录制文件数
    uint32_t sd_write_errors;           // SD写入错误数
    uint32_t dropped_frames;            // 丢帧数
} SystemStats;

/******************************************************************************
 * 全局变量声明
 ******************************************************************************/

// 系统状态
extern volatile SystemState g_system_state;
extern SystemStats g_stats;

// FreeRTOS对象
extern QueueHandle_t g_cmd_queue;           // 命令队列（AI->录制）
extern QueueHandle_t g_frame_queue;         // 帧队列（录制->写入）
extern EventGroupHandle_t g_event_group;    // 事件组
extern SemaphoreHandle_t g_state_mutex;     // 状态互斥锁
extern SemaphoreHandle_t g_stats_mutex;     // 统计互斥锁
extern SemaphoreHandle_t g_fil_mutex;       // 文件句柄互斥锁
extern SemaphoreHandle_t g_ai_mutex;        // AI引擎互斥锁
extern SemaphoreHandle_t g_camera_mutex;    // 相机互斥锁

// 任务句柄（用于任务间通信）
extern TaskHandle_t g_task_ai;
extern TaskHandle_t g_task_recorder;
extern TaskHandle_t g_task_writer;
extern TaskHandle_t g_task_monitor;
extern TaskHandle_t g_task_alarm;

#ifdef __cplusplus
extern "C" {
#endif

bool ai_init(int mode);
bool ai_switch_mode(int new_mode);
bool ai_run_inference(uint8_t* input_frame);
int ai_get_detection_result(void);
float ai_get_confidence(void);
int ai_get_detection_boxes(DetectionBox* boxes, int max_boxes);

#ifdef __cplusplus
}
#endif

// 模型指针
extern const unsigned char* g_model_l1;
extern const unsigned char* g_model_l2;

/******************************************************************************
 * 任务函数声明
 ******************************************************************************/

// 任务入口函数
void task_ai_master(void *pvParameters);
void task_video_recorder(void *pvParameters);
void task_file_writer(void *pvParameters);
void task_system_monitor(void *pvParameters);
void task_alarm_handler(void *pvParameters);

/******************************************************************************
 * 功能模块接口
 ******************************************************************************/

// memory_manager.c
bool ddr_sanity_check(void);
bool ddr_init(void);
bool load_model_to_ddr(uint32_t model_id);
void* get_model_address(uint32_t model_id);

// hal.c (硬件抽象层)
bool hal_init(void);
bool camera_init(void);
void camera_power_down(void);
bool camera_capture_rgb(uint8_t* buffer, int width, int height);
bool camera_capture_yuv320(uint8_t* buffer);
void trigger_alarm(void);
void stop_alarm(void);
void set_cpu_frequency(uint32_t freq_hz);
void enter_low_power_mode(uint32_t sleep_ms);
void wait_for_interrupt(void);
uint32_t get_current_ms(void);

// storage.c
bool sd_card_init(void);
bool storage_check_space(void);
void sd_card_sleep(void);

/******************************************************************************
 * 工具函数
 ******************************************************************************/

static inline int quantize_float_to_int8(float value, float scale, int zero_point) {
    return (int)(value / scale) + zero_point;
}

static inline float dequantize_int8_to_float(int8_t value, float scale, int zero_point) {
    return (float)(value - zero_point) * scale;
}

static inline int get_max_index(int8_t* arr, int len) {
    int max_idx = 0;
    int8_t max_val = arr[0];
    for (int i = 1; i < len; i++) {
        if (arr[i] > max_val) {
            max_val = arr[i];
            max_idx = i;
        }
    }
    return max_idx;
}

static inline void quantize_frame(uint8_t* src, int8_t* dst, int len) {
    for (int i = 0; i < len; i++) {
        dst[i] = (int8_t)((int)src[i] - 128);  // [0,255] -> [-128,127]
    }
}

/******************************************************************************
 * 调试和日志
 ******************************************************************************/

#define SENTRY_LOG_LEVEL_INFO   1
#define SENTRY_LOG_LEVEL_DEBUG  2
#define SENTRY_LOG_LEVEL_ERROR  0

#ifndef SENTRY_LOG_LEVEL
#define SENTRY_LOG_LEVEL SENTRY_LOG_LEVEL_INFO
#endif

#if SENTRY_LOG_LEVEL >= SENTRY_LOG_LEVEL_DEBUG
    #define LOG_DEBUG(fmt, ...) printf("[D] " fmt "\r\n", ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...)
#endif

#if SENTRY_LOG_LEVEL >= SENTRY_LOG_LEVEL_INFO
    #define LOG_INFO(fmt, ...) printf("[I] " fmt "\r\n", ##__VA_ARGS__)
#else
    #define LOG_INFO(fmt, ...)
#endif

#define LOG_ERROR(fmt, ...) printf("[E] " fmt "\r\n", ##__VA_ARGS__)

#endif /* __SENTRY_MODE_H__ */
