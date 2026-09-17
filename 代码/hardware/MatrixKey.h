#ifndef __MATRIXKEY_H
#define __MATRIXKEY_H

#include <stdint.h>

void MatrixKey_Init(void);
uint8_t MatrixKey_GetValue(void);
uint8_t MatrixKey_GetValueFast(void);
uint8_t MatrixKey_IsStarHeld(void);
uint8_t MatrixKey_determine(void);
uint8_t MatrixKey_IsCombo(uint8_t key1, uint8_t key2);

#endif
