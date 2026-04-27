# 车载哨兵模式视觉监控系统 Makefile
# 目标平台: Nuclei N900FD (32位 RISC-V)
# RTOS: FreeRTOS

TARGET = sentry_mode_rtos

# SDK根目录
NUCLEI_SDK_ROOT = $(CURDIR)/../../..

# CPU配置
CORE = n900fd
ARCH_EXT = _xxldsp

# 下载模式: DDR（大内存支持）
DOWNLOAD = ddr

# RTOS选择
RTOS = FreeRTOS

# 源码目录
SRCDIRS = . src
CXX_SRCDIRS = src
INCDIRS = . inc

# 编译优化
COMMON_FLAGS += -O2 -g
COMMON_FLAGS += -DUSE_NMSIS_NN
COMMON_FLAGS += -DSENTRY_LOG_LEVEL=1

# TFLM 中间件集成（自动加载 build.mk）
MIDDLEWARE := tflm

# NMSIS-NN 库
NMSIS_LIB := nmsis_nn

# 模拟/占位模式（无实际功能，仅用于编译测试）
COMMON_FLAGS += -DNO_STORAGE
# 注意: 不再定义 NO_AI_ENGINE，使用真实 TFLM

# 链接标志
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(TARGET).map

# 包含目录（添加FreeRTOS和TFLite路径）
INCDIRS += $(NUCLEI_SDK_NMSIS)/Core/Include
INCDIRS += $(NUCLEI_SDK_ROOT)/OS/FreeRTOS/Source/include
INCDIRS += $(NUCLEI_SDK_ROOT)/OS/FreeRTOS/Source/portable/GCC/RISC-V

# 链接脚本（DDR模式）
LDSCRIPT = $(NUCLEI_SDK_ROOT)/SoC/evalsoc/Board/nuclei_fpga_eval/Source/GCC/gcc_evalsoc_ddr.ld

# 引入SDK构建系统
include $(NUCLEI_SDK_ROOT)/Build/Makefile.base

# 自定义目标
.PHONY: cleanall flash debug

cleanall: clean
	@echo "Deep clean..."
	@rm -f $(TARGET).map $(TARGET).lst $(TARGET).sym

# 模型烧录（将模型文件烧录到Flash特定地址）
flash-models:
	@echo "Flashing AI models to Flash..."
	@echo "Level 1 model @ 0x20020000"
	@echo "Level 2 model @ 0x20040000"
	# TODO: 使用OpenOCD或其他工具烧录模型

# 调试目标
debug:
	@echo "Starting GDB..."
	$(GDB) -ex "target remote :3333" -ex "file $(TARGET).elf"

# 打印内存使用情况
meminfo: $(TARGET).elf
	@echo "=== Memory Usage ==="
	@$(SIZE) $(TARGET).elf
	@echo ""
	@echo "=== Section Layout ==="
	@$(OBJDUMP) -h $(TARGET).elf | grep -E "^\\s*[0-9]+"
