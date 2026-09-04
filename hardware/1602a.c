#include "1602a.h"
#include "stm32f10x_rcc.h"

void delay_us(unsigned int us)
{
	unsigned int  i;	
	do
	{
		i = 10;
		while(i--) __nop();
	} 
	while (--us);	
}

void GPIO_INIT(void)
{		
	GPIO_InitTypeDef PB;
	GPIO_InitTypeDef PA;	
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA, ENABLE );
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOB, ENABLE );
	
	PB.GPIO_Pin = EN|RW|RS;
	PB.GPIO_Mode = GPIO_Mode_Out_PP;
	PB.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &PB);
	
	PA.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|
								GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|
								GPIO_Pin_6|GPIO_Pin_7;
	PA.GPIO_Mode = GPIO_Mode_Out_PP;
	PA.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &PA);	
}

void LCD_INIT(void)
{
	GPIO_INIT();	
	GPIO_Write( GPIOA, 0x0000 );
	GPIO_Write( GPIOB, 0x0000 );
	
	delay_us(15000);
	LCD_WRITE_CMD( 0x38 );
	delay_us(5000);
	LCD_WRITE_CMD( 0x38 );
	delay_us(5000);
	LCD_WRITE_CMD( 0x38 );
	delay_us(5000);
	
	LCD_WRITE_CMD( 0x08 );
	delay_us(5000);
	LCD_WRITE_CMD( 0x01 );
	delay_us(5000);
	LCD_WRITE_CMD( 0x06 );
	delay_us(5000);	
	LCD_WRITE_CMD( 0x0C );
	delay_us(5000);
}

// ====================== ✅ 标准1602全屏驱动（修复完成！）======================
void LCD_SetCursor(unsigned char Column)
{
    // 标准 1602 规则
    // 0~15   第一行
    // 64~79 第二行
    if (Column < 16)
    {
        LCD_WRITE_CMD(0x80 + Column);        // 第一行 0~15
    }
    else
    {
        LCD_WRITE_CMD(0x80 + 0x40 + (Column - 16)); // 第二行 0~15
    }
}

void LCD_WRITE_CMD( unsigned char CMD )
{	
	ReadBusy();
	GPIO_ResetBits( GPIOB, RS );
	GPIO_ResetBits( GPIOB, RW );
	GPIO_ResetBits( GPIOB, EN );
	GPIO_Write( GPIOA, CMD );	
	GPIO_SetBits( GPIOB, EN );
	GPIO_ResetBits( GPIOB, EN );
}

void LCD_WRITE_ByteDATA( unsigned char ByteData )
{	
	ReadBusy();
	GPIO_SetBits( GPIOB, RS );
	GPIO_ResetBits( GPIOB, RW );
	GPIO_ResetBits( GPIOB, EN );
	GPIO_Write( GPIOA, ByteData );
	GPIO_SetBits( GPIOB, EN );
	GPIO_ResetBits( GPIOB, EN );
}

// ====================== ✅ 标准全屏字符串显示 ======================
void LCD_WRITE_StrDATA(unsigned char *StrData, unsigned char col)
{
    unsigned char i;
    for (i = 0; StrData[i] != '\0'; i++)
    {
        LCD_SetCursor(col + i);  // 自动连续定位
        LCD_WRITE_ByteDATA(StrData[i]);
    }
}

void ReadBusy(void)
{		
	GPIO_Write( GPIOA, 0x00ff );	
	GPIO_InitTypeDef p;
	p.GPIO_Pin = GPIO_Pin_7;
	p.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	p.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init( GPIOA, &p );
	
	GPIO_ResetBits( GPIOB, RS );
	GPIO_SetBits( GPIOB, RW );	
	GPIO_SetBits( GPIOB, EN );
	while( GPIO_ReadInputDataBit( GPIOA, GPIO_Pin_7 ) );
	GPIO_ResetBits( GPIOB, EN );
		
	p.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|
							 GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|
					 		 GPIO_Pin_6|GPIO_Pin_7;
	p.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init( GPIOA, &p  );
}

void WUserImg(unsigned char pos,unsigned char *ImgInfo)
{
	unsigned char cgramAddr;		
	if( pos <= 1 ) cgramAddr = 0x40;
	if( pos > 1 && pos <= 3 ) cgramAddr = 0x50;
	if( pos > 3 && pos <= 5 ) cgramAddr = 0x60;
	if( pos > 5 && pos <= 7 ) cgramAddr = 0x70;

	LCD_WRITE_CMD( (cgramAddr + (pos%2) * 8) );	
	while( *ImgInfo != '\0' )
	{		
		LCD_WRITE_ByteDATA( *ImgInfo );
		ImgInfo++;
	}
}