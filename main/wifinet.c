
#include "main.h"
#include "wifinet.h"


const char *WIFINET = "WIFINET";

xQueueHandle wifinet_evt_queue;

esp_mqtt_client_handle_t client;

char server_url[]="xxx.xxxx.xxxx";
char device_ID[]="0000000000";
char mqtt_password[]="e857cc3696058c9adacd2ca3ac355911";
char mqtt_tx_topic[]="device_txd/0000000000";
char mqtt_rx_topic[]="device_rxd/0000000000";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;

static void smartconfig_example_task(void * parm);

uint8_t sen=0;
uint8_t add=0;
uint8_t progress=0;

char wifi_ssid[32];
char wifi_pass[32];


//数据打包,打包的数据放入Data_Buffer中
void DataCombine( uint16_t com, uint8_t sn, uint8_t *data, uint16_t data_len )
{
    uint8_t Data_Buffer[2048];
    uint16_t j = 0;
    uint16_t i = 0;
    uint8_t err = 0;
    uint16_t len = data_len + 18; //除去数据体  最低报文长度是18
    uint8_t checksum = 0; //校验和
    uint8_t timeS[4];//4字节时间戳

    Data_Buffer[j] = 0xAA; //报文头
    j++;
    Data_Buffer[j] = len >> 8; //报文长度高位
    j++;
    Data_Buffer[j] = len & 0xff; //报文长度低位
    j++;

    for(i = 0; i < 10; i++) //硬件ID
    {
        Data_Buffer[j] = device_ID[i];
        j++;
    }

    Data_Buffer[j] = sn;
    j++;

    Data_Buffer[j] = com >> 8; //指令类型高位
    j++;
    Data_Buffer[j] = com & 0xff; //指令类型低位
    j++;

    for(i = 0; i < data_len; i++)
    {
        Data_Buffer[j] = data[i];
        j++;
    }

    Data_Buffer[j] = 0; //校验和
    j++;

    Data_Buffer[j] = 0xdd; //报文结尾
    j++;

    for(i = 0; i < j; i++) //计算校验和
    {
        checksum += Data_Buffer[i];
    }

    Data_Buffer[j - 2] = checksum;

    // if ((client)&&(client->state)) {
    //     ESP_LOGE(TAG, "Client was not initialized");
    //     return ESP_ERR_INVALID_ARG;
    // }
    if (client) {
        esp_mqtt_client_publish(client, mqtt_tx_topic, (char *)Data_Buffer, j, 1, 0);
    }
}

uint16_t Data_jx_com = 0;
uint8_t Data_jx_data[512];
uint16_t Data_jx_len = 0;
uint8_t Data_rxsn;

//数据解析，将 Buffer中的数据解析出来
uint8_t DataSeparate(uint8_t *Buffer)
{
    uint16_t len = 0;
    uint16_t j = 0;
    uint8_t he = 0; //临时保存和
    uint8_t checksum = 0; //校验和
    uint16_t i = 0;

    if(Buffer[j] != 0xAA) //协议头不是0xAA
    {
        ESP_LOGI(WIFINET,"error 1\r\n"); 
        return 0;
    }

    j++;
    len = Buffer[j]; //报文长度高位
    j++;
    len <<= 8;
    len += Buffer[j]; //报文长度低位
    j++;

    for (i = 0; i < 10; i++) //ID号
    {
        if (Buffer[j] != device_ID[i]) 
        {
            ESP_LOGI(WIFINET,"error 2\r\n"); 
            return 0; //ID号不正确
        }

        j++;
    }

    Data_rxsn = Buffer[j];
    j++;

    Data_jx_com = Buffer[j]; //指令类型高位
    j++;
    Data_jx_com <<= 8;
    Data_jx_com += Buffer[j]; //指令类型低位
    j++;

    if((len > 500) | (len < 18)) 
    {
        ESP_LOGI(WIFINET,"error 3\r\n"); 
        return 0;
    }

    Data_jx_len = len - 18; //数据体长度

    for(i = 0; i < Data_jx_len; i++) //获取数据体
    {
        Data_jx_data[i] = Buffer[j];
        j++;
    }

    he = Buffer[len - 2]; //先缓存校验和
    Buffer[len - 2] = 0; //校验和清零

    for(i = 0; i < len; i++)
    {
        checksum += Buffer[i];
    }

    if(he != checksum) //校验和不正确
    {
       // ESP_LOGI(WIFINET,"error 4\r\n"); 
        //return 0;
    }

    if(Buffer[len - 1] != 0xDD) //协议尾不是0xDD
    {
        ESP_LOGI(WIFINET,"error 5\r\n"); 
        return 0;
    }

    return 1;
}

//将万用表功能标识转换为协议文档中的标识
uint8_t conversion_fun_id(uint8_t fun)
{
    switch (fun)
    {
        case FUN_DCV://直流电压
            return 0x01;
        break;
        case FUN_ACV://交流电压
            return 0x02;
        break;
        case FUN_DCA://直流电流
            if(choice_dca==CUR_UA) //uA 档位 
            {
                 return 0x03;
            }
            else if(choice_dca==CUR_MA)//mA 档位 
            {
                 return 0x04;
            }
            else if(choice_dca==CUR_A)//A 档位
            {
                 return 0x05;
            }
        break;
        case FUN_ACA://交流电压
            if(choice_aca==CUR_MA)//mA 档位
            {
                return 0x06;
            }
            else if(choice_aca==CUR_A)//A 档位
            {
                return 0x07;
            }
        break;
        case FUN_2WR://两线电阻
            return 0x08;
        break;
        case FUN_4WR://四线电阻
            return 0x09;
        break;
        case FUN_DIODE://二极管
            return 0x0A;
        break;
        case FUN_BEEP://通断档
            return 0x0B;
        break;
        case FUN_DCW://直流功率
            return 0x0C;
        break;
        case FUN_ACW://交流功率
            return 0x0D;
        break;
        case FUN_TEMP://测量温度
            return 0x0E;
        break;

        default:
        break;
    }
    return 0x00;
}


void sntp_set_time_sync_callback(struct timeval *tv)
{
    struct tm timeinfo = {0};
    ESP_LOGI(WIFINET, "tv_sec: %lld", (uint64_t)tv->tv_sec);
    localtime_r((const time_t *)&(tv->tv_sec), &timeinfo);
    ESP_LOGI(WIFINET, "%d %d %d %d:%d:%d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

	uint8_t del_time[4];
	del_time[3]=(timeinfo.tm_year+1900)-2000;
	del_time[2]=timeinfo.tm_mon+1;
	del_time[0]=timeinfo.tm_mday;
	PCF8563_Init_Day(del_time);
	del_time[2]=timeinfo.tm_hour;
	del_time[1]=timeinfo.tm_min; 
	del_time[0]=timeinfo.tm_sec; 
	PCF8563_Init_Time(del_time);

    time_state=1;
   
    // sntp_stop();
    // utc_set_time((uint64_t)tv->tv_sec);
}

static void esp_initialize_sntp(void)
{
    char strftime_buf[64];
    ESP_LOGI(WIFINET, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "ntp.aliyun.com");
    sntp_setservername(1, "ntp.ntsc.ac.cn");
    sntp_setservername(2, "edu.ntp.org.cn");
    sntp_setservername(3, "time1.cloud.tencent.com");

	sntp_set_time_sync_notification_cb(&sntp_set_time_sync_callback);
    sntp_init();


    // time_t now = 0;
    // struct tm timeinfo = { 0 };
    // int retry = 0;

    // while (timeinfo.tm_year < (2022 - 1900)) {
    //     ESP_LOGD(WIFINET, "Waiting for system time to be set... (%d)", ++retry);
    //     vTaskDelay(100 / portTICK_PERIOD_MS);
    //     time(&now);
    //     localtime_r(&now, &timeinfo);
    // }

    // // set timezone to China Standard Time
    // setenv("TZ", "CST-8", 1);
    // tzset();

    // strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    // ESP_LOGI(WIFINET, "The current date/time in Shanghai is: %s", strftime_buf);

}


void start_config_router()
{
    ESP_LOGI(WIFINET,"srart_config_router\r\n"); 
    esp_timer_stop(config_router_timer_handle);
    progress=PROG_START;
    add=0;
    esp_timer_start_periodic(config_router_timer_handle, 10 * 1000);
}

uint8_t ch=0;

char ls_wifi_ssid[32];
char ls_wifi_pass[32];

/// 定时器回调函数 
void config_router_timer_cb(void *arg) 
{
    uint8_t evt;
    static uint8_t rx_data[70];
    static uint8_t rx_data_len=0;
    static uint8_t rx_len=0;
    static uint8_t rx_ssid_len=0;
    static uint8_t rx_pass_len=0;
    uint8_t checksum=0;
    uint8_t i=0;

    if(progress==PROG_START)//等待接收起始信号
    {
        if(gpio_get_level(SEN)==0)//白屏 1
        {
            if(++add>=100)//持续1秒钟的白屏1
            {
                add=100;
            }
        }
        else //黑屏 0
        {
            if(add>=100)//开始发送有效位
            {
                esp_timer_stop(config_router_timer_handle);
                rx_data_len=0;
                progress=PROG_DATA;
                evt=KEY_CONFIG;
                xQueueSendFromISR(gui_evt_queue, &evt, NULL);
                vTaskDelay(pdMS_TO_TICKS(50));
                esp_timer_start_periodic(config_router_timer_handle, 100 * 1000);
                add=0;
                ch=0;  
                ESP_LOGI(WIFINET,"progress=PROG_DATA\r\n");
            }
            else
            {
                add=0;
            }
        }
    }
    else if(progress==PROG_DATA)
    {
        ch<<=1;
        if(gpio_get_level(SEN)==0)//白屏 1
        {
            ch|=0x01;
        }
        add++;
        if(add>=8)
        {
            rx_data[rx_data_len++]=ch;
            if((ch==0x00)|(ch==0xFF))
            {
                ESP_LOGI(WIFINET,"error 3\r\n"); 
                esp_timer_stop(config_router_timer_handle);
                progress=PROG_ERROR;
                evt=KEY_CONFIG;
                xQueueSendFromISR(gui_evt_queue, &evt, NULL);  
            }

            if(rx_data_len==3)
            {
                rx_len=rx_data[0];
                rx_ssid_len=rx_data[1];
                rx_pass_len=rx_data[2];

                ESP_LOGI(WIFINET,"rx_len=%d\r\n",rx_len); 
                ESP_LOGI(WIFINET,"rx_ssid_len=%d\r\n",rx_ssid_len); 
                ESP_LOGI(WIFINET,"rx_pass_len=%d\r\n",rx_pass_len); 


                if((rx_len>68)|(rx_ssid_len>32)|(rx_pass_len>32)|(rx_len!=(rx_ssid_len+rx_pass_len+4)))//数据长度校验错误
                {
                    ESP_LOGI(WIFINET,"error 1\r\n"); 
                    esp_timer_stop(config_router_timer_handle);
                    progress=PROG_ERROR;
                    evt=KEY_CONFIG;
                    xQueueSendFromISR(gui_evt_queue, &evt, NULL);  
                }
            }
            ch=0;
            add=0; 
        }

        if((rx_data_len>=rx_len)&(rx_data_len>3))
        {
            checksum=0;
            for(i=0;i<(rx_len-1);i++)
            {
                checksum+=rx_data[i];
            }
            
            ESP_LOGI(WIFINET,"checksum1=%02x\r\n",checksum); 
            ESP_LOGI(WIFINET,"checksum2=%02x\r\n",rx_data[(rx_len-1)]); 
            
            if(checksum!=rx_data[(rx_len-1)])//校验和错误
            {
                ESP_LOGI(WIFINET,"error 2\r\n"); 
                esp_timer_stop(config_router_timer_handle);
                progress=PROG_ERROR;
                evt=KEY_CONFIG;
                xQueueSendFromISR(gui_evt_queue, &evt, NULL);  
            }
            else
            {
                for(i=0;i<rx_ssid_len;i++)
                {
                    ls_wifi_ssid[i]=rx_data[3+i];
                }
                ls_wifi_ssid[rx_ssid_len]=0;
                for(i=0;i<rx_pass_len;i++)
                {
                    ls_wifi_pass[i]=rx_data[3+rx_ssid_len+i];
                }
                ls_wifi_pass[rx_pass_len]=0;


                ESP_LOGI(WIFINET,"wifi_ssid=%s\r\n",ls_wifi_ssid); 
                ESP_LOGI(WIFINET,"wifi_pass=%s\r\n",ls_wifi_pass); 
                esp_timer_stop(config_router_timer_handle);
                
                progress=PROG_OVER;
                evt=KEY_CONFIG;
                xQueueSendFromISR(gui_evt_queue, &evt, NULL);  
            }
            
        }

    }
    
	//ESP_LOGI(WIFINET,"config_router_timer\r\n"); 
	
}  
esp_timer_handle_t config_router_timer_handle = 0;
//定义一个周期重复运行的定时器结构体
esp_timer_create_args_t config_router_periodic_arg = { 
        .callback = &config_router_timer_cb, //设置回调函数
		.arg = NULL, //不携带参数
		.name = "config_router_timer" //定时器名字
		};

// 在 event_handler 中，通过获取不同的时间执行相对应的操作。
static void event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
   if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
       xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
   } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
       esp_wifi_connect(); // 开始连接WiFi
       xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
   } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
       xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
   } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
       ESP_LOGI(WIFINET, "Scan done");
   } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
       ESP_LOGI(WIFINET, "Found channel");
   } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
       ESP_LOGI(WIFINET, "Got SSID and password");

       smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
       wifi_config_t wifi_config;
       uint8_t ssid[33] = { 0 };
       uint8_t password[65] = { 0 };
       uint8_t rvd_data[33] = { 0 };
       bzero(&wifi_config, sizeof(wifi_config_t));
       memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
       memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
       wifi_config.sta.bssid_set = evt->bssid_set;

 if (wifi_config.sta.bssid_set == true) {
           memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
       }
       memcpy(ssid, evt->ssid, sizeof(evt->ssid));
       memcpy(password, evt->password, sizeof(evt->password));
       ESP_LOGI(WIFINET, "SSID:%s", ssid);
       ESP_LOGI(WIFINET, "PASSWORD:%s", password);
       if (evt->type == SC_TYPE_ESPTOUCH_V2) {
           ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
           ESP_LOGI(WIFINET, "RVD_DATA:");
           for (int i=0; i<33; i++) {
               ESP_LOGI(WIFINET,"%02x ", rvd_data[i]);
           }
           ESP_LOGI(WIFINET,"\n");
       }
       ESP_ERROR_CHECK( esp_wifi_disconnect() );
       ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
       esp_wifi_connect();
   } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
       xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
   }
}


 void initialise_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// 创建事件组 s_wifi_event_group = xEventGroupCreate();，触发相关事件则置标志位，在任务中循环检测标志位处理相应事件。
// smartconfig_example_task 任务中，获取 CONNECTED_BIT 和 ESPTOUCH_DONE_BIT 表示连接上 AP 和 SmartConfig 配置完成。
static void smartconfig_example_task(void * parm)
{
   EventBits_t uxBits;
   ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS) );    // 设置 SmartConfig 的协议类型
   smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
   ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) ); //开始 smartconfig 一键配网
   while (1) {
       uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
       if(uxBits & CONNECTED_BIT) {
           ESP_LOGI(WIFINET, "WiFi Connected to ap");
       }
       if(uxBits & ESPTOUCH_DONE_BIT) {
           ESP_LOGI(WIFINET, "smartconfig over");
           esp_smartconfig_stop();      // 配网结束,释放 esp_smartconfig_start 占用的缓冲区。
           vTaskDelete(NULL);
       }
   }
}



static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    int msg_id;
    uint8_t evt=0;
    uint16_t ls_data=0;
    uint32_t identifier;

    time_t now;
    long totalSeconds;
    uint8_t tx_buf[128];
    client = event->client;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            net_state=2;
            evt=REFRESH_TOP_ICON;
            xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
            msg_id = esp_mqtt_client_subscribe(client, mqtt_rx_topic, 0);
            ESP_LOGI(WIFINET, "sent subscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(WIFINET, "MQTT_EVENT_DISCONNECTED");
            net_state=1;
            evt=REFRESH_TOP_ICON;
            xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(WIFINET, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(WIFINET, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            //ESP_LOGI(WIFINET, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA: //收到MQTT消息
            ESP_LOGI(WIFINET, "MQTT_EVENT_DATA");
            //ESP_LOGI(WIFINET,"主题长度:%d 数据长度:%d\n",event->topic_len,event->data_len);
            //ESP_LOGI(WIFINET,"TOPIC=%.*s\r\n", event->topic_len, event->topic);
            //ESP_LOGI(WIFINET,"DATA=%.*s\r\n", event->data_len, event->data);

            if(DataSeparate((uint8_t *)event->data) == 1)
            {
                ESP_LOGI(WIFINET,"Data_jx_com = 0x%04x\r\n", Data_jx_com);
                switch(Data_jx_com)
                {
                    case 0x0701://获取硬件信息
                        evt=WIFINET_INFO;
                        xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                        break;
                    case 0x0709://固件升级
                        evt=WIFINET_OTA;
                        xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                        break;
                    case 0x0801://万用表设置参数
                        if(key_sound!=0) beep_start(2);
                        

                        ls_data=Data_jx_data[0];
                        ls_data<<=8;
                        ls_data+=Data_jx_data[1];
                        if((ls_data!=0xFFFF)&(ls_data>1))
                        {
                            current_freq=ls_data;
                            write_config_in_nvs(CONFIG_OTHER); 
                            set_current_freq();
                        }
                        if(Data_jx_data[2]!=0xFF)
                        {
                            if(Data_jx_data[2]==0x01)
                            {
                                if(current_fun!=FUN_DCV)
                                {
                                    ui_display_st=DISPLAY_DCV;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                }
                            }
                            else if(Data_jx_data[2]==0x02)
                            {   
                                if(current_fun!=FUN_ACV)
                                {
                                    ui_display_st=DISPLAY_ACV;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                }
                            }
                            else if(Data_jx_data[2]==0x03)
                            {  
                                
                                    choice_dca=CUR_UA;
                                    ui_display_st=DISPLAY_DCA;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                
                            }
                            else if(Data_jx_data[2]==0x04)
                            {
                                
                                    choice_dca=CUR_MA;
                                    ui_display_st=DISPLAY_DCA;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                
                            }
                            else if(Data_jx_data[2]==0x05)
                            {
                                
                                    choice_dca=CUR_A;
                                    ui_display_st=DISPLAY_DCA;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                
                            }
                            else if(Data_jx_data[2]==0x06)
                            {
                                
                                    choice_aca=CUR_MA;
                                    ui_display_st=DISPLAY_ACA;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                
                            }
                            else if(Data_jx_data[2]==0x07)
                            {
                                
                                    choice_aca=CUR_A;
                                    ui_display_st=DISPLAY_ACA;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                
                            }
                            else if(Data_jx_data[2]==0x08)
                            {
                                if(current_fun!=FUN_2WR)
                                {
                                    ui_display_st=DISPLAY_2WR;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                }
                            }
                            else if(Data_jx_data[2]==0x09)
                            {
                                if(current_fun!=FUN_4WR)
                                {
                                    ui_display_st=DISPLAY_4WR;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                }
                            }
                            else if(Data_jx_data[2]==0x0A)
                            {
                                if(current_fun!=FUN_DIODE)
                                {
                                    ui_display_st=DISPLAY_DIODE;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                }
                            }
                            else if(Data_jx_data[2]==0x0B)
                            {
                                if(current_fun!=FUN_BEEP)
                                {
                                    ui_display_st=DISPLAY_BEEP;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                }
                            }
                            else if(Data_jx_data[2]==0x0C)
                            {
                                if(current_fun!=FUN_DCW)
                                {
                                    ui_display_st=DISPLAY_DCW;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                }
                            }
                            else if(Data_jx_data[2]==0x0D)
                            {
                                if(current_fun!=FUN_ACW)
                                {
                                    ui_display_st=DISPLAY_ACW;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                }
                            }
                            else if(Data_jx_data[2]==0x0E)
                            { 
                                if(current_fun!=FUN_TEMP)
                                {
                                    ui_display_st=DISPLAY_TEMP;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                }
                            }
                            else if(Data_jx_data[2]==0x0F)
                            {
                                if(current_fun!=FUN_UART)
                                {
                                    ui_display_st=DISPLAY_UART;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                }
                            }
                            else if(Data_jx_data[2]==0x10)
                            {
                               if(current_fun!=FUN_RS485)
                                {
                                    ui_display_st=DISPLAY_RS485;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                }
                            }
                            else if(Data_jx_data[2]==0x11)
                            {
                                if(current_fun!=FUN_CAN)
                                {
                                    ui_display_st=DISPLAY_CAN;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                }
                            }
                            else if(Data_jx_data[2]==0x12)
                            {
                               if(current_fun!=FUN_SIGNAL)
                                {
                                    ui_display_st=DISPLAY_SIGNAL;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                }
                            }
                            else if(Data_jx_data[2]==0x13)
                            {
                               if(current_fun!=FUN_PWM)
                                {
                                    ui_display_st=DISPLAY_PWM;

                                    ui_exit=1;
                                    evt=REFRESH_VALUE;
                                    xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                                }
                            }
                        }
                        
                        DataCombine(0x0802,Data_rxsn,NULL,0);

                        break;
                    case 0x0803://万用表查询参数
                        tx_buf[0] = (current_freq>>8)&0xFF;
                        tx_buf[1] = (current_freq)&0xFF;
                        tx_buf[2] = conversion_fun_id(current_fun);
                        DataCombine(0x0804,Data_rxsn,tx_buf,3);
                        break;
                    case 0x0806://主动获取万用表测量值
                        evt=WIFINET_MVOM;
                        xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                        break;
                    case 0x0807://万用表归零
                        if(key_sound!=0) beep_start(2);
                        Zero_b=1;
                        DataCombine(0x0808,Data_rxsn,NULL,0);
                        break;
                    case 0x0809://万用表校准
                        DataCombine(0x080A,Data_rxsn,NULL,0);
                        break;
                    case 0x0905://直流信号源 电压电流设置
                        if(key_sound!=0) beep_start(2);

                        if((Data_jx_data[0]!=0xFF)&(Data_jx_data[1]!=0xFF)) 
                        {
                            signal_V=Data_jx_data[0];
                            signal_V<<=8;
                            signal_V|=Data_jx_data[1];
                        }

                        if((Data_jx_data[4]!=0xFF)&(Data_jx_data[5]!=0xFF)) 
                        {
                            signal_A=Data_jx_data[4];
                            signal_A<<=8;
                            signal_A|=Data_jx_data[5];
                        }
                        evt=REFRESH_VALUE;
                        xQueueSendFromISR(gui_evt_queue, &evt, NULL);

                        screen_status();
                        GP8403_WriteReg();
                        DataCombine(0x0906,Data_rxsn,NULL,0);
                        break;
                    case 0x0907://直流信号源 电压电流查询
                        evt=WIFINET_SIGNAL_CONFIG;
                        xQueueSendFromISR(wifinet_evt_queue, &evt, NULL); 
                        break;

                    case 0x0C01://串口 设置参数
                        if(Data_jx_data[0]==0x04) uart_config.baud_rate=1200;
                        else if(Data_jx_data[0]==0x05) uart_config.baud_rate=2400;
                        else if(Data_jx_data[0]==0x06) uart_config.baud_rate=4800;
                        else if(Data_jx_data[0]==0x07) uart_config.baud_rate=9600;
                        else if(Data_jx_data[0]==0x08) uart_config.baud_rate=14400;
                        else if(Data_jx_data[0]==0x09) uart_config.baud_rate=19200;
                        else if(Data_jx_data[0]==0x0A) uart_config.baud_rate=38400;
                        else if(Data_jx_data[0]==0x0B) uart_config.baud_rate=56000;
                        else if(Data_jx_data[0]==0x0C) uart_config.baud_rate=115200;
                        else if(Data_jx_data[0]==0x0D) uart_config.baud_rate=230400;
                        else if(Data_jx_data[0]==0x0E) uart_config.baud_rate=256000;

                        if(Data_jx_data[1]==0x00) uart_config.parity=UART_PARITY_DISABLE;
                        else if(Data_jx_data[1]==0x01) uart_config.parity=UART_PARITY_ODD;
                        else if(Data_jx_data[1]==0x02) uart_config.parity=UART_PARITY_EVEN;

                        if(Data_jx_data[2]==0x00) uart_config.stop_bits=UART_STOP_BITS_1;
                        else if(Data_jx_data[2]==0x01) uart_config.stop_bits=UART_STOP_BITS_1_5;
                        else if(Data_jx_data[2]==0x02) uart_config.stop_bits=UART_STOP_BITS_2;

                        if(Data_jx_data[3]==0x00) uart_config.data_bits=UART_DATA_5_BITS;
                        else if(Data_jx_data[3]==0x01) uart_config.data_bits=UART_DATA_6_BITS;
                        else if(Data_jx_data[3]==0x02) uart_config.data_bits=UART_DATA_7_BITS;
                        else if(Data_jx_data[3]==0x03) uart_config.data_bits=UART_DATA_8_BITS;

                        usart1_init();
                        if(key_sound!=0) beep_start(2);
                        ui_exit=1;
                        evt=REFRESH_VALUE;
                        xQueueSendFromISR(gui_evt_queue, &evt, NULL);

                        DataCombine(0x0C02,Data_rxsn,NULL,0);
                        break;
                    case 0x0C03://串口 查询参数
                        evt=WIFINET_UART_CONFIG;
                        xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                        break;
                    case 0x0C07://串口 软件发送实时数据
                        memcpy(tx_uart_data,Data_jx_data,Data_jx_len);
                        tx_uart_data_len=Data_jx_len;
                        evt=UART_TX_DATA;
                        xQueueSendFromISR(usart_evt_queue, &evt, NULL);
                        DataCombine(0x0C08,Data_rxsn,NULL,0);
                        break;
                    case 0x0C09://清空接收数据
                        DataCombine(0x0C0A,Data_rxsn,NULL,0);
                        if((ui_display_st==DISPLAY_UART)||(ui_display_st==DISPLAY_RS485))
                        {
                            uart_485_clean_rx();
                        }
                        break;
                    case 0x0D01://CAN 设置参数
                        if(key_sound!=0) beep_start(2);
                        if(Data_jx_data[0]!=0xFF)
                        {
                            twai_set_baud(Data_jx_data[0]);
                        }
                        else
                        {
                            can_extd=Data_jx_data[1];  
                            can_filter_id =Data_jx_data[3];
                            can_filter_id<<=8;
                            can_filter_id |=Data_jx_data[4];
                            can_filter_id<<=8;
                            can_filter_id |=Data_jx_data[5];
                            can_filter_id<<=8;
                            can_filter_id |=Data_jx_data[6];
                            can_filter_mask =Data_jx_data[7];
                            can_filter_mask<<=8;
                            can_filter_mask |=Data_jx_data[8];
                            can_filter_mask<<=8;
                            can_filter_mask |=Data_jx_data[9];
                            can_filter_mask<<=8;
                            can_filter_mask |=Data_jx_data[10];
                            can_filter=Data_jx_data[11];
                            twai_set_filter(can_filter,can_extd,can_filter_id,can_filter_mask);
                        } 

                        evt=REFRESH_CAN;
                        xQueueSendFromISR(gui_evt_queue, &evt, NULL);

                        DataCombine(0x0D02,Data_rxsn,NULL,0);
                        break;
                    case 0x0D03://CAN 查询参数
                        evt=WIFINET_CAN_CONFIG;
                        xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                        break;
                    case 0x0D07://CAN 软件发送实时数据
                        
                        identifier =Data_jx_data[0];
                        identifier<<=8;
                        identifier |=Data_jx_data[1];
                        identifier<<=8;
                        identifier |=Data_jx_data[2];
                        identifier<<=8;
                        identifier |=Data_jx_data[3];
                        twai_tx_data(identifier,Data_jx_data[4],Data_jx_data[5],&Data_jx_data[6]);
                        DataCombine(0x0D08,Data_rxsn,NULL,0);
                        break;
                    case 0x0D09://清空接收数据
                        DataCombine(0x0D0A,Data_rxsn,NULL,0);
                        if(ui_display_st==DISPLAY_CAN)
                        {
                            can_clean_rx();
                        }
                        break;
                    case 0x0E05://PWM信号源 频率占空比设置
                        if(key_sound!=0) beep_start(2);

                        if((Data_jx_data[0]!=0xFF)&(Data_jx_data[1]!=0xFF)) 
                        {
                            pwm_freq=Data_jx_data[0];
                            pwm_freq<<=8;
                            pwm_freq|=Data_jx_data[1];
                        }
                        
                        if(Data_jx_data[2]!=0xFF)
                        {
                            pwm_duty1=Data_jx_data[2];
                        }

                        if(Data_jx_data[3]!=0xFF)
                        {
                            pwm_duty2=Data_jx_data[3];
                        }
 
                        evt=REFRESH_VALUE;
                        xQueueSendFromISR(gui_evt_queue, &evt, NULL);

                        screen_status();
                        DataCombine(0x0E06,Data_rxsn,NULL,0);
                        break;
                    case 0x0E07://PWM信号源 频率占空比查询
                        evt=WIFINET_PWM_CONFIG;
                        xQueueSendFromISR(wifinet_evt_queue, &evt, NULL); 
                        break;


                    
                }
            }
           
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(WIFINET, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(WIFINET, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(WIFINET, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}


static void mqtt_app_start(void)
{
    int i=0;

    for(i=0;i<10;i++)
    {
        mqtt_tx_topic[i+11]=device_ID[i];
        mqtt_rx_topic[i+11]=device_ID[i];
    }
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .host= server_url,
            .event_handle = mqtt_event_handler,//注册回调函数
            .keepalive=30,
            .port = 1883,
            .username = device_ID,
			.password = mqtt_password,
            .client_id = device_ID
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}




static EventGroupHandle_t wifi_event_group;
 
/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */

//wifi 事件
static esp_err_t event_handler2(void *ctx, system_event_t *event)
{
    uint8_t evt;
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(WIFINET, "\r\n--------SYSTEM_EVENT_STA_START ---------");
            if(wifi_on==1) esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGI(WIFINET, "\r\n--------SYSTEM_EVENT_STA_CONNECTED ---------");
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(WIFINET, "\r\n--------wifi ok ---------");
             
            net_state=1;
            evt=REFRESH_TOP_ICON;
            xQueueSendFromISR(gui_evt_queue, &evt, NULL); 

            if(progress==PROG_CONNECTED)
            {
                progress=PROG_CONNECTED_OK;
                evt=KEY_CONFIG;
                xQueueSendFromISR(gui_evt_queue, &evt, NULL);

                memcpy(wifi_ssid, ls_wifi_ssid, sizeof(ls_wifi_ssid));

                //存放当前的配网信息
                nvs_handle_t wificonfig_set_handle;
                ESP_ERROR_CHECK( nvs_open("wificonfig",NVS_READWRITE,&wificonfig_set_handle) );
                ESP_ERROR_CHECK( nvs_set_str(wificonfig_set_handle,"SSID",(const char *)ls_wifi_ssid) );
                ESP_ERROR_CHECK( nvs_set_str(wificonfig_set_handle,"PASSWORD", (const char *)ls_wifi_pass) );
                ESP_ERROR_CHECK( nvs_commit(wificonfig_set_handle) );
                nvs_close(wificonfig_set_handle);
            }

            if (client) {
                esp_mqtt_client_reconnect(client);
            }
            else
            {
                mqtt_app_start();
            }
            
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            if(time_state==0) esp_initialize_sntp();
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(WIFINET, "\r\n--------SYSTEM_EVENT_STA_DISCONNECTED ---------");

            wifi_event_sta_disconnected_t *sta_disconnect_evt = (wifi_event_sta_disconnected_t*)event;
             ESP_LOGI(WIFINET, "wifi disconnect reason:%d", sta_disconnect_evt->reason);

            if(progress==PROG_CONNECTED)
            {
                progress=PROG_CONNECTED_ERR;
                evt=KEY_CONFIG;
                xQueueSendFromISR(gui_evt_queue, &evt, NULL);
            }
            
            /* This is a workaround as ESP32 WiFi libs don't currently
               auto-reassociate. */
           if(wifi_on==1) esp_wifi_connect();
           net_state=0;
            evt=REFRESH_TOP_ICON;
            xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

//初始化wifi
void app_wifi_initialise(void)
{
    nvs_handle_t wificonfig_get_handle;
    esp_err_t err;
    uint8_t Len;
    //esp_wifi_set_ps(WIFI_PS_NONE);
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler2, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    // wifi_config_t wifi_config = {
    //     .sta = {
    //         .ssid = "",
    //         .password = "",
    //         .pmf_cfg = {
    //             .capable = true,
    //             .required = false
    //         },
    //     },
        

    // };

    wifi_config_t wifi_config;
    bzero(&wifi_config, sizeof(wifi_config_t));


    nvs_open("wificonfig", NVS_READWRITE, &wificonfig_get_handle);
     Len=32;
     err = nvs_get_str(wificonfig_get_handle, "SSID", (char *)wifi_ssid, &Len);
    if(err == ESP_OK)
    {
        memcpy(wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_ssid));
    }
    Len=32;
    err = nvs_get_str(wificonfig_get_handle, "PASSWORD",(char *)wifi_pass, &Len);
    if(err == ESP_OK)
    {
        memcpy(wifi_config.sta.password, wifi_pass, sizeof(wifi_pass));
    }
     nvs_close(wificonfig_get_handle);

    //ESP_LOGI(WIFINET, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    //ESP_LOGI(WIFINET, "Setting WiFi configuration PASS %s...", wifi_config.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

//定时发送万用表硬件信息
esp_timer_handle_t esp_timer_handle_txinfo = 0;
/*定时器中断函数*/
void esp_timer_txinfo_cb(void *arg){
    uint8_t evt;

    evt=WIFINET_INFO;
    xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);

    if((current_fun==FUN_UART)|(current_fun==FUN_RS485))
    {
        evt=WIFINET_UART_CONFIG;
        xQueueSendFromISR(wifinet_evt_queue, &evt, NULL); 
    }
    else if(current_fun==FUN_CAN)
    {
        evt=WIFINET_CAN_CONFIG;
        xQueueSendFromISR(wifinet_evt_queue, &evt, NULL); 
    }
    else if(current_fun==FUN_SIGNAL)
    {
        evt=WIFINET_SIGNAL_CONFIG;
        xQueueSendFromISR(wifinet_evt_queue, &evt, NULL); 
    }
    else if(current_fun==FUN_PWM)
    {
        evt=WIFINET_PWM_CONFIG;
        xQueueSendFromISR(wifinet_evt_queue, &evt, NULL); 
    }

}

//使用获取到的路由器信息链接路由器
void wifi_connecting_routers(void)
{
    wifi_config_t wifi_config;

    bzero(&wifi_config, sizeof(wifi_config_t));
    //initialise_wifi();
    memcpy(wifi_config.sta.ssid, ls_wifi_ssid, sizeof(ls_wifi_ssid));
    memcpy(wifi_config.sta.password, ls_wifi_pass, sizeof(ls_wifi_pass));

    //使用获取的配网信息链接无线网络
    ESP_ERROR_CHECK( esp_wifi_disconnect() );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_connect() );
}







void wifinet_task(void *arg)
{
    uint8_t evt;
    time_t now;
    int i=0;
    long totalSeconds;
    uint8_t tx_buf[128];
    uint8_t sn_count=0;
    uint8_t fun=0;
    wifinet_evt_queue = xQueueCreate(3, sizeof(uint8_t)); // wifi网络事件队列
    //vTaskDelay(pdMS_TO_TICKS(1000));
    //initialise_wifi();                   // 初始化WiFi为sta模式，等待APP进行配网
    app_wifi_initialise();

        //定时器结构体初始化
    esp_timer_create_args_t esp_timer_create_args_txinfo = {
        .callback = &esp_timer_txinfo_cb, //定时器回调函数
        .arg = NULL, //传递给回调函数的参数
        .name = "esp_timer_txinfo" //定时器名称
    };

    /*创建定时器*/       
    esp_err_t err = esp_timer_create(&esp_timer_create_args_txinfo, &esp_timer_handle_txinfo);
    err = esp_timer_start_periodic(esp_timer_handle_txinfo, 3000 * 1000);
    while (1) 
    {
        if (xQueueReceive(wifinet_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
                case WIFINET_INFO: //发送万用表信息

                    switch (current_fun)
                    {
                        case FUN_DCV://直流电压
                        case FUN_ACV://交流电压
                        case FUN_DCA://直流电流
                        case FUN_ACA://交流电压
                        case FUN_2WR://两线电阻
                        case FUN_4WR://四线电阻
                        case FUN_DIODE://二极管
                        case FUN_BEEP://通断档
                        case FUN_DCW://直流功率
                        case FUN_ACW://交流功率
                        case FUN_TEMP://测量温度
                            fun=0;
                        break;
                        case FUN_UART://UART调试
                        case FUN_RS485://RS485调试
                            fun=1;
                        break;
                        case FUN_CAN://CAN调试
                            fun=2;
                        break;
                        case FUN_SIGNAL://直流信号源
                            fun=3;
                        break;
                        case FUN_PWM://PWM信号源
                            fun=4;
                        break;
                        default:
                            fun=0xFF;
                        break;
                    }
                    time(&now);
                    totalSeconds=(long)now;
                    //ESP_LOGI(WIFINET,"totalSeconds = %ld\r\n", totalSeconds);
                    tx_buf[0] = (totalSeconds >> 24) & 0XFF;
                    tx_buf[1] = (totalSeconds >> 16) & 0XFF;
                    tx_buf[2] = (totalSeconds >> 8) & 0XFF;
                    tx_buf[3] = (totalSeconds) & 0XFF;
                    tx_buf[4] =  ver[0];//版本
                    tx_buf[5] =  ver[1];//版本
                    tx_buf[6] =  equipment_type[0];//设备类型
                    tx_buf[7] =  equipment_type[1];//设备类型
                    tx_buf[8] =  electricity_st;//电量
                    tx_buf[9] =  signal_st;//信号强度
                    tx_buf[10] =  net_type;//网络类型
                    tx_buf[11] =  fun;//当前功能
                    DataCombine(0x0702,Data_rxsn,tx_buf,12);
                break;

                case WIFINET_MVOM: //发送万用表测量值
                    //ESP_LOGI(WIFINET, "\r\n--------WIFINET_MVOM ---------");
                    time(&now);
                    totalSeconds=(long)now;
                    tx_buf[0] = (current_freq>>8)&0xFF;
                    tx_buf[1] = (current_freq)&0xFF;
                    tx_buf[2]=conversion_fun_id(current_fun);
                    tx_buf[3]=sign;
                    tx_buf[4] = (measured_value >> 24) & 0XFF;
                    tx_buf[5] = (measured_value >> 16) & 0XFF;
                    tx_buf[6] = (measured_value >> 8) & 0XFF;
                    tx_buf[7] = (measured_value) & 0XFF;
                    tx_buf[8] =  unit;
                    tx_buf[9] = (totalSeconds >> 24) & 0XFF;
                    tx_buf[10] = (totalSeconds >> 16) & 0XFF;
                    tx_buf[11] = (totalSeconds >> 8) & 0XFF;
                    tx_buf[12] = (totalSeconds) & 0XFF;
                    DataCombine(0x0805,sn_count++,tx_buf,13);   
                break;

                case WIFINET_UART: //发送串口数据
                    DataCombine(0x0C05,sn_count++,rx_uart_data,rx_uart_data_len);  
                     
                break;

                case WIFINET_UART_CONFIG: //发送串口配置
                    if(uart_config.baud_rate==1200)        tx_buf[0]=0x04;
                    else if(uart_config.baud_rate==2400)   tx_buf[0]=0x05;
                    else if(uart_config.baud_rate==4800)   tx_buf[0]=0x06;
                    else if(uart_config.baud_rate==9600)   tx_buf[0]=0x07;
                    else if(uart_config.baud_rate==14400)  tx_buf[0]=0x08;
                    else if(uart_config.baud_rate==19200)  tx_buf[0]=0x09;
                    else if(uart_config.baud_rate==38400)  tx_buf[0]=0x0A;
                    else if(uart_config.baud_rate==56000)  tx_buf[0]=0x0B;
                    else if(uart_config.baud_rate==115200) tx_buf[0]=0x0C;
                    else if(uart_config.baud_rate==230400) tx_buf[0]=0x0D;
                    else if(uart_config.baud_rate==256000) tx_buf[0]=0x0E;

                    if(uart_config.parity==UART_PARITY_DISABLE)     tx_buf[1]=0x00;
                    else if(uart_config.parity==UART_PARITY_ODD)    tx_buf[1]=0x01;
                    else if(uart_config.parity==UART_PARITY_EVEN)   tx_buf[1]=0x02;

                    if(uart_config.stop_bits==UART_STOP_BITS_1)          tx_buf[2]=0x00;
                    else if(uart_config.stop_bits==UART_STOP_BITS_1_5)   tx_buf[2]=0x01;
                    else if(uart_config.stop_bits==UART_STOP_BITS_2)     tx_buf[2]=0x02;

                    if(uart_config.data_bits==UART_DATA_5_BITS)        tx_buf[3]=0x00;
                    else if(uart_config.data_bits==UART_DATA_6_BITS)   tx_buf[3]=0x01;
                    else if(uart_config.data_bits==UART_DATA_7_BITS)   tx_buf[3]=0x02;
                    else if(uart_config.data_bits==UART_DATA_8_BITS)   tx_buf[3]=0x03;    
                    DataCombine(0x0C04,Data_rxsn,tx_buf,4);
                break;

                case WIFINET_CAN: //发送CAN数据
                    tx_buf[0] = (can_rx_data.identifier >> 24) & 0XFF;
                    tx_buf[1] = (can_rx_data.identifier >> 16) & 0XFF;
                    tx_buf[2] = (can_rx_data.identifier >> 8) & 0XFF;
                    tx_buf[3] = (can_rx_data.identifier) & 0XFF;
                    tx_buf[4] = can_rx_data.can_extd; //帧格式
                    tx_buf[5] = can_rx_data.can_rtr; //帧类型
                    for(i=0;i<8;i++)
                    {
                        tx_buf[6+i] =can_rx_data.can_data[i];
                    }
                    DataCombine(0x0D05,sn_count++,tx_buf,14);   
                break;

                case WIFINET_CAN_CONFIG: //发送CAN配置
                    tx_buf[0]=can_baud;
                    tx_buf[1]=can_extd;
                    tx_buf[2]=can_rtr;

                    tx_buf[3]=(can_filter_id >> 24) & 0xFF;
                    tx_buf[4]=(can_filter_id >> 16) & 0xFF;
                    tx_buf[5]=(can_filter_id >> 8) & 0xFF;
                    tx_buf[6]=(can_filter_id >> 0) & 0xFF;

                    tx_buf[7]=(can_filter_mask >> 24) & 0xFF;
                    tx_buf[8]=(can_filter_mask >> 16) & 0xFF;
                    tx_buf[9]=(can_filter_mask >> 8) & 0xFF;
                    tx_buf[10]=(can_filter_mask >> 0) & 0xFF;

                    tx_buf[11]=can_filter;  
                    DataCombine(0x0D04,Data_rxsn,tx_buf,12);
                break;

                case WIFINET_SIGNAL_CONFIG: //发送直流源配置
                    tx_buf[0] = (signal_V>>8)&0xFF;
                    tx_buf[1] = (signal_V)&0xFF;
                    tx_buf[2] = 0xFF;
                    tx_buf[3] = 0xFF;
                    tx_buf[4] = (signal_A>>8)&0xFF;
                    tx_buf[5] = (signal_A)&0xFF;
                    tx_buf[6] = 0xFF;
                    tx_buf[7] = 0xFF;
                    tx_buf[8] = 0xFF;
                    DataCombine(0x0908,Data_rxsn,tx_buf,9);
                break;

                case WIFINET_PWM_CONFIG: //发送PWM配置
                    tx_buf[0] = (pwm_freq>>8)&0xFF;
                    tx_buf[1] = (pwm_freq)&0xFF;
                    tx_buf[2] = pwm_duty1;
                    tx_buf[3] = pwm_duty2;
                    tx_buf[4] = 0xFF;

                    DataCombine(0x0E08,Data_rxsn,tx_buf,5);
                break;

                case WIFINET_MQTTSTOP: //断开MQTT连接
                esp_mqtt_client_stop(client);
                vTaskDelay(pdMS_TO_TICKS(500));
                esp_mqtt_client_stop(client);
                vTaskDelay(pdMS_TO_TICKS(500));
                esp_mqtt_client_stop(client);
                
                break;

                case WIFINET_MARK: //万用表标记测量值
                    tx_buf[0]=0x01;
                    DataCombine(0x080B,sn_count++,tx_buf,1);   
                break;

                case WIFINET_OTA: //固件升级  
                     xTaskCreate(&ota_task, "ota_task", 1024 * 8, NULL, 20, NULL);
                break;

                case WIFINET_OTA_PRO: //固件升级进度
                    tx_buf[0]=percentage;
                    DataCombine(0x070A,sn_count++,tx_buf,1);   
                break;
            }
            
        }
    }

}