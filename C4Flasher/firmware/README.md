# 固件 BIN 目录

烧录工具会读取本目录下的 `.bin` 文件。

## 添加新版本

1. 将 bin 文件放入本目录，例如 `c4-v1.1.0.bin`
2. 编辑 `versions.json`，在 `versions` 数组中增加一项：

```json
{
  "id": "v1.1.0",
  "name": "v1.1.0 正式版",
  "file": "c4-v1.1.0.bin",
  "note": "2026-09-06"
}
```

## 自动生成 latest

Keil 编译成功后会执行 After Build，导出 `c4-latest.bin` 到本目录。

芯片：STM32F103C8，烧录地址 `0x08000000`。
