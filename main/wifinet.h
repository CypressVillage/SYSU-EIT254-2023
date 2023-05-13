#ifndef WIFINET_H
#define WIFINET_H

extern esp_timer_handle_t config_router_timer_handle ;
extern esp_timer_create_args_t config_router_periodic_arg;

extern xQueueHandle wifinet_evt_queue;

extern uint8_t progress;

extern char ls_wifi_ssid[32];
extern char ls_wifi_pass[32];


extern char wifi_ssid[32];
extern char wifi_pass[32];

extern char device_ID[11];
extern char mqtt_password[33];


/* 配网步骤 */
enum progress
{
    PROG_STOP = 0, //未开始配网
    PROG_START,    //开始接收起始信号
    PROG_DATA,     //开始接收数据
    PROG_OVER,     //接收完成
    PROG_ERROR,    //接收错误
    PROG_CONNECTED, //连接路由器
    PROG_CONNECTED_OK, //连接路由器成功
    PROG_CONNECTED_ERR, //连接路由器失败
};

enum WIFINET
{
    WIFINET_INFO = 0, //发送万用表信息
    WIFINET_MVOM, //发送万用表测量值
    WIFINET_UART,      //发送串口数据
    WIFINET_UART_CONFIG,//发送串口配置
    WIFINET_CAN,     //发送CAN数据
    WIFINET_CAN_CONFIG,//发送CAN配置
    WIFINET_SIGNAL_CONFIG,//发送直流源配置
    WIFINET_PWM_CONFIG,//发送PWM配置
    WIFINET_MQTTSTOP,//断开MQTT连接
    WIFINET_MARK,    //万用表标记测量值
    WIFINET_OTA,     //固件升级
    WIFINET_OTA_PRO, //固件进度
};

void wifi_connecting_routers(void);
void wifinet_task(void *arg);
void start_config_router();
void app_wifi_initialise(void);

#endif