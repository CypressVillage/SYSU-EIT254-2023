#include "main.h"
#include "calibration.h"

const char *CAL = "calibration";

uint8_t  Zero_b=0;  //开始校零
uint8_t  Slope_b=0; //开始校准斜率

int   Zero_dcv_data[3]={0,0,0};//校零数值
float Slope_dcv_data[3]={1,1,1};//斜率校准数值

int   Zero_acv_data[3]={0,0,0};//校零数值

int   Zero_dca_data[3]={0,0,0};//校零数值
float Slope_dca_data[3]={1,1,1};//斜率校准数值

int   Zero_aca_data[2]={0,0};//校零数值

int Zero_2wr_data[4]={0,0,0,0};//校零数值
float Current_2wr_data[4]={497.84,1,1,1};//电阻档恒流源校准数值


uint32_t calibration_dcv(float voltage,uint8_t n,uint32_t v)
{
    uint32_t Vol;
    if(Zero_b==1)
    {
        Zero_b=0;
        Zero_dcv_data[n]=voltage;
        write_config_in_nvs(CONFIG_CAL); 
        ESP_LOGI(CAL,"Zero_dcv_data[%d]=%d\r\n",n,Zero_dcv_data[n]);
    }
    voltage-=Zero_dcv_data[n];
    Vol= abs(voltage);  //求绝对值
    if(Slope_b!=0)
    { 
        Slope_b=0;
        Slope_dcv_data[n]=(float)v/Vol; 
        write_config_in_nvs(CONFIG_CAL); 
        ESP_LOGI(CAL,"Slope_dcv_data[%d]=%f\r\n",n,Slope_dcv_data[n]);  
    }
    Vol*=Slope_dcv_data[n];
    return Vol;
}

uint32_t calibration_acv(float voltage,uint8_t n)
{
    uint32_t Vol;
    ESP_LOGI(CAL,"voltage=%f\r\n",voltage);  
    if(Zero_b==1)
    {
        Zero_b=0;
        Zero_acv_data[n]=voltage;
        write_config_in_nvs(CONFIG_CAL); 
        ESP_LOGI(CAL,"Zero_acv_data[%d]=%d\r\n",n,Zero_acv_data[n]);
    }
    Vol= abs(voltage);  //求绝对值
    Vol*=Slope_dcv_data[n];//先用直流档校准分压电阻
    Vol*=1.25;             //然后校准小信号整流电路
    return Vol;
}

uint32_t calibration_dca(float voltage,uint8_t n,uint32_t v)
{
    uint32_t Vol;
    if(Zero_b==1)
    {
        Zero_b=0;
        Zero_dca_data[n]=voltage;
        write_config_in_nvs(CONFIG_CAL); 
        ESP_LOGI(CAL,"Zero_dca_data[%d]=%d\r\n",n,Zero_dca_data[n]);
    }
    voltage-=Zero_dca_data[n];
    Vol= abs(voltage);  //求绝对值
    if(Slope_b!=0)
    { 
        Slope_b=0;
        Slope_dca_data[n]=(float)v/Vol;
        write_config_in_nvs(CONFIG_CAL);  
        ESP_LOGI(CAL,"Slope_dca_data[%d]=%f\r\n",n,Slope_dca_data[n]);  
    }
    Vol*=Slope_dca_data[n];
    return Vol;
}

uint32_t calibration_aca(float voltage,uint8_t n)
{
    uint32_t Vol;
    if(Zero_b==1)
    {
        Zero_b=0;
        Zero_aca_data[n]=voltage;
        write_config_in_nvs(CONFIG_CAL); 
        ESP_LOGI(CAL,"Zero_aca_data[%d]=%d\r\n",n,Zero_aca_data[n]);
    }
    voltage-=Zero_aca_data[n];
    Vol= abs(voltage);  //求绝对值
    Vol*=Slope_dca_data[n+1];//先用直流档校准采样电阻
    Vol*=1.25;               //然后校准小信号整流电路
    return Vol;
}


// 标定方法：
// 1、先将10V电压档调零
// 2、以1K档举例，用1K标准电阻进行标定，计算得出1K并联10MΩ是999.9Ω ，根据测量的电压除以这个电阻，即可得到恒流源的电流值
// n为档位，r为标准电阻并联10M后的阻值
float calibration_2wr(float voltage,uint8_t n,float r)
{
    float resistance=0;
    voltage-=Zero_dcv_data[2]; //电阻档使用的是直流电压10V量程，先调零
    if(Slope_b!=0)
    { 
        Slope_b=0;
        Current_2wr_data[n]=voltage/r; 
        write_config_in_nvs(CONFIG_CAL); 
        ESP_LOGI(CAL,"Current_2wr_data[%d]=%f\r\n",n,Current_2wr_data[n]);  
    }
    resistance= (voltage)/Current_2wr_data[n];//电压除以电流 得到被测电阻

    if(Zero_b==1)
    {
        Zero_b=0;
        Zero_2wr_data[n]=(resistance*1000);
        write_config_in_nvs(CONFIG_CAL); 
        ESP_LOGI(CAL,"Zero_2wr_data[%d]=%d\r\n",n,Zero_2wr_data[n]);
    }
    resistance-=(Zero_2wr_data[n]/1000.0);

    if(resistance<0) resistance=0;
    return resistance;
}

float calibration_4wr(float voltage)
{
    float resistance=0;
    ESP_LOGI(CAL,"voltage=%f\r\n",voltage);
    resistance= voltage/(Current_2wr_data[0]*10);//电压除以电流 得到被测电阻
    return resistance;
    
}


