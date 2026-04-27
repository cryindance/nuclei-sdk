/******************************************************************************
 * @file    tasks.c
 * @brief   FreeRTOS任务实现 - AI主任务、录像、写入、警报、监控
 ******************************************************************************/

#include "sentry_mode.h"

/* 外部变量声明 */
extern volatile SystemState g_system_state;
extern SystemStats g_stats;
extern QueueHandle_t g_cmd_queue;
extern QueueHandle_t g_frame_queue;

/* 帧历史缓冲区 */
static int8_t s_frame_history[LEVEL2_HISTORY_FRAMES][LEVEL2_INPUT_WIDTH * LEVEL2_INPUT_HEIGHT * LEVEL2_INPUT_CHANNELS]
    __attribute__((section(".ddr_bss")));
static uint8_t s_frame_idx = 0;
static uint8_t s_consecutive_detections = 0;

/* 录制状态 */
static bool s_recording = false;
#ifndef NO_STORAGE
static FIL s_fil;
#endif
static uint32_t s_record_start_time = 0;
static uint8_t s_normal_count = 0;

/* 推理帧缓冲（DDR分配，避免栈溢出） */
static uint8_t s_level1_frame[LEVEL1_INPUT_WIDTH * LEVEL1_INPUT_HEIGHT * LEVEL1_INPUT_CHANNELS]
    __attribute__((section(".ddr_bss")));

/* L2/L3 共用帧缓冲和时序输入缓冲（这两分支互斥，可复用） */
static uint8_t s_level23_frame[LEVEL2_INPUT_WIDTH * LEVEL2_INPUT_HEIGHT * LEVEL2_INPUT_CHANNELS]
    __attribute__((section(".ddr_bss")));
static int8_t s_temporal_input[LEVEL2_HISTORY_FRAMES][LEVEL2_INPUT_WIDTH * LEVEL2_INPUT_HEIGHT * LEVEL2_INPUT_CHANNELS]
    __attribute__((section(".ddr_bss")));

/* 录制/写入任务视频帧缓冲（DDR分配） */
static uint8_t s_recorder_frame[VIDEO_FRAME_SIZE]
    __attribute__((section(".ddr_bss")));
static uint8_t s_writer_frame[VIDEO_FRAME_SIZE]
    __attribute__((section(".ddr_bss")));

/**
 * @brief AI主任务 - 状态机和推理
 */
void task_ai_master(void *pvParameters)
{
    (void)pvParameters;
    
    LOG_INFO("AI Task started");
    
    /* 初始化为Level 1 */
    if (!ai_init(MODE_LEVEL1)) {
        LOG_ERROR("AI init failed!");
        vTaskDelete(NULL);
    }
    
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        SystemState current_state;
        xSemaphoreTake(g_state_mutex, portMAX_DELAY);
        current_state = g_system_state;
        xSemaphoreGive(g_state_mutex);
        
        if (current_state == STATE_SLEEP) {
            /* 唤醒后进入Level 1 */
            xSemaphoreTake(g_state_mutex, portMAX_DELAY);
            g_system_state = STATE_LEVEL1_MONITORING;
            xSemaphoreGive(g_state_mutex);
            
        } else if (current_state == STATE_LEVEL1_MONITORING) {
            /* Level 1: 低功耗人形检测 */
            bool person_detected = false;
            
            /* 采集并推理 */
            xSemaphoreTake(g_camera_mutex, portMAX_DELAY);
            camera_capture_rgb(s_level1_frame, LEVEL1_INPUT_WIDTH, LEVEL1_INPUT_HEIGHT);
            xSemaphoreGive(g_camera_mutex);
            
            if (ai_run_inference(s_level1_frame)) {
                int result = ai_get_detection_result();
                float conf = ai_get_confidence();
                
                xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
                g_stats.level1_frames++;
                xSemaphoreGive(g_stats_mutex);
                
                if (result == 1 && conf > 0.6f) {
                    s_consecutive_detections++;
                    if (s_consecutive_detections >= 2) {
                        LOG_INFO("Person detected! Waking up and switching to Level 2");
                        xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
            g_stats.level1_detections++;
            xSemaphoreGive(g_stats_mutex);
                        person_detected = true;
                        s_consecutive_detections = 0;
                    }
                } else {
                    s_consecutive_detections = 0;
                }
            }
            
            if (person_detected) {
                /* 唤醒系统：退出低功耗，进入高性能模式 */
                set_cpu_frequency(160000000);
                
                /* 切换到Level 2 */
                if (ai_switch_mode(MODE_LEVEL2)) {
                    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
                    g_system_state = STATE_LEVEL2_TRACKING;
                    xSemaphoreGive(g_state_mutex);
                    
                    SystemCommand cmd = {CMD_START_RECORDING, 0};
                    xQueueSend(g_cmd_queue, &cmd, 0);
                    
                    xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
            g_stats.level2_activations++;
            xSemaphoreGive(g_stats_mutex);
                }
            } else {
                /* 无人检测：进入深度休眠 */
                LOG_INFO("No person detected, entering deep sleep for %d ms", LEVEL1_INTERVAL_MS);
                
                /* 关闭摄像头节省功耗 */
                camera_power_down();
                
                /* 降低CPU频率 */
                set_cpu_frequency(16000000);
                
                /* 设置状态为休眠 */
                xSemaphoreTake(g_state_mutex, portMAX_DELAY);
                g_system_state = STATE_SLEEP;
                xSemaphoreGive(g_state_mutex);
                
                /* 进入WFI低功耗模式，等待定时器唤醒 */
                enter_low_power_mode(LEVEL1_INTERVAL_MS);
                
                /* 被唤醒后恢复 */
                LOG_INFO("Wakeup from sleep, resuming Level 1");
                camera_init();
                set_cpu_frequency(160000000);
                
                xSemaphoreTake(g_state_mutex, portMAX_DELAY);
                g_system_state = STATE_LEVEL1_MONITORING;
                xSemaphoreGive(g_state_mutex);
            }
            
        } else if (current_state == STATE_LEVEL2_TRACKING) {
            /* Level 2: 危险动作识别 */
            static uint32_t s_level2_start_time = 0;
            static uint32_t s_last_person_time = 0;
            static bool s_person_present = false;
            
            if (s_level2_start_time == 0) {
                s_level2_start_time = get_current_ms();
                s_last_person_time = s_level2_start_time;
                s_person_present = true;
            }
            
            xSemaphoreTake(g_camera_mutex, portMAX_DELAY);
            camera_capture_rgb(s_level23_frame, LEVEL2_INPUT_WIDTH, LEVEL2_INPUT_HEIGHT);
            xSemaphoreGive(g_camera_mutex);
            
            /* 保存到历史缓冲区 */
            quantize_frame(s_level23_frame, s_frame_history[s_frame_idx], sizeof(s_level23_frame));
            s_frame_idx = (s_frame_idx + 1) % LEVEL2_HISTORY_FRAMES;
            
            /* 构建时序输入 */
            for (int i = 0; i < LEVEL2_HISTORY_FRAMES; i++) {
                int src = (s_frame_idx + i) % LEVEL2_HISTORY_FRAMES;
                memcpy(s_temporal_input[i], s_frame_history[src], sizeof(s_level23_frame));
            }
            
            /* 运行推理 */
            if (ai_run_inference((uint8_t *)s_temporal_input)) {
                int action = ai_get_detection_result();
                float conf = ai_get_confidence();
                
                xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
                g_stats.level2_frames++;
                xSemaphoreGive(g_stats_mutex);
                LOG_INFO("Level 2: Action=%d (%s), Conf=%.2f", 
                        action, action_names[action], conf);
                
                /* 检测人是否还在 */
                if (action != ACTION_NORMAL || conf > 0.3f) {
                    s_person_present = true;
                    s_last_person_time = get_current_ms();
                } else {
                    s_person_present = false;
                }
                
                /* 检测危险动作 - 进入Level 3 */
                if (action != ACTION_NORMAL && conf > 0.6f) {
                    LOG_INFO("DANGER DETECTED: %s! Switching to Level 3", action_names[action]);
                    xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
            g_stats.danger_detections++;
            xSemaphoreGive(g_stats_mutex);
                    
                    /* 切换到Level 3 */
                    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
                    g_system_state = STATE_LEVEL3_CONTINUOUS;
                    xSemaphoreGive(g_state_mutex);
                    
                    /* 发送升级命令 */
                    SystemCommand cmd = {CMD_LEVEL2_TO_L3, action};
                    xQueueSend(g_cmd_queue, &cmd, 0);
                    
                    /* 触发警报 */
                    if (g_task_alarm != NULL) {
                        xTaskNotifyGive(g_task_alarm);
                    } else {
                        LOG_ERROR("Alarm task handle is NULL!");
                    }
                    
                    /* 重置计时器 */
                    s_level2_start_time = 0;
                }
                /* 人员离开超过10秒 - 返回Level 1 */
                else if (!s_person_present && 
                         (get_current_ms() - s_last_person_time > 10000)) {
                    LOG_INFO("Person left for 10s, returning to Level 1");
                    
                    SystemCommand stop_cmd = {CMD_STOP_RECORDING, 0};
                    xQueueSend(g_cmd_queue, &stop_cmd, 0);
                    vTaskDelay(pdMS_TO_TICKS(50));
                    
                    SystemCommand cmd = {CMD_LEVEL2_TIMEOUT, 0};
                    xQueueSend(g_cmd_queue, &cmd, 0);
                    
                    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
                    g_system_state = STATE_LEVEL1_MONITORING;
                    xSemaphoreGive(g_state_mutex);
                    
                    ai_switch_mode(MODE_LEVEL1);
                    s_level2_start_time = 0;
                }
                /* Level 2超时（10秒） - 返回Level 1 */
                else if (get_current_ms() - s_level2_start_time > LEVEL2_MAX_DURATION_MS) {
                    LOG_INFO("Level 2 timeout, returning to Level 1");
                    
                    SystemCommand stop_cmd = {CMD_STOP_RECORDING, 0};
                    xQueueSend(g_cmd_queue, &stop_cmd, 0);
                    vTaskDelay(pdMS_TO_TICKS(50));
                    
                    SystemCommand cmd = {CMD_LEVEL2_TIMEOUT, 0};
                    xQueueSend(g_cmd_queue, &cmd, 0);
                    
                    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
                    g_system_state = STATE_LEVEL1_MONITORING;
                    xSemaphoreGive(g_state_mutex);
                    
                    ai_switch_mode(MODE_LEVEL1);
                    s_level2_start_time = 0;
                }
            }
            
            /* Level 2周期: 200ms (5 FPS) */
            vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(LEVEL2_FRAME_PERIOD_MS));
            
        } else if (current_state == STATE_LEVEL3_CONTINUOUS) {
            /* Level 3: 持续监控+录像 */
            static uint32_t s_level3_start_time = 0;
            static uint32_t s_last_person_time_l3 = 0;
            static uint8_t s_no_person_count = 0;
            
            if (s_level3_start_time == 0) {
                s_level3_start_time = get_current_ms();
                s_last_person_time_l3 = s_level3_start_time;
                s_no_person_count = 0;
                s_normal_count = 0;
                LOG_INFO("Entering Level 3: Continuous monitoring");
            }
            
            xSemaphoreTake(g_camera_mutex, portMAX_DELAY);
            camera_capture_rgb(s_level23_frame, LEVEL3_INPUT_WIDTH, LEVEL3_INPUT_HEIGHT);
            xSemaphoreGive(g_camera_mutex);
            
            /* 保存到历史缓冲区 */
            quantize_frame(s_level23_frame, s_frame_history[s_frame_idx], sizeof(s_level23_frame));
            s_frame_idx = (s_frame_idx + 1) % LEVEL2_HISTORY_FRAMES;
            
            /* 构建时序输入 */
            for (int i = 0; i < LEVEL2_HISTORY_FRAMES; i++) {
                int src = (s_frame_idx + i) % LEVEL2_HISTORY_FRAMES;
                memcpy(s_temporal_input[i], s_frame_history[src], sizeof(s_level23_frame));
            }
            
            /* 运行推理 */
            if (ai_run_inference((uint8_t *)s_temporal_input)) {
                int action = ai_get_detection_result();
                float conf = ai_get_confidence();
                
                xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
                g_stats.level2_frames++;  /* 复用计数器 */
                xSemaphoreGive(g_stats_mutex);
                LOG_INFO("Level 3: Action=%d (%s), Conf=%.2f", 
                        action, action_names[action], conf);
                
                /* 检测人是否还在 */
                if (action != ACTION_NORMAL || conf > 0.3f) {
                    s_last_person_time_l3 = get_current_ms();
                    s_no_person_count = 0;
                } else {
                    s_no_person_count++;
                }
                
                /* 危险解除检测 */
                if (action == ACTION_NORMAL) {
                    s_normal_count++;
                } else {
                    s_normal_count = 0;
                    if (conf > 0.6f) {
                        LOG_INFO("Level 3: Danger continues - %s", action_names[action]);
                        s_last_person_time_l3 = get_current_ms();
                    }
                }
            }
            
            /* 检查退出条件 */
            uint32_t current_time = get_current_ms();
            
            /* 条件1: 危险解除(3秒)但人还在 - 退回L2 */
            if (s_normal_count >= 15 && s_no_person_count < 15) {
                LOG_INFO("Danger cleared for 3s, person still present, returning to Level 2");
                
                SystemCommand stop_cmd = {CMD_STOP_RECORDING, 0};
                xQueueSend(g_cmd_queue, &stop_cmd, 0);
                vTaskDelay(pdMS_TO_TICKS(50));
                
                SystemCommand cmd = {CMD_LEVEL3_TO_L2, 0};
                xQueueSend(g_cmd_queue, &cmd, 0);
                
                xSemaphoreTake(g_state_mutex, portMAX_DELAY);
                g_system_state = STATE_LEVEL2_TRACKING;
                xSemaphoreGive(g_state_mutex);
                
                s_level3_start_time = 0;
                s_normal_count = 0;
                s_no_person_count = 0;
            }
            /* 条件2: 人员离开超过15秒 - 退回L1 */
            else if ((current_time - s_last_person_time_l3 > LEVEL3_PERSON_LOST_TIMEOUT_MS) &&
                (s_no_person_count > 30)) {
                LOG_INFO("Person lost for 15s in Level 3, returning to Level 1");
                
                SystemCommand stop_cmd = {CMD_STOP_RECORDING, 0};
                xQueueSend(g_cmd_queue, &stop_cmd, 0);
                vTaskDelay(pdMS_TO_TICKS(50));
                
                SystemCommand cmd = {CMD_LEVEL3_TO_L1, 0};
                xQueueSend(g_cmd_queue, &cmd, 0);
                
                xSemaphoreTake(g_state_mutex, portMAX_DELAY);
                g_system_state = STATE_LEVEL1_MONITORING;
                xSemaphoreGive(g_state_mutex);
                
                ai_switch_mode(MODE_LEVEL1);
                s_level3_start_time = 0;
                s_normal_count = 0;
            }
            /* 条件3: Level 3最大持续时间（60秒）- 退回L1 */
            else if (current_time - s_level3_start_time > LEVEL3_MAX_DURATION_MS) {
                LOG_INFO("Level 3 max duration reached, returning to Level 1");
                
                SystemCommand stop_cmd = {CMD_STOP_RECORDING, 0};
                xQueueSend(g_cmd_queue, &stop_cmd, 0);
                vTaskDelay(pdMS_TO_TICKS(50));
                
                SystemCommand cmd = {CMD_LEVEL3_TIMEOUT, 0};
                xQueueSend(g_cmd_queue, &cmd, 0);
                
                xSemaphoreTake(g_state_mutex, portMAX_DELAY);
                g_system_state = STATE_LEVEL1_MONITORING;
                xSemaphoreGive(g_state_mutex);
                
                ai_switch_mode(MODE_LEVEL1);
                s_level3_start_time = 0;
                s_normal_count = 0;
            }
            
            /* Level 3周期: 200ms (5 FPS) */
            vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(LEVEL3_FRAME_PERIOD_MS));
        }
    }
}

/**
 * @brief 视频录制任务
 */
void task_video_recorder(void *pvParameters)
{
    (void)pvParameters;
    
    LOG_INFO("Recorder Task started");
    
    SystemCommand cmd;
    
    while (1) {
        if (xQueueReceive(g_cmd_queue, &cmd, portMAX_DELAY)) {
            if (cmd.type == CMD_START_RECORDING && !s_recording) {
                LOG_INFO("Starting video recording");
                
                /* 创建文件名 */
                char filename[64];
                snprintf(filename, sizeof(filename), 
                        "/sentry/%08lu.avi", get_current_ms());
                
#ifndef NO_STORAGE
                xSemaphoreTake(g_fil_mutex, portMAX_DELAY);
                if (f_open(&s_fil, filename, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
#endif
                    s_recording = true;
                    s_record_start_time = get_current_ms();
                    xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
                    g_stats.video_files_created++;
                    xSemaphoreGive(g_stats_mutex);
                    
                    /* 录制30秒 */
                    while (s_recording) {
                        xSemaphoreTake(g_camera_mutex, portMAX_DELAY);
                        camera_capture_yuv320(s_recorder_frame);
                        xSemaphoreGive(g_camera_mutex);
                        
                        /* 发送到写入队列 */
                        if (xQueueSend(g_frame_queue, s_recorder_frame, 0) != pdPASS) {
                            xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
                            g_stats.dropped_frames++;
                            xSemaphoreGive(g_stats_mutex);
                        }
                        
                        /* 检查录制时长 */
                        if (get_current_ms() - s_record_start_time > VIDEO_RECORD_DURATION_S * 1000) {
                            s_recording = false;
                        }
                        
                        vTaskDelay(pdMS_TO_TICKS(VIDEO_FRAME_PERIOD_MS));
                    }
                    
                    /* 等待 Writer 消费完队列中所有帧，再关文件 */
                    while (uxQueueMessagesWaiting(g_frame_queue) > 0) {
                        vTaskDelay(pdMS_TO_TICKS(1));
                    }
                    
#ifndef NO_STORAGE
                    f_close(&s_fil);
                }
                xSemaphoreGive(g_fil_mutex);
#endif
                    LOG_INFO("Recording finished: %s", filename);
                
            } else if (cmd.type == CMD_STOP_RECORDING) {
                s_recording = false;
            }
        }
    }
}

/**
 * @brief 文件写入任务
 */
void task_file_writer(void *pvParameters)
{
    (void)pvParameters;
    
    LOG_INFO("File Writer Task started");
    
#ifndef NO_STORAGE
    UINT written;
#endif
    
    while (1) {
        if (xQueueReceive(g_frame_queue, s_writer_frame, portMAX_DELAY)) {
#ifndef NO_STORAGE
            /* 写入SD卡（可能耗时，但不会影响AI任务） */
            xSemaphoreTake(g_fil_mutex, portMAX_DELAY);
            FRESULT res = f_write(&s_fil, s_writer_frame, VIDEO_FRAME_SIZE, &written);
            xSemaphoreGive(g_fil_mutex);
            
            if (res != FR_OK || written != VIDEO_FRAME_SIZE) {
                LOG_ERROR("SD write failed!");
                xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
                g_stats.sd_write_errors++;
                xSemaphoreGive(g_stats_mutex);
            }
#else
            (void)s_writer_frame;
#endif
        }
    }
}

/**
 * @brief 警报处理任务
 */
void task_alarm_handler(void *pvParameters)
{
    (void)pvParameters;
    
    LOG_INFO("Alarm Task started");
    
    while (1) {
        /* 等待通知 */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        LOG_INFO("ALARM TRIGGERED!");
            xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
            g_stats.alarms_triggered++;
            xSemaphoreGive(g_stats_mutex);
        
        /* 触发警报 */
        trigger_alarm();
        
        /* 持续30秒 */
        vTaskDelay(pdMS_TO_TICKS(30000));
        
        stop_alarm();
        
        /* 返回Level 1 */
        xSemaphoreTake(g_state_mutex, portMAX_DELAY);
        g_system_state = STATE_LEVEL1_MONITORING;
        xSemaphoreGive(g_state_mutex);
        
        ai_switch_mode(MODE_LEVEL1);
    }
}

/**
 * @brief 系统监控任务
 */
void task_system_monitor(void *pvParameters)
{
    (void)pvParameters;
    
    LOG_INFO("Monitor Task started");
    
    while (1) {
        /* 每10秒打印一次统计信息 */
        vTaskDelay(pdMS_TO_TICKS(10000));
        
        xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
        LOG_INFO("==== System Stats ====");
        LOG_INFO("Level1 frames: %lu", g_stats.level1_frames);
        LOG_INFO("Level1 detections: %lu", g_stats.level1_detections);
        LOG_INFO("Level2 activations: %lu", g_stats.level2_activations);
        LOG_INFO("Level2 frames: %lu", g_stats.level2_frames);
        LOG_INFO("Danger detections: %lu", g_stats.danger_detections);
        LOG_INFO("Alarms: %lu", g_stats.alarms_triggered);
        LOG_INFO("Videos: %lu", g_stats.video_files_created);
        LOG_INFO("SD errors: %lu", g_stats.sd_write_errors);
        LOG_INFO("Dropped frames: %lu", g_stats.dropped_frames);
        xSemaphoreGive(g_stats_mutex);
        
        /* 检查SD卡空间 */
        /* TODO: 实现SD卡空间检查 */
    }
}
