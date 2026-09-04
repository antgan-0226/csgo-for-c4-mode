#include "hold_progress.h"
#include "1602a.h"
#include "delay.h"
#include <stdio.h>

static uint16_t s_elapsed = 0;
static uint16_t s_lastDraw = 0;
static uint8_t s_releaseStreak = 0;
static uint16_t s_releaseMs = 0;
static uint8_t s_labelShown = 0;

void HoldProgress_Reset(void)
{
    s_elapsed = 0;
    s_lastDraw = 0;
    s_releaseStreak = 0;
    s_releaseMs = 0;
    s_labelShown = 0;
}

uint16_t HoldProgress_GetElapsed(void)
{
    return s_elapsed;
}

uint8_t HoldProgress_Tick(uint8_t keyHeld, uint16_t deltaMs, uint16_t targetMs)
{
    if (!keyHeld) {
        HoldProgress_Reset();
        return 0;
    }
    if (s_elapsed + deltaMs >= targetMs) {
        s_elapsed = targetMs;
        return 1;
    }
    s_elapsed += deltaMs;
    return 0;
}

uint8_t HoldProgress_TickStable(uint8_t keyHeld, uint16_t deltaMs, uint16_t targetMs)
{
    if (keyHeld) {
        s_releaseStreak = 0;
        if (s_elapsed + deltaMs >= targetMs) {
            s_elapsed = targetMs;
            return 1;
        }
        s_elapsed += deltaMs;
        return 0;
    }

    s_releaseStreak++;
    if (s_releaseStreak >= 5) {
        HoldProgress_Reset();
    }
    return 0;
}

uint8_t HoldProgress_TickPlant(uint8_t keyHeld, uint16_t deltaMs, uint16_t targetMs)
{
    if (keyHeld) {
        s_releaseMs = 0;
        if (s_elapsed + deltaMs >= targetMs) {
            s_elapsed = targetMs;
            return 1;
        }
        s_elapsed += deltaMs;
        return 0;
    }

    s_releaseMs += deltaMs;
    if (s_releaseMs >= 400) {
        HoldProgress_Reset();
    }
    return 0;
}

void HoldProgress_Draw(uint16_t elapsed, uint16_t target)
{
    int pct;
    int i;
    char bar[17];

    if (elapsed != target && s_lastDraw != 0 && (elapsed - s_lastDraw) < 100) {
        return;
    }
    s_lastDraw = elapsed;
    pct = (target > 0) ? (int)((uint32_t)elapsed * 10 / target) : 0;
    if (pct > 10) {
        pct = 10;
    }
    bar[0] = '[';
    for (i = 0; i < 10; i++) {
        bar[1 + i] = (i < pct) ? '=' : ' ';
    }
    bar[11] = ']';
    bar[12] = '\0';
    LCD_WRITE_StrData((unsigned char*)bar, 16);
}

void HoldProgress_DrawWithLabel(const char* label, uint16_t elapsed, uint16_t target)
{
    if (!s_labelShown) {
        LCD_WRITE_CMD(0x01);
        Delay_ms(5);
        LCD_WRITE_StrData((unsigned char*)label, 0);
        s_labelShown = 1;
        s_lastDraw = 0;
        HoldProgress_Draw(0, target);
    }
    HoldProgress_Draw(elapsed, target);
}
