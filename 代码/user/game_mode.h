#ifndef GAME_MODE_H
#define GAME_MODE_H

#include "flashdata.h"

void GameMode_Init(void);
GameMode GameMode_Get(void);
void GameMode_Set(GameMode mode);
const char* GameMode_GetName(GameMode mode);
void GameMode_Cycle(void);
void GameMode_ShowSwitchPrompt(void);
uint8_t GameMode_TrySwitchTripleHash(uint8_t key);

#endif
