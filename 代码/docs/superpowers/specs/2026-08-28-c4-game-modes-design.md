# C4 三种游戏模式设计规格

**日期：** 2026-08-28  
**项目：** CSGO C4 手工模型（STM32F103 + 1602 LCD + 矩阵键盘）  
**状态：** 已实现

---

## 1. 背景与目标

在保留原版下包/拆弹玩法的前提下，新增两种模式，并支持运行时切换与 Flash 持久化。移除原有「长按 `#` 进入闹钟模式」功能。

### 成功标准

- 三种模式可循环切换，断电后模式保持
- 原版（Classic）行为与现版一致（除闹钟删除）
- 算术模式（Math）、长按模式（Hold）按规格独立运行
- 改动以增量方式接入现有状态机，不破坏 MP3 / LED / Timer 等硬件层

---

## 2. 三种模式定义

| 模式 | 枚举 | 下包 | 拆包 |
|------|------|------|------|
| 原版 | `MODE_CLASSIC` | 输入 `7355608`，满 7 位校验 | 倒计时内输入相同 7 位密码 |
| 算术 | `MODE_MATH` | 同原版 | 屏幕显示算术题；输入 7 位正确答案 + `#`；倒计时照常 |
| 长按 | `MODE_HOLD` | 空闲态长按 `*` 3 秒（LCD 进度条） | 随机隐藏加速数字 0–9；长按该数字 5 秒或长按其他键（含 `#`）10 秒；松手清零 |

**模式循环顺序：** Classic → Math → Hold → Classic

---

## 3. 架构设计

### 3.1 推荐实现方案

采用 **轻量模式调度层 + 增量扩展**（非整体重构）：

- 新增 `game_mode.c/h`：模式枚举、Flash 读写、切换逻辑、切换提示 UI
- 新增 `math_quiz.c/h`：算术题生成与校验
- 新增 `hold_progress.c/h`：长按计时与 LCD 进度条
- `main.c` 保留现有四状态框架，各 `handleXxxState()` 按模式分发
- 矩阵键盘新增 `MatrixKey_IsCombo()`，不改动现有 `MatrixKey_GetValue()`

### 3.2 状态机

保留四个核心状态，删除闹钟：

```
STATE_PASSWORD_INPUT   → 下包/安包
STATE_PASSWORD_VERIFY  → 拆弹倒计时
STATE_UNLOCK_SUCCESS   → CT 胜
STATE_UNLOCK_FAILURE   → T 胜
```

**删除项：**

- `STATE_ALARM_CLOCK_MODE`
- `checkHashPress()` 中闹钟相关逻辑
- `handleAlarmClockModeState()`

### 3.3 Flash 持久化

扩展 `FlashData`（`flashdata.h`）：

```c
typedef enum {
    MODE_CLASSIC = 0,
    MODE_MATH    = 1,
    MODE_HOLD    = 2
} GameMode;

typedef struct {
    uint32_t magic;
    uint16_t version;      /* 升级为 0x0002 */
    uint16_t reserved;
    uint8_t  volume;
    uint8_t  gameMode;     /* 新增，默认 MODE_CLASSIC */
} FlashData;
```

- 上电 `FlashData_Read()` 恢复 `currentGameMode`
- 切换模式后 `FlashData_Write()` 保存
- 读取旧版 Flash 时，`gameMode` 默认 `MODE_CLASSIC`

### 3.4 模式切换

| 条件 | 行为 |
|------|------|
| 触发方式 | 空闲态同时按下 `*` + `#` |
| 生效范围 | 仅 `STATE_PASSWORD_INPUT`，且未进入胜负态 |
| 反馈 | LED 短闪 + LCD 显示模式名 1.5 秒，然后 `showDefaultScreen()` |
| 显示文案 | `Mode: Classic` / `Mode: Math` / `Mode: Hold` |
| 持久化 | 切换后立即写入 Flash |

**矩阵键盘实现：** `MatrixKey_IsCombo('*', '#')` — `*` 与 `#` 同在第 3 行，检测两列同时为低，消抖约 200ms 后触发一次。

**优先级：** 空闲态 `*+#` 优先于 Hold 模式下长按 `*` 下包。

---

## 4. 原版模式（MODE_CLASSIC）

行为与现版 `main.c` 完全一致：

- 下包：`deployPasswordScan()` + 7 位校验 `7355608`
- 拆包：`handlePasswordVerifyState()` 原逻辑（倒计时 + `unlockPasswordScan()` + 相同密码）
- 胜负界面、MP3、LED、星号动画不变

**唯一变更：** 移除闹钟模式相关代码。

---

## 5. 算术模式（MODE_MATH）

### 5.1 下包

与 Classic 相同：满 7 位 `7355608` 校验通过 → `STATE_PASSWORD_VERIFY`。

### 5.2 LCD 布局（拆弹阶段）

| 行 | 内容 | 示例 |
|----|------|------|
| 第 1 行 | 算式（左对齐，进入拆弹即显示且固定） | `1234567+89=` |
| 第 2 行 | 7 位答案输入（居中，`spaceCount=4`） | `***1234` |

- 第 1 行在整段拆弹倒计时内不被星号动画覆盖
- 星号动画仅在不输入时作用于非算式区域，或改为不显示星号动画（实现时以「第 1 行算式始终可见」为优先）

### 5.3 出题规则

- 运算符：`+`、`-`、`*` 随机，不含除法
- **结果必须恰好 7 位整数：** `1,000,000 ~ 9,999,999`
- 减法：被减数 > 减数
- 乘法：两因子受控，积为 7 位
- 每局一题，进入 `STATE_PASSWORD_VERIFY` 时生成，局内不变
- 随机种子：建议使用 TIM2 计数或拆弹进入时的循环计数

### 5.4 输入与校验

- `0–9`：填入答案（索引式，类似 `unlockPasswordScan`）
- `*`：退格
- `#`：提交（需已满 7 位）

| 结果 | 行为 |
|------|------|
| 答案正确 | `STATE_UNLOCK_SUCCESS` |
| 答案错误 | 清空第 2 行，第 1 行算式不变，可重试 |
| 倒计时结束 | `STATE_UNLOCK_FAILURE` |

倒计时机制（LED 闪烁、加速滴答）与 Classic 相同。

---

## 6. 长按模式（MODE_HOLD）

### 6.1 下包（STATE_PASSWORD_INPUT）

- 连续按住 `*`（非 `*+#` 组合）
- 目标时长：**3000 ms**
- LCD 第 1 行：`Planting...` 或 `Hold *`
- LCD 第 2 行：进度条
- **松手 → 进度清零**；满 3 秒 → `STATE_PASSWORD_VERIFY`

### 6.2 拆包（STATE_PASSWORD_VERIFY）

- 进入时随机生成 `bonusDigit`（0–9），**不在 LCD 显示**
- 拆包者长按某键：

| 按键 | 目标时长 |
|------|----------|
| `bonusDigit` | 5000 ms |
| 其他 `0–9` 或 `#` | 10000 ms |

- **松手 → 进度清零**；达标 → `STATE_UNLOCK_SUCCESS`
- 倒计时照常，超时 → `STATE_UNLOCK_FAILURE`
- 未长按任何键时：保留 Classic 的 LED / 星号动画
- **正在长按时：** 第 2 行显示进度条（可覆盖星号动画）

### 6.3 进度条规格

- 格式：`[=====>          ]`（16 字符，约 10 格有效长度）
- 共用模块：`HoldProgress_Update(elapsedMs, targetMs)` → 返回 0–100 进度
- LCD 刷新：约每 100ms 更新一次，避免频繁写屏
- 适用场景：Hold 模式下包（3s）与拆包（5s/10s）

---

## 7. 模块与文件变更清单

| 文件 | 变更 |
|------|------|
| `user/main.c` | 模式分发、删除闹钟、接入新模块 |
| `hardware/flashdata.h` | 增加 `GameMode`、`gameMode` 字段，版本号 |
| `hardware/flashdata.c` | 默认值与读写兼容 |
| `hardware/MatrixKey.c/h` | 新增 `MatrixKey_IsCombo()` |
| `user/game_mode.c/h` | **新建** — 模式管理、切换 UI、Flash 接口 |
| `user/math_quiz.c/h` | **新建** — 出题、算式格式化、答案校验 |
| `user/hold_progress.c/h` | **新建** — 长按计时、进度条渲染 |
| `Project.uvprojx` | 添加新源文件 |

---

## 8. 边界与冲突处理

| 场景 | 处理 |
|------|------|
| 空闲态 `*+#` | 切换模式，不触发 Hold 下包 |
| 拆弹中 | 不响应模式切换 |
| 胜负界面 | 不响应（已有 `while(1)`） |
| Classic 长按 `#` | 不再进入闹钟（功能已删除） |
| Hold 拆包长按 `#` | 视为错误键，需 10 秒 |
| Math 模式 `#` | 仅作为提交键，在满 7 位时生效 |

---

## 9. 测试计划

1. **Flash 持久化：** 切换至 Math → 断电 → 上电仍为 Math
2. **Classic 回归：** 下包 7355608 → 拆包同密码 → CT 胜；超时 → T 胜
3. **Math：** 算式结果为 7 位；错答可重试；`#` 提交；超时 T 胜
4. **Hold 下包：** 长按 `*` 3 秒进度条满 → 进入拆弹；中途松手清零
5. **Hold 拆包：** 隐藏数字 5 秒成功 / 错键 10 秒；松手清零；超时 T 胜
6. **模式切换：** 仅空闲态 `*+#` 有效；切换提示 1.5 秒；不误触发下包
7. **闹钟移除：** 任意模式下长按 `#` 不再进入 Alarm Clock

---

## 10. 不在本次范围

- 闹钟模式功能（已明确删除）
- 除法运算
- 拆包加速数字 LCD 显示（Hold 模式为隐藏猜数字）
- 整体重构 `main.c` 为完全模块化（后续可迭代）
