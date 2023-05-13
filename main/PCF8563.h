#ifndef _PCF8563_H_
#define _PCF8563_H_


#define ReadCode 0xa3
#define WriteCode 0xa2
void PCF8563_Init(void);
void PCF8563_Init_Time(uint8_t *p);
void PCF8563_Init_Day(uint8_t *p);
uint8_t PCF8563_ReaDAdress(uint8_t Adress);
void PCF8563_WriteAdress(uint8_t Adress, uint8_t DataTX);

extern uint8_t PCF8563_Time[7];
extern uint8_t VL_value;
void PCF8563_ReadTimes(void);

void PCF8563_CLKOUT_1s(void);
#endif
