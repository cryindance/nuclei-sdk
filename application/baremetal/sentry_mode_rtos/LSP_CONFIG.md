# LSP配置说明

## 关于LSP告警

您看到的LSP（语言服务器协议）告警**不会影响实际编译**。这些错误是因为LSP服务器没有配置正确的头文件搜索路径。

### 实际编译

代码可以正常编译，因为Makefile已经正确配置了所有路径：

```bash
cd application/baremetal/sentry_mode_rtos
make CORE=n600fd ARCH_EXT=_xxldsp all
```

### LSP错误原因

1. **找不到头文件** (`nuclei_sdk_soc.h`等)
   - 原因: LSP服务器不知道去哪里找这些头文件
   - 解决: 已创建`.clangd`配置文件，请重启LSP或IDE

2. **类型未定义** (`uint8_t`, `bool`, `QueueHandle_t`等)
   - 原因: 头文件没找到，导致类型定义缺失
   - 解决: 头文件路径配置正确后会自动消失

3. **函数未声明** (`LOG_INFO`, `ai_init`等)
   - 原因: 同上，函数声明在头文件中
   - 解决: 头文件路径配置正确后会自动消失

### 解决LSP告警的方法

#### 方法1: 使用.clangd（已创建）

项目根目录已创建`.clangd`文件，包含所有必要的头文件路径。

**操作步骤：**
1. 确保您的IDE/编辑器使用clangd作为LSP服务器
2. 重启LSP: 在VS Code中按`Ctrl+Shift+P`，输入"Restart Language Server"
3. 等待索引完成

#### 方法2: 手动配置VS Code

如果使用VS Code + C/C++扩展，创建`.vscode/c_cpp_properties.json`：

```json
{
    "configurations": [
        {
            "name": "Nuclei RISC-V",
            "includePath": [
                "${workspaceFolder}/inc",
                "${workspaceFolder}/src",
                "${workspaceFolder}/../../../NMSIS/Core/Include",
                "${workspaceFolder}/../../../NMSIS/NN/Include",
                "${workspaceFolder}/../../../SoC/evalsoc/Common/Include",
                "${workspaceFolder}/../../../SoC/evalsoc/Board/nuclei_fpga_eval/Include",
                "${workspaceFolder}/../../../OS/FreeRTOS/Source/include",
                "${workspaceFolder}/../../../OS/FreeRTOS/Source/portable/GCC/RISC-V"
            ],
            "defines": [
                "__RISCV_FEATURE_DSP=1",
                "USE_NMSIS_NN=1"
            ],
            "compilerPath": "riscv64-unknown-elf-gcc",
            "cStandard": "c11",
            "cppStandard": "c++17",
            "intelliSenseMode": "gcc-arm"
        }
    ],
    "version": 4
}
```

#### 方法3: 忽略LSP错误

这些LSP错误**完全不影响**：
- ✅ 代码编译
- ✅ 代码运行
- ✅ 代码调试

只要`make`能成功编译，就可以放心使用。

### 头文件路径清单

| 头文件 | 实际路径 |
|--------|----------|
| `nuclei_sdk_soc.h` | `SoC/evalsoc/Common/Include/` |
| `FreeRTOS.h` | `OS/FreeRTOS/Source/include/` |
| `nmsis_core.h` | `NMSIS/Core/Include/` |
| `riscv_nnfunctions.h` | `NMSIS/NN/Include/` |

### 验证编译

运行以下命令验证代码可以正常编译：

```bash
cd /home/jingo/work/nuclei-sdk/application/baremetal/sentry_mode_rtos
make clean
make CORE=n600fd ARCH_EXT=_xxldsp all 2>&1 | head -50
```

如果没有编译错误，说明代码是正确的，LSP告警只是路径配置问题。

### 总结

- **LSP告警 ≠ 代码错误**
- 代码已通过编译测试
- 已提供`.clangd`配置文件
- 如需完美LSP体验，请根据您的IDE配置头文件路径
