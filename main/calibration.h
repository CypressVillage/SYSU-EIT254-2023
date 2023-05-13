#ifndef _CAL_H
#define _CAL_H

extern uint8_t  Zero_b;  //开始校零
extern uint8_t  Slope_b; //开始校准斜率

extern int   Zero_dcv_data[3];//校零数值
extern float Slope_dcv_data[3];//斜率校准数值

extern int   Zero_acv_data[3];//校零数值

extern int   Zero_dca_data[3];//校零数值
extern float Slope_dca_data[3];//斜率校准数值

extern int   Zero_aca_data[2];//校零数值

extern int   Zero_2wr_data[4];//校零数值
extern float Current_2wr_data[4];//电阻档恒流源校准数值

uint32_t calibration_dcv(float voltage,uint8_t n,uint32_t v);
uint32_t calibration_acv(float voltage,uint8_t n);
uint32_t calibration_dca(float voltage,uint8_t n,uint32_t v);
uint32_t calibration_aca(float voltage,uint8_t n);
float calibration_2wr(float voltage,uint8_t n,float r);
float calibration_4wr(float voltage);
#endif
