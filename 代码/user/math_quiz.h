#ifndef MATH_QUIZ_H
#define MATH_QUIZ_H

#include <stdint.h>

void MathQuiz_Generate(void);
void MathQuiz_InitInputBuffer(unsigned char pass[8]);
uint8_t MathQuiz_GetInputStartIndex(void);
void MathQuiz_ShowQuestion(void);
uint8_t MathQuiz_CheckAnswer(const unsigned char pass[8]);

#endif
