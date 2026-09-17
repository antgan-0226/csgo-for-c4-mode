#include "stm32f10x.h"
#include "MatrixKey.h"
#include "Delay.h"
#define ROWS (GPIO_Pin_5 | GPIO_Pin_15 | GPIO_Pin_7)
static uint8_t stableKey = ' ';
static const uint16_t rows[3] = {GPIO_Pin_5, GPIO_Pin_15, GPIO_Pin_7};
static const uint16_t cols[4] = {GPIO_Pin_4, GPIO_Pin_9, GPIO_Pin_8, GPIO_Pin_6};
static const uint8_t keys[3][4] = {{'1','4','7','*'}, {'2','5','8','0'}, {'3','6','9','#'}};
void MatrixKey_Init(void)
{
    GPIO_InitTypeDef p;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
    GPIO_SetBits(GPIOB, ROWS);
    /* Release inactive rows; avoid fighting outputs on multiple presses. */
    p.GPIO_Pin = ROWS;
    p.GPIO_Mode = GPIO_Mode_Out_OD;
    p.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &p);
    p.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_9 | GPIO_Pin_8 | GPIO_Pin_6;
    p.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &p);
    stableKey = ' ';
}
static uint16_t scanMask(void)
{
    uint16_t mask = 0, inputs;
    uint8_t r, c;
    for (r = 0; r < 3; r++) {
        GPIO_SetBits(GPIOB, ROWS);
        GPIO_ResetBits(GPIOB, rows[r]);
        Delay_us(50);
        inputs = GPIO_ReadInputData(GPIOB);
        for (c = 0; c < 4; c++) {
            if (!(inputs & cols[c])) mask |= (uint16_t)(1u << (r * 4 + c));
        }
    }
    GPIO_SetBits(GPIOB, ROWS);
    return mask;
}
static uint8_t rawKey(void)
{
    uint16_t mask = scanMask();
    uint8_t i;
    if (!mask) return ' ';
    if (mask & (mask - 1)) return 0;
    for (i = 0; i < 12; i++) {
        if (mask & (1u << i)) return keys[i / 4][i % 4];
    }
    return 0;
}
uint8_t MatrixKey_GetValue(void)
{
    uint8_t candidate = rawKey(), i;
    if (!candidate || candidate == stableKey) return stableKey;
    /* Require a stable release before accepting another key. */
    if (stableKey != ' ' && candidate != ' ') return stableKey;
    for (i = 0; i < 3; i++) {
        Delay_ms(5);
        if (rawKey() != candidate) return stableKey;
    }
    stableKey = candidate;
    return stableKey;
}
uint8_t MatrixKey_GetValueFast(void)
{
    uint8_t key = rawKey();
    return key ? key : ' ';
}
uint8_t MatrixKey_determine(void)
{
    uint16_t inputs = GPIO_ReadInputData(GPIOB);
    uint8_t c, result = 0;
    for (c = 0; c < 4; c++) {
        if (!(inputs & cols[c])) {
            if (result) return 0;
            result = c + 1;
        }
    }
    return result;
}
uint8_t MatrixKey_IsStarHeld(void)
{
    uint8_t i, hits = 0;
    for (i = 0; i < 3; i++) {
        if (rawKey() == '*') hits++;
        Delay_ms(1);
    }
    return hits >= 2;
}
uint8_t MatrixKey_IsCombo(uint8_t key1, uint8_t key2)
{
    if ((key1 == '*' && key2 == '#') || (key1 == '#' && key2 == '*')) {
        return scanMask() == ((1u << 3) | (1u << 11));
    }
    return 0;
}
