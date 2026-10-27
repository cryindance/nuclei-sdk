# Application - AGENTS.md

**Parent:** [SDK Root](../AGENTS.md)
**Scope:** 应用示例和模板

## OVERVIEW

应用层包含裸机和RTOS示例程序，按RTOS类型分类组织。共 25+ 示例，涵盖从入门到高级功能的完整演示。

## STRUCTURE

```
application/
├── baremetal/           # 裸机应用(无操作系统)
│   ├── benchmark/       # 性能测试(dhrystone, coremark, whetstone)
│   ├── helloworld/      # 入门示例 - 从这里开始
│   ├── cpuinfo/         # CPU信息检测
│   ├── demo_eclic/      # ECLIC中断控制器
│   ├── demo_plic/       # PLIC中断控制器
│   ├── demo_timer/      # 定时器使用
│   ├── demo_cache/      # Cache操作
│   ├── demo_dsp/        # DSP指令
│   ├── demo_pmp/        # PMP内存保护
│   ├── demo_smpu/       # SMPU安全单元
│   ├── smphello/        # 多核SMP示例
│   └── demo_*           # 更多演示...
├── freertos/            # FreeRTOS应用
│   ├── demo/
│   └── smpdemo/         # SMP多核FreeRTOS
├── rtthread/            # RT-Thread应用
│   ├── demo/
│   ├── demo_smode/      # S-Mode特权级
│   └── msh/             # Finsh命令行
├── threadx/             # ThreadX应用
│   └── demo/
└── ucosii/              # μC/OS-II应用
    └── demo/
```

## WHERE TO LOOK

| 任务 | 位置 | 说明 |
|------|------|------|
| 学习入门 | `baremetal/helloworld/` | 最简单的示例 |
| CPU特性检测 | `baremetal/cpuinfo/` | 查看支持的ISA扩展 |
| 添加新应用 | `baremetal/helloworld/` | 复制并修改 |
| 性能测试 | `baremetal/benchmark/` | Dhrystone/Coremark |
| 中断演示 | `demo_eclic/`, `demo_plic/` | 中断控制器使用 |
| SMP示例 | `baremetal/smphello/` | 多核启动 |
| RTOS入门 | `{rtos}/demo/` | 各RTOS基础示例 |

## CODE MAP

### 标准应用结构

```
application/baremetal/<app>/
├── main.c               # 应用主函数
├── *.c/h               # 应用源码(可选)
├── inc/                # 头文件目录
├── Makefile            # 构建配置
└── *.md                # 文档(可选)
```

### Makefile 模板

```makefile
TARGET = app_name
NUCLEI_SDK_ROOT = ../../..
SRCDIRS = . src
INCDIRS = . inc
include $(NUCLEI_SDK_ROOT)/Build/Makefile.base
```

## CONVENTIONS

### 应用命名

- **目录名:** 小写 + 下划线 (e.g., `demo_eclic`, `smphello`)
- **TARGET:** 与目录名一致
- **main.c:** 每个应用必须有 `int main(void)`

### 构建变量

| 变量 | 用途 | 示例 |
|------|------|------|
| `TARGET` | 输出文件名 | `TARGET = helloworld` |
| `SRCDIRS` | 源码目录 | `SRCDIRS = . src` |
| `INCDIRS` | 头文件目录 | `INCDIRS = . inc` |
| `COMMON_FLAGS` | 编译器标志 | `COMMON_FLAGS := -O2` |

### 应用配置

- **本地配置:** 创建 `Makefile.local` (gitignore)
- **全局配置:** 在 SDK 根目录创建 `Makefile.global`
- **优化级别:** 默认 `-O2`，调试用 `-O0`

## ANTI-PATTERNS

### ❌ 错误做法

```makefile
# 错误：直接覆盖 RISCV_ARCH
RISCV_ARCH = rv32imafdc

# 错误：使用绝对路径
NUCLEI_SDK_ROOT = /home/user/nuclei-sdk

# 错误：不 clean 就切换 CORE
make CORE=n300 all
make CORE=n600 all  # 可能链接失败！
```

### ✅ 正确做法

```makefile
# 正确：使用 CORE 变量间接指定
# Build 系统会自动设置 ARCH/ABI

# 正确：使用相对路径
NUCLEI_SDK_ROOT = ../../..

# 正确：切换 CORE 前 clean
make clean
make CORE=n600 all
```

## COMMANDS

```bash
# 编译应用
cd application/baremetal/helloworld
make CORE=n300 DOWNLOAD=ilm all

# 清理
make clean

# 烧录到板子
make CORE=n300 DOWNLOAD=ilm upload

# 使用 QEMU 模拟运行
make CORE=n300 DOWNLOAD=ilm run_qemu

# 调试 (选项1 - 双终端)
make CORE=n300 DOWNLOAD=ilm run_openocd  # 终端1
make CORE=n300 DOWNLOAD=ilm run_gdb      # 终端2

# 调试 (选项2 - 单终端)
make CORE=n300 DOWNLOAD=ilm debug

# 查看详细编译信息
make V=1 all

# 从 SDK 根目录编译
make PROGRAM=application/baremetal/helloworld CORE=n300 DOWNLOAD=ilm all
```

## UNIQUE STYLES

### 裸机 vs RTOS 应用

| 类型 | 特点 | 示例 |
|------|------|------|
| **Baremetal** | 直接调用NMSIS API，无OS | `helloworld`, `demo_eclic` |
| **FreeRTOS** | 使用任务、队列、信号量 | `freertos/demo/` |
| **RT-Thread** | 使用组件、Finsh命令行 | `rtthread/msh/` |
| **ThreadX** | 使用ThreadX API | `threadx/demo/` |
| **uC/OS-II** | 使用uC/OS-II API | `ucosii/demo/` |

### CPU 系列选择

通过 `CORE` 变量自动选择 CPU 系列 (200/300/600/900/1000)：
- `n300`, `n300fd` → 300 系列
- `ux600fd` → 600 系列  
- `nx900fd` → 900 系列
- `nx1000fd` → 1000 系列

### 下载模式选择

- **`ilm`** — 调试首选，代码放 ILM (RAM)，断电丢失
- **`flash`** — 生产模式，代码从 Flash 复制到 ILM 运行
- **`flashxip`** — 从 Flash 直接执行 (XIP)
- **`ddr`** — 大内存应用，需要 DDR 支持

## NOTES

### 添加新应用步骤

1. 复制 `baremetal/helloworld/` 到新目录
2. 修改 `TARGET` 为新应用名
3. 替换 `main.c` 内容
4. 添加需要的源文件到 `SRCDIRS`
5. 运行 `make clean && make CORE=n300 all`

### 应用调试技巧

- 使用 `cpuinfo` 应用确认 CPU 支持的特性
- 调试时加 `V=1` 查看完整编译命令
- GDB 端口可通过 `GDB_PORT=3344` 自定义
- 使用 `make run_qemu` 无需硬件即可测试

### 跨RTOS移植

同一个应用逻辑可以移植到不同RTOS：
- 裸机: 直接调用NMSIS API
- FreeRTOS: 包装成任务
- RT-Thread: 使用线程和Finsh

在线文档: https://doc.nucleisys.com/nuclei_sdk/design/app.html
