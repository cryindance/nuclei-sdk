# 哨兵模式项目文档索引

本目录集中存放 `sentry_mode_rtos`（车载哨兵模式视觉监控系统）的设计、AI 集成、修改记录与配置文档。
代码目录保留 `README.md`（项目说明）与 `Makefile`，其余文档统一放这里。

## 一、文档列表

### 设计
| 文档 | 说明 |
|------|------|
| [design/哨兵模式详细设计文档.md](design/哨兵模式详细设计文档.md) | **主设计文档 v4.0（N900）**：三级状态机、三级 AI 架构、内存布局、任务划分、构建与调试 |
| [design/哨兵模式状态机参考.md](design/哨兵模式状态机参考.md) | 状态机状态、事件、迁移关系速查（含状态机图） |

### AI 集成
| 文档 | 说明 |
|------|------|
| [ai/哨兵功能神经网络模型集成记录.md](ai/哨兵功能神经网络模型集成记录.md) | TFLM + SSD MobileNet v2 FPN-lite 的真实模型集成过程、编译问题与解决记录 |

### 变更历史
| 文档 | 说明 |
|------|------|
| [history/哨兵模式修改记录.md](history/哨兵模式修改记录.md) | 需求、设计、代码的变更历史与决策记录 |

### 配置
| 文档 | 说明 |
|------|------|
| [setup/LSP_CONFIG.md](setup/LSP_CONFIG.md) | clangd / compile_commands.json / .clangd 的 LSP 配置说明 |

## 二、阅读顺序建议

| 场景 | 建议顺序 |
|------|---------|
| 第一次接触项目 | `../README.md` → `design/哨兵模式详细设计文档.md` → `design/哨兵模式状态机参考.md` |
| 排查 AI 推理问题 | `ai/哨兵功能神经网络模型集成记录.md` → `design/哨兵模式详细设计文档.md`（第 9 章 AI 相关章节） |
| 恢复开发环境 | `setup/LSP_CONFIG.md` → `../README.md`（编译命令） |
| 查历史决策 | `history/哨兵模式修改记录.md` |

## 三、资源说明

| 资源 | 状态 |
|------|------|
| `design/SENTRY_MODE_DESIGN_images/` | ✅ 三级状态机图（`.dot` + `.svg`） |
| `design/DETAILED_DESIGN_images/tflm_freertos_arch.*` | ✅ TFLM 软件栈图（从 nuclei-linux-sdk 文档中迁回） |
| `design/DETAILED_DESIGN_images/detailed_memory_layout.*` | ✅ 已重新生成（`.dot` 源文件同目录，依据 §3.3 与实际链接脚本） |
| `ai/哨兵功能神经网络模型集成记录_images/` | ✅ L1 网络架构图（`.dot` + `.svg`） |

> **构建说明**：当前 `Makefile` 使用 `gcc_evalsoc_ddr.ld`（用 DDR 模拟 ROM/RAM），ILM/DLM 未启用；内存布局图按实际构建绘制。设计文档 §6.4 对链接脚本的描述与实现存在差异，待核对。

## 四、版本提示

- 设计文档 **v4.0（N900 平台，2026-04-25）**
- 代码目录 `README.md` 已同步为 **N900** 平台描述

## 五、文档维护约定

1. 新增文档按类别放入 `design/`、`ai/`、`history/`、`setup/`
2. 图片目录命名为 `{文档名}_images/`，与文档同目录，使用相对路径引用
3. 图表使用 Graphviz DOT 生成，保留 `.dot` 源文件
4. 新增或重命名文档后同步更新本索引与仓库根 `AGENTS.md`
5. 本目录内容备份到 `~/work/doc/nuclei-sdk/sentry/`
