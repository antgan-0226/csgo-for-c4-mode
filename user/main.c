#include "stm32f10x.h"
#include "stm32f10x_flash.h"
#include "matrixkey.h"
#include "1602a.h"
#include "delay.h"
#include "LED.H"
#include "math.h"
#include "TIMER.h"
#include "Serial.h"
#include "mp3.h"
#include "string.h"
#include "game_mode.h"
#include "math_quiz.h"
#include "hold_progress.h"
#include <stdbool.h>
#include <stdio.h>

void showDefaultScreen(void);
char arraysEqual(unsigned char arr1[], unsigned char arr2[], int size);
void rightShiftArray(unsigned char arr[], int size);
void leftShiftArray(unsigned char arr[], int size);
void updateKeyPressState(void);
void handleKeyPressFeedback(void);
void clearPasswordArray(unsigned char pass[], int size);
void updatePasswordDisplay(unsigned char pass[]);
void showStarAnimation(void);
void showLine2StarAnimation(void);
void updateMathAnswerDisplay(unsigned char pass[]);
void unlockPasswordScan(unsigned char pass[]);
void mathAnswerScan(unsigned char pass[]);
void deployPasswordScan(unsigned char pass[]);

void handleClassicPlantInput(void);
void handleHoldPlantInput(void);
void handlePasswordInputState(void);
void handleClassicDefuseState(void);
void handleMathDefuseState(void);
void handleHoldDefuseState(void);
void handlePasswordVerifyState(void);
void handleUnlockSuccessState(void);
void handleUnlockFailureState(void);
static void holdDefuseStep(uint8_t bonusDigit);

typedef enum {
    STATE_PASSWORD_INPUT,
    STATE_PASSWORD_VERIFY,
    STATE_UNLOCK_SUCCESS,
    STATE_UNLOCK_FAILURE
} SystemState;

SystemState currentState = STATE_PASSWORD_INPUT;
uint8_t volumeLevel = 30;

unsigned char defaultPassword[8] = {'7','3','5','5','6','0','8','\0'};
unsigned char password[8] = {'*','*','*','*','*','*','*','\0'};
unsigned char unlockPassword[8] = {'*','*','*','*','*','*','*','\0'};

char KeyNum, sign;
int unlockArrayIndex = 0;
const int spaceCount = 4;
uint8_t isPressed = 0;
uint16_t countdown = 100;
uint16_t Num = 0, Num_sign = 0;

int main()
{
    mp3_Init();
    Timer_Init();
    LED_Init();
    LCD_INIT();
    MatrixKey_Init();
    GameMode_Init();

    Delay_ms(1000);
    LCD_WRITE_CMD(0x01);
    Delay_ms(5);
    showDefaultScreen();

    Delay_ms(1000);
    MP3CMD(0x06, 30);

    while (1) {
        KeyNum = MatrixKey_GetValue();

        switch (currentState) {
            case STATE_PASSWORD_INPUT:
                handlePasswordInputState();
                break;
            case STATE_PASSWORD_VERIFY:
                handlePasswordVerifyState();
                break;
            case STATE_UNLOCK_SUCCESS:
                handleUnlockSuccessState();
                break;
            case STATE_UNLOCK_FAILURE:
                handleUnlockFailureState();
                break;
            default:
                currentState = STATE_PASSWORD_INPUT;
                break;
        }
        Delay_ms(10);
    }
}

void handlePasswordInputState(void)
{
    uint8_t switchResult;

    switchResult = GameMode_TrySwitchTripleHash(KeyNum);
    if (switchResult == 1) {
        showDefaultScreen();
        return;
    }
    if (switchResult == 2) {
        return;
    }
    if (GameMode_Get() == MODE_HOLD) {
        handleHoldPlantInput();
        return;
    }
    handleClassicPlantInput();
}

void handleClassicPlantInput(void)
{
    deployPasswordScan(password);

    if (password[0] != '*') {
        for (long i = 0; i < 50000; i++) {
            deployPasswordScan(password);
        }

        if (password[0] != '*' && password[6] != '*') {
            Delay_ms(200);
            if (arraysEqual(password, defaultPassword, 7)) {
                while (MatrixKey_GetValue() != ' ') {
                    Delay_ms(10);
                }
                Delay_ms(50);
                sign = 0;
                isPressed = 0;
                unlockArrayIndex = 0;
                clearPasswordArray(unlockPassword, 7);
                currentState = STATE_PASSWORD_VERIFY;
            } else {
                LCD_WRITE_CMD(0x01);
                Delay_ms(5);
                LCD_WRITE_StrData((unsigned char*)"Password Error!", 0);
                for (int i = 0; i < 5; i++) {
                    LED1_Turn();
                    Delay_ms(50);
                }
                LED1_OFF();
                Delay_ms(2000);
                LCD_WRITE_CMD(0x01);
                Delay_ms(5);
                clearPasswordArray(password, 7);
                currentState = STATE_PASSWORD_INPUT;
                showDefaultScreen();
            }
        }
    }
}

static void holdDefusePoll(uint8_t bonusDigit);

static uint8_t s_defuseKeyHeld = 0;
static uint16_t s_defuseTargetMs = 10000;
static uint8_t s_defuseRoundSoundPlayed = 0;
static uint8_t s_wasDefuseHolding = 0;

static void defuseRoundSoundReset(void)
{
    s_defuseRoundSoundPlayed = 0;
}

static void defuseRoundSoundTryPlay(void)
{
    if (!s_defuseRoundSoundPlayed) {
        mp3_defuse_start();
        s_defuseRoundSoundPlayed = 1;
    }
}

#define PLANT_HOLD_BEEP_COUNT 7

void handleHoldPlantInput(void)
{
    uint8_t done;
    uint8_t starHeld;
    uint16_t elapsed;
    uint8_t beepIndex;
    static uint8_t wasHolding = 0;
    static uint8_t lastPlantBeepIndex = 255;

    starHeld = MatrixKey_IsStarHeld();
    /* 主循环实际周期约 25~30ms，按 30ms 累计，100 次约 3 秒 */
    done = HoldProgress_TickPlant(starHeld, 30, 3000);
    if (starHeld) {
        wasHolding = 1;
        elapsed = HoldProgress_GetElapsed();
        beepIndex = (uint8_t)((uint32_t)elapsed * PLANT_HOLD_BEEP_COUNT / 3000);
        if (beepIndex >= PLANT_HOLD_BEEP_COUNT) {
            beepIndex = PLANT_HOLD_BEEP_COUNT - 1;
        }
        if (beepIndex != lastPlantBeepIndex) {
            LED1_Turn();
            Delay_ms(15);
            LED1_Turn();
            lastPlantBeepIndex = beepIndex;
        }
        HoldProgress_DrawWithLabel("Hold *", elapsed, 3000);
    } else {
        lastPlantBeepIndex = 255;
        if (wasHolding && HoldProgress_GetElapsed() == 0) {
            wasHolding = 0;
            showDefaultScreen();
        }
    }
    if (done) {
        lastPlantBeepIndex = 255;
        HoldProgress_Reset();
        sign = 0;
        isPressed = 0;
        unlockArrayIndex = 0;
        clearPasswordArray(unlockPassword, 7);
        LCD_WRITE_CMD(0x01);
        Delay_ms(5);
        currentState = STATE_PASSWORD_VERIFY;
    }
}

void showDefaultScreen(void)
{
    LCD_WRITE_CMD(0x01);
    Delay_ms(5);
    if (GameMode_Get() == MODE_HOLD) {
        clearPasswordArray(password, 7);
    }
    LCD_WRITE_StrData(password, spaceCount);
}

void handlePasswordVerifyState(void)
{
    switch (GameMode_Get()) {
        case MODE_MATH:
            handleMathDefuseState();
            return;
        case MODE_HOLD:
            handleHoldDefuseState();
            return;
        default:
            handleClassicDefuseState();
            return;
    }
}

void handleClassicDefuseState(void)
{
    int j;

    LED2_ON();
    mp3_over();
    defuseRoundSoundReset();

    for (j = 0; j < countdown; j++) {
        LED1_ON();

        for (int i = 0; i < 2; i++) {
            KeyNum = MatrixKey_GetValue();
            if (KeyNum != ' ') {
                sign = 1;
            }

            if (sign == 0) {
                Delay_ms(20);
            } else {
                unlockPasswordScan(unlockPassword);
            }
        }

        LED1_OFF();
        for (int i = 0; i < ((int)((pow(((double)0.978), ((double)j))) * 50) - 2); i++) {
            KeyNum = MatrixKey_GetValue();
            if (KeyNum != ' ') {
                sign = 1;
            }

            if (sign == 0) {
                showStarAnimation();
            } else {
                unlockPasswordScan(unlockPassword);
            }
        }

        if (unlockPassword[6] != '*') {
            if (arraysEqual(password, unlockPassword, 7)) {
                currentState = STATE_UNLOCK_SUCCESS;
                return;
            } else {
                for (int i = 0; i < 7; i++) {
                    unlockPassword[i] = '*';
                }
                unlockPassword[7] = '\0';
                unlockArrayIndex = 0;
                sign = 0;
                defuseRoundSoundReset();
                LCD_WRITE_StrData(unlockPassword, spaceCount);
            }
        }
    }
    currentState = STATE_UNLOCK_FAILURE;
}

void handleMathDefuseState(void)
{
    int j;

    LED2_ON();
    mp3_over();
    defuseRoundSoundReset();
    MathQuiz_Generate();
    MathQuiz_ShowQuestion();
    MathQuiz_InitInputBuffer(unlockPassword);
    unlockArrayIndex = MathQuiz_GetInputStartIndex();
    sign = 0;
    isPressed = 0;
    updateMathAnswerDisplay(unlockPassword);

    for (j = 0; j < countdown; j++) {
        if (currentState == STATE_UNLOCK_SUCCESS) {
            return;
        }

        LED1_ON();

        for (int i = 0; i < 2; i++) {
            KeyNum = MatrixKey_GetValue();
            if (KeyNum != ' ') {
                sign = 1;
            }

            if (sign == 0) {
                Delay_ms(20);
            } else {
                mathAnswerScan(unlockPassword);
            }
            if (currentState == STATE_UNLOCK_SUCCESS) {
                return;
            }
        }

        LED1_OFF();
        for (int i = 0; i < ((int)((pow(((double)0.978), ((double)j))) * 50) - 2); i++) {
            KeyNum = MatrixKey_GetValue();
            if (KeyNum != ' ') {
                sign = 1;
            }

            if (sign == 0) {
                showLine2StarAnimation();
            } else {
                mathAnswerScan(unlockPassword);
            }
            if (currentState == STATE_UNLOCK_SUCCESS) {
                return;
            }
        }

        if (unlockPassword[6] != '*') {
            if (MathQuiz_CheckAnswer(unlockPassword)) {
                currentState = STATE_UNLOCK_SUCCESS;
                return;
            } else {
                MathQuiz_InitInputBuffer(unlockPassword);
                unlockArrayIndex = MathQuiz_GetInputStartIndex();
                sign = 0;
                defuseRoundSoundReset();
                updateMathAnswerDisplay(unlockPassword);
            }
        }
    }
    currentState = STATE_UNLOCK_FAILURE;
}

void handleHoldDefuseState(void)
{
    int j;
    uint32_t rng = (uint32_t)Num + 0x2468ACE0u;
    uint8_t bonusDigit;

    LED2_ON();
    mp3_over();
    defuseRoundSoundReset();
    rng = rng * 1103515245u + 12345u;
    bonusDigit = (uint8_t)((rng >> 16) % 10);
    HoldProgress_Reset();
    sign = 0;
    isPressed = 0;
    s_defuseKeyHeld = 0;
    s_wasDefuseHolding = 0;

    LCD_WRITE_CMD(0x01);
    Delay_ms(5);
    showStarAnimation();

    for (j = 0; j < countdown; j++) {
        if (currentState == STATE_UNLOCK_SUCCESS) {
            return;
        }

        LED1_ON();

        for (int i = 0; i < 2; i++) {
            holdDefusePoll(bonusDigit);
            if (currentState == STATE_UNLOCK_SUCCESS) {
                return;
            }
            Delay_ms(20);
        }

        LED1_OFF();
        for (int i = 0; i < ((int)((pow(((double)0.978), ((double)j))) * 50) - 2); i++) {
            holdDefusePoll(bonusDigit);
            if (currentState == STATE_UNLOCK_SUCCESS) {
                return;
            }

            if (sign == 0) {
                showStarAnimation();
            } else {
                if (s_defuseKeyHeld) {
                    HoldProgress_Draw(HoldProgress_GetElapsed(), s_defuseTargetMs);
                }
                Delay_ms(20);
            }
        }
    }
    currentState = STATE_UNLOCK_FAILURE;
}

static void holdDefusePoll(uint8_t bonusDigit)
{
    uint8_t key;
    uint8_t validHold;
    uint16_t target;

    key = MatrixKey_GetValueFast();
    validHold = ((key >= '0' && key <= '9') || key == '#') ? 1 : 0;
    target = 10000;
    if (key >= '0' && key <= '9' && key == (char)('0' + bonusDigit)) {
        target = 5000;
    }

    if (validHold) {
        defuseRoundSoundTryPlay();
    }

    if (HoldProgress_Tick(validHold, 20, target)) {
        currentState = STATE_UNLOCK_SUCCESS;
        return;
    }

    s_defuseKeyHeld = validHold;
    s_defuseTargetMs = target;

    if (validHold) {
        sign = 1;
        s_wasDefuseHolding = 1;
    } else {
        sign = 0;
        defuseRoundSoundReset();
        if (s_wasDefuseHolding) {
            s_wasDefuseHolding = 0;
            LCD_WRITE_StrData((unsigned char*)"                ", 16);
        }
    }
}

void handleUnlockSuccessState(void)
{
    mp3_ct_win();

    LCD_WRITE_CMD(0x01);
    Delay_ms(5);
    Delay_ms(50);

    for (int i = 0; i < 5; i++) {
        LED1_Turn();
        Delay_ms(50);
    }
    LCD_WRITE_StrData(unlockPassword, spaceCount);

    for (int i = 0; i < 2; i++) {
        Delay_ms(300);
        LCD_WRITE_CMD(0x01);
        Delay_ms(5);
        Delay_ms(300);
        LCD_WRITE_StrData(unlockPassword, spaceCount);
    }
    LCD_WRITE_CMD(0x01);
    Delay_ms(5);
    LCD_WRITE_StrData((unsigned char*)"CT win!", spaceCount);

    while (1) {
        LED2_ON();
    }
}

void handleUnlockFailureState(void)
{
    LED1_ON();
    LCD_WRITE_CMD(0x01);
    mp3_boom_music();

    Delay_ms(4000);
    LED1_OFF();

    LCD_WRITE_StrData((unsigned char*)"T win!", spaceCount);
    while (1) {
        LED2_OFF();
    }
}

void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET) {
        if (Num_sign == 0) {
            Num++;
            TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
            if (Num >= 13) {
                Num_sign = 1;
            }
        } else {
            Num--;
            TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
            if (Num <= 0) {
                Num_sign = 0;
            }
        }
    }
}

char arraysEqual(unsigned char arr1[], unsigned char arr2[], int size)
{
    for (int i = 0; i < size; i++) {
        if (arr1[i] != arr2[i]) {
            return 0;
        }
    }
    return 1;
}

void rightShiftArray(unsigned char arr[], int size)
{
    if (size > 0) {
        for (int i = size - 1; i > 0; i--) {
            arr[i] = arr[i - 1];
        }
        arr[0] = '*';
    }
}

void leftShiftArray(unsigned char arr[], int size)
{
    if (size > 0) {
        for (int i = 0; i < size - 1; i++) {
            arr[i] = arr[i + 1];
        }
        arr[7] = '\0';
    }
}

void handleKeyPressFeedback(void)
{
    LED1_Turn();
    Delay_ms(50);
    LED1_Turn();
}

void clearPasswordArray(unsigned char pass[], int size)
{
    for (int i = 0; i < size; i++) {
        pass[i] = '*';
    }
    pass[7] = '\0';
}

void updateKeyPressState(void)
{
    if (KeyNum != ' ') {
        isPressed = 1;
    } else {
        isPressed = 0;
    }
}

void updatePasswordDisplay(unsigned char pass[])
{
    LCD_WRITE_StrData((unsigned char*)"                ", 0);
    LCD_WRITE_StrData(pass, spaceCount);
}

void updateMathAnswerDisplay(unsigned char pass[])
{
    LCD_WRITE_StrData((unsigned char*)"                ", 16);
    LCD_WRITE_StrData(pass, 16 + spaceCount);
}

void showStarAnimation(void)
{
    LCD_WRITE_StrData((unsigned char*)"                ", 0);
    LCD_WRITE_StrData((unsigned char*)"***", Num);
    Delay_ms(20);
}

void showLine2StarAnimation(void)
{
    LCD_WRITE_StrData((unsigned char*)"                ", 16);
    LCD_WRITE_StrData((unsigned char*)"***", 16 + Num);
    Delay_ms(20);
}

void unlockPasswordScan(unsigned char pass[])
{
    KeyNum = MatrixKey_GetValue();
    if (isPressed == 0) {
        if ((KeyNum != ' ') && (KeyNum != '*') && (KeyNum != '#') && (isPressed == 0)) {
            Delay_ms(50);
            handleKeyPressFeedback();
            if (unlockArrayIndex == 0) {
                defuseRoundSoundTryPlay();
            }
            pass[unlockArrayIndex] = KeyNum;
            unlockArrayIndex++;
            if (unlockArrayIndex >= 7) {
                unlockArrayIndex = 0;
            }
            pass[7] = '\0';
            updatePasswordDisplay(pass);
        } else if (KeyNum == '*' && (isPressed == 0)) {
            Delay_ms(50);
            handleKeyPressFeedback();
            unlockArrayIndex--;
            if (unlockArrayIndex < 0) {
                unlockArrayIndex = 0;
            }
            pass[unlockArrayIndex] = '*';
            pass[7] = '\0';
            updatePasswordDisplay(pass);
        } else if (KeyNum == '#' && (isPressed == 0)) {
            Delay_ms(50);
            handleKeyPressFeedback();
            clearPasswordArray(pass, 7);
            unlockArrayIndex = 0;
            defuseRoundSoundReset();
            updatePasswordDisplay(pass);
        }
    }

    if (KeyNum != ' ') {
        isPressed = 1;
        Delay_ms(20);
    } else {
        isPressed = 0;
        Delay_ms(20);
    }
}

void mathAnswerScan(unsigned char pass[])
{
    const int inputStart = MathQuiz_GetInputStartIndex();

    KeyNum = MatrixKey_GetValue();
    if (isPressed == 0) {
        if ((KeyNum != ' ') && (KeyNum != '*') && (KeyNum != '#') && (isPressed == 0)) {
            Delay_ms(50);
            handleKeyPressFeedback();
            if (unlockArrayIndex == inputStart) {
                defuseRoundSoundTryPlay();
            }
            if (unlockArrayIndex < inputStart) {
                unlockArrayIndex = inputStart;
            }
            pass[unlockArrayIndex] = KeyNum;
            unlockArrayIndex++;
            if (unlockArrayIndex > 6) {
                unlockArrayIndex = 6;
            }
            pass[7] = '\0';
            updateMathAnswerDisplay(pass);
        } else if (KeyNum == '*' && (isPressed == 0)) {
            Delay_ms(50);
            handleKeyPressFeedback();
            unlockArrayIndex--;
            if (unlockArrayIndex < inputStart) {
                unlockArrayIndex = inputStart;
            }
            pass[unlockArrayIndex] = '*';
            pass[7] = '\0';
            updateMathAnswerDisplay(pass);
        } else if (KeyNum == '#' && (isPressed == 0)) {
            Delay_ms(50);
            handleKeyPressFeedback();
            MathQuiz_InitInputBuffer(pass);
            unlockArrayIndex = inputStart;
            defuseRoundSoundReset();
            updateMathAnswerDisplay(pass);
        }
    }

    if (KeyNum != ' ') {
        isPressed = 1;
        Delay_ms(20);
    } else {
        isPressed = 0;
        Delay_ms(20);
    }
}

void deployPasswordScan(unsigned char pass[])
{
    KeyNum = MatrixKey_GetValue();
    bool needDisplayUpdate = false;

    if ((KeyNum >= '0' && KeyNum <= '9') && (isPressed == 0)) {
        Delay_ms(50);
        handleKeyPressFeedback();
        leftShiftArray(pass, 8);
        pass[6] = KeyNum;
        pass[7] = '\0';
        needDisplayUpdate = true;
    } else if (KeyNum == '*' && (isPressed == 0)) {
        Delay_ms(50);
        handleKeyPressFeedback();
        rightShiftArray(pass, 7);
        pass[7] = '\0';
        needDisplayUpdate = true;
    } else if (KeyNum == '#' && (isPressed == 0)) {
        Delay_ms(50);
        handleKeyPressFeedback();
        clearPasswordArray(pass, 7);
        needDisplayUpdate = true;
    }

    updateKeyPressState();

    if (needDisplayUpdate) {
        updatePasswordDisplay(pass);
    }
}
