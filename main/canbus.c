#include "main.h"
#include "canbus.h"

const char *CANBUS = "CAN_TASK";

TaskHandle_t CAN_Handle_task = NULL;

static twai_timing_config_t t_config = TWAI_TIMING_CONFIG_50KBITS();


uint8_t can_close=0;

uint8_t can_baud=TWAI_50KBITS;
uint8_t can_stop=0;    //暂停/继续
uint8_t can_filter=0; //过滤开关
uint8_t can_extd=1;   //过滤标准帧/扩展帧
uint8_t can_rtr=0;    //过滤数据帧/远程帧
uint32_t can_filter_id=0x123;
uint32_t can_filter_mask=0x1FFFFFFF;

uint32_t can_rx_len=0;//接收的包数
uint32_t can_tx_len=0;//发送的包数

CAN_DATA can_rx_data;

const twai_timing_config_t t_config_16KBITS  =      TWAI_TIMING_CONFIG_16KBITS();
const twai_timing_config_t t_config_20KBITS  =      TWAI_TIMING_CONFIG_20KBITS();
const twai_timing_config_t t_config_25KBITS  =      TWAI_TIMING_CONFIG_25KBITS();
const twai_timing_config_t t_config_50KBITS  =      TWAI_TIMING_CONFIG_50KBITS();
const twai_timing_config_t t_config_100KBITS  =     TWAI_TIMING_CONFIG_100KBITS();
const twai_timing_config_t t_config_125KBITS  =     TWAI_TIMING_CONFIG_125KBITS();
const twai_timing_config_t t_config_250KBITS  =     TWAI_TIMING_CONFIG_250KBITS();
const twai_timing_config_t t_config_500KBITS  =     TWAI_TIMING_CONFIG_500KBITS();
const twai_timing_config_t t_config_800KBITS  =     TWAI_TIMING_CONFIG_800KBITS();
const twai_timing_config_t t_config_1MBITS  =       TWAI_TIMING_CONFIG_1MBITS();

twai_filter_config_t f_config;

// //Set TX queue length to 0 due to listen only mode
 static const twai_general_config_t g_config = {.mode = TWAI_MODE_NORMAL,
                                               .tx_io = CANT_PIN, .rx_io = CANR_PIN,
                                               .clkout_io = TWAI_IO_UNUSED, .bus_off_io = TWAI_IO_UNUSED,
                                               .tx_queue_len = 2, .rx_queue_len = 32,
                                               .alerts_enabled = TWAI_ALERT_NONE,
                                               .clkout_divider = 0};

//发送CAN数据
void twai_tx_data(uint32_t identifier,uint8_t extd,uint8_t rtr,uint8_t *data)
{
   uint8_t evt=0;
   twai_message_t twaiBuf;
   twaiBuf.identifier = identifier;
   twaiBuf.extd = extd;  //扩展帧为1
   twaiBuf.rtr = rtr;   //远程帧为1
   twaiBuf.ss = 1;    //错误或者仲裁丢失时，消息不会重发
   twaiBuf.self = 0;  //自收发
   twaiBuf.dlc_non_comp = 0; //消息长度大于8，将不遵守标准协议
   twaiBuf.data_length_code = 8;
   memcpy(twaiBuf.data,data,8);
   can_tx_len+=1;
   evt=REFRESH_VALUE;
   xQueueSendFromISR(gui_evt_queue, &evt, NULL);

   esp_err_t ret = twai_transmit(&twaiBuf, pdMS_TO_TICKS(1000));

   if (ret == ESP_OK) {
      ESP_LOGI(CANBUS, "twai_transmit success");
   } else {

      ESP_LOGE(CANBUS, "twai_transmit Fail %s", esp_err_to_name(ret));
   }
}


void twai_set_config()
{
   uint8_t err=0;
   ESP_LOGI(CANBUS, "twai_set_config");
   
   err=0;
   while(twai_stop()!=ESP_OK)
   {
      vTaskDelay(pdMS_TO_TICKS(100));
      if(++err>10) break;
   }
   err=0;
   while(twai_driver_uninstall()!=ESP_OK)
   {
      vTaskDelay(pdMS_TO_TICKS(100));
      if(++err>10) break;
   }

   ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
   ESP_ERROR_CHECK(twai_start());
}

//设置波特率
void twai_set_baud(uint8_t baud)
{
   can_baud=baud;
   switch (baud)
   {
   case TWAI_16KBITS:
      t_config=t_config_16KBITS;
      break;
   case TWAI_20KBITS:
      t_config=t_config_20KBITS;
      break;
   case TWAI_25KBITS:
      t_config=t_config_25KBITS;
      break;
   case TWAI_50KBITS:
      t_config=t_config_50KBITS;
      break;
   case TWAI_100KBITS:
      t_config=t_config_100KBITS;
      break;
   case TWAI_125KBITS:
      t_config=t_config_125KBITS;
      break;
   case TWAI_250KBITS:
      t_config=t_config_250KBITS;
      break;
   case TWAI_500KBITS:
      t_config=t_config_500KBITS;
      break;
   case TWAI_800KBITS:
      t_config=t_config_800KBITS;
      break;
   case TWAI_1MBITS:
      t_config=t_config_1MBITS;
      break;
   
   default:
      break;
   }
   twai_set_config();
}

//设置过滤器
void twai_set_filter(uint8_t Vfilter_on,uint8_t Vextd,uint32_t Vcode,uint32_t Vmask)
{

can_filter=Vfilter_on; //过滤开关
can_extd=Vextd;   //过滤标准帧/扩展帧
can_rtr=0;    //过滤数据帧/远程帧
can_filter_id=Vcode;
can_filter_mask=Vmask;

// 标准帧
// f_config.acceptance_code = (0x100 << 21);
// f_config.acceptance_mask = ~(0x1FF << 21);
//扩展帧
// f_config.acceptance_code = (0x12345<< 3);
// f_config.acceptance_mask = ~(0X1FFFFFFF<< 3);

   if(Vfilter_on==0)
   {
      f_config.acceptance_code =0;
      f_config.acceptance_mask =0xFFFFFFFF;
      f_config.single_filter = false;
   }
   else
   {
      if(Vextd==0)//标准帧
      {
         f_config.acceptance_code = (Vcode << 21);
         f_config.acceptance_mask = ~(Vmask << 21);
      }
      else//扩展帧
      {
         f_config.acceptance_code = (Vcode<< 3);
         f_config.acceptance_mask = ~(Vmask<< 3);
      }
      f_config.single_filter = true;
   }
   twai_set_config();
}

void canbus_task(void *arg)
{
   twai_message_t rx_msg;
   uint8_t err=0;
   uint8_t evt;
   gpio_set_level(CANVPK_PIN, 0);
   //Install and start TWAI driver
   ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
   ESP_LOGI(CANBUS, "Driver installed");
   ESP_ERROR_CHECK(twai_start());
   ESP_LOGI(CANBUS, "Driver started");

   twai_set_baud(can_baud);
   twai_set_filter(can_filter,can_extd,can_filter_id,can_filter_mask);
   gpio_set_pull_mode(CANR_PIN,GPIO_PULLUP_ONLY);
   gpio_set_pull_mode(CANT_PIN,GPIO_PULLUP_ONLY);

   while (1)
   {
      esp_err_t ret = twai_receive(&rx_msg, 0);
      if (ret == ESP_OK) 
      {
         can_rx_len+=1;
         evt=REFRESH_VALUE;
         xQueueSendFromISR(gui_evt_queue, &evt, NULL);

         memcpy(can_rx_data.can_data,rx_msg.data,8);
         can_rx_data.identifier=rx_msg.identifier;
         if(rx_msg.flags==0) //标准帧 数据帧
         {
            can_rx_data.can_extd=0;
            can_rx_data.can_rtr=0;
         }
         else if(rx_msg.flags==1) //扩展帧 数据帧
         {
            can_rx_data.can_extd=1;
            can_rx_data.can_rtr=0;
         }
         else if(rx_msg.flags==2) //标准帧 远程帧
         {
            can_rx_data.can_extd=0;
            can_rx_data.can_rtr=1;
         }
         else if(rx_msg.flags==3) //扩展帧 远程帧
         {
            can_rx_data.can_extd=1;
            can_rx_data.can_rtr=1;
         }
         evt=SD_RECEIVE_CAN;
         xQueueSendFromISR(sd_evt_queue, &evt, NULL);

         evt=WIFINET_CAN;
         xQueueSendFromISR(wifinet_evt_queue, &evt, NULL); 


         //ESP_LOGI(CANBUS,"twai_receive identifier=0x%x flags=0x%x data_length_code=%d",rx_msg.identifier, rx_msg.flags, rx_msg.data_length_code);
               //ESP_LOG_BUFFER_HEXDUMP(CANBUS, rx_msg.data, rx_msg.data_length_code, ESP_LOG_INFO);
      }
      else
      {
         vTaskDelay(pdMS_TO_TICKS(1));
      }
      
      if(can_close!=0) break; 
 	}
    can_close=0;
   err=0;
   while(twai_stop()!=ESP_OK)
   {
      vTaskDelay(pdMS_TO_TICKS(100));
      if(++err>10) break;
   }
   err=0;
   while(twai_driver_uninstall()!=ESP_OK)
   {
      vTaskDelay(pdMS_TO_TICKS(100));
      if(++err>10) break;
   }
   gpio_set_level(CANVPK_PIN, 1);
    ESP_LOGI(CANBUS, "can vTaskDelete");
   vTaskDelete(CAN_Handle_task); 
}

