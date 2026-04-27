/******************************************************************************
 * @file    main.c
 * @brief   哨兵模式主程序入口
 ******************************************************************************/

#include "sentry_mode.h"

// 全局变量定义
volatile SystemState g_system_state = STATE_SLEEP;
SystemStats g_stats = {0};

// FreeRTOS对象
QueueHandle_t g_cmd_queue = NULL;
QueueHandle_t g_frame_queue = NULL;
EventGroupHandle_t g_event_group = NULL;
SemaphoreHandle_t g_state_mutex = NULL;
SemaphoreHandle_t g_stats_mutex = NULL;
SemaphoreHandle_t g_fil_mutex = NULL;
SemaphoreHandle_t g_ai_mutex = NULL;
SemaphoreHandle_t g_camera_mutex = NULL;

// 任务句柄（用于任务间通信）
TaskHandle_t g_task_ai = NULL;
TaskHandle_t g_task_recorder = NULL;
TaskHandle_t g_task_writer = NULL;
TaskHandle_t g_task_monitor = NULL;
TaskHandle_t g_task_alarm = NULL;

// 模型指针（默认指向Flash，可切换到DDR）
const unsigned char* g_model_l1 = (const unsigned char*)FLASH_MODEL_L1_ADDR;
const unsigned char* g_model_l2 = (const unsigned char*)FLASH_MODEL_L2_ADDR;

// DDR中分配的内存（链接时放在DDR段）
static uint8_t s_tensor_arena[DDR_TENSOR_ARENA_SIZE] __attribute__((section(".ddr_bss")));

// 任务栈（放在DLM）
static StaticTask_t s_task_ai_buffer;
static StaticTask_t s_task_recorder_buffer;
static StaticTask_t s_task_writer_buffer;
static StaticTask_t s_task_monitor_buffer;
static StaticTask_t s_task_alarm_buffer;

static StackType_t s_task_ai_stack[4096] __attribute__((section(".dlm")));
static StackType_t s_task_recorder_stack[4096] __attribute__((section(".dlm")));
static StackType_t s_task_writer_stack[4096] __attribute__((section(".dlm")));
static StackType_t s_task_monitor_stack[2048] __attribute__((section(".dlm")));
static StackType_t s_task_alarm_stack[2048] __attribute__((section(".dlm")));

// 队列缓冲区
static uint8_t s_cmd_queue_storage[4 * sizeof(SystemCommand)];
static uint8_t s_frame_queue_storage[VIDEO_QUEUE_LENGTH * VIDEO_FRAME_SIZE];
static StaticQueue_t s_cmd_queue_buffer;
static StaticQueue_t s_frame_queue_buffer;

// 事件组和互斥锁
static StaticEventGroup_t s_event_group_buffer;
static StaticSemaphore_t s_state_mutex_buffer;
static StaticSemaphore_t s_stats_mutex_buffer;
static StaticSemaphore_t s_fil_mutex_buffer;
static StaticSemaphore_t s_ai_mutex_buffer;
static StaticSemaphore_t s_camera_mutex_buffer;

/******************************************************************************
 * 系统初始化
 ******************************************************************************/
static bool system_init(void) {
    LOG_INFO("Sentry Mode Starting...");
    
    // 初始化硬件
    if (!hal_init()) {
        LOG_ERROR("Hardware init failed!");
        return false;
    }
    
    // 检查DDR可用性
    if (!ddr_sanity_check()) {
        LOG_ERROR("DDR not available! System halted.");
        return false;
    }
    LOG_INFO("DDR check passed");
    
    // 创建FreeRTOS对象（使用静态分配，避免堆碎片）
    g_cmd_queue = xQueueCreateStatic(4, sizeof(SystemCommand), 
                                     s_cmd_queue_storage, &s_cmd_queue_buffer);
    g_frame_queue = xQueueCreateStatic(VIDEO_QUEUE_LENGTH, VIDEO_FRAME_SIZE,
                                       s_frame_queue_storage, &s_frame_queue_buffer);
    g_event_group = xEventGroupCreateStatic(&s_event_group_buffer);
    g_state_mutex = xSemaphoreCreateMutexStatic(&s_state_mutex_buffer);
    g_stats_mutex = xSemaphoreCreateMutexStatic(&s_stats_mutex_buffer);
    g_fil_mutex = xSemaphoreCreateMutexStatic(&s_fil_mutex_buffer);
    g_ai_mutex = xSemaphoreCreateMutexStatic(&s_ai_mutex_buffer);
    g_camera_mutex = xSemaphoreCreateMutexStatic(&s_camera_mutex_buffer);
    
    if (!g_cmd_queue || !g_frame_queue || !g_event_group || !g_state_mutex) {
        LOG_ERROR("FreeRTOS object creation failed!");
        return false;
    }
    
    // 初始化SD卡
    if (!sd_card_init()) {
        LOG_ERROR("SD card init failed! Video recording disabled.");
    }
    
    LOG_INFO("System init completed");
    return true;
}

/******************************************************************************
 * 主函数
 ******************************************************************************/
int main(void) {
    // 系统初始化
    if (!system_init()) {
        LOG_ERROR("System init failed!");
        while(1) { __WFI(); }
    }
    
    LOG_INFO("Creating FreeRTOS tasks...");
    
    // 创建AI主任务（最高优先级）
    g_task_ai = xTaskCreateStatic(task_ai_master, "AI", sizeof(s_task_ai_stack)/4, NULL, 3,
                      s_task_ai_stack, &s_task_ai_buffer);
    
    // 创建视频录制任务（中优先级）
    g_task_recorder = xTaskCreateStatic(task_video_recorder, "REC", sizeof(s_task_recorder_stack)/4, NULL, 2,
                      s_task_recorder_stack, &s_task_recorder_buffer);
    
    // 创建文件写入任务（低优先级）
    g_task_writer = xTaskCreateStatic(task_file_writer, "WRITE", sizeof(s_task_writer_stack)/4, NULL, 1,
                      s_task_writer_stack, &s_task_writer_buffer);
    
    // 创建系统监控任务（低优先级）
    g_task_monitor = xTaskCreateStatic(task_system_monitor, "MON", sizeof(s_task_monitor_stack)/4, NULL, 1,
                      s_task_monitor_stack, &s_task_monitor_buffer);
    
    // 创建警报处理任务（最高优先级）
    g_task_alarm = xTaskCreateStatic(task_alarm_handler, "ALARM", sizeof(s_task_alarm_stack)/4, NULL, 4,
                      s_task_alarm_stack, &s_task_alarm_buffer);
    
    LOG_INFO("Starting scheduler...");
    
    // 启动调度器
    vTaskStartScheduler();
    
    // 不会到达这里
    LOG_ERROR("Scheduler returned unexpectedly!");
    while(1) { }
    
    return 0;
}

/******************************************************************************
 * FreeRTOS 静态分配钩子函数
 ******************************************************************************/

/* 空闲任务内存 */
static StaticTask_t s_idle_task_buffer;
static StackType_t s_idle_task_stack[512];

/* 定时器任务内存 */
static StaticTask_t s_timer_task_buffer;
static StackType_t s_timer_task_stack[512];

/* 空闲任务钩子 - 可在此实现低功耗逻辑 */
void vApplicationIdleHook(void)
{
    /* 可在此添加系统空闲时的额外处理，如进入WFI模式 */
}

/* 获取空闲任务内存 */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &s_idle_task_buffer;
    *ppxIdleTaskStackBuffer = s_idle_task_stack;
    *pulIdleTaskStackSize = sizeof(s_idle_task_stack) / sizeof(StackType_t);
}

/* 获取定时器任务内存 */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &s_timer_task_buffer;
    *ppxTimerTaskStackBuffer = s_timer_task_stack;
    *pulTimerTaskStackSize = sizeof(s_timer_task_stack) / sizeof(StackType_t);
}

/* 栈溢出钩子 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    LOG_ERROR("Stack overflow in task: %s", pcTaskName);
    while (1) { }
}
