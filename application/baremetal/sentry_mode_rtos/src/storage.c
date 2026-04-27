/******************************************************************************
 * @file    storage.c
 * @brief   FatFs文件系统操作实现
 ******************************************************************************/

#include "sentry_mode.h"

#ifndef NO_STORAGE
static FATFS s_fatfs;
#endif
static bool s_sd_initialized = false;

/**
 * @brief 初始化SD卡和FatFs
 */
bool sd_card_init(void)
{
    if (s_sd_initialized) {
        return true;
    }
    
    LOG_INFO("Initializing SD card...");
    
#ifdef NO_STORAGE
    LOG_INFO("SD card disabled (NO_STORAGE mode)");
    s_sd_initialized = true;
    return true;
#else
    FRESULT res = f_mount(&s_fatfs, "", 1);
    if (res != FR_OK) {
        LOG_ERROR("SD mount failed: %d", res);
        return false;
    }
    
    /* 创建sentry目录 */
    res = f_mkdir("/sentry");
    if (res != FR_OK && res != FR_EXIST) {
        LOG_WARN("Failed to create /sentry directory: %d", res);
    }
    
    /* 获取并显示SD卡信息 */
    DWORD fre_clust;
    FATFS *fs;
    if (f_getfree("", &fre_clust, &fs) == FR_OK) {
        uint64_t total = (fs->n_fatent - 2) * fs->csize * 512;
        uint64_t free = fre_clust * fs->csize * 512;
        LOG_INFO("SD Card: Total=%llu MB, Free=%llu MB", 
                total / (1024 * 1024), free / (1024 * 1024));
    }
    
    s_sd_initialized = true;
    LOG_INFO("SD card initialized");
    return true;
#endif
}

/**
 * @brief 检查SD卡剩余空间
 */
bool storage_check_space(void)
{
    if (!s_sd_initialized) {
        return false;
    }
    
#ifdef NO_STORAGE
    return true;
#else
    DWORD fre_clust;
    FATFS *fs;
    FRESULT res = f_getfree("", &fre_clust, &fs);
    
    if (res != FR_OK) {
        LOG_ERROR("Failed to get free space");
        return false;
    }
    
    uint64_t free_bytes = fre_clust * fs->csize * 512;
    uint64_t free_mb = free_bytes / (1024 * 1024);
    
    /* 保留100MB */
    if (free_mb < 100) {
        LOG_WARN("SD card space low: %llu MB", free_mb);
        return false;
    }
    
    return true;
#endif
}

/**
 * @brief 删除最旧的视频文件（循环覆盖）
 */
#ifndef NO_STORAGE
static void delete_oldest_file(void)
{
    DIR dir;
    FILINFO fno;
    FILINFO oldest;
    char oldest_name[256];
    bool found = false;
    
    if (f_opendir(&dir, "/sentry") != FR_OK) {
        return;
    }
    
    oldest.fdate = 0xFFFF;
    oldest.ftime = 0xFFFF;
    
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        if (fno.fdate < oldest.fdate || 
            (fno.fdate == oldest.fdate && fno.ftime < oldest.ftime)) {
            oldest = fno;
            strcpy(oldest_name, fno.fname);
            found = true;
        }
    }
    
    f_closedir(&dir);
    
    if (found) {
        char path[300];
        snprintf(path, sizeof(path), "/sentry/%s", oldest_name);
        f_unlink(path);
        LOG_INFO("Deleted old file: %s", path);
    }
}
#endif

/**
 * @brief 创建视频文件
 */
bool storage_create_file(const char *filename)
{
    if (!s_sd_initialized) {
        return false;
    }
    
#ifndef NO_STORAGE
    /* 检查空间 */
    if (!storage_check_space()) {
        delete_oldest_file();
    }
#endif
    
    /* TODO: 实际的文件创建在tasks.c中通过f_open完成 */
    (void)filename;
    
    return true;
}

/**
 * @brief SD卡休眠
 */
void sd_card_sleep(void)
{
    /* TODO: 实现SD卡低功耗 */
    LOG_DEBUG("SD card sleep");
}
