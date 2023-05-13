#include "main.h"
#include "usart_485.h"

xQueueHandle usart_evt_queue;
uint8_t tx_uart_data[1024];
uint16_t tx_uart_data_len;

uint8_t rx_uart_data[1024];
uint16_t rx_uart_data_len;

char uart_data[UDATA_MAX][39]={
"                                        ",
"                                        ",
"                                        ",
"                                        ",
"                                        ",
"                                        ",
"                                        ",
"                                        ",
"                                        ",
"                                        ",
"                                        ",
"                                        ",
"                                        ",
"                                        ",
};
uint16_t uart_data_len=0;
uint16_t uart_data_rows=0;
uint16_t uart_data_start=0;
uint16_t uart_data_end=0;
uint8_t  uart_data_hex=0;
uint8_t  uart_data_stop=1;

uart_config_t uart_config = {
       .baud_rate = 115200,
       .data_bits = UART_DATA_8_BITS,
       .parity = UART_PARITY_DISABLE,
       .stop_bits = UART_STOP_BITS_1,
       .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
       .source_clk = UART_SCLK_APB,
};

const char *USART_485_TAG = "USART_485_TASK";


static const int RX_BUF_SIZE = 1024;

uint8_t sd_uart_data[1024];
uint16_t sd_uart_data_len;



void usart0_init() {  
    //We won't use a buffer for sending data.
   uart_driver_install(UART_NUM_0, RX_BUF_SIZE, 0, 0, NULL, 0);
   uart_param_config(UART_NUM_0, &uart_config);
   uart_set_pin(UART_NUM_0, 43, 44, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void usart1_init() {
   
    //We won't use a buffer for sending data.
   uart_driver_install(UART_NUM_1, RX_BUF_SIZE, 0, 0, NULL, 0);
   uart_param_config(UART_NUM_1, &uart_config);
   if(current_fun==FUN_UART)
   {
      uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
   }
   else if (current_fun==FUN_RS485)
   {
      uart_set_pin(UART_NUM_1, RS485T_PIN, RS485R_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
   }
   
}


//在UI界面中增加一行数据
void uart_data_add(uint8_t* d,uint8_t len)
{
   uint8_t i=0;
   uint8_t evt;
   for(i=0;i<39;i++)
   {
      if(i>=len)
      {   
         uart_data[uart_data_end][i]=' ';
      }
      else uart_data[uart_data_end][i]=d[i];
   }

   if(uart_data_rows<(UDATA_MAX-1)) uart_data_rows++;  //总条数

   uart_data_end++;
   if(uart_data_end>(UDATA_MAX-1)) uart_data_end=0;
   
   if(uart_data_rows>11) uart_data_start++;
   if(uart_data_start>(UDATA_MAX-1)) uart_data_start=0;

   // ESP_LOGI(USART_485_TAG,"uart_data_rows=%d\r\n",uart_data_rows);
   // ESP_LOGI(USART_485_TAG,"uart_data_start=%d\r\n",uart_data_start);
   // ESP_LOGI(USART_485_TAG,"uart_data_end=%d\r\n",uart_data_end);

  // ESP_LOGI(USART_485_TAG, "uart_data[0]= %s", uart_data[0]);

   evt=REFRESH_VALUE;
   xQueueSendFromISR(gui_evt_queue, &evt, NULL);

   


}

void  HexToChar(uint8_t dat,uint8_t *ls_p)
{
    uint8_t sj;
	sj=(dat&0xF0)>>4;
    if(sj>9)
	{
		ls_p[0]='A'+sj-10;
	}
	else ls_p[0]=sj+'0';

	sj=(dat&0x0F);
    if(sj>9)
	{
		ls_p[1]='A'+sj-10;
	}
	else ls_p[1]=sj+'0';

   ls_p[2]=' ';
}

void  HexToSrt(uint8_t *dat,uint8_t *ls_p,uint16_t len)
{
   uint16_t i=0;

   for(i=0;i<len;i++)
   {
      HexToChar(dat[i],ls_p+i*3);
   }
}

static void usart1_rx_task(void *arg)
{
   uint16_t i=0;
   uint16_t j=0;
   uint16_t len=0;
   uint8_t rows=0;
   uint8_t srt_data[39];
   uint8_t evt;

   static const char *RX_TASK_TAG = "RX_TASK";
   esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
   uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
   while (1) {
       const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 20 / portTICK_RATE_MS);

       if ((rxBytes > 0)&(uart_data_stop==0)) 
       {
         //   data[rxBytes] = 0;
           // ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
         //   ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);

         uart_data_len+=rxBytes;


         rx_uart_data_len=rxBytes;
         memcpy(rx_uart_data,data,rx_uart_data_len);
         evt=WIFINET_UART;
         xQueueSendFromISR(wifinet_evt_queue, &evt, NULL); 

         if(uart_data_hex==0)
         {
            sd_uart_data_len=rxBytes;
            memcpy(sd_uart_data,data,sd_uart_data_len);//数据准备写入SD卡

            len=0;
            for(i=0;i<rxBytes;i++)
            {
               if(data[i]=='\r') data[i]=' ';
               if(((data[i]<0x20)|(data[i]>0x7E))&(data[i]!='\n')) data[i]='*'; //不可见字符转换成*

               if(data[i]=='\n')
               {
                  uart_data_add(data+i-len,len);
                  len=0;
               }
               else
               {
                  len++;
               } 
               if(len==39)
               {
                  uart_data_add(data+i-len+1,len);
                  len=0;
               }    
            }
            if(len!=0) uart_data_add(data+rxBytes-len,len); //数据末尾不足40字节，并且没有遇到换行符，直接显示出来
         }
         else
         {

            HexToSrt(data,sd_uart_data,rxBytes);
            sd_uart_data_len=rxBytes*3;
            ESP_LOG_BUFFER_HEX(USART_485_TAG, sd_uart_data, sd_uart_data_len);

            len=0;
            for(i=0;i<rxBytes;i++)
            {
               len++;
               if(len==13)
               {
                  HexToSrt(data+i-len+1,srt_data,len);
                  uart_data_add(srt_data,39);
                  len=0;
               }  
            }
            if(len!=0)
            {
               HexToSrt(data+i-len,srt_data,len);
               uart_data_add(srt_data,len*3);

               
            }  
         }

        evt=SD_RECEIVE_PORT;
         xQueueSendFromISR(sd_evt_queue, &evt, NULL); 

       }
   }
   free(data);
}



void usart_485_task(void *arg)
{
   uint8_t evt;
   usart_evt_queue = xQueueCreate(3, sizeof(uint8_t));
   
   usart1_init();
   xTaskCreate(usart1_rx_task, "uart1_rx_task", 1024*2, NULL, 20, NULL);
   

   while (1) {
      //vTaskDelay(pdMS_TO_TICKS(1000));
      //const int txBytes = uart_write_bytes(UART_NUM_1, "usart tx data\r\n", 15);
      //ESP_LOGI(USART_485_TAG, "usart tx %d bytes", txBytes);

      if (xQueueReceive(usart_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
                case UART_TX_DATA: 
                   uart_write_bytes(UART_NUM_1, tx_uart_data, tx_uart_data_len);
                   break;
            }
        }

   }

}


void usart0_task(void *arg)
{
   char* ls;
   int i=0;
   char* data = (char*) malloc(RX_BUF_SIZE+1);
   while (1) {
      const int rxBytes = uart_read_bytes(UART_NUM_0, data, RX_BUF_SIZE, 20 / portTICK_RATE_MS);
      if (rxBytes > 0) 
      {
         data[rxBytes]=0;

         ls=strstr(data,"fun=dcv0");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            choice_dcv=VOL_1000V;
            analog_switch(ASW_DCV0);
         }

         ls=strstr(data,"fun=dcv1");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            choice_dcv=VOL_100V;
            analog_switch(ASW_DCV1);
         }

         ls=strstr(data,"fun=dcv2");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            choice_dcv=VOL_10V;
            analog_switch(ASW_DCV2);
         }

         ls=strstr(data,"fun=acv0");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            choice_acv=VOL_1000V;
            analog_switch(ASW_ACV0);
         }

         ls=strstr(data,"fun=acv1");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            choice_acv=VOL_100V;
            analog_switch(ASW_ACV1);
         }

         ls=strstr(data,"fun=acv2");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            choice_acv=VOL_10V;
            analog_switch(ASW_ACV2);
         }

         ls=strstr(data,"fun=dcua");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            choice_dca=CUR_UA;
            analog_switch(ASW_DCUA);
         }

         ls=strstr(data,"fun=dcma");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            choice_dca=CUR_MA;
            analog_switch(ASW_DCMA);
         }

         ls=strstr(data,"fun=dca");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            choice_dca=CUR_A;
            analog_switch(ASW_DCA);
         }

         ls=strstr(data,"fun=acma");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            choice_aca=CUR_MA;
            analog_switch(ASW_ACMA);
         }

         ls=strstr(data,"fun=aca");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            choice_aca=CUR_A;
            analog_switch(ASW_ACA);
         }

         ls=strstr(data,"fun=2wr0");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            choice_2wr=RES_1K;
            analog_switch(ASW_2WR0);
         }

         ls=strstr(data,"fun=2wr1");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            choice_2wr=RES_20K;
            analog_switch(ASW_2WR1);
         }

         ls=strstr(data,"fun=2wr2");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            choice_2wr=RES_500K;
            analog_switch(ASW_2WR2);
         }

         ls=strstr(data,"fun=2wr3");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            choice_2wr=RES_100M;
            analog_switch(ASW_2WR3);
         }

         ls=strstr(data,"fun=dcout"); //开始标定直流源，将输出电压电流调至最大
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            signal_V=100;
            signal_A=300;
            GP8403_WriteReg(); 
         }

         ls=strstr(data,"dcvout=");//标定直流电压源：dcvout=10000
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            Slope_dcvout=(ls[7]-'0')*10000+(ls[8]-'0')*1000+(ls[9]-'0')*100+(ls[10]-'0')*10+(ls[11]-'0')*1;
            GP8403_WriteReg(); 
            write_config_in_nvs(CONFIG_CAL); 
         }

         ls=strstr(data,"dcaout=");//标定直流电流源：dcaout=30000
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            Slope_dcaout=(ls[7]-'0')*10000+(ls[8]-'0')*1000+(ls[9]-'0')*100+(ls[10]-'0')*10+(ls[11]-'0')*1;
            GP8403_WriteReg(); 
            write_config_in_nvs(CONFIG_CAL); 
         }

         ls=strstr(data,"zero");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            Zero_b=1;
         }

         ls=strstr(data,"slope");
         if(ls!=0)
         {
            printf( "%s OK\r\n", data);
            Slope_b=1;
         }

         ls=strstr(data,"ID=");
         if(ls!=0)
         {
            for(i=0;i<10;i++)
            {
               device_ID[i]=ls[3+i];
            }
            write_config_in_nvs(CONFIG_OTHER); 
            printf( "device_ID=%s\r\n", device_ID);
         }

         ls=strstr(data,"PASS=");
         if(ls!=0)
         {
            for(i=0;i<32;i++)
            {
               mqtt_password[i]=ls[5+i];
            }
            write_config_in_nvs(CONFIG_OTHER); 
            printf( "mqtt_password=%s\r\n", mqtt_password);
         }

         ls=strstr(data,"QR=");
         if(ls!=0)
         {
            for(i=0;i<(rxBytes+1);i++)
            {
               QRcode[i]=ls[3+i];
            }
            write_config_in_nvs(CONFIG_OTHER); 
            printf( "QRcode=%s\r\n", QRcode);
         }

         //ESP_LOGI(USART_485_TAG, "usart0 data: %s", data);
       
      }
   }
}

