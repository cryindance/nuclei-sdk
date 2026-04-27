/******************************************************************************
 * @file    memory_manager.c
 * @brief   DDR内存管理和模型动态加载模块
 ******************************************************************************/

#include "sentry_mode.h"

/* DDR内存区域定义 */
#define DDR_BASE                    0xA0000000
#define DDR_SIZE                    (512 * 1024 * 1024)  /* 512MB */

#define DDR_MODEL_L2_ADDR           0xA0200000
#define DDR_TENSOR_ARENA_ADDR       0xA0300000
#define DDR_FRAME_HISTORY_ADDR      0xA0400000
#define DDR_VIDEO_QUEUE_ADDR        0xA0500000

/* 内存区域大小 */
#define DDR_MODEL_L2_SIZE           (300 * 1024)
#define DDR_FRAME_HISTORY_SIZE      (LEVEL2_HISTORY_FRAMES * LEVEL2_INPUT_WIDTH * LEVEL2_INPUT_HEIGHT * LEVEL2_INPUT_CHANNELS)
#define DDR_VIDEO_QUEUE_SIZE        (VIDEO_QUEUE_LENGTH * VIDEO_FRAME_SIZE)

/* 静态分配的DDR内存 */
static uint8_t s_tensor_arena[DDR_TENSOR_ARENA_SIZE] __attribute__((section(".ddr_bss"), aligned(16)));
static uint8_t s_video_queue_buffer[VIDEO_QUEUE_LENGTH * VIDEO_FRAME_SIZE] __attribute__((section(".ddr_bss")));

static volatile bool s_ddr_ready = false;

/**
 * @brief DDR完整性检查
 * @return true: DDR可用, false: DDR不可用
 */
bool ddr_sanity_check(void)
{
    volatile uint32_t *test_addr = (volatile uint32_t *)DDR_BASE;
    const uint32_t test_patterns[] = {0x12345678, 0x87654321, 0xA5A5A5A5, 0x5A5A5A5A};
    
    for (int i = 0; i < 4; i++) {
        *test_addr = test_patterns[i];
        __DSB();
        
        if (*test_addr != test_patterns[i]) {
            LOG_ERROR("DDR sanity check failed at pattern %d", i);
            return false;
        }
    }
    
    /* 更大范围的测试 */
    volatile uint32_t *test_end = (volatile uint32_t *)(DDR_BASE + DDR_SIZE - 4);
    *test_end = 0xDEADBEEF;
    __DSB();
    
    if (*test_end != 0xDEADBEEF) {
        LOG_ERROR("DDR end address test failed");
        return false;
    }
    
    return true;
}

/**
 * @brief 初始化DDR（如果BootROM未初始化）
 * @return true: 成功, false: 失败
 */
bool ddr_init(void)
{
    if (s_ddr_ready) {
        return true;
    }
    
    LOG_INFO("Initializing DDR...");
    
    /* 检查DDR是否已被BootROM初始化 */
    if (ddr_sanity_check()) {
        LOG_INFO("DDR already initialized by BootROM");
        s_ddr_ready = true;
        return true;
    }
    
    /* TODO: 如果DDR未初始化，需要在这里初始化DDR控制器
     * 注意：这通常需要配置DDR PHY、时序等，具体实现取决于SoC设计
     */
    
    LOG_ERROR("DDR not initialized and auto-init not implemented!");
    return false;
}

/**
 * @brief 将模型从Flash加载到DDR
 * @param model_id: 1=Level 1模型, 2=Level 2模型
 * @return true: 成功, false: 失败
 */
bool load_model_to_ddr(uint32_t model_id)
{
    uint32_t flash_src;
    uint32_t ddr_dst;
    size_t model_size;
    
    if (model_id == 1) {
        flash_src = FLASH_MODEL_L1_ADDR;
        ddr_dst = DDR_MODEL_L2_ADDR;  /* Level 1也可以放DDR（如果需要） */
        model_size = LEVEL1_MODEL_SIZE;
    } else if (model_id == 2) {
        flash_src = FLASH_MODEL_L2_ADDR;
        ddr_dst = DDR_MODEL_L2_ADDR;
        model_size = LEVEL2_MODEL_SIZE;
    } else {
        LOG_ERROR("Invalid model ID: %lu", model_id);
        return false;
    }
    
    if (!s_ddr_ready) {
        LOG_ERROR("DDR not ready, cannot load model");
        return false;
    }
    
    LOG_INFO("Loading model %lu (%zu KB) from Flash 0x%08X to DDR 0x%08X", 
             model_id, model_size / 1024, flash_src, ddr_dst);
    
    /* 从Flash复制到DDR */
    memcpy((void *)ddr_dst, (void *)flash_src, model_size);
    
    /* 数据同步屏障 */
    __DSB();
    __ISB();
    
    /* 如果启用D-Cache，使对应Cache行失效 */
    #if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1)
    /* 注意：实际实现取决于NMSIS DCACHE API */
    /* DCACHE_InvalidateByAddr((void *)ddr_dst, model_size); */
    #endif
    
    /* 验证加载 */
    if (memcmp((void *)ddr_dst, (void *)flash_src, 16) != 0) {
        LOG_ERROR("Model load verification failed!");
        return false;
    }
    
    LOG_INFO("Model %lu loaded successfully", model_id);
    return true;
}

/**
 * @brief 获取张量工作内存地址
 * @return 指向 Tensor Arena 工作内存的指针
 */
void *get_tensor_arena(void)
{
    return (void *)s_tensor_arena;
}

/**
 * @brief 获取视频队列缓冲区地址
 * @return 指向视频队列缓冲区的指针
 */
void *get_video_queue_buffer(void)
{
    return (void *)s_video_queue_buffer;
}

/**
 * @brief 获取指定模型在DDR中的地址
 * @param model_id: 模型ID
 * @return 模型地址
 */
void *get_model_address(uint32_t model_id)
{
    if (model_id == 2) {
        return (void *)DDR_MODEL_L2_ADDR;
    }
    return NULL;
}
