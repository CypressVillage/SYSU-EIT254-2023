
#include "main.h"
#include "st7789.h"
#include "ascii.h"

#define TAG "ST7789"
#define	_DEBUG_ 0

#define LCD_HOST    SPI2_HOST

static const int SPI_Command_Mode = 0;
static const int SPI_Data_Mode = 1;


void spi_master_init(TFT_t * dev, int16_t GPIO_MOSI, int16_t GPIO_SCLK, int16_t GPIO_CS, int16_t GPIO_DC)
{
	esp_err_t ret;

	gpio_pad_select_gpio( GPIO_CS );
	gpio_set_direction( GPIO_CS, GPIO_MODE_OUTPUT );
	gpio_set_level( GPIO_CS, 0 );

	gpio_pad_select_gpio( GPIO_DC );
	gpio_set_direction( GPIO_DC, GPIO_MODE_OUTPUT );
	gpio_set_level( GPIO_DC, 0 );

	spi_bus_config_t buscfg = {
		.mosi_io_num = GPIO_MOSI,
		.miso_io_num = -1,
		.sclk_io_num = GPIO_SCLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 0,
		.flags = 0
	};

	ret = spi_bus_initialize( LCD_HOST, &buscfg, SPI_DMA_CH_AUTO );
	assert(ret==ESP_OK);

	spi_device_interface_config_t devcfg;
	memset(&devcfg, 0, sizeof(devcfg));
	devcfg.clock_speed_hz = 80000000;
	devcfg.queue_size = 7;
	devcfg.mode = 0;
	devcfg.flags = SPI_DEVICE_NO_DUMMY;

	if ( GPIO_CS >= 0 ) {
		devcfg.spics_io_num = GPIO_CS;
	} else {
		devcfg.spics_io_num = -1;
	}
	
	spi_device_handle_t handle;
	ret = spi_bus_add_device( LCD_HOST, &devcfg, &handle);
	assert(ret==ESP_OK);
	dev->_dc = GPIO_DC;
	dev->_SPIHandle = handle;
}


bool spi_master_write_byte(spi_device_handle_t SPIHandle, const uint8_t* Data, size_t DataLength)
{
	spi_transaction_t SPITransaction;
	esp_err_t ret;
	int i=0;
	int n=0;
	int len=DataLength;



	if ( DataLength > 4092 ) {
		// memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
		// SPITransaction.length = 4092 * 8;
		// SPITransaction.tx_buffer = Data;
		// ret = spi_device_transmit( SPIHandle, &SPITransaction );
		// assert(ret==ESP_OK); 

		// memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
		// SPITransaction.length = (DataLength-4092) * 8;
		// SPITransaction.tx_buffer = Data+4092;
		// ret = spi_device_transmit( SPIHandle, &SPITransaction );
		n=DataLength/4092;
		if(DataLength%4092!=0)
		{
			n++;
		}
		for(i=0;i<n;i++)
		{
			memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
			if(len>=4092)
			{
				SPITransaction.length = 4092 * 8;
			}
			else
			{
				SPITransaction.length = len * 8;
			}
			
			SPITransaction.tx_buffer = Data;
			ret = spi_device_transmit( SPIHandle, &SPITransaction );
			len-=4092;
			Data+=4092;
		}

		assert(ret==ESP_OK); 
	}else{
		memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
		SPITransaction.length = DataLength * 8;
		SPITransaction.tx_buffer = Data;
		ret = spi_device_transmit( SPIHandle, &SPITransaction );
		assert(ret==ESP_OK); 
	}

	return true;
}

bool spi_master_write_command(TFT_t * dev, uint8_t cmd)
{
	static uint8_t Byte = 0;
	Byte = cmd;
	gpio_set_level( dev->_dc, SPI_Command_Mode );
	return spi_master_write_byte( dev->_SPIHandle, &Byte, 1 );
}

bool spi_master_write_data_byte(TFT_t * dev, uint8_t data)
{
	static uint8_t Byte = 0;
	Byte = data;
	gpio_set_level( dev->_dc, SPI_Data_Mode );
	return spi_master_write_byte( dev->_SPIHandle, &Byte, 1 );
}


bool spi_master_write_data_word(TFT_t * dev, uint16_t data)
{
	static uint8_t Byte[2];
	Byte[0] = (data >> 8) & 0xFF;
	Byte[1] = data & 0xFF;
	gpio_set_level( dev->_dc, SPI_Data_Mode );
	return spi_master_write_byte( dev->_SPIHandle, Byte, 2);
}

bool spi_master_write_addr(TFT_t * dev, uint16_t addr1, uint16_t addr2)
{
	static uint8_t Byte[4];
	Byte[0] = (addr1 >> 8) & 0xFF;
	Byte[1] = addr1 & 0xFF;
	Byte[2] = (addr2 >> 8) & 0xFF;
	Byte[3] = addr2 & 0xFF;
	gpio_set_level( dev->_dc, SPI_Data_Mode );
	return spi_master_write_byte( dev->_SPIHandle, Byte, 4);
}

bool spi_master_write_color(TFT_t * dev, uint16_t color, uint16_t size)
{
	static uint8_t Byte[1024];
	int index = 0;
	for(int i=0;i<size;i++) {
		Byte[index++] = (color >> 8) & 0xFF;
		Byte[index++] = color & 0xFF;
	}
	gpio_set_level( dev->_dc, SPI_Data_Mode );
	return spi_master_write_byte( dev->_SPIHandle, Byte, size*2);
}



// Add 202001
bool spi_master_write_colors(TFT_t * dev, uint16_t * colors, uint16_t size)
{
	static uint8_t Byte[1024];
	int index = 0;
	for(int i=0;i<size;i++) {
		Byte[index++] = (colors[i] >> 8) & 0xFF;
		Byte[index++] = colors[i] & 0xFF;
	}
	gpio_set_level( dev->_dc, SPI_Data_Mode );
	return spi_master_write_byte( dev->_SPIHandle, Byte, size*2);
}

void delayMS(int ms) {
	int _ms = ms + (portTICK_PERIOD_MS - 1);
	TickType_t xTicksToDelay = _ms / portTICK_PERIOD_MS;
	ESP_LOGD(TAG, "ms=%d _ms=%d portTICK_PERIOD_MS=%d xTicksToDelay=%d",ms,_ms,portTICK_PERIOD_MS,xTicksToDelay);
	vTaskDelay(xTicksToDelay);
}


void lcdInit(TFT_t * dev, int width, int height)
{
	dev->_width = width;
	dev->_height = height;
	
	// spi_master_write_command(dev, 0x01);	//Software Reset
	// delayMS(10);

	// spi_master_write_command(dev, 0x11);	//Sleep Out
	// delayMS(10);
	
	// spi_master_write_command(dev, 0x3A);	//Interface Pixel Format
	// spi_master_write_data_byte(dev, 0x55);
	// delayMS(10);
	
	// spi_master_write_command(dev, 0x36);	//Memory Data Access Control
	// spi_master_write_data_byte(dev, 0xA0);

	// spi_master_write_command(dev, 0x2A);	//Column Address Set
	// spi_master_write_data_byte(dev, 0x00);
	// spi_master_write_data_byte(dev, 0x00);
	// spi_master_write_data_byte(dev, 0x00);
	// spi_master_write_data_byte(dev, 0xF0);

	// spi_master_write_command(dev, 0x2B);	//Row Address Set
	// spi_master_write_data_byte(dev, 0x00);
	// spi_master_write_data_byte(dev, 0x00);
	// spi_master_write_data_byte(dev, 0x00);
	// spi_master_write_data_byte(dev, 0xF0);
	// delayMS(10);
	// lcdFillScreen(dev, WHITE);
	// delayMS(10);

	// spi_master_write_command(dev, 0x20);	//Display Inversion On
	// delayMS(10);

	// spi_master_write_command(dev, 0x13);	//Normal Display Mode On

	// delayMS(10);
	// spi_master_write_command(dev, 0x29);	//Display ON
	// delayMS(10);

	   spi_master_write_command(dev,0x11); 
	   delayMS(120);				  //Delay 120ms 
   //-----------------------------------ST7789S analog setting------------------------------------// 
   
	   spi_master_write_command(dev,0xD0);//power control 
	   spi_master_write_data_byte(dev,0xa4); 
	   spi_master_write_data_byte(dev,0x81); 
   
	   spi_master_write_command(dev,0x3a);
	   spi_master_write_data_byte(dev,0x05);
	   
	   spi_master_write_command(dev,0xbb); //VCOM
	   spi_master_write_data_byte(dev,0x35); //35 
   //-------------------------------ST7789S gamma setting----------------------------------------// 
	   spi_master_write_command(dev,0xe0); //Positive gamma
	   spi_master_write_data_byte(dev,0xd0); 
	   spi_master_write_data_byte(dev,0x00); 
	   spi_master_write_data_byte(dev,0x02); 
	   spi_master_write_data_byte(dev,0x07); 
	   spi_master_write_data_byte(dev,0x0b); 
	   spi_master_write_data_byte(dev,0x1a); 
	   spi_master_write_data_byte(dev,0x31); 
	   spi_master_write_data_byte(dev,0x54); 
	   spi_master_write_data_byte(dev,0x40); 
	   spi_master_write_data_byte(dev,0x29); 
	   spi_master_write_data_byte(dev,0x12); 
	   spi_master_write_data_byte(dev,0x12); 
	   spi_master_write_data_byte(dev,0x12); 
	   spi_master_write_data_byte(dev,0x17);
	   
	   spi_master_write_command(dev,0xe1); //Negative gamma
	   spi_master_write_data_byte(dev,0xd0); 
	   spi_master_write_data_byte(dev,0x00); 
	   spi_master_write_data_byte(dev,0x02); 
	   spi_master_write_data_byte(dev,0x07); 
	   spi_master_write_data_byte(dev,0x05); 
	   spi_master_write_data_byte(dev,0x25); 
	   spi_master_write_data_byte(dev,0x2d); 
	   spi_master_write_data_byte(dev,0x44); 
	   spi_master_write_data_byte(dev,0x45); 
	   spi_master_write_data_byte(dev,0x1c); 
	   spi_master_write_data_byte(dev,0x18); 
	   spi_master_write_data_byte(dev,0x16); 
	   spi_master_write_data_byte(dev,0x1c); 
	   spi_master_write_data_byte(dev,0x1d); 

	   

		spi_master_write_command(dev, 0x36);	//Memory Data Access Control
		spi_master_write_data_byte(dev, 0xA0);
   //----------------------------------------------------------------------------------------------------// 
	   spi_master_write_command(dev,0x29); 
	   spi_master_write_command(dev,0x2c); 
	   spi_master_write_command(dev,0x21); //反色
	
}


// Draw pixel
// x:X coordinate
// y:Y coordinate
// color:color
void lcdDrawPixel(TFT_t * dev, uint16_t x, uint16_t y, uint16_t color){

	spi_master_write_command(dev, 0x2A);	// set column(x) address
	spi_master_write_addr(dev, x, x);
	spi_master_write_command(dev, 0x2B);	// set Page(y) address
	spi_master_write_addr(dev, y, y);
	spi_master_write_command(dev, 0x2C);	//	Memory Write
	spi_master_write_data_word(dev, color);
}


// Draw multi pixel
// x:X coordinate
// y:Y coordinate
// size:Number of colors
// colors:colors
void lcdDrawMultiPixels(TFT_t * dev, uint16_t x, uint16_t y, uint16_t size, uint16_t * colors) {

	spi_master_write_command(dev, 0x2A);	// set column(x) address
	spi_master_write_addr(dev, x, (x+size));
	spi_master_write_command(dev, 0x2B);	// set Page(y) address
	spi_master_write_addr(dev, y, y);
	spi_master_write_command(dev, 0x2C);	//	Memory Write
	spi_master_write_colors(dev, colors, size);
}

// Draw rectangle of filling
// x1:Start X coordinate
// y1:Start Y coordinate
// x2:End X coordinate
// y2:End Y coordinate
// color:color
//填充色块
void lcdDrawFillRect(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {

	if (x1 >= dev->_width) return;
	if (x2 >= dev->_width) x2=dev->_width-1;
	if (y1 >= dev->_height) return;
	if (y2 >= dev->_height) y2=dev->_height-1;
	//ESP_LOGD(TAG,"offset(x)=%d offset(y)=%d",dev->_offsetx,dev->_offsety);

	spi_master_write_command(dev, 0x2A);	// set column(x) address
	spi_master_write_addr(dev, x1, x2);
	spi_master_write_command(dev, 0x2B);	// set Page(y) address
	spi_master_write_addr(dev, y1, y2);
	spi_master_write_command(dev, 0x2C);	//	Memory Write
	for(int i=x1;i<=x2;i++){
		uint16_t size = y2-y1+1;
		spi_master_write_color(dev, color, size);
	}
}

// Display OFF
void lcdDisplayOff(TFT_t * dev) {
	spi_master_write_command(dev, 0x28);	//Display off
}
 
// Display ON
void lcdDisplayOn(TFT_t * dev) {
	spi_master_write_command(dev, 0x29);	//Display on
}

// Fill screen
// color:color
void lcdFillScreen(TFT_t * dev, uint16_t color) {
	lcdDrawFillRect(dev, 0, 0, dev->_width-1, dev->_height-1, color);
}

// Draw line
// x1:Start X coordinate
// y1:Start Y coordinate
// x2:End X coordinate
// y2:End Y coordinate
// color:color 
//画线
void lcdDrawLine(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
	int i;
	int dx,dy;
	int sx,sy;
	int E;

	/* distance between two points */
	dx = ( x2 > x1 ) ? x2 - x1 : x1 - x2;
	dy = ( y2 > y1 ) ? y2 - y1 : y1 - y2;

	/* direction of two point */
	sx = ( x2 > x1 ) ? 1 : -1;
	sy = ( y2 > y1 ) ? 1 : -1;

	/* inclination < 1 */
	if ( dx > dy ) {
		E = -dx;
		for ( i = 0 ; i <= dx ; i++ ) {
			lcdDrawPixel(dev, x1, y1, color);
			x1 += sx;
			E += 2 * dy;
			if ( E >= 0 ) {
			y1 += sy;
			E -= 2 * dx;
		}
	}

	/* inclination >= 1 */
	} else {
		E = -dy;
		for ( i = 0 ; i <= dy ; i++ ) {
			lcdDrawPixel(dev, x1, y1, color);
			y1 += sy;
			E += 2 * dx;
			if ( E >= 0 ) {
				x1 += sx;
				E -= 2 * dy;
			}
		}
	}
}

// Draw rectangle
// x1:Start X coordinate
// y1:Start Y coordinate
// x2:End	X coordinate
// y2:End	Y coordinate
// color:color
//画矩形
void lcdDrawRect(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
	lcdDrawLine(dev, x1, y1, x2, y1, color);
	lcdDrawLine(dev, x2, y1, x2, y2, color);
	lcdDrawLine(dev, x2, y2, x1, y2, color);
	lcdDrawLine(dev, x1, y2, x1, y1, color);
}

// Draw rectangle with angle
// xc:Center X coordinate
// yc:Center Y coordinate
// w:Width of rectangle
// h:Height of rectangle
// angle :Angle of rectangle
// color :color

//When the origin is (0, 0), the point (x1, y1) after rotating the point (x, y) by the angle is obtained by the following calculation.
// x1 = x * cos(angle) - y * sin(angle)
// y1 = x * sin(angle) + y * cos(angle)
//画带有旋转角度的矩形 lcdDrawRectAngle(&dev,100,100,60,20,15,BLUE);
void lcdDrawRectAngle(TFT_t * dev, uint16_t xc, uint16_t yc, uint16_t w, uint16_t h, uint16_t angle, uint16_t color) {
	double xd,yd,rd;
	int x1,y1;
	int x2,y2;
	int x3,y3;
	int x4,y4;
	rd = -angle * M_PI / 180.0;
	xd = 0.0 - w/2;
	yd = h/2;
	x1 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
	y1 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

	yd = 0.0 - yd;
	x2 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
	y2 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

	xd = w/2;
	yd = h/2;
	x3 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
	y3 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

	yd = 0.0 - yd;
	x4 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
	y4 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

	lcdDrawLine(dev, x1, y1, x2, y2, color);
	lcdDrawLine(dev, x1, y1, x3, y3, color);
	lcdDrawLine(dev, x2, y2, x4, y4, color);
	lcdDrawLine(dev, x3, y3, x4, y4, color);
}

// Draw triangle
// xc:Center X coordinate
// yc:Center Y coordinate
// w:Width of triangle
// h:Height of triangle
// angle :Angle of triangle
// color :color

//When the origin is (0, 0), the point (x1, y1) after rotating the point (x, y) by the angle is obtained by the following calculation.
// x1 = x * cos(angle) - y * sin(angle)
// y1 = x * sin(angle) + y * cos(angle)
//画带有旋转角度的三角形 lcdDrawTriangle(&dev,100,100,60,20,15,BLUE);
void lcdDrawTriangle(TFT_t * dev, uint16_t xc, uint16_t yc, uint16_t w, uint16_t h, uint16_t angle, uint16_t color) {
	double xd,yd,rd;
	int x1,y1;
	int x2,y2;
	int x3,y3;
	rd = -angle * M_PI / 180.0;
	xd = 0.0;
	yd = h/2;
	x1 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
	y1 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

	xd = w/2;
	yd = 0.0 - yd;
	x2 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
	y2 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

	xd = 0.0 - w/2;
	x3 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
	y3 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

	lcdDrawLine(dev, x1, y1, x2, y2, color);
	lcdDrawLine(dev, x1, y1, x3, y3, color);
	lcdDrawLine(dev, x2, y2, x3, y3, color);
}

// Draw circle
// x0:Central X coordinate
// y0:Central Y coordinate
// r:radius
// color:color
//画空心圆形
void lcdDrawCircle(TFT_t * dev, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
	int x;
	int y;
	int err;
	int old_err;

	x=0;
	y=-r;
	err=2-2*r;
	do{
		lcdDrawPixel(dev, x0-x, y0+y, color); 
		lcdDrawPixel(dev, x0-y, y0-x, color); 
		lcdDrawPixel(dev, x0+x, y0-y, color); 
		lcdDrawPixel(dev, x0+y, y0+x, color); 
		if ((old_err=err)<=x)	err+=++x*2+1;
		if (old_err>y || err>x) err+=++y*2+1;	 
	} while(y<0);
}

// Draw circle of filling
// x0:Central X coordinate
// y0:Central Y coordinate
// r:radius
// color:color
//画填充圆形
void lcdDrawFillCircle(TFT_t * dev, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
	int x;
	int y;
	int err;
	int old_err;
	int ChangeX;

	x=0;
	y=-r;
	err=2-2*r;
	ChangeX=1;
	do{
		if(ChangeX) {
			lcdDrawLine(dev, x0-x, y0-y, x0-x, y0+y, color);
			lcdDrawLine(dev, x0+x, y0-y, x0+x, y0+y, color);
		} // endif
		ChangeX=(old_err=err)<=x;
		if (ChangeX)			err+=++x*2+1;
		if (old_err>y || err>x) err+=++y*2+1;
	} while(y<=0);
} 

// Draw rectangle with round corner
// x1:Start X coordinate
// y1:Start Y coordinate
// x2:End	X coordinate
// y2:End	Y coordinate
// r:radius
// color:color
//画圆角矩形
void lcdDrawRoundRect(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t r, uint16_t color) {
	int x;
	int y;
	int err;
	int old_err;
	unsigned char temp;

	if(x1>x2) {
		temp=x1; x1=x2; x2=temp;
	} // endif
	  
	if(y1>y2) {
		temp=y1; y1=y2; y2=temp;
	} // endif

	ESP_LOGD(TAG, "x1=%d x2=%d delta=%d r=%d",x1, x2, x2-x1, r);
	ESP_LOGD(TAG, "y1=%d y2=%d delta=%d r=%d",y1, y2, y2-y1, r);
	if (x2-x1 < r) return; // Add 20190517
	if (y2-y1 < r) return; // Add 20190517

	x=0;
	y=-r;
	err=2-2*r;

	do{
		if(x) {
			lcdDrawPixel(dev, x1+r-x, y1+r+y, color); 
			lcdDrawPixel(dev, x2-r+x, y1+r+y, color); 
			lcdDrawPixel(dev, x1+r-x, y2-r-y, color); 
			lcdDrawPixel(dev, x2-r+x, y2-r-y, color);
		} // endif 
		if ((old_err=err)<=x)	err+=++x*2+1;
		if (old_err>y || err>x) err+=++y*2+1;	 
	} while(y<0);

	ESP_LOGD(TAG, "x1+r=%d x2-r=%d",x1+r, x2-r);
	lcdDrawLine(dev, x1+r,y1  ,x2-r,y1	,color);
	lcdDrawLine(dev, x1+r,y2  ,x2-r,y2	,color);
	ESP_LOGD(TAG, "y1+r=%d y2-r=%d",y1+r, y2-r);
	lcdDrawLine(dev, x1  ,y1+r,x1  ,y2-r,color);
	lcdDrawLine(dev, x2  ,y1+r,x2  ,y2-r,color);  
} 


// RGB565 conversion
// RGB565 is R(5)+G(6)+B(5)=16bit color format.
// Bit image "RRRRRGGGGGGBBBBB"
uint16_t rgb565_conv(uint16_t r,uint16_t g,uint16_t b) {
	return (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}


/******************************************************************************
      函数说明：显示图片
      入口数据：x,y起点坐标
                length 图片长度
                width  图片宽度
                pic[]  图片数组    
      返回值：  无
******************************************************************************/
void LCD_ShowPicture(TFT_t * dev, uint16_t x, uint16_t y ,uint16_t length, uint16_t width,const uint8_t pic[])
{

	spi_master_write_command(dev, 0x2A);	// set column(x) address
	spi_master_write_addr(dev, x, (x + length-1));
	spi_master_write_command(dev, 0x2B);	// set Page(y) address
	spi_master_write_addr(dev, y, (y + width-1));
	spi_master_write_command(dev, 0x2C);	//	Memory Write

    gpio_set_level( dev->_dc, SPI_Data_Mode );
	spi_master_write_byte( dev->_SPIHandle, pic, length*width*2);		
}


void LCD_ASCII_8X16(TFT_t * dev, uint16_t x, uint16_t y ,char ch,uint16_t textColor,uint16_t BackColor)
{
	uint8_t width=8;
	uint8_t height=16;


	uint16_t i,j;
	unsigned char *zf=&ASCII_8X16[(ch-0x20)*(16)];//字模数组中第一个空格是0x20，减0x20得到偏移乘以每个字的字节数
	unsigned char cache[256];
	uint8_t textColor_H=textColor>>8;
	uint8_t textColor_L=textColor&0xFF;
	uint8_t BackColor_H=BackColor>>8;
	uint8_t BackColor_L=BackColor&0xFF;

	j=0;
	for(i=0;i<16;i++)
	{
		if((zf[i]&0x80)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x40)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x20)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x10)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x08)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x04)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x02)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x01)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
	}
	spi_master_write_command(dev, 0x2A);	// set column(x) address
	spi_master_write_addr(dev, x, (x + width-1));
	spi_master_write_command(dev, 0x2B);	// set Page(y) address
	spi_master_write_addr(dev, y, (y + height-1));
	spi_master_write_command(dev, 0x2C);	//	Memory Write
    gpio_set_level( dev->_dc, SPI_Data_Mode );
	spi_master_write_byte( dev->_SPIHandle, cache, 256);		
}


void LCD_ASCII_8X16_str(TFT_t * dev, uint16_t x, uint16_t y ,char *ch,uint8_t len,uint16_t textColor,uint16_t BackColor)
{
	uint8_t i=0;
	for(i=0;i<len;i++)
	{
		LCD_ASCII_8X16(dev,x+(i*8),y,ch[i],textColor,BackColor);
	}	
}

void LCD_ASCII_16X24(TFT_t * dev, uint16_t x, uint16_t y ,char ch,uint16_t textColor,uint16_t BackColor)
{
	uint8_t width=16;
	uint8_t height=24;

	uint16_t i,j;
	unsigned char *zf=&ASCII_16X24[(ch-0x20)*(width*height/8)];//字模数组中第一个空格是0x20，减0x20得到偏移乘以每个字的字节数
	unsigned char cache[width*height*2];
	uint8_t textColor_H=textColor>>8;
	uint8_t textColor_L=textColor&0xFF;
	uint8_t BackColor_H=BackColor>>8;
	uint8_t BackColor_L=BackColor&0xFF;

	j=0;
	for(i=0;i<(width*height/8);i++)
	{
		if((zf[i]&0x80)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x40)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x20)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x10)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x08)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x04)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x02)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x01)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
	}
	spi_master_write_command(dev, 0x2A);	// set column(x) address
	spi_master_write_addr(dev, x, (x + width-1));
	spi_master_write_command(dev, 0x2B);	// set Page(y) address
	spi_master_write_addr(dev, y, (y + height-1));
	spi_master_write_command(dev, 0x2C);	//	Memory Write
    gpio_set_level( dev->_dc, SPI_Data_Mode );
	spi_master_write_byte( dev->_SPIHandle, cache, width*height*2);			
}

void LCD_ASCII_24X30(TFT_t * dev, uint16_t x, uint16_t y ,char num,uint16_t textColor,uint16_t BackColor)
{
	uint8_t width=24;
	uint8_t height=30;

	uint16_t i,j;
	unsigned char *zf=&ASCII_24X30[num*(width*height/8)];//字模数组中第一个空格是0x20，减0x20得到偏移乘以每个字的字节数
	unsigned char cache[width*height*2];
	uint8_t textColor_H=textColor>>8;
	uint8_t textColor_L=textColor&0xFF;
	uint8_t BackColor_H=BackColor>>8;
	uint8_t BackColor_L=BackColor&0xFF;

	j=0;
	for(i=0;i<(width*height/8);i++)
	{
		if((zf[i]&0x80)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x40)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x20)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x10)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x08)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x04)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x02)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x01)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
	}
	spi_master_write_command(dev, 0x2A);	// set column(x) address
	spi_master_write_addr(dev, x, (x + width-1));
	spi_master_write_command(dev, 0x2B);	// set Page(y) address
	spi_master_write_addr(dev, y, (y + height-1));
	spi_master_write_command(dev, 0x2C);	//	Memory Write
    gpio_set_level( dev->_dc, SPI_Data_Mode );
	spi_master_write_byte( dev->_SPIHandle, cache, width*height*2);			
}

void LCD_ASCII_24X30_str(TFT_t * dev, uint16_t x, uint16_t y ,char *ch,uint8_t len,uint16_t textColor,uint16_t BackColor)
{
	uint8_t i=0;
	uint8_t x1=0;
	uint8_t num=0;

	for(i=0;i<len;i++)
	{
		if(ch[i]=='.')
		{
			num=10;
		}
		else if(ch[i]=='-')
		{
			num=11;
		}
		else if(ch[i]==' ')
		{
			num=12;
		}
		else
		{
			num=ch[i]-'0';
		}

		LCD_ASCII_24X30(dev,x+x1,y,num,textColor,BackColor);

		if(ch[i]=='.')//小数点只占正常字符的一半宽度
		{
			x1+=12;
		}
		else
		{
			x1+=24;
		}
		
	}	
}

void LCD_monochrome_24X24(TFT_t * dev, uint16_t x, uint16_t y ,const unsigned char *zf,uint16_t textColor,uint16_t BackColor)
{
	uint8_t width=24;
	uint8_t height=24;
	uint16_t i,j;
	unsigned char cache[width*height*2];
	uint8_t textColor_H=textColor>>8;
	uint8_t textColor_L=textColor&0xFF;
	uint8_t BackColor_H=BackColor>>8;
	uint8_t BackColor_L=BackColor&0xFF;

	j=0;
	for(i=0;i<(width*height/8);i++)
	{
		if((zf[i]&0x80)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x40)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x20)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x10)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x08)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x04)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x02)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x01)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
	}
	spi_master_write_command(dev, 0x2A);	// set column(x) address
	spi_master_write_addr(dev, x, (x + width-1));
	spi_master_write_command(dev, 0x2B);	// set Page(y) address
	spi_master_write_addr(dev, y, (y + height-1));
	spi_master_write_command(dev, 0x2C);	//	Memory Write
    gpio_set_level( dev->_dc, SPI_Data_Mode );
	spi_master_write_byte( dev->_SPIHandle, cache, width*height*2);			
}

void LCD_monochrome_32X32(TFT_t * dev, uint16_t x, uint16_t y ,const unsigned char *zf,uint16_t textColor,uint16_t BackColor)
{
	uint8_t width=32;
	uint8_t height=32;
	uint16_t i,j;
	unsigned char cache[width*height*2];
	uint8_t textColor_H=textColor>>8;
	uint8_t textColor_L=textColor&0xFF;
	uint8_t BackColor_H=BackColor>>8;
	uint8_t BackColor_L=BackColor&0xFF;

	j=0;
	for(i=0;i<(width*height/8);i++)
	{
		if((zf[i]&0x80)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x40)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x20)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x10)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x08)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x04)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x02)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x01)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
	}
	spi_master_write_command(dev, 0x2A);	// set column(x) address
	spi_master_write_addr(dev, x, (x + width-1));
	spi_master_write_command(dev, 0x2B);	// set Page(y) address
	spi_master_write_addr(dev, y, (y + height-1));
	spi_master_write_command(dev, 0x2C);	//	Memory Write
    gpio_set_level( dev->_dc, SPI_Data_Mode );
	spi_master_write_byte( dev->_SPIHandle, cache, width*height*2);			
}

//任意尺寸的单色位图
void LCD_monochrome_any(TFT_t * dev, uint16_t x, uint16_t y ,const unsigned char *zf,uint16_t width, uint16_t height ,uint16_t textColor,uint16_t BackColor)
{

	uint16_t i,j;
	unsigned char cache[width*height*2];
	uint8_t textColor_H=textColor>>8;
	uint8_t textColor_L=textColor&0xFF;
	uint8_t BackColor_H=BackColor>>8;
	uint8_t BackColor_L=BackColor&0xFF;

	j=0;
	for(i=0;i<(width*height/8);i++)
	{
		if((zf[i]&0x80)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x40)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x20)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x10)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x08)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x04)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x02)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
		if((zf[i]&0x01)!=0) 
		{
			cache[j++]=textColor_H;
			cache[j++]=textColor_L;	
		}
		else
		{
			cache[j++]=BackColor_H;
			cache[j++]=BackColor_L;
		}
	}
	spi_master_write_command(dev, 0x2A);	// set column(x) address
	spi_master_write_addr(dev, x, (x + width-1));
	spi_master_write_command(dev, 0x2B);	// set Page(y) address
	spi_master_write_addr(dev, y, (y + height-1));
	spi_master_write_command(dev, 0x2C);	//	Memory Write
    gpio_set_level( dev->_dc, SPI_Data_Mode );
	spi_master_write_byte( dev->_SPIHandle, cache, width*height*2);			
}





