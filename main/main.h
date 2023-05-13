#ifndef MAIN_H
#define MAIN_H
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "esp_private/system_internal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "mqtt_client.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "driver/rtc_io.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "st7789.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/twai.h" // Update from V4.2
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <driver/spi_master.h>
#include "mbedtls/md5.h"
#include "sdmmc_cmd.h"
#include "esp_sntp.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/timer.h"
#include "driver/ledc.h"
#include "adc_dac.h"
#include "usart_485.h"
#include "canbus.h"
#include "lcd_ui.h"
#include "micro_sd.h"
#include "wifinet.h"
#include "switch_fun.h"
#include "key.h"
#include "temp.h"
#include "calibration.h"
#include "qr_encode.h"
#include "PCF8563.h"
#include "ota.h"



#define I2C_SDA 38
#define I2C_SCL 37
 
#define PIN_NUM_MISO		8
#define PIN_NUM_MOSI		10
#define PIN_NUM_CLK			9
#define PIN_NUM_CS			11

#define CONFIG_MOSI_GPIO 39
#define CONFIG_SCLK_GPIO 42
#define CONFIG_CS_GPIO 40
#define CONFIG_DC_GPIO 41
#define CONFIG_BL_GPIO 45


#define ADC_SCLK 2
#define ADC_CS 4
#define ADC_DOUT 3
#define ADC_DIN  5

#define TXD_PIN 33
#define RXD_PIN 34

#define RS485T_PIN 48
#define RS485R_PIN 47

#define CANT_PIN 35
#define CANR_PIN 36
#define CANVPK_PIN 46

#define BEEP 7

#define SEN 12

#define V16_EN 21

#define PWR_EN 16

#define PWR_KEY 17
#define KEY1 0
#define KEY2 1
#define CHRG 6
#define SWA 14
#define SWB 15
#define SWK 13



/* 参数存储 */
enum config_write
{
    CONFIG_CAL=0, //标定值
    CONFIG_FUN,   //档位
    CONFIG_OTHER, //其他参数
};

extern char ver[2];//固件版本
extern char QRcode[100];
extern char equipment_type[2]; //设备类型
extern char signal_st; //信号：范围0-5 ，为5信号最强，为0没信号
extern char net_type; //网络类型：1-WIFI，2-4G，3-NBIOT


void set_pwnch1(uint16_t duty,int time_ms);
void set_pwnch2(uint16_t duty,int time_ms);
void pwm_ledc_init(void);
void pwm_ledc_off(void);
void write_config_in_nvs(uint8_t config);

extern uint16_t pwm_freq;// 两路PWM信号的频率，1-9000
extern uint8_t pwm_duty1;  // 第一路占空比 0-100
extern uint8_t pwm_duty2;  // 第二路占空比 0-100

extern uint16_t servo_type; //舵机类型
extern uint8_t servo_angle1;  // 第一路舵机角度
extern uint8_t servo_angle2;  // 第二路舵机角度

#endif