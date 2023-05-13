#ifndef MAIN_ST7789_H_
#define MAIN_ST7789_H_

#include "driver/spi_master.h"

#define CONFIG_WIDTH  320
#define CONFIG_HEIGHT 240

#define   BLACK     0x0000  // 黑色    0,   0,   0
#define   NAVY      0x000F  // 深蓝色  0,   0, 127
#define   DGREEN    0x03E0  // 深绿色  0,  127,  0
#define   DCYAN     0x03EF  // 深青色  0,  127, 127       
#define   MAROON    0x7800  // 深红色  127,   0,   0      
#define   PURPLE    0x780F  // 紫色    127,   0, 127      
#define   OLIVE     0x7BE0  // 橄榄绿  127, 127,   0      
#define   LGRAY     0xC618  // 灰白色  192, 192, 192      
#define   DGRAY     0x7BEF  // 深灰色  127, 127, 127      
#define   BLUE      0x001F  // 蓝色    0,   0, 255        
#define   GREEN     0x07E0  // 绿色    0, 255,   0        
#define   CYAN      0x07FF  // 青色    0, 255, 255        
#define   RED       0xF800  // 红色    255,   0,   0      
#define   MAGENTA   0xF81F  // 品红    255,   0, 255      
#define   YELLOW    0xFFE0  // 黄色    255, 255, 0        
#define   WHITE     0xFFFF  // 白色    255, 255, 255  



#define DIRECTION0		0
#define DIRECTION90		1
#define DIRECTION180		2
#define DIRECTION270		3


typedef struct {
	uint16_t _width;
	uint16_t _height;
	int16_t _dc;
	spi_device_handle_t _SPIHandle;
} TFT_t;

void spi_master_init(TFT_t * dev, int16_t GPIO_MOSI, int16_t GPIO_SCLK, int16_t GPIO_CS, int16_t GPIO_DC);
bool spi_master_write_byte(spi_device_handle_t SPIHandle, const uint8_t* Data, size_t DataLength);
bool spi_master_write_command(TFT_t * dev, uint8_t cmd);
bool spi_master_write_data_byte(TFT_t * dev, uint8_t data);
bool spi_master_write_data_word(TFT_t * dev, uint16_t data);
bool spi_master_write_addr(TFT_t * dev, uint16_t addr1, uint16_t addr2);
bool spi_master_write_color(TFT_t * dev, uint16_t color, uint16_t size);
bool spi_master_write_colors(TFT_t * dev, uint16_t * colors, uint16_t size);

void delayMS(int ms);
void lcdInit(TFT_t * dev, int width, int height);
void lcdDrawPixel(TFT_t * dev, uint16_t x, uint16_t y, uint16_t color);
void lcdDrawMultiPixels(TFT_t * dev, uint16_t x, uint16_t y, uint16_t size, uint16_t * colors);
void lcdDrawFillRect(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void lcdDisplayOff(TFT_t * dev);
void lcdDisplayOn(TFT_t * dev);
void lcdFillScreen(TFT_t * dev, uint16_t color);
void lcdDrawLine(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void lcdDrawRect(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void lcdDrawRectAngle(TFT_t * dev, uint16_t xc, uint16_t yc, uint16_t w, uint16_t h, uint16_t angle, uint16_t color);
void lcdDrawTriangle(TFT_t * dev, uint16_t xc, uint16_t yc, uint16_t w, uint16_t h, uint16_t angle, uint16_t color);
void lcdDrawCircle(TFT_t * dev, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
void lcdDrawFillCircle(TFT_t * dev, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
void lcdDrawRoundRect(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t r, uint16_t color);
void LCD_ShowPicture(TFT_t * dev, uint16_t x, uint16_t y ,uint16_t length, uint16_t width,const uint8_t pic[]);
uint16_t rgb565_conv(uint16_t r, uint16_t g, uint16_t b);
void LCD_ASCII_8X16(TFT_t * dev, uint16_t x, uint16_t y ,char ch,uint16_t textColor,uint16_t BackColor);
void LCD_ASCII_8X16_str(TFT_t * dev, uint16_t x, uint16_t y ,char *ch,uint8_t len,uint16_t textColor,uint16_t BackColor);
void LCD_ASCII_16X24(TFT_t * dev, uint16_t x, uint16_t y ,char ch,uint16_t textColor,uint16_t BackColor);
void LCD_ASCII_24X30(TFT_t * dev, uint16_t x, uint16_t y ,char num,uint16_t textColor,uint16_t BackColor);
void LCD_ASCII_24X30_str(TFT_t * dev, uint16_t x, uint16_t y ,char *ch,uint8_t len,uint16_t textColor,uint16_t BackColor);
void LCD_monochrome_32X32(TFT_t * dev, uint16_t x, uint16_t y ,const unsigned char *zf,uint16_t textColor,uint16_t BackColor);
void LCD_monochrome_24X24(TFT_t * dev, uint16_t x, uint16_t y ,const unsigned char *zf,uint16_t textColor,uint16_t BackColor);
void LCD_monochrome_any(TFT_t * dev, uint16_t x, uint16_t y ,const unsigned char *zf,uint16_t width, uint16_t height ,uint16_t textColor,uint16_t BackColor);


#endif 

