#ifndef HOLD_PROGRESS_H
#define HOLD_PROGRESS_H

#include "stm32f10x.h"

void HoldProgress_Reset(void);
uint16_t HoldProgress_GetElapsed(void);
uint8_t HoldProgress_Tick(uint8_t keyHeld, uint16_t deltaMs, uint16_t targetMs);
uint8_t HoldProgress_TickStable(uint8_t keyHeld, uint16_t deltaMs, uint16_t targetMs);
uint8_t HoldProgress_TickPlant(uint8_t keyHeld, uint16_t deltaMs, uint16_t targetMs);
void HoldProgress_Draw(uint16_t elapsed, uint16_t target);
void HoldProgress_DrawWithLabel(const char* label, uint16_t elapsed, uint16_t target);

#endif
