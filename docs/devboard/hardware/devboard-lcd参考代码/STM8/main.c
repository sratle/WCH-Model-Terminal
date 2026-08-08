 /********************金逸晨**************************
*****************2.0寸  7PIN SPI TFT FOR STM8S*************
*****STM8S003F3驱动**************************
***** PC3----3  SCK   ,  PC4----4  SDA   ,  
***** BY:GU 
*********ST7789V2

从左往右，从上往下    240*320  18bit 6 6 6 RGB 模式  高位模式
********************************************************/
#include "IOSTM8S003F3.h"

#define set_0   0x01
#define set_1   0x02
#define set_2   0x04
#define set_3   0x08    
#define set_4   0x10
#define set_5   0x20
#define set_6   0x40
#define set_7   0x80

#define clr_0   0xFE
#define clr_1   0xFD
#define clr_2   0xFB
#define clr_3   0xF7    
#define clr_4   0xEF
#define clr_5   0xDF
#define clr_6   0xBF
#define clr_7   0x7F
/**********SPI引脚分配，连接oled屏，更具实际情况修改*********/

#define SPI_SCK_0  PC_ODR&=clr_3        //PC3       
#define SPI_SCK_1  PC_ODR|=set_3       
#define SPI_SDA_0  PC_ODR&=clr_4        //PC4        
#define SPI_SDA_1  PC_ODR|=set_4
#define SPI_RST_0  PC_ODR&=clr_5        //PC5        
#define SPI_RST_1  PC_ODR|=set_5
#define SPI_DC_0  PC_ODR&=clr_6           //PC6
#define SPI_DC_1  PC_ODR|=set_6
#define SPI_CS_0  PC_ODR&=clr_7          //PC7
#define SPI_CS_1  PC_ODR|=set_7
#define SPI_CS2_0  PD_ODR&=clr_2          //PD2
#define SPI_CS2_1  PD_ODR|=set_2
#define SPI_FSO  (PD_IDR&set_3)              //PD3


#define OLED_COLUMN_NUMBER 240
#define OLED_LINE_NUMBER 320
#define OLED_COLUMN_OFFSET 0

#define RED 	0XFF0000
#define GREEN  	0X00FF00
#define BLUE 	0X0000FF

const unsigned char china_char[][32]=   		//
{
{0x00,0x00,0xE4,0x3F,0x28,0x20,0x28,0x25,0x81,0x08,0x42,0x10,0x02,0x02,0x08,0x02,
0xE8,0x3F,0x04,0x02,0x07,0x07,0x84,0x0A,0x44,0x12,0x34,0x62,0x04,0x02,0x00,0x02},/*"深",0*/
{0x88,0x20,0x88,0x24,0x88,0x24,0x88,0x24,0x88,0x24,0xBF,0x24,0x88,0x24,0x88,0x24,
0x88,0x24,0x88,0x24,0x88,0x24,0xB8,0x24,0x87,0x24,0x42,0x24,0x40,0x20,0x20,0x20},/*"圳",1*/
{0x80,0x00,0x80,0x00,0x40,0x01,0x20,0x02,0x10,0x04,0x08,0x08,0xF4,0x17,0x83,0x60,
0x80,0x00,0xFC,0x1F,0x80,0x00,0x88,0x08,0x90,0x08,0x90,0x04,0xFF,0x7F,0x00,0x00},/*"金",2*/
{0x80,0x00,0x82,0x00,0x84,0x0F,0x44,0x08,0x20,0x04,0xF0,0x3F,0x27,0x22,0x24,0x22,
0xE4,0x3F,0x04,0x05,0x84,0x0C,0x84,0x54,0x44,0x44,0x24,0x78,0x0A,0x00,0xF1,0x7F},/*"逸",3*/
{0xF8,0x0F,0x08,0x08,0xF8,0x0F,0x08,0x08,0xF8,0x0F,0x00,0x00,0xFC,0x3F,0x04,0x00,
0xF4,0x1F,0x04,0x00,0xFC,0x7F,0x94,0x10,0x14,0x09,0x12,0x06,0x52,0x18,0x31,0x60},/*"晨",4*/
{0x80,0x00,0x80,0x00,0x80,0x00,0xFC,0x1F,0x84,0x10,0x84,0x10,0x84,0x10,0xFC,0x1F,
0x84,0x10,0x84,0x10,0x84,0x10,0xFC,0x1F,0x84,0x50,0x80,0x40,0x80,0x40,0x00,0x7F},/*"电",5*/
{0x00,0x00,0xFE,0x1F,0x00,0x08,0x00,0x04,0x00,0x02,0x80,0x01,0x80,0x00,0xFF,0x7F,
0x80,0x00,0x80,0x00,0x80,0x00,0x80,0x00,0x80,0x00,0x80,0x00,0xA0,0x00,0x40,0x00},/*"子",6*/
{0x00,0x00,0xE4,0x1F,0x48,0x10,0x48,0x10,0x41,0x10,0x82,0x08,0x92,0x08,0x90,0x08,
0x08,0x05,0x08,0x05,0x07,0x02,0x04,0x02,0x04,0x05,0x84,0x08,0x44,0x10,0x30,0x60},/*"汉",7*/
{0x40,0x00,0x80,0x00,0xFE,0x7F,0x02,0x40,0x01,0x20,0xF8,0x07,0x00,0x02,0x00,0x01,
0x80,0x00,0xFF,0x7F,0x80,0x00,0x80,0x00,0x80,0x00,0x80,0x00,0xA0,0x00,0x40,0x00},/*"字",8*/
{0x00,0x01,0x04,0x02,0xE8,0x3F,0x28,0x20,0x01,0x00,0xC2,0x1F,0x02,0x02,0xC8,0x1F,
0x48,0x12,0xC4,0x1F,0x47,0x12,0xC4,0x1F,0x04,0x00,0x84,0x08,0x44,0x10,0x20,0x20},/*"演",9*/
{0x00,0x00,0xFC,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x7F,0x80,0x00,
0x80,0x00,0x88,0x08,0x88,0x10,0x84,0x20,0x82,0x40,0x81,0x40,0xA0,0x00,0x40,0x00},/*"示",10*/


};

/*******时钟初始化******************/
void CLK_init(void)
{
  
  CLK_CKDIVR|= 0x00;       /*设置时钟为内部16M高速时钟*/ 
}
void delay_us(unsigned int _us_time)
{       
  unsigned char x=0;
  for(;_us_time>0;_us_time--)
  {
    x++;
  }
}
void delay_ms(unsigned int _ms_time)
  {
    unsigned int i,j;
    for(i=0;i<_ms_time;i++)
    {
    for(j=0;j<900;j++)
      {;}
    }
  }
/*************SPI配置函数*******************
，SCL空闲时低电平，第一个上升沿采样
模拟SPI
******************************************/

/**************************SPI模块发送函数************************************************

 *************************************************************************/
void SPI_SendByte(unsigned char byte)
{
  
  unsigned char counter;
   
  for(counter=0;counter<8;counter++)
  { 
    SPI_SCK_0;
	  
    if((byte&0x80)==0)
    {
      SPI_SDA_0;
    }
    else SPI_SDA_1;
    byte=byte<<1;
	
    SPI_SCK_1;	
	
	SPI_SCK_0;
		
  } 
}

void TFT_SEND_CMD(unsigned char o_command)
  {
    SPI_DC_0;
    SPI_CS_0;
    SPI_SendByte(o_command);
    SPI_CS_1;
	 
    //SPI_DC_1;
  }
void TFT_SEND_DATA(unsigned char o_data)
  { 
    SPI_DC_1;
    SPI_CS_0;
    SPI_SendByte(o_data);
    SPI_CS_1;
	  
   }
void OLED_clear(void)
  {
    unsigned int ROW,column;
	TFT_SEND_CMD(0x2a); 		//Column address set
	TFT_SEND_DATA(0x00); 		//start column
	TFT_SEND_DATA(0x00); 
	TFT_SEND_DATA(0x00);		//end column
	TFT_SEND_DATA(0xEF);

	TFT_SEND_CMD(0x2b); 		//Row address set
	TFT_SEND_DATA(0x00); 		//start row
	TFT_SEND_DATA(0x00); 
	TFT_SEND_DATA(0x01);		//end row
	TFT_SEND_DATA(0x3F);
	  TFT_SEND_CMD(0x2C);			//Memory write
    for(ROW=0;ROW<OLED_LINE_NUMBER;ROW++)             //ROW loop
      { 
	  
          for(column=0;column<OLED_COLUMN_NUMBER;column++)	//column loop
            {
              TFT_SEND_DATA(0x00);
			  TFT_SEND_DATA(0x00);
				TFT_SEND_DATA(0x00);
            }
      }
  }
void OLED_full(unsigned long color)
  {
    unsigned int ROW,column;
	  TFT_SEND_CMD(0x2a); 		//Column address set
	TFT_SEND_DATA(0x00); 		//start column
	TFT_SEND_DATA(0x00); 
	TFT_SEND_DATA(0x00);		//end column
	TFT_SEND_DATA(0xEF);

	TFT_SEND_CMD(0x2b); 		//Row address set
	TFT_SEND_DATA(0x00); 		//start row
	TFT_SEND_DATA(0x00); 
	TFT_SEND_DATA(0x01);		//end row
	TFT_SEND_DATA(0x3F);
	  TFT_SEND_CMD(0x2C);			//Memory write
    for(ROW=0;ROW<OLED_LINE_NUMBER;ROW++)             //ROW loop
      { 
	  
	for(column=0;column<OLED_COLUMN_NUMBER ;column++)	//column loop
          {
            TFT_SEND_DATA(color>>16);
			  TFT_SEND_DATA(color>>8);
			  TFT_SEND_DATA(color);
          }
      }
  }
void OLED_init(void)				////ST7789V2
  {
	SPI_SCK_0;
	SPI_RST_0;
	delay_ms(1000);
	SPI_RST_1;
	delay_ms(1000);
    TFT_SEND_CMD(0x11); 			//Sleep Out
	delay_ms(120);               //DELAY120ms 
	//--------------------------------ST7789S Frame rate setting----------------------------------// 
	TFT_SEND_CMD(0x2a); 		//Column address set
	TFT_SEND_DATA(0x00); 		//start column
	TFT_SEND_DATA(0x00); 
	TFT_SEND_DATA(0x00);		//end column
	TFT_SEND_DATA(0xef);

	TFT_SEND_CMD(0x2b); 		//Row address set
	TFT_SEND_DATA(0x00); 		//start row
	TFT_SEND_DATA(0x28); 
	TFT_SEND_DATA(0x01);		//end row
	TFT_SEND_DATA(0x17);

	TFT_SEND_CMD(0xb2); 		//Porch control
	TFT_SEND_DATA(0x0c); 
	TFT_SEND_DATA(0x0c); 
	TFT_SEND_DATA(0x00); 
	TFT_SEND_DATA(0x33); 
	TFT_SEND_DATA(0x33); 

	TFT_SEND_CMD(0x20); 		//Display Inversion Off

	TFT_SEND_CMD(0xb7); 		//Gate control
	TFT_SEND_DATA(0x56);   		//35
//---------------------------------ST7789S Power setting--------------------------------------// 
	TFT_SEND_CMD(0xbb); //VCOMS Setting
	TFT_SEND_DATA(0x18);  //1f

	TFT_SEND_CMD(0xc0); 		//LCM Control
	TFT_SEND_DATA(0x2c); 

	TFT_SEND_CMD(0xc2); 		//VDV and VRH Command Enable
	TFT_SEND_DATA(0x01); 

	TFT_SEND_CMD(0xc3); //VRH Set
	TFT_SEND_DATA(0x1f); //12

	TFT_SEND_CMD(0xc4); 			//VDV Setting
	TFT_SEND_DATA(0x20); 

	TFT_SEND_CMD(0xc6); 			//FR Control 2
	TFT_SEND_DATA(0x0f); 
//TFT_SEND_CMD(0xca); 
//TFT_SEND_DATA(0x0f); 
//TFT_SEND_CMD(0xc8); 
//TFT_SEND_DATA(0x08); 
//TFT_SEND_CMD(0x55); 
//TFT_SEND_DATA(0x90); 
	TFT_SEND_CMD(0xd0);  //Power Control 1
	TFT_SEND_DATA(0xa6);   //a4
	TFT_SEND_DATA(0xa1); 
//--------------------------------ST7789S gamma setting---------------------------------------// 

	TFT_SEND_CMD(0xe0); 
	TFT_SEND_DATA(0xd0); 
	TFT_SEND_DATA(0x0d); 
	TFT_SEND_DATA(0x14); 
	TFT_SEND_DATA(0x0b); 
	TFT_SEND_DATA(0x0b); 
	TFT_SEND_DATA(0x07); 
	TFT_SEND_DATA(0x3a);  
	TFT_SEND_DATA(0x44); 
	TFT_SEND_DATA(0x50); 
	TFT_SEND_DATA(0x08); 
	TFT_SEND_DATA(0x13); 
	TFT_SEND_DATA(0x13); 
	TFT_SEND_DATA(0x2d); 
	TFT_SEND_DATA(0x32); 

	TFT_SEND_CMD(0xe1); 				//Negative Voltage Gamma Contro
	TFT_SEND_DATA(0xd0); 
	TFT_SEND_DATA(0x0d); 
	TFT_SEND_DATA(0x14); 
	TFT_SEND_DATA(0x0b); 
	TFT_SEND_DATA(0x0b); 
	TFT_SEND_DATA(0x07); 
	TFT_SEND_DATA(0x3a); 
	TFT_SEND_DATA(0x44); 
	TFT_SEND_DATA(0x50); 
	TFT_SEND_DATA(0x08); 
	TFT_SEND_DATA(0x13); 
	TFT_SEND_DATA(0x13); 
	TFT_SEND_DATA(0x2d); 
	TFT_SEND_DATA(0x32);
	
	TFT_SEND_CMD(0x36); 			//Memory data access control
	TFT_SEND_DATA(0x00); 
	
	TFT_SEND_CMD(0x3A); 			//Interface pixel format
	//TFT_SEND_DATA(0x55);			//65K	
	TFT_SEND_DATA(0x66);			//262K  RGB 6 6 6

	TFT_SEND_CMD(0xe7); 			//SPI2 enable    启用2数据通道模式
	TFT_SEND_DATA(0x00); 


	TFT_SEND_CMD(0x21);			//Display inversion on
	TFT_SEND_CMD(0x29); 			//Display on
	TFT_SEND_CMD(0x2C);			//Memory write
  }

void Picture_display(const unsigned char *ptr_pic)
  {
    unsigned int ROW,column;
	 TFT_SEND_CMD(0x2a); 		//Column address set
	TFT_SEND_DATA(0x00); 		//start column
	TFT_SEND_DATA(0x00); 
	TFT_SEND_DATA(0x00);		//end column
	TFT_SEND_DATA(0x63);

	TFT_SEND_CMD(0x2b); 		//Row address set
	TFT_SEND_DATA(0x00); 		//start row
	TFT_SEND_DATA(0x00); 
	TFT_SEND_DATA(0x00);		//end row
	TFT_SEND_DATA(0x63);
	  TFT_SEND_CMD(0x2C);			//Memory write
    for(ROW=0;ROW<100;ROW++)        //ROW loop
      {   
		
	for(column=0;column<100;column++)	//column loop
          {
            TFT_SEND_DATA(*ptr_pic++);
			TFT_SEND_DATA(*ptr_pic++);
			  TFT_SEND_DATA(*ptr_pic++);
          }
      }
  }
void display_char16_16(unsigned int x,unsigned int y,unsigned long color,const unsigned char *point)
{
	 unsigned int column;
	unsigned char tm=0,temp;
	 TFT_SEND_CMD(0x2a); 		//Column address set
	TFT_SEND_DATA(x>>8); 		//start column
	TFT_SEND_DATA(x);
	x=x+15;
	TFT_SEND_DATA(x>>8);		//end column
	TFT_SEND_DATA(x);

	TFT_SEND_CMD(0x2b); 		//Row address set
	TFT_SEND_DATA(y>>8); 		//start row
	TFT_SEND_DATA(y); 
	y=y+15;
	TFT_SEND_DATA(y>>8);		//end row
	TFT_SEND_DATA(y);
	  TFT_SEND_CMD(0x2C);			//Memory write
    
		
	for(column=0;column<32;column++)	//column loop
          {
			  temp=*point;
			  for(tm=0;tm<8;tm++)
			  {
			  if(temp&0x01)
					{
					TFT_SEND_DATA(color>>16);
					TFT_SEND_DATA(color>>8);
					TFT_SEND_DATA(color);
					}
			  else 
					{
					TFT_SEND_DATA(0);
					TFT_SEND_DATA(0);
					TFT_SEND_DATA(0);
					}
					temp>>=1;
				}
			  point++;
          
      }
}
void Picture_ReverseDisplay(const unsigned char *ptr_pic)
{
    unsigned int ROW,column,data;
	TFT_SEND_CMD(0x2a); 		//Column address set
	TFT_SEND_DATA(0x00); 		//start column
	TFT_SEND_DATA(0x00); 
	TFT_SEND_DATA(0x00);		//end column
	TFT_SEND_DATA(0xef);

	TFT_SEND_CMD(0x2b); 		//Row address set
	TFT_SEND_DATA(0x00); 		//start row
	TFT_SEND_DATA(0x28); 
	TFT_SEND_DATA(0x01);		//end row
	TFT_SEND_DATA(0x17);
	TFT_SEND_CMD(0x2C);			//Memory write
    for(ROW=0;ROW<OLED_LINE_NUMBER;ROW++)        //ROW loop
      { 
	  
	for(column=0;column<OLED_COLUMN_NUMBER;column++)	//column loop
          {
            data=*ptr_pic++;
            data=~data;
            TFT_SEND_DATA(data);
			
          }
      }
  }
void IO_init(void)
{
  PC_DDR|=0xFF;                         //PC  06,07，03,04,07输出模式
  PC_CR1|=0xFF;
  PC_CR2|=0xFF;
  PC_ODR=0XFF;
  PD_DDR|=0xF7;                         //PD  02输出模式 03 上拉输入模式
  PD_CR1|=0xF7;                        /*设置上拉输入*/
  PD_CR2|=0xF7;                         /*设置无中断*/
  PD_ODR=0XFF;

}

int main()
{ 

  IO_init();
  SPI_SCK_0;
  SPI_RST_0;
  delay_ms(100);
  SPI_RST_1;
  OLED_init();
	OLED_full(RED);
	delay_ms(1000);
	OLED_full(GREEN);
	delay_ms(1000);
	OLED_full(BLUE);
	delay_ms(1000);
	OLED_clear();
  while(1)
  {
    
		display_char16_16(104,200,RED,china_char[0]);
		display_char16_16(120,200,GREEN,china_char[1]);
		display_char16_16(80,256,BLUE,china_char[2]);
		display_char16_16(96,256,0XFFFFFF,china_char[3]);
		display_char16_16(112,256,BLUE,china_char[4]);
		display_char16_16(138,256,GREEN,china_char[5]);
		display_char16_16(154,256,RED,china_char[6]);
		display_char16_16(96,280,BLUE,china_char[7]);
		display_char16_16(112,280,GREEN,china_char[8]);
		display_char16_16(138,280,RED,china_char[9]);
		display_char16_16(154,280,BLUE,china_char[10]);
		delay_ms(1000);
		OLED_clear();
                delay_ms(1000);
  }
}

