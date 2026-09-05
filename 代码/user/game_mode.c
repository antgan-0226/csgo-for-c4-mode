#include "game_mode.h"
#include "flashdata.h"
#include "1602a.h"
#include "LED.H"
#include "delay.h"

static GameMode s_currentMode = MODE_CLASSIC;
static uint8_t s_hashTapCount = 0;
static uint16_t s_sinceLastHash = 0;
static uint8_t s_lastKey = ' ';

#define HASH_TAP_WINDOW 50

void GameMode_Init(void)
{
    FlashData_Init();
    FlashData data;
    FlashData_Read(&data);
    if (data.gameMode <= MODE_HOLD) {
        s_currentMode = (GameMode)data.gameMode;
    }
}

GameMode GameMode_Get(void)
{
    return s_currentMode;
}

void GameMode_Set(GameMode mode)
{
    FlashData data;

    if (mode > MODE_HOLD) {
        return;
    }
    s_currentMode = mode;
    FlashData_Read(&data);
    data.magic = 0x5A5A5A5A;
    data.version = 0x0002;
    data.gameMode = (uint8_t)mode;
    FlashData_Write(&data);
}

const char* GameMode_GetName(GameMode mode)
{
    switch (mode) {
        case MODE_MATH:
            return "Mode: Math";
        case MODE_HOLD:
            return "Mode: Hold";
        default:
            return "Mode: Classic";
    }
}

void GameMode_Cycle(void)
{
    GameMode next = (GameMode)((s_currentMode + 1) % 3);
    GameMode_Set(next);
}

void GameMode_ShowSwitchPrompt(void)
{
    int i;

    LCD_WRITE_CMD(0x01);
    Delay_ms(5);
    LCD_WRITE_StrData((unsigned char*)GameMode_GetName(s_currentMode), 0);
    for (i = 0; i < 3; i++) {
        LED1_Turn();
        Delay_ms(80);
    }
    LED1_OFF();
    Delay_ms(1500);
}

uint8_t GameMode_TrySwitchTripleHash(uint8_t key)
{
    if (key != '#') {
        s_sinceLastHash++;
        if (s_sinceLastHash > HASH_TAP_WINDOW) {
            s_hashTapCount = 0;
        }
        s_lastKey = key;
        return 0;
    }

    if (s_lastKey != '#') {
        if (s_sinceLastHash > HASH_TAP_WINDOW) {
            s_hashTapCount = 1;
        } else {
            s_hashTapCount++;
        }
        s_sinceLastHash = 0;
        s_lastKey = key;

        if (s_hashTapCount >= 3) {
            s_hashTapCount = 0;
            GameMode_Cycle();
            GameMode_ShowSwitchPrompt();
            return 1;
        }
        return 2;
    }

    s_lastKey = key;
    return 0;
}
