# SoC/evalsoc - AGENTS.md

**Parent:** [SDK Root](../AGENTS.md)
**Scope:** EvalSoC 评估板支持包（参考实现）

## OVERVIEW

EvalSoC 是 Nuclei SDK 的**参考 SoC 实现**，展示如何为 Nuclei RISC-V CPU 创建完整的板级支持包(BSP)。可作为移植到自定义 SoC 的模板。

**支持工具链:** GCC/Clang, IAR, Terapines ZCC

## STRUCTURE

```
SoC/evalsoc/
├── Board/
│   └── nuclei_fpga_eval/           # FPGA评估板
│       ├── Include/
│       │   ├── nuclei_sdk_hal.h    # HAL接口(必须)
│       │   └── board_nuclei_fpga_eval.h
│       ├── Source/
│       │   ├── GCC/                # GCC链接脚本
│       │   │   ├── gcc_evalsoc_ilm.ld
│       │   │   ├── gcc_evalsoc_flash.ld
│       │   │   ├── gcc_evalsoc_flashxip.ld
│       │   │   ├── gcc_evalsoc_ddr.ld
│       │   │   └── gcc_evalsoc_sram.ld
│       │   └── IAR/                # IAR链接脚本
│       ├── openocd_evalsoc.cfg     # OpenOCD调试配置
│       └── npk.yml                 # Nuclei Studio配置
├── Common/
│   ├── Include/
│   │   ├── cpufeature.h            # CPU特性(自动生成)
│   │   ├── evalsoc.h               # SoC核心定义
│   │   ├── evalsoc_uart.h          # UART驱动
│   │   ├── system_evalsoc.h        # 系统初始化
│   │   └── nuclei_sdk_soc.h        # SDK接口(必须)
│   ├── Source/
│   │   ├── GCC/                    # GCC启动代码
│   │   │   ├── startup_evalsoc.S   # 启动汇编
│   │   │   ├── intexc_evalsoc.S    # 中断/异常(M模式)
│   │   │   └── intexc_evalsoc_s.S  # 中断/异常(S模式)
│   │   ├── IAR/                    # IAR启动代码
│   │   ├── Drivers/
│   │   │   └── evalsoc_uart.c      # UART实现
│   │   ├── Stubs/                  # C库桩函数
│   │   │   ├── newlib/             # Newlib桩
│   │   │   ├── libncrt/            # Nuclei C Runtime桩
│   │   │   └── iardlib/            # IAR DLib桩
│   │   ├── evalsoc_common.c        # 通用函数
│   │   └── system_evalsoc.c        # 系统初始化
│   └── evalsoc.svd                 # SVD寄存器定义
├── build.mk                        # 构建规则(必须)
├── cpufeature.mk                   # CPU扩展选择
├── runmode.mk                      # 运行模式(内部用)
└── README.md                       # 移植指南
```

## WHERE TO LOOK

| 任务 | 位置 | 说明 |
|------|------|------|
| 移植到自定义SoC | `README.md` | 完整移植指南 |
| 添加新板子 | `Board/<new_board>/` | 参考 `nuclei_fpga_eval` |
| 修改链接脚本 | `Board/*/Source/GCC/*.ld` | 内存布局 |
| 调试配置 | `Board/*/openocd_*.cfg` | OpenOCD配置文件 |
| SoC定义 | `Common/Include/evalsoc.h` | 寄存器地址、中断号 |
| 启动代码 | `Common/Source/GCC/startup_*.S` | 启动汇编 |
| UART驱动 | `Common/Source/Drivers/` | 串口实现 |

## CODE MAP

### 关键符号

| 符号 | 类型 | 位置 | 作用 |
|------|------|------|------|
| `_start` | label | startup_evalsoc.S | CPU复位入口 |
| `SystemInit()` | function | system_evalsoc.c | 系统初始化 |
| `evalsoc_init()` | function | evalsoc_common.c | SoC初始化 |
| `ECLIC_Init()` | function | evalsoc.h | 中断控制器初始化 |
| `SystemCoreClock` | variable | system_evalsoc.h | 系统时钟频率 |

### 启动流程

```
_start (startup_evalsoc.S)
    ↓
SystemInit() (system_evalsoc.c)
    ↓
    ├── 系统时钟初始化
    ├── Cache初始化
    ├── ECLIC/PLIC初始化
    ├── UART初始化
    └── SMP启动(如支持)
    ↓
main() (应用程序)
```

### 中断处理

| 文件 | 模式 | 用途 |
|------|------|------|
| `intexc_evalsoc.S` | M模式 | 机器模式中断/异常 |
| `intexc_evalsoc_s.S` | S模式 | 监督模式中断/异常 |

## CONVENTIONS

### 必需文件

每个 SoC 必须提供：

| 文件 | 位置 | 说明 |
|------|------|------|
| `build.mk` | `SoC/<soc>/` | 构建配置 |
| `nuclei_sdk_soc.h` | `Common/Include/` | SDK入口 |
| `nuclei_sdk_hal.h` | `Board/<board>/Include/` | HAL入口 |

### 命名规范

- **SoC名:** 全小写 (e.g., `evalsoc`, `gd32vf103`)
- **板子名:** 全小写 (e.g., `nuclei_fpga_eval`)
- **文件名:** 小写 + 下划线 (e.g., `system_evalsoc.c`)
- **链接脚本:** `gcc_<soc>_<mode>.ld`

### 链接脚本命名

| 文件 | 代码位置 | 数据位置 | 用途 |
|------|----------|----------|------|
| `gcc_*_ilm.ld` | ILM | DLM | 调试 |
| `gcc_*_flash.ld` | Flash→ILM | Flash→DLM | 生产 |
| `gcc_*_flashxip.ld` | Flash | Flash→DLM | XIP执行 |
| `gcc_*_ddr.ld` | DDR | DDR | 大内存 |
| `gcc_*_sram.ld` | SRAM | SRAM | 小内存 |

## ANTI-PATTERNS

### ❌ 禁止事项

- **不要** 直接修改 `cpufeature.h` — 由 `nuclei_gen` 工具自动生成
- **不要** 在应用 Makefile 中覆盖 `RISCV_ARCH` — 使用 `CORE` 变量
- **不要** 使用 `runmode.mk` — 标记为"Nuclei内部使用"

### IAR 限制

- IAR 不支持 `flash` 下载模式
- 向量表不放在 ILM/ROM 起始地址 — 需要手动对齐 MTVT CSR
- SMP 应用使用自定义 `startup.S` 而非 IAR 提供的

## UNIQUE STYLES

### build.mk 结构

```makefile
# 必须定义的内容
BOARD = nuclei_fpga_eval
NUCLEI_SDK_SOC_BOARD = $(NUCLEI_SDK_ROOT)/SoC/evalsoc/Board/$(BOARD)
NUCLEI_SDK_SOC_COMMON = $(NUCLEI_SDK_ROOT)/SoC/evalsoc/Common

# 链接脚本
LINKER_SCRIPT = $(NUCLEI_SDK_SOC_BOARD)/Source/GCC/gcc_evalsoc_$(DOWNLOAD).ld

# OpenOCD配置
OPENOCD_CFG = $(NUCLEI_SDK_SOC_BOARD)/openocd_evalsoc.cfg

# ARCH/ABI 由 CORE 变量自动选择
# 从 Makefile.core 获取
```

### NPK 配置

`npk.yml` 用于 Nuclei Studio IDE：
```yaml
package:
  name: SoC.evalsoc
  vendor: nuclei
  version: x.x.x
  description: EvalSoC Support Package
```

### CPU 系列自动识别

Makefile.conf 自动识别 CPU 系列：
```makefile
ifneq ($(findstring 30,$(CORE)),)
CPU_SERIES ?= 300
else ifneq ($(findstring 60,$(CORE)),)
CPU_SERIES ?= 600
# ... 等等
```

## COMMANDS

```bash
# 使用 evalsoc 构建(默认)
make CORE=n300 DOWNLOAD=ilm all

# 指定 SoC 和 Board
make SOC=evalsoc BOARD=nuclei_fpga_eval CORE=n300 all

# 调试
make CORE=n300 DOWNLOAD=ilm run_openocd  # 终端1
make CORE=n300 DOWNLOAD=ilm run_gdb      # 终端2

# QEMU 模拟 (仅 evalsoc)
make CORE=n300 DOWNLOAD=ilm run_qemu
```

## NOTES

### 移植到自定义 SoC

参考: https://doc.nucleisys.com/nuclei_studio_supply/28-quick_porting_from_evalsoc_to_customsoc_based_on_Nuclei_SDK

关键步骤：
1. 复制 `SoC/evalsoc/` 到新目录 `SoC/<yoursoc>/`
2. 修改 `evalsoc.h` → `<yoursoc>.h`，更新寄存器地址
3. 修改 `system_evalsoc.c` 更新时钟初始化
4. 创建新的链接脚本匹配你的内存映射
5. 更新 `build.mk` 中的路径
6. 创建 `nuclei_sdk_soc.h` 和 `nuclei_sdk_hal.h`

### SVD 文件

`evalsoc.svd` 用于调试器显示寄存器，可从 SoC 设计工具导出。

### 启动代码定制

- **GCC:** 编辑 `startup_evalsoc.S`
- **IAR:** 编辑 `startup.S` 和 `startup_evalsoc.c`
- 保持 `_start` 符号作为入口点

### 多核 SMP

- 使用 `CORE=ux900fd` (带 SMP 标记的 CPU)
- 定义 `SMP_CPU_CNT=2` (或 4)
- 需要 DDR 链接脚本
- 参考 `application/baremetal/smphello/`

在线文档: https://doc.nucleisys.com/nuclei_sdk/design/soc/evalsoc.html
