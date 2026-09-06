# C4 固件烧录工具

无需打开 Keil，选择 bin 版本后 ST-Link 一键烧录。

**内置 pyOCD**，打包后的 `C4Flasher.exe` 不依赖 STM32CubeProgrammer。

## 依赖

- **ST-Link 调试器** + USB 线
- Windows 需安装 **ST-Link USB 驱动**（可用 [STSW-LINK009](https://www.st.com/en/development-tools/stsw-link009.html) 或装过 Keil / CubeProgrammer 时通常已有）

## 运行方式

### 方式 A：Python 直接运行（开发调试）

```bat
cd C4Flasher\tools\c4-flasher
python -m pip install -r requirements.txt
python flasher.py
```

### 方式 B：打包成 exe

```bat
cd C4Flasher\tools\c4-flasher
build_exe.bat
```

生成文件直接输出到 **C4Flasher** 目录：

```
D:\antgan\C4模型\c4-切换模式版\C4Flasher\
├── C4Flasher.exe      ← 打包输出（双击运行）
├── config.json
└── firmware/
    ├── c4-latest.bin
    └── versions.json
```

## 配置 config.json

```json
{
  "target": "stm32f103c8",
  "frequency_hz": 4000000
}
```

## 固件版本

- Keil 编译后自动生成 `C4Flasher/firmware/c4-latest.bin`
- 手动版本：复制 bin 到 `C4Flasher/firmware/`，并编辑 `versions.json`

## 界面操作

1. 插入 ST-Link，连接目标板
2. 选择固件版本
3. 点击「检测 ST-Link」（可选）
4. 点击「开始烧录」

芯片：STM32F103C8 @ 0x08000000
