#include "math_quiz.h"

#include "1602a.h"

#include "delay.h"

#include <stdio.h>



extern volatile uint16_t Num;



#define MATH_INPUT_START 4



static char s_equation[17];

static char s_answer[8];

static uint32_t s_seed = 1;



static uint32_t rng_next(void)

{

    s_seed = s_seed * 1103515245u + 12345u;

    return (s_seed >> 16) & 0x7FFFu;

}



void MathQuiz_Generate(void)

{

    char op;

    long a, b, result;

    int tries = 0;



    s_seed ^= (uint32_t)Num + 0x13579BDFu;

    do {

        switch (rng_next() % 3) {

            case 0: op = '+'; break;

            case 1: op = '-'; break;

            default: op = '*'; break;

        }

        if (op == '+') {

            result = 100L + (long)(rng_next() % 900L);

            b = 1L + (long)(rng_next() % (result - 1));

            a = result - b;

        } else if (op == '-') {

            result = 100L + (long)(rng_next() % 900L);

            b = 1L + (long)(rng_next() % 500L);

            a = result + b;

        } else {

            a = 2L + (long)(rng_next() % 98L);

            b = 2L + (long)(rng_next() % 98L);

            result = a * b;

        }

        tries++;

    } while ((result < 100L || result > 999L) && tries < 200);



    if (result < 100L || result > 999L) {

        a = 123L;

        b = 456L;

        result = a + b;

        op = '+';

    }



    sprintf(s_answer, "%07ld", result);

    sprintf(s_equation, "%ld%c%ld=", a, op, b);

}



void MathQuiz_InitInputBuffer(unsigned char pass[8])

{

    int i;



    for (i = 0; i < MATH_INPUT_START; i++) {

        pass[i] = (unsigned char)s_answer[i];

    }

    for (i = MATH_INPUT_START; i < 7; i++) {

        pass[i] = '*';

    }

    pass[7] = '\0';

}



uint8_t MathQuiz_GetInputStartIndex(void)

{

    return MATH_INPUT_START;

}



void MathQuiz_ShowQuestion(void)

{

    unsigned char i;

    unsigned char len;



    LCD_WRITE_StrData((unsigned char*)"                ", 0);

    LCD_WRITE_StrData((unsigned char*)s_equation, 0);



    len = 0;

    while (s_equation[len] != '\0' && len < 16) {

        len++;

    }

    for (i = len; i < 16; i++) {

        LCD_SetCursor(i);

        LCD_WRITE_ByteDATA(' ');

    }

}



uint8_t MathQuiz_CheckAnswer(const unsigned char pass[8])

{

    int i;



    for (i = 0; i < 7; i++) {

        if (pass[i] != (unsigned char)s_answer[i]) {

            return 0;

        }

    }

    return 1;

}


