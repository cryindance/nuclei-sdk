# Build - AGENTS.md

**Parent:** [SDK Root](../AGENTS.md)
**Scope:** Makefile 构建系统核心

## OVERVIEW

Nuclei SDK 构建系统是一套复杂的分层 Makefile 体系，支持多 SoC、多 CPU、多工具链、多 RTOS。核心设计原则：**约定优于配置**，通过变量自动推导构建参数。

## STRUCTURE

```
Build/
├── Makefile.base           # 入口文件(应用Makefile包含)
├── Makefile.soc            # SoC验证和加载
├── Makefile.core           # CPU核心定义(ARCH/ABI)
├── Makefile.conf           # 工具链和调试配置
├── Makefile.rules          # 构建规则(all/upload/debug)
├── Makefile.rtos           # RTOS集成
├── Makefile.components     # 组件/中间件
├── Makefile.files          # 文件路径模板
├── Makefile.global         # 全局变量
├── Makefile.misc           # 杂项工具函数
├── Makefile.toolchain      # 工具链选择
├── gmsl/                   # GNU Make Standard Library
└── toolchain/
    ├── nuclei_gnu.mk       # Nuclei GCC
    ├── nuclei_llvm.mk      # Nuclei LLVM
    └── terapines.mk        # Terapines ZCC
```

## WHERE TO LOOK

| 任务 | 位置 | 说明 |
|------|------|------|
| 添加新CPU | `Makefile.core` | 定义 CORE_ARCH_ABI |
| 修改编译标志 | `Makefile.conf` | CFLAGS, LDFLAGS |
| 添加构建规则 | `Makefile.rules` | all, upload, debug |
| 支持新RTOS | `Makefile.rtos` | 加载 OS/{rtos}/build.mk |
| 工具链配置 | `toolchain/*.mk` | GCC/LLVM/IAR/ZCC |
| SoC集成 | `Makefile.soc` | 加载 SoC/build.mk |

## CODE MAP

### 包含链 (从应用到核心)

```
Application Makefile
    ↓ include Build/Makefile.base
Build/Makefile.base
    ↓ include Makefile.files
    ↓ include $(NUCLEI_SDK_NMSIS)/build.mk
    ↓ include Makefile.soc
    ↓ include Makefile.conf
    ↓ (optional) include Makefile.global
    ↓ (optional) include Makefile.local
Build/Makefile.soc
    ↓ 验证 SOC/BOARD
    ↓ include $(NUCLEI_SDK_SOC)/build.mk
SoC/evalsoc/build.mk
    ↓ 设置 ARCH/ABI/链接脚本
    ↓ (optional) include cpufeature.mk
Build/Makefile.conf
    ↓ include toolchain/$(TOOLCHAIN).mk
    ↓ include Makefile.rules
    ↓ (optional) include Makefile.rtos
    ↓ (optional) include Makefile.components
```

### 关键变量定义位置

| 变量 | 定义位置 | 说明 |
|------|----------|------|
| `CORE` | 命令行/环境 | CPU型号选择 |
| `SOC` | Makefile.base | SoC选择(默认evalsoc) |
| `BOARD` | SoC/build.mk | 板子选择 |
| `RISCV_ARCH` | Makefile.core | 从CORE自动推导 |
| `RISCV_ABI` | Makefile.core | 从CORE自动推导 |
| `LINKER_SCRIPT` | SoC/build.mk | 链接脚本路径 |
| `OPENOCD_CFG` | SoC/build.mk | OpenOCD配置 |

### CPU 定义 (Makefile.core)

```makefile
# 格式: NXXX_CORE_ARCH_ABI = ARCH ABI TUNE
N300_CORE_ARCH_ABI = rv32imac ilp32 nuclei-300-series
N300FD_CORE_ARCH_ABI = rv32imafdc ilp32d nuclei-300-series
NX600FD_CORE_ARCH_ABI = rv64imafdc lp64d nuclei-600-series
# ... 等等

SUPPORTED_CORES = n200 n200e n201 ... ux1000fd
```

## CONVENTIONS

### 变量命名

| 前缀 | 含义 | 示例 |
|------|------|------|
| `NUCLEI_SDK_` | SDK路径 | `NUCLEI_SDK_ROOT` |
| `RISCV_` | RISC-V参数 | `RISCV_ARCH`, `RISCV_ABI` |
| `CORE_` | CPU相关 | `CORE_ARCH_ABI` |
| `SOC_` | SoC相关 | 内部使用 |

### 覆盖规则

使用 `override` 强制设置变量：
```makefile
override NEWLIB := $(STDCLIB)
```

### 条件编译

```makefile
# CPU系列自动检测
ifneq ($(findstring 30,$(CORE)),)
CPU_SERIES ?= 300
endif

# 下载模式
ifeq ($(DOWNLOAD),ilm)
DOWNLOAD_MODE := DOWNLOAD_MODE_ILM
endif
```

## ANTI-PATTERNS

### ❌ 禁止事项

```makefile
# 错误：在应用Makefile中直接设置ARCH/ABI
RISCV_ARCH = rv32imafdc
RISCV_ABI = ilp32d

# 错误：覆盖只读变量
SOC = mysoc  # 应该通过命令行传入

# 错误：在子Makefile中重新包含Build系统
include $(NUCLEI_SDK_ROOT)/Build/Makefile.base
```

### ✅ 正确做法

```makefile
# 正确：使用CORE变量
# make CORE=n300fd all

# 正确：通过命令行或环境变量
make SOC=mysoc CORE=n300 all
export SOC=mysoc

# 正确：只包含一次base
include $(NUCLEI_SDK_ROOT)/Build/Makefile.base
```

## UNIQUE STYLES

### CPU系列自动推导

根据 `CORE` 变量自动识别系列：
```makefile
# n300 → 300系列
# ux600fd → 600系列  
# nx900fd → 900系列
# nx1000fd → 1000系列
```

### 下载模式 (DOWNLOAD)

| 模式 | 说明 | 链接器宏 |
|------|------|----------|
| `ilm` | ILM代码 + DLM数据 | `DOWNLOAD_MODE_ILM` |
| `flash` | Flash→ILM代码 + Flash→DLM数据 | `DOWNLOAD_MODE_FLASH` |
| `flashxip` | Flash代码(XIP) + DLM数据 | `DOWNLOAD_MODE_FLASHXIP` |
| `ddr` | DDR代码 + DDR数据 | `DOWNLOAD_MODE_DDR` |
| `sram` | SRAM代码 + SRAM数据 | `DOWNLOAD_MODE_SRAM` |

### 多工具链支持

通过 `TOOLCHAIN` 变量切换：
```makefile
TOOLCHAIN ?= nuclei_gnu  # 默认
# nuclei_gnu, nuclei_llvm, terapines, iar
```

工具链文件定义：
- `CC`, `CXX`, `AS`, `LD`, `AR`
- 编译器标志
- 链接器标志

### 调试配置

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `GDB_PORT` | 3333 | GDB服务器端口 |
| `RBB_PORT` | 9824 | Remote Bitbang端口 |
| `GDBREMOTE` | OpenOCD管道 | GDB远程目标 |

### 源文件发现

自动从 `SRCDIRS` 收集源文件：
```makefile
SRCDIRS = . src
# 自动包含 .c, .S, .cpp 文件
```

### 中间件组件

通过 `Makefile.components` 集成：
```makefile
# Components/profiling/build.mk
COMPONENTS += profiling
```

## COMMANDS

### 构建命令

```bash
# 基础构建
make CORE=n300 DOWNLOAD=ilm all

# 清理
make clean

# 完全清理(所有配置)
make cleanall

# 从SDK根目录构建
make PROGRAM=application/baremetal/helloworld CORE=n300 all

# 构建所有应用
make -k buildall

# 详细输出
make V=1 all
```

### 调试命令

```bash
# 启动 OpenOCD (终端1)
make CORE=n300 DOWNLOAD=ilm run_openocd

# 启动 GDB (终端2)
make CORE=n300 DOWNLOAD=ilm run_gdb

# 一键调试
make CORE=n300 DOWNLOAD=ilm debug

# 自定义GDB端口
make GDB_PORT=3344 run_openocd
make GDB_PORT=3344 run_gdb
```

### 运行命令

```bash
# QEMU模拟
make CORE=n300 DOWNLOAD=ilm run_qemu

# xlspike模拟
make CORE=n300 DOWNLOAD=ilm run_xlspike

# 上传程序
make CORE=n300 DOWNLOAD=ilm upload
```

## NOTES

### 添加新CPU支持

1. 在 `Makefile.core` 添加：
```makefile
NXXX_CORE_ARCH_ABI = rv32imac ilp32 nuclei-xxx-series
```

2. 添加到 `SUPPORTED_CORES`：
```makefile
SUPPORTED_CORES = ... nxxx ...
```

3. 在 SoC 的 `cpufeature.mk` 中添加特性支持

### 添加新下载模式

1. 在 `Makefile.conf` 添加模式检测
2. 创建新的链接脚本 `gcc_<soc>_<mode>.ld`
3. 更新 SoC 的 `build.mk`

### 构建系统调试

```bash
# 查看变量值
make -p | grep RISCV_ARCH

# 查看包含的文件
make -d 2>&1 | head -100

# 详细编译
make V=1 all 2>&1 | tee build.log
```

### 性能优化

```makefile
# 应用Makefile中优化
COMMON_FLAGS := -O3 -funroll-loops -fpeel-loops
```

### 内存优化

```bash
# 使用小体积C库
make STDCLIB=newlib_small all
# 或
make STDCLIB=libncrt_small all
```

在线文档: https://doc.nucleisys.com/nuclei_sdk/develop/buildsystem.html
