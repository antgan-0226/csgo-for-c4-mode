# C4 三种游戏模式 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 STM32 C4 模型固件中新增 Math / Hold 两种玩法，保留 Classic，支持 `*+#` 切换与 Flash 持久化，并删除闹钟模式。

**Architecture:** 轻量模式调度层 — `game_mode.c` 管理模式与 Flash；`math_quiz.c` / `hold_progress.c` 封装玩法逻辑；`main.c` 四状态框架不变，各 handler 按 `GameMode` 分发。矩阵键盘仅新增 `MatrixKey_IsCombo()`。

**Tech Stack:** STM32F103C8, Keil uVision, STM32 Standard Peripheral Library, 1602 LCD, 矩阵键盘, MP3 串口模块

## Global Constraints

- 三种模式：`MODE_CLASSIC` / `MODE_MATH` / `MODE_HOLD`，循环顺序 Classic → Math → Hold → Classic
- 模式切换：仅空闲态 `STATE_PASSWORD_INPUT`，同时按 `*` + `#`，LCD 提示 1.5 秒后恢复，写入 Flash
- Flash 版本：`FLASH_DATA_VERSION = 0x0002`，字段 `gameMode` 默认 `MODE_CLASSIC`
- Math 拆弹：算式第 1 行固定显示，7 位结果，`#` 提交，倒计时照常，超时 T 胜
- Hold 下包：长按 `*` 3000 ms；Hold 拆包：隐藏 `bonusDigit`，正确键 5000 ms，其他键含 `#` 10000 ms；松手清零；LCD 进度条
- 删除：`STATE_ALARM_CLOCK_MODE`、`checkHashPress()` 闹钟逻辑、`handleAlarmClockModeState()`
- 不改动：MP3 / LED / Timer2 硬件驱动层接口

---

## File Map

| 文件 | 职责 |
|------|------|
| `hardware/flashdata.h` | `GameMode` 枚举、`FlashData.gameMode`、版本号 |
| `hardware/flashdata.c` | 默认值含 `gameMode = MODE_CLASSIC` |
| `hardware/MatrixKey.h/c` | 新增 `MatrixKey_IsCombo()` |
| `user/game_mode.h/c` | 模式读写、切换、提示 UI |
| `user/math_quiz.h/c` | 7 位算术题生成、格式化、校验 |
| `user/hold_progress.h/c` | 长按毫秒计时、进度条字符串、LCD 渲染 |
| `user/main.c` | 删除闹钟；handler 分发；接入三模式 |
| `Project.uvprojx` | 注册 3 个新 .c 文件 |

---

### Task 1: Flash 数据结构与 GameMode 枚举

**Files:**
- Modify: `hardware/flashdata.h`
- Modify: `hardware/flashdata.c`
- Create: `user/game_mode.h`

**Interfaces:**
- Produces:
  - `typedef enum { MODE_CLASSIC=0, MODE_MATH=1, MODE_HOLD=2 } GameMode;`
  - `FlashData.gameMode` 字段
  - `GameMode GameMode_Get(void);`
  - `void GameMode_Set(GameMode mode);`
  - `void GameMode_Init(void);` — 上电从 Flash 加载

- [ ] **Step 1: 扩展 flashdata.h**

```c
/* flashdata.h — 在 #include 后、FlashData 前加入 GameMode（或移到 game_mode.h 再 include） */
#define FLASH_DATA_VERSION    0x0002

typedef enum {
    MODE_CLASSIC = 0,
    MODE_MATH    = 1,
    MODE_HOLD    = 2
} GameMode;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint8_t  volume;
    uint8_t  gameMode;
} FlashData;
```

- [ ] **Step 2: 更新 flashdata.c 默认值**

在 `FlashData_Read()` 无效数据分支：

```c
data->volume   = 20;
data->gameMode = MODE_CLASSIC;
```

在 `isDataValid()` 通过后，若 `flashData->version < 0x0002`，强制 `data->gameMode = MODE_CLASSIC`（兼容旧 Flash）。

- [ ] **Step 3: 创建 game_mode.h 骨架**

```c
#ifndef GAME_MODE_H
#define GAME_MODE_H
#include "flashdata.h"

void GameMode_Init(void);
GameMode GameMode_Get(void);
void GameMode_Set(GameMode mode);
const char* GameMode_GetName(GameMode mode);
void GameMode_Cycle(void);          /* Classic→Math→Hold→Classic + Flash 写入 */
void GameMode_ShowSwitchPrompt(void); /* LED 闪 + LCD 1.5s */

#endif
```

- [ ] **Step 4: Keil 编译验证**

Build Project，Expected: 0 Error（`game_mode.c` 尚未实现时可能 link 失败，Task 2 完成后全过）

- [ ] **Step 5: Commit**

```bash
git add hardware/flashdata.h hardware/flashdata.c user/game_mode.h
git commit -m "feat: extend FlashData with GameMode field"
```

---

### Task 2: 矩阵键盘组合键检测

**Files:**
- Modify: `hardware/MatrixKey.h`
- Modify: `hardware/MatrixKey.c`

**Interfaces:**
- Produces: `uint8_t MatrixKey_IsCombo(uint8_t key1, uint8_t key2);` — 两键同按返回 1

- [ ] **Step 1: MatrixKey.h 声明**

```c
uint8_t MatrixKey_IsCombo(uint8_t key1, uint8_t key2);
```

- [ ] **Step 2: MatrixKey.c 实现**

`*` 与 `#` 同在第 3 行（PB7 低电平）。检测 PB4（`*` 列）与 PB6（`#` 列）同时为 0：

```c
static uint8_t MatrixKey_Row3BothStarHash(void)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_7);
    GPIO_SetBits(GPIOB, GPIO_Pin_5);
    GPIO_SetBits(GPIOB, GPIO_Pin_15);
    Delay_ms(5);
    uint8_t star = (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_4) == 0);
    uint8_t hash = (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_6) == 0);
    return (star && hash);
}

uint8_t MatrixKey_IsCombo(uint8_t key1, uint8_t key2)
{
    if ((key1 == '*' && key2 == '#') || (key1 == '#' && key2 == '*'))
        return MatrixKey_Row3BothStarHash();
    return 0;
}
```

- [ ] **Step 3: 编译验证** — Build，Expected: 0 Error

- [ ] **Step 4: Commit**

```bash
git add hardware/MatrixKey.h hardware/MatrixKey.c
git commit -m "feat: add MatrixKey_IsCombo for star+hash detection"
```

---

### Task 3: game_mode.c — 模式管理与切换 UI

**Files:**
- Create: `user/game_mode.c`
- Modify: `user/main.c`（仅 init 调用）

**Interfaces:**
- Consumes: `FlashData_Read/Write`, `MatrixKey_IsCombo`, `LCD_WRITE_StrDATA`, `LED1_Turn`, `Delay_ms`
- Produces: Task 1 中全部 `GameMode_*` 函数

- [ ] **Step 1: 实现 game_mode.c**

```c
#include "game_mode.h"
#include "flashdata.h"
#include "1602a.h"
#include "LED.H"
#include "delay.h"
#include "MatrixKey.h"

static GameMode s_currentMode = MODE_CLASSIC;
static uint16_t s_comboDebounce = 0;

void GameMode_Init(void)
{
    FlashData_Init();
    FlashData data;
    if (FlashData_Read(&data)) {
        if (data.gameMode <= MODE_HOLD)
            s_currentMode = (GameMode)data.gameMode;
    }
}

GameMode GameMode_Get(void) { return s_currentMode; }

void GameMode_Set(GameMode mode)
{
    if (mode > MODE_HOLD) return;
    s_currentMode = mode;
    FlashData data;
    FlashData_Read(&data);
    data.magic    = 0x5A5A5A5A;
    data.version  = 0x0002;
    data.gameMode = (uint8_t)mode;
    FlashData_Write(&data);
}

const char* GameMode_GetName(GameMode mode)
{
    switch (mode) {
        case MODE_MATH:    return (const char*)"Mode: Math";
        case MODE_HOLD:    return (const char*)"Mode: Hold";
        default:           return (const char*)"Mode: Classic";
    }
}

void GameMode_Cycle(void)
{
    GameMode next = (GameMode)((s_currentMode + 1) % 3);
    GameMode_Set(next);
}

void GameMode_ShowSwitchPrompt(void)
{
    LCD_WRITE_CMD(0x01);
    Delay_ms(5);
    LCD_WRITE_StrDATA((unsigned char*)GameMode_GetName(s_currentMode), 0);
    for (int i = 0; i < 3; i++) { LED1_Turn(); Delay_ms(80); }
    LED1_OFF();
    Delay_ms(1500);
}

/* 主循环空闲态调用；返回 1 表示本次已切换 */
uint8_t GameMode_TrySwitchCombo(void)
{
    if (!MatrixKey_IsCombo('*', '#')) {
        s_comboDebounce = 0;
        return 0;
    }
    s_comboDebounce++;
    if (s_comboDebounce >= 20) {  /* 20 * 10ms = 200ms */
        GameMode_Cycle();
        GameMode_ShowSwitchPrompt();
        s_comboDebounce = 0;
        return 1;
    }
    return 0;
}
```

在 `game_mode.h` 追加：`uint8_t GameMode_TrySwitchCombo(void);`

- [ ] **Step 2: main.c 初始化**

```c
#include "game_mode.h"
// main() 内 MatrixKey_Init() 之后：
GameMode_Init();
```

- [ ] **Step 3: Project.uvprojx 添加 `user/game_mode.c`**

- [ ] **Step 4: 编译 + 板上测试**

空闲态 `*+#` → LCD 显示 `Mode: Math` → 断电上电仍为 Math

- [ ] **Step 5: Commit**

```bash
git add user/game_mode.c user/game_mode.h user/main.c Project.uvprojx
git commit -m "feat: game mode manager with Flash persistence and switch UI"
```

---

### Task 4: math_quiz.c — 算术题模块

**Files:**
- Create: `user/math_quiz.h`
- Create: `user/math_quiz.c`

**Interfaces:**
- Produces:
  - `void MathQuiz_Generate(void);`
  - `void MathQuiz_ShowQuestion(void);` — 写 LCD 第 1 行
  - `uint8_t MathQuiz_CheckAnswer(const unsigned char pass[8]);` — 比较 7 位
  - `const char* MathQuiz_GetEquation(void);` — 返回内部算式缓冲

- [ ] **Step 1: math_quiz.h**

```c
#ifndef MATH_QUIZ_H
#define MATH_QUIZ_H
void MathQuiz_Generate(void);
void MathQuiz_ShowQuestion(void);
uint8_t MathQuiz_CheckAnswer(const unsigned char pass[8]);
#endif
```

- [ ] **Step 2: math_quiz.c 核心生成逻辑**

```c
#include "math_quiz.h"
#include "1602a.h"
#include "delay.h"
#include <stdio.h>

static char s_equation[17];
static char s_answer[8];
static uint32_t s_seed = 1;

static uint32_t rng_next(void) {
    s_seed = s_seed * 1103515245u + 12345u;
    return (s_seed >> 16) & 0x7FFF;
}

void MathQuiz_Generate(void)
{
    s_seed ^= (uint32_t)Num;  /* 来自 main.c 的 TIM2 变量，extern 声明 */
    char op;
    long a, b, result;
    int tries = 0;
    do {
        switch (rng_next() % 3) {
            case 0: op = '+'; break;
            case 1: op = '-'; break;
            default: op = '*'; break;
        }
        if (op == '+') {
            result = 1000000L + (long)(rng_next() % 8999999L);
            b = 100L + (long)(rng_next() % 9999900L);
            if (b >= result) continue;
            a = result - b;
        } else if (op == '-') {
            a = 2000000L + (long)(rng_next() % 7999999L);
            b = 100L + (long)(rng_next() % (a - 1000000L));
            result = a - b;
        } else {
            a = 1000L + (long)(rng_next() % 9000L);
            b = 1000L + (long)(rng_next() % 9000L);
            result = a * b;
        }
        tries++;
    } while ((result < 1000000L || result > 9999999L) && tries < 200);

    sprintf(s_answer, "%07ld", result);
    if (op == '*')
        sprintf(s_equation, "%ld%c%ld=", a, op, b);
    else
        sprintf(s_equation, "%ld%c%ld=", a, op, b);
}

void MathQuiz_ShowQuestion(void)
{
    LCD_SetCursor(0);
    LCD_WRITE_StrDATA((unsigned char*)s_equation, 0);
}

uint8_t MathQuiz_CheckAnswer(const unsigned char pass[8])
{
    for (int i = 0; i < 7; i++) {
        if (pass[i] != (unsigned char)s_answer[i]) return 0;
    }
    return 1;
}
```

- [ ] **Step 3: 加入 Project.uvprojx，编译 0 Error**

- [ ] **Step 4: Commit**

```bash
git add user/math_quiz.c user/math_quiz.h Project.uvprojx
git commit -m "feat: add 7-digit math quiz generator for defuse mode"
```

---

### Task 5: hold_progress.c — 长按进度条

**Files:**
- Create: `user/hold_progress.h`
- Create: `user/hold_progress.c`

**Interfaces:**
- Produces:
  - `void HoldProgress_Reset(void);`
  - `uint8_t HoldProgress_Tick(uint8_t keyHeld, uint16_t deltaMs, uint16_t targetMs);`
    - `keyHeld=0` → 清零，返回 0
    - 累加达 target → 返回 1（完成）
  - `void HoldProgress_Draw(uint16_t elapsed, uint16_t target);` — LCD 第 2 行
  - `void HoldProgress_DrawWithLabel(const char* label, uint16_t elapsed, uint16_t target);`

- [ ] **Step 1: hold_progress.c**

```c
#include "hold_progress.h"
#include "1602a.h"
#include "delay.h"
#include <stdio.h>

static uint16_t s_elapsed = 0;
static uint16_t s_lastDraw = 0;

void HoldProgress_Reset(void) { s_elapsed = 0; s_lastDraw = 0; }

uint8_t HoldProgress_Tick(uint8_t keyHeld, uint16_t deltaMs, uint16_t targetMs)
{
    if (!keyHeld) { HoldProgress_Reset(); return 0; }
    if (s_elapsed + deltaMs >= targetMs) { s_elapsed = targetMs; return 1; }
    s_elapsed += deltaMs;
    return 0;
}

void HoldProgress_Draw(uint16_t elapsed, uint16_t target)
{
    if (elapsed - s_lastDraw < 100 && elapsed != target) return;
    s_lastDraw = elapsed;
    int pct = (target > 0) ? (int)((uint32_t)elapsed * 10 / target) : 0;
    if (pct > 10) pct = 10;
    char bar[17];
    bar[0] = '[';
    int i;
    for (i = 0; i < 10; i++) bar[1 + i] = (i < pct) ? '=' : ' ';
    bar[11] = ']';
    bar[12] = '\0';
    LCD_SetCursor(16);
    LCD_WRITE_StrDATA((unsigned char*)bar, 0);
}

void HoldProgress_DrawWithLabel(const char* label, uint16_t elapsed, uint16_t target)
{
    LCD_WRITE_CMD(0x01);
    Delay_ms(5);
    LCD_WRITE_StrDATA((unsigned char*)label, 0);
    HoldProgress_Draw(elapsed, target);
}
```

- [ ] **Step 2: Project.uvprojx 注册，编译 0 Error**

- [ ] **Step 3: Commit**

```bash
git add user/hold_progress.c user/hold_progress.h Project.uvprojx
git commit -m "feat: add hold progress bar for LCD"
```

---

### Task 6: main.c — 删除闹钟 + 三模式分发

**Files:**
- Modify: `user/main.c`

**Interfaces:**
- Consumes: 全部前述模块

- [ ] **Step 1: 删除闹钟相关**

移除：
- `STATE_ALARM_CLOCK_MODE` 枚举值
- `hashPressed`, `hashTimer` 全局变量（若仅闹钟用）
- `checkHashPress()` 函数及 main 循环调用
- `handleAlarmClockModeState()` 及 switch case
- 函数声明

- [ ] **Step 2: 添加 include 与 plant 分发**

```c
#include "game_mode.h"
#include "math_quiz.h"
#include "hold_progress.h"
```

重构 `handlePasswordInputState()`:

```c
void handlePasswordInputState(void) {
    if (GameMode_TrySwitchCombo()) {
        showDefaultScreen();
        return;
    }
    switch (GameMode_Get()) {
        case MODE_HOLD:
            handleHoldPlantInput();  /* 新建静态函数 */
            break;
        default:
            handleClassicPlantInput(); /* 原 deployPasswordScan 逻辑抽出 */
            break;
    }
}
```

`handleClassicPlantInput()` = 现有 `handlePasswordInputState` 主体不变。

`handleHoldPlantInput()`:

```c
static void handleHoldPlantInput(void) {
    if (MatrixKey_IsCombo('*', '#')) return;
    uint8_t key = MatrixKey_GetValue();
    uint8_t done = HoldProgress_Tick(key == '*', 10, 3000);
    if (key == '*') {
        HoldProgress_DrawWithLabel("Hold *", /* elapsed via getter or static */);
    }
    if (done) {
        HoldProgress_Reset();
        sign = 0; isPressed = 0;
        currentState = STATE_PASSWORD_VERIFY;
    }
}
```

需在 `hold_progress.c` 增加 `uint16_t HoldProgress_GetElapsed(void);`

- [ ] **Step 3: 拆弹 dispatch**

将 `handlePasswordVerifyState()` 开头按模式分支：

```c
void handlePasswordVerifyState(void) {
    switch (GameMode_Get()) {
        case MODE_MATH:  handleMathDefuseState(); return;
        case MODE_HOLD:  handleHoldDefuseState(); return;
        default:         break; /* 原 Classic 逻辑保留在下方 */
    }
    /* ... 原有 Classic 代码 ... */
}
```

**handleMathDefuseState():**
- 进入时一次 `MathQuiz_Generate()` + `MathQuiz_ShowQuestion()`
- 倒计时循环同 Classic，但 `showStarAnimation()` 改为只清第 2 行或跳过（第 1 行算式不动）
- 用改造版 `unlockPasswordScan` 或新函数 `mathAnswerScan()`：`#` 在满 7 位时调用 `MathQuiz_CheckAnswer()`
- 正确 → SUCCESS；错误 → 清空第 2 行；超时 → FAILURE

**handleHoldDefuseState():**
- 进入时 `bonusDigit = rng() % 10`
- 倒计时循环内：检测当前按住键，`target = (key == bonusDigit+'0') ? 5000 : 10000`（仅数字或 `#` 触发进度）
- `HoldProgress_Tick(held, 10, target)` 完成 → SUCCESS
- 未按键时 Classic LED/星号动画

- [ ] **Step 4: 编译 0 Error**

- [ ] **Step 5: 全量手动测试（规格 §9）**

| # | 测试 | 预期 |
|---|------|------|
| 1 | Flash 持久化 | 切换 Math 断电保持 |
| 2 | Classic 回归 | 7355608 下包/拆包 |
| 3 | Math | 算式 7 位、# 提交、错答重试 |
| 4 | Hold 下包 | `*` 3 秒进度条 |
| 5 | Hold 拆包 | 5s/10s、松手清零 |
| 6 | 模式切换 | 仅空闲态、1.5s 提示 |
| 7 | 闹钟移除 | 长按 `#` 无效 |

- [ ] **Step 6: Commit**

```bash
git add user/main.c user/hold_progress.c user/hold_progress.h
git commit -m "feat: integrate Classic/Math/Hold modes and remove alarm clock"
```

---

### Task 7: 收尾与 spec 状态更新

**Files:**
- Modify: `docs/superpowers/specs/2026-08-28-c4-game-modes-design.md` — 状态改为「已实现」

- [ ] **Step 1: 更新 spec 状态行**

- [ ] **Step 2: 最终 Build 0 Error / 0 Warning（允许既有 warning）**

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/specs/2026-08-28-c4-game-modes-design.md
git commit -m "docs: mark c4 game modes spec as implemented"
```

---

## Plan Self-Review

| Spec 章节 | 对应 Task |
|-----------|-----------|
| §2 三模式定义 | Task 6 |
| §3 Flash 持久化 | Task 1, 3 |
| §3.4 模式切换 | Task 2, 3, 6 |
| §4 Classic | Task 6（原逻辑保留） |
| §5 Math | Task 4, 6 |
| §6 Hold | Task 5, 6 |
| §8 边界 | Task 6（`*+#` 优先、拆弹不切模式） |
| §9 测试 | Task 6 Step 5 |
| 删除闹钟 | Task 6 Step 1 |

无 TBD/占位符；接口命名跨 Task 一致。
