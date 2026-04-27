/******************************************************************************
 * @file    hal.c
 * @brief   硬件抽象层实现
 ******************************************************************************/

#include "sentry_mode.h"

/* 模拟硬件寄存器地址（实际应根据SoC手册修改） */
#define CAMERA_CTRL_REG     (*(volatile uint32_t *)0xB0000000)
#define CAMERA_STATUS_REG   (*(volatile uint32_t *)0xB0000004)
#define CAMERA_DATA_REG     (*(volatile uint32_t *)0xB0000008)

#define ALARM_CTRL_REG      (*(volatile uint32_t *)0xB0001000)
#define ALARM_STATUS_REG    (*(volatile uint32_t *)0xB0001004)

#define SYS_CLOCK_CTRL      (*(volatile uint32_t *)0xB0002000)

static volatile uint32_t s_system_tick = 0;
static bool s_camera_powered = false;
static bool s_alarm_active = false;

// 函数前置声明
void delay_ms(uint32_t ms);

/**
 * @brief 系统时钟中断处理（1ms）
 */
void SysTick_Handler(void)
{
    s_system_tick++;
}

bool hal_init(void)
{
    LOG_INFO("HAL initializing...");
    
    /* 初始化系统时钟 */
    s_system_tick = 0;
    
    /* 初始化GPIO等 */
    /* TODO: 根据具体硬件实现 */
    
    LOG_INFO("HAL initialized");
    return true;
}

bool camera_init(void)
{
    if (s_camera_powered) {
        return true;
    }
    
    LOG_DEBUG("Camera power on");
    
    /* 上电摄像头 */
    CAMERA_CTRL_REG = 0x01;
    
    /* 等待稳定 */
    delay_ms(50);
    
    /* 配置分辨率 */
    CAMERA_CTRL_REG = 0x01 | (0x01 << 4);  /* 640x480模式 */
    
    s_camera_powered = true;
    return true;
}

void camera_power_down(void)
{
    if (!s_camera_powered) {
        return;
    }
    
    LOG_DEBUG("Camera power down");
    
    CAMERA_CTRL_REG = 0x00;
    s_camera_powered = false;
}

bool camera_capture_rgb(uint8_t *buffer, int width, int height)
{
    if (!s_camera_powered || buffer == NULL) {
        return false;
    }
    
    for (int i = 0; i < width * height * 3; i++) {
        buffer[i] = (uint8_t)(i % 256);
    }
    
    return true;
}

bool camera_capture_yuv320(uint8_t *buffer)
{
    if (!s_camera_powered || buffer == NULL) {
        return false;
    }
    
    /* 采集320x240 YUV422 */
    /* TODO: 实际硬件实现 */
    
    /* 填充测试数据 */
    for (int i = 0; i < 320 * 240 * 2; i++) {
        buffer[i] = 0x80;  /* 中性灰 */
    }
    
    return true;
}

void trigger_alarm(void)
{
    if (s_alarm_active) {
        return;
    }
    
    LOG_INFO("ALARM TRIGGERED!");
    
    /* 启动蜂鸣器 */
    ALARM_CTRL_REG = 0x01;
    
    /* 闪烁车灯 */
    ALARM_CTRL_REG |= (0x01 << 1);
    
    s_alarm_active = true;
}

void stop_alarm(void)
{
    if (!s_alarm_active) {
        return;
    }
    
    LOG_INFO("Alarm stopped");
    
    ALARM_CTRL_REG = 0x00;
    s_alarm_active = false;
}

void set_cpu_frequency(uint32_t freq_hz)
{
    LOG_DEBUG("Setting CPU frequency to %lu Hz", freq_hz);
    
    /* TODO: 根据具体SoC实现时钟切换 */
    if (freq_hz <= 16000000) {
        /* 低功耗模式 */
        SYS_CLOCK_CTRL = 0x00;
    } else {
        /* 高性能模式 */
        SYS_CLOCK_CTRL = 0x01;
    }
}

void delay_ms(uint32_t ms)
{
    uint64_t start = SysTimer_GetLoadValue();
    uint64_t end = start + (SOC_TIMER_FREQ / 1000) * ms;
    while (SysTimer_GetLoadValue() < end);
}

void enter_low_power_mode(uint32_t sleep_ms)
{
    /* 关闭外设 */
    camera_power_down();
    
    /* 降低CPU频率 */
    set_cpu_frequency(16000000);  /* 16MHz低功耗 */
    
    /* 设置唤醒定时器 */
    uint64_t wakeup = SysTimer_GetLoadValue() + 
                      (SOC_TIMER_FREQ / 1000) * sleep_ms;
    SysTimer_SetCompareValue(wakeup);
    
    LOG_INFO("Entering low power mode for %lu ms", sleep_ms);
    
    /* 进入WFI - 等待定时器中断唤醒 */
    __WFI();
    
    /* 被唤醒后恢复高频 */
    set_cpu_frequency(160000000);  /* 160MHz高性能 */
    
    LOG_INFO("Wakeup from low power mode");
}

void wait_for_interrupt(void)
{
    /* 纯WFI，不改变时钟 */
    __WFI();
}

uint32_t get_current_ms(void)
{
    return (uint32_t)(SysTimer_GetLoadValue() / (SOC_TIMER_FREQ / 1000));
}
