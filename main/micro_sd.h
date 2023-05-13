#ifndef _SD_H
#define _SD_H

extern uint8_t sd_onoff;     //存储开关
extern uint8_t sd_state;	  // sd卡是否已经挂载
extern uint8_t time_state;	  // RTC是否正常
extern uint32_t bytes_total; //全部容量 
extern uint32_t bytes_free;  //剩余容量
extern xQueueHandle sd_evt_queue; 

enum SD
{
    SD_SWFUN = 0, //万用表切换了档位
    SD_RECEIVE_ADC,   //收到测量值
    SD_RECEIVE_PORT,  //收到串口数据
    SD_RECEIVE_CAN,   //收到CAN数据
};



void micro_sd_task(void *arg);
#endif
