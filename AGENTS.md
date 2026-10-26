# Nuclei SDK - AGENTS.md

**Generated:** 2025-03-25
**Commit:** 2fd229ce
**Branch:** master

## OVERVIEW

Nuclei SDK 是芯来科技 RISC-V 处理器的软件开发工具包。支持 Nuclei 200/300/600/900/1000 系列 CPU，集成 NMSIS 框架，支持 FreeRTOS/UCOSII/RT-Thread/ThreadX 等多个 RTOS。

**核心栈:** RISC-V GCC/Clang, IAR, Make, OpenOCD, NMSIS

## STRUCTURE

```
./                          # SDK 根目录
├── application/            # 应用示例 (裸机/RTOS)
│   ├── baremetal/          # 25+ 裸机示例程序
│   ├── freertos/           # FreeRTOS 应用
│   ├── rtthread/           # RT-Thread 应用
│   ├── threadx/            # ThreadX 应用
│   └── ucosii/             # μC/OS-II 应用
├── Build/                  # Makefile 构建系统核心
├── NMSIS/                  # Nuclei 微控制器软件接口标准
│   ├── Core/               # 核心头文件
│   ├── DSP/                # 数字信号处理库
│   ├── NN/                 # 神经网络库
│   └── Library/            # 预编译库
├── OS/                     # RTOS 包
│   ├── FreeRTOS/
│   ├── RTThread/
│   ├── ThreadX/
│   └── UCOSII/
├── SoC/                    # SoC 支持包
│   ├── evalsoc/            # 评估 SoC (参考实现)
│   ├── gd32vf103/          # GD32VF103 SoC
│   └── gd32vw55x/          # GD32VW55x SoC
├── test/                   # CTEST 测试框架
├── tools/                  # 脚本工具
└── doc/                    # 文档源码
```

## WHERE TO LOOK

| 任务 | 位置 | 说明 |
|------|------|------|
| 添加新应用 | `application/baremetal/` | 复制 helloworld 模板 |
| 修改构建规则 | `Build/*.mk` | Makefile.core, Makefile.conf |
| 支持新 CPU | `Build/Makefile.core` | 添加 CORE_ARCH_ABI |
| 新 SoC 移植 | `SoC/evalsoc/` | 参考实现 |
| 调试配置 | `SoC/evalsoc/Board/*/Source/` | OpenOCD 配置文件 |
| 链接脚本 | `SoC/*/Board/*/Source/GCC/` | .ld 文件 |
| 测试框架 | `test/` | ctest.h + core/ |
| CI 配置 | `.github/workflows/`, `.gitlab-ci.yml` | GitHub/GitLab CI |

## CODE MAP

### 关键符号

| 符号 | 类型 | 位置 | 作用 |
|------|------|------|------|
| `main()` | function | application/*/*/main.c | 应用入口 |
| `_start` | label | startup_evalsoc.S | CPU 启动入口 |
| `SystemInit()` | function | system_evalsoc.c | 系统初始化 |
| `ECLIC_Init()` | function | evalsoc.h | 中断控制器初始化 |
| `nmsis_get_cpu_freq()` | function | system_evalsoc.h | 获取 CPU 频率 |

### 核心架构

```
应用层 (application/)
    ↓
NMSIS API (NMSIS/Core/Include/)
    ↓
SoC HAL (SoC/*/Common/Include/)
    ↓
硬件 (SoC 实现)
```

## CONVENTIONS

### 代码风格 (.editorconfig)

- **缩进:** 4 空格 (C/H 文件)
- **换行:** LF (Unix)
- **括号:** K&R 风格 (左括号不换行)
- **编码:** UTF-8
- **Makefile:** Tab 缩进, 8 字符

### 命名规范

- **文件:** 小写 + 下划线 (e.g., `startup_evalsoc.S`)
- **SoC 目录:** 全小写 (e.g., `evalsoc`, `gd32vf103`)
- **宏定义:** 全大写 + 下划线 (e.g., `NUCLEI_SDK_SOC_H`)
- **函数:** 小写 + 下划线 (e.g., `evalsoc_init()`)

### Makefile 模式

```makefile
TARGET = app_name
NUCLEI_SDK_ROOT = ../../..
SRCDIRS = . src
INCDIRS = . inc
include $(NUCLEI_SDK_ROOT)/Build/Makefile.base
```

详见下方 **BUILD SYSTEM** 章节的完整构建变量说明。

## ANTI-PATTERNS (THIS PROJECT)

### 禁止事项

- **不要** 修改 `OS/` 下的 RTOS 源码 (应使用 port 层)
- **不要** 直接编辑 `NMSIS/Library/` 中的预编译库
- **不要** 在应用 Makefile 中覆盖 `RISCV_ARCH` (使用 `CORE` 变量)
- **不要** 跳过 `make clean` 就切换 `CORE` 或 `DOWNLOAD`

### 常见错误

```bash
# ❌ 错误：不 clean 直接切换 CORE
make CORE=n300 all
make CORE=n600 all  # 可能链接错误！

# ✅ 正确：先 clean
make CORE=n300 all
make clean
make CORE=n600 all
```

## UNIQUE STYLES

### CPU 系列识别

Makefile 自动识别 CPU 系列 (200/300/600/900/1000) 用于配置优化:
```makefile
# Build/Makefile.conf 自动处理
ifneq ($(findstring 30,$(CORE)),)
CPU_SERIES ?= 300
```

### NPK 配置

每个 SoC/Board 都有 `npk.yml` 用于 Nuclei Studio IDE 集成。

## BUILD SYSTEM

### 构建系统架构

Nuclei SDK 使用基于 Makefile 的构建系统，核心文件位于 `Build/` 目录：

| 文件 | 作用 |
|------|------|
| `Makefile.base` | 构建系统入口，所有应用 Makefile 必须包含 |
| `Makefile.conf` | 编译器配置、调试选项 |
| `Makefile.rules` | 构建规则、目标定义 |
| `Makefile.files` | 通用源文件和头文件路径 |
| `Makefile.soc` | SoC 相关配置 |
| `Makefile.rtos` | RTOS 支持 |
| `Makefile.core` | CPU 核心列表定义 |
| `toolchain/` | 工具链支持 (GNU, LLVM, ZCC) |

### Makefile 变量

**SoC/Board 配置**：
| 变量 | 说明 | 示例 |
|------|------|------|
| `SOC` | SoC 名称 | `evalsoc`, `gd32vf103` |
| `BOARD` | 板级名称 | `nuclei_fpga_eval` |
| `CORE` | CPU 核心 | `n300`, `n600fd`, `ux900fd` |
| `ARCH_EXT` | 架构扩展 | `_xxldsp` (DSP 扩展) |
| `DOWNLOAD` | 下载模式 | `ilm`, `flash`, `flashxip`, `ddr` |

**工具链配置**：
| 变量 | 说明 |
|------|------|
| `TOOLCHAIN` | 工具链类型 (`nuclei_gnu`, `nuclei_llvm`, `zcc`) |
| `STDCLIB` | 标准 C 库 (`newlib_nano`, `libncrt_small`) |

**应用级变量**：
| 变量 | 说明 |
|------|------|
| `TARGET` | 目标文件名 |
| `SRCDIRS` | 源码目录 (e.g., `. src`) |
| `INCDIRS` | 头文件目录 (e.g., `. inc`) |
| `RTOS` | RTOS 类型 (FreeRTOS, UCOSII, RTThread) |
| `COMMON_FLAGS` | 通用编译选项 (CFLAGS/CXXFLAGS/ASMFLAGS) |
| `LDFLAGS` | 链接选项 |

### 下载模式 (DOWNLOAD)

| 模式 | 代码位置 | 数据位置 | 用途 |
|------|----------|----------|------|
| `ilm` | ILM (RAM) | DLM | 调试首选，掉电丢失 |
| `flash` | Flash→ILM | Flash→DLM | 生产模式，从 Flash 复制到 ILM |
| `flashxip` | Flash | Flash→DLM | XIP 执行 |
| `ddr` | DDR | DDR | 大内存应用，需要 DDR 支持 |

### 编译流程

**方式1：在应用目录编译**（推荐）
```bash
# 进入应用目录
cd application/baremetal/helloworld

# 清理之前的构建（重要！切换 CORE 或 DOWNLOAD 时必须 clean）
make CORE=n300 DOWNLOAD=ilm clean

# 编译
make CORE=n300 DOWNLOAD=ilm all

# 查看编译生成的 ELF 文件
ls -la *.elf
```

**方式2：从 SDK 根目录编译**
```bash
# 在 SDK 根目录使用 PROGRAM 变量
make PROGRAM=application/baremetal/helloworld CORE=n300 DOWNLOAD=ilm all
```

### 常用 Make 目标

```bash
make all          # 编译应用
make clean        # 清理构建文件
make upload       # 下载并运行到板子
make debug        # 启动调试（GDB）
make run_qemu     # QEMU 模拟运行
make run_openocd  # 启动 OpenOCD 调试服务器
make run_gdb      # 启动 GDB 客户端
make help         # 显示所有可用目标
make info         # 显示当前配置信息
```

### 环境设置

```bash
# Linux - 设置工具链环境
source setup.sh

# 验证环境
echo $PATH
which riscv64-unknown-elf-gcc openocd make

# 如果手动设置工具链路径
export NUCLEI_TOOL_ROOT=/path/to/NucleiStudio/toolchain
export PATH=$NUCLEI_TOOL_ROOT/gcc/bin:$PATH
export PATH=$NUCLEI_TOOL_ROOT/openocd/bin:$PATH
```

### 编译输出解读

```
Current Configuration: 
  TOOLCHAIN=nuclei_gnu
  RISCV_ARCH=rv32imafdc_xxldsp
  RISCV_ABI=ilp32d
  SOC=evalsoc
  CORE=n600fd
  DOWNLOAD=ddr

   text    data     bss     dec     hex filename
  25508   68680 4749644 4843832  49e938 app.elf
```

- **text**: 代码段大小 (Flash/ILM)
- **data**: 已初始化数据 (Flash → RAM)
- **bss**: 未初始化数据 (RAM)
- **dec/hex**: 总大小

### 实际编译示例

**编译 sentry_mode_rtos 项目**（N600FD + DDR + FreeRTOS）：
```bash
cd /home/jingo/work/nuclei-sdk

# 设置环境
source setup.sh

# 编译
cd application/baremetal/sentry_mode_rtos
make CORE=n600fd ARCH_EXT=_xxldsp DOWNLOAD=ddr clean
make CORE=n600fd ARCH_EXT=_xxldsp DOWNLOAD=ddr all

# 或使用 PROGRAM 变量从根目录编译
make PROGRAM=application/baremetal/sentry_mode_rtos \
     CORE=n600fd ARCH_EXT=_xxldsp DOWNLOAD=ddr all
```

**关键参数说明**：
- `CORE=n600fd`: N600FD 处理器（32位，带 FPU 和 DSP）
- `ARCH_EXT=_xxldsp`: 启用 DSP 扩展
- `DOWNLOAD=ddr`: 代码和数据都放到 DDR

### 调试命令

```bash
# 方法1：双终端调试
# 终端1 - 启动 OpenOCD
make CORE=n300 DOWNLOAD=ilm run_openocd

# 终端2 - 启动 GDB
make CORE=n300 DOWNLOAD=ilm run_gdb

# 在 GDB 中
(gdb) load        # 加载程序
(gdb) b main      # 在 main 设置断点
(gdb) c           # 继续运行

# 方法2：单命令调试（自动启动 OpenOCD + GDB）
make CORE=n300 DOWNLOAD=ilm debug
```

## COMMANDS

```bash
# 环境设置 (Linux)
source setup.sh

# 编译应用
cd application/baremetal/helloworld
make CORE=n300 DOWNLOAD=ilm all

# 烧录到板子
make CORE=n300 DOWNLOAD=ilm upload

# 调试 (需要两个终端)
# 终端1:
make CORE=n300 DOWNLOAD=ilm run_openocd
# 终端2:
make CORE=n300 DOWNLOAD=ilm run_gdb

# 使用 QEMU 模拟运行
make CORE=n300 DOWNLOAD=ilm run_qemu

# 运行测试
cd test/core
make CORE=n300 all

# 查看帮助
make help

# 构建文档
cd doc
make html
```

## NOTES

### 工具链要求

- Nuclei Studio >= 2023.10
- 工具链前缀: `riscv64-unknown-elf-` (0.5.0+)
- OpenOCD 用于调试

### 调试端口

- GDB 默认端口: 3333
- RBB (Remote Bitbang) 默认: 9824
- 可通过 `GDB_PORT=xxxx` 自定义

### 多核支持 (SMP)

- 在 `CORE` 中添加 SMP 标记: e.g., `CORE=ux900fd`
- 使用 `SMP_CPU_CNT=4` 指定核数
- 需要 DDR 链接脚本

### 性能分析

使用 `Components/profiling/` 中的工具:
- `gprof` - 函数级性能分析
- `gcov` - 代码覆盖率

### 文档构建

```bash
cd doc
make html  # 构建 HTML 文档
```

在线文档: https://doc.nucleisys.com/nuclei_sdk

## SENTRY MODE DOCS

哨兵模式（`application/baremetal/sentry_mode_rtos`）的项目文档统一存放于
`application/baremetal/sentry_mode_rtos/doc/`，完整索引见该目录的 `README.md`。
开始分析或修改哨兵相关代码前，先读索引确认已有设计与历史记录。

| 主题 | 入口文档 | 内容 |
|------|---------|------|
| 详细设计 | [doc/design/哨兵模式详细设计文档.md](application/baremetal/sentry_mode_rtos/doc/design/哨兵模式详细设计文档.md) | 主设计文档 v4.0（N900）：状态机、AI 架构、内存布局 |
| 状态机参考 | [doc/design/哨兵模式状态机参考.md](application/baremetal/sentry_mode_rtos/doc/design/哨兵模式状态机参考.md) | 状态/事件/迁移速查 |
| AI 集成 | [doc/ai/哨兵功能神经网络模型集成记录.md](application/baremetal/sentry_mode_rtos/doc/ai/哨兵功能神经网络模型集成记录.md) | TFLM + SSD MobileNet v2 集成与问题记录 |
| 修改记录 | [doc/history/哨兵模式修改记录.md](application/baremetal/sentry_mode_rtos/doc/history/哨兵模式修改记录.md) | 需求/设计/代码变更历史 |
| LSP 配置 | [doc/setup/LSP_CONFIG.md](application/baremetal/sentry_mode_rtos/doc/setup/LSP_CONFIG.md) | clangd 与 compile_commands 配置 |

**维护约定**：新增文档按类别放入 `design/` / `ai/` / `history/` / `setup/`；图片目录 `{文档名}_images/`；保留 Graphviz `.dot` 源文件；更新后同步 `doc/README.md` 与本索引。
