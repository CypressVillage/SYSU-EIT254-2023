/* Screen Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */


#include "main.h"


static const char *TAG = "MAIN";

char ver[]="AC"; //固件版本
char QRcode[100]="xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
char equipment_type[2]="W2"; //设备类型
char signal_st=5; //信号：范围0-5 ，为5信号最强，为0没信号
char net_type=1; //网络类型：1-WIFI，2-4G，3-NBIOT


uint16_t pwm_freq=50;// 两路PWM信号的频率，1-9000
uint8_t pwm_duty1=50;  // 第一路占空比 0-100
uint8_t pwm_duty2=50;  // 第二路占空比 0-100

uint16_t servo_type=180;//舵机类型
uint8_t servo_angle1=90;  // 第一路舵机角度
uint8_t servo_angle2=90;  // 第二路舵机角度

/*定义union类型*/
typedef union {
	uint32_t uint32val;
	int intval;
	float floatval;
}NUION32;


#define PWM1_PIN (TXD_PIN)
#define PWM2_PIN (RXD_PIN)



////////////////// pwm //////////////////////////
// 使用PWM控制LED。。。

ledc_channel_config_t  ledc_channel1= {
        .channel    = LEDC_CHANNEL_1,
        .duty       = 0,
        .gpio_num   = PWM1_PIN, 
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_0   
    };
ledc_channel_config_t  ledc_channel2= {
        .channel    = LEDC_CHANNEL_2,
        .duty       = 0,
        .gpio_num   = PWM2_PIN, 
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_0   
    };

//设置通道1
void set_pwnch1(uint16_t duty,int time_ms){

   ledc_set_fade_with_time(ledc_channel1.speed_mode,ledc_channel1.channel, duty, time_ms);
   ledc_fade_start(ledc_channel1.speed_mode,ledc_channel1.channel, LEDC_FADE_NO_WAIT);
}

//设置通道2
void set_pwnch2(uint16_t duty,int time_ms){
   ledc_set_fade_with_time(ledc_channel2.speed_mode,ledc_channel2.channel, duty, time_ms);
   ledc_fade_start(ledc_channel2.speed_mode,ledc_channel2.channel, LEDC_FADE_NO_WAIT);
}    


void pwm_ledc_init(void){

     //1. PWM: 定时器配置
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
        .freq_hz = pwm_freq,                      // frequency of PWM signal
        .speed_mode = LEDC_LOW_SPEED_MODE,           // timer mode
        .timer_num = LEDC_TIMER_0,            // timer index
        .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
    };
    ledc_timer_config(&ledc_timer);
    //2. PWM:通道配置
    ledc_channel_config(&ledc_channel1); 
    ledc_channel_config(&ledc_channel2);

    //3.PWM:使用硬件渐变
    ledc_fade_func_install(0);

    set_pwnch1(81.92*pwm_duty1,0);
    set_pwnch2(81.92*pwm_duty2,0);
}

void pwm_ledc_off(void){
    ledc_stop(ledc_channel1.speed_mode,ledc_channel1.channel,0);
    ledc_stop(ledc_channel2.speed_mode,ledc_channel2.channel,0);
}


void gpio_init(void)
{

   gpio_set_direction(PWR_EN, GPIO_MODE_OUTPUT);
   gpio_set_level(PWR_EN, 0);

   gpio_pad_select_gpio(ADC_SCLK);
   gpio_set_direction(ADC_SCLK, GPIO_MODE_OUTPUT);
   gpio_set_level(ADC_SCLK, 1);

   gpio_pad_select_gpio(ADC_CS);
   gpio_set_direction(ADC_CS, GPIO_MODE_OUTPUT);
   gpio_set_level(ADC_CS, 1);

   gpio_pad_select_gpio(ADC_DIN);
   gpio_set_direction(ADC_DIN, GPIO_MODE_OUTPUT);
   gpio_set_level(ADC_DIN, 1);

   gpio_pad_select_gpio(ADC_DOUT);
   gpio_set_direction(ADC_DOUT, GPIO_MODE_INPUT);


   gpio_pad_select_gpio(BEEP);
   gpio_set_direction(BEEP, GPIO_MODE_OUTPUT);
   gpio_set_level(BEEP, 0);

   gpio_pad_select_gpio(CANVPK_PIN);
   gpio_set_direction(CANVPK_PIN, GPIO_MODE_OUTPUT);
   gpio_set_level(CANVPK_PIN, 1);

   gpio_pad_select_gpio(V16_EN);
   gpio_set_direction(V16_EN, GPIO_MODE_OUTPUT);
   gpio_set_level(V16_EN, 0);

   gpio_pad_select_gpio(PWR_KEY);
   gpio_set_direction(PWR_KEY, GPIO_MODE_INPUT);

   gpio_pad_select_gpio(KEY1);
   gpio_set_direction(KEY1, GPIO_MODE_INPUT);

   gpio_pad_select_gpio(KEY2);
   gpio_set_direction(KEY2, GPIO_MODE_INPUT);

   gpio_pad_select_gpio(CHRG);
   gpio_set_direction(CHRG, GPIO_MODE_INPUT);
   gpio_pullup_en(CHRG);

   gpio_pad_select_gpio(SWA);
   gpio_set_direction(SWA, GPIO_MODE_INPUT);

   gpio_pad_select_gpio(SWB);
   gpio_set_direction(SWB, GPIO_MODE_INPUT);

   gpio_pad_select_gpio(SWK);
   gpio_set_direction(SWK, GPIO_MODE_INPUT);

//    gpio_pad_select_gpio(MUX_IOA);
//    gpio_set_direction(MUX_IOA, GPIO_MODE_INPUT);

//    gpio_pad_select_gpio(MUX_IOB);
//    gpio_set_direction(MUX_IOB, GPIO_MODE_INPUT);



   gpio_pad_select_gpio(SEN);
   gpio_set_direction(SEN, GPIO_MODE_INPUT);
   gpio_pullup_en(SEN);	
}


// 里面什么也没有写，但是可以用来测试
void other_task(void *arg)
{

	while (1) 
    {

        vTaskDelay(pdMS_TO_TICKS(500));



        // while(gpio_get_level(PWR_KEY)==0)
        // {
        //     ESP_LOGI(TAG, "key");
        //     vTaskDelay(pdMS_TO_TICKS(500));
        //     gpio_set_level(PWR_EN, 0);
        // }

//  if(gpio_get_level(KEY1)==0) ESP_LOGI(TAG, "\r\n  KEY1  \r\n");
//  if(gpio_get_level(KEY2)==0) ESP_LOGI(TAG, "\r\n  KEY2  \r\n");
// if(gpio_get_level(CHRG)==0) ESP_LOGI(TAG, "\r\n  CHRG  \r\n");
// if(gpio_get_level(SWA)==0) ESP_LOGI(TAG, "\r\n  SWA  \r\n");
// if(gpio_get_level(SWB)==0) ESP_LOGI(TAG, "\r\n  SWB  \r\n");
// if(gpio_get_level(SWK)==0) ESP_LOGI(TAG, "\r\n  SWK  \r\n");
 //if(gpio_get_level(SEN)==0) ESP_LOGI(TAG, "\r\n  SEN  \r\n");



   }
}


////////////////////////////读写配置文件////////////////////////////

uint8_t Already_saved=0x34;

void write_config_in_nvs(uint8_t config)
{
    nvs_handle_t config_get_handle;
    NUION32 union32;

    nvs_open("parameter", NVS_READWRITE, &config_get_handle);
    ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"Alreadysaved", Already_saved) );

    switch (config)
    {
    case CONFIG_CAL: //标定值
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"Zero_dcv0", Zero_dcv_data[0]) );
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"Zero_dcv1", Zero_dcv_data[1]) );
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"Zero_dcv2", Zero_dcv_data[2]) );
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"Zero_acv0", Zero_acv_data[0]) );
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"Zero_acv1", Zero_acv_data[1]) );
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"Zero_acv2", Zero_acv_data[2]) );
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"Zero_dca0", Zero_dca_data[0]) );
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"Zero_dca1", Zero_dca_data[1]) );
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"Zero_dca2", Zero_dca_data[2]) );
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"Zero_aca0", Zero_aca_data[0]) );
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"Zero_aca1", Zero_aca_data[1]) );
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"Zero_2wr0", Zero_2wr_data[0]) );
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"Zero_2wr1", Zero_2wr_data[1]) );
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"Zero_2wr2", Zero_2wr_data[2]) );
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"Zero_2wr3", Zero_2wr_data[3]) );

        union32.floatval=Slope_dcv_data[0];
        ESP_ERROR_CHECK( nvs_set_u32(config_get_handle,"Slope_dcv0", union32.uint32val));
        union32.floatval=Slope_dcv_data[1];
        ESP_ERROR_CHECK( nvs_set_u32(config_get_handle,"Slope_dcv1", union32.uint32val));
        union32.floatval=Slope_dcv_data[2];
        ESP_ERROR_CHECK( nvs_set_u32(config_get_handle,"Slope_dcv2", union32.uint32val));
       
        union32.floatval=Slope_dca_data[0];
        ESP_ERROR_CHECK( nvs_set_u32(config_get_handle,"Slope_dca0", union32.uint32val));
        union32.floatval=Slope_dca_data[1];
        ESP_ERROR_CHECK( nvs_set_u32(config_get_handle,"Slope_dca1", union32.uint32val));
        union32.floatval=Slope_dca_data[2];
        ESP_ERROR_CHECK( nvs_set_u32(config_get_handle,"Slope_dca2", union32.uint32val));
        
        union32.floatval=Current_2wr_data[0];
        ESP_ERROR_CHECK( nvs_set_u32(config_get_handle,"Current_2wr0", union32.uint32val));
        union32.floatval=Current_2wr_data[1];
        ESP_ERROR_CHECK( nvs_set_u32(config_get_handle,"Current_2wr1", union32.uint32val));
        union32.floatval=Current_2wr_data[2];
        ESP_ERROR_CHECK( nvs_set_u32(config_get_handle,"Current_2wr2", union32.uint32val));
        union32.floatval=Current_2wr_data[3];
        ESP_ERROR_CHECK( nvs_set_u32(config_get_handle,"Current_2wr3", union32.uint32val));

        ESP_ERROR_CHECK( nvs_set_u16(config_get_handle,"Slope_dcvout", Slope_dcvout) );
        ESP_ERROR_CHECK( nvs_set_u16(config_get_handle,"Slope_dcaout", Slope_dcaout) );
        break;
    case CONFIG_FUN: //档位
        
        break;
    case CONFIG_OTHER: //其他参数
        ESP_ERROR_CHECK( nvs_set_str(config_get_handle,"device_ID",(const char *)device_ID) );
        ESP_ERROR_CHECK( nvs_set_str(config_get_handle,"password",(const char *)mqtt_password) );
        ESP_ERROR_CHECK( nvs_set_str(config_get_handle,"QRcode",(const char *)QRcode) );
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"shortcut_F1", shortcut_F1) );
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"shortcut_F2", shortcut_F2) );
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"r_buzzer", r_buzzer_threshold) );
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"d_buzzer", d_buzzer_threshold) );    
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"choice_temp", choice_temp) );  
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"VI_select", setup_VI_select) );  
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"VI_REV1", vi_reverse1) );  
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"VI_REV2", vi_reverse2) );  
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"VI_REV3", vi_reverse3) ); 
        ESP_ERROR_CHECK( nvs_set_i32(config_get_handle,"baud_rate", uart_config.baud_rate) );  
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"data_bits", uart_config.data_bits) ); 
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"stop_bits", uart_config.stop_bits) ); 
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"parity", uart_config.parity) ); 
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"data_hex", uart_data_hex) ); 
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"backlight", backlight) ); 
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"backtime", backtime) ); 
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"auto_off", auto_off) );
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"k_sound", key_sound) ); 
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"o_sound", on_off_sound) ); 
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"p_sound", low_power_sound) ); 
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"wifi_on", wifi_on) ); 
        ESP_ERROR_CHECK( nvs_set_u16(config_get_handle,"c_freq", current_freq) );
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"choice_dcv", choice_dcv) );
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"choice_acv", choice_acv) );
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"choice_dca", choice_dca) );
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"choice_2wr", choice_2wr) );
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"choice_wdc", choice_wdc) );
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"choice_aca", choice_aca) );
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"choice_wac", choice_wac) );
        ESP_ERROR_CHECK( nvs_set_u8(config_get_handle,"sd_onoff", sd_onoff) );
        break;
    
    default:
        break;
    }

    nvs_close(config_get_handle);
}

static void read_config_in_nvs(void)
{
    nvs_handle_t config_get_handle;
    uint8_t u8ConfigVal;
    uint8_t Len;
    NUION32 union32;

    nvs_open("parameter", NVS_READWRITE, &config_get_handle);  
    nvs_get_u8(config_get_handle, "Alreadysaved", &u8ConfigVal);
     ESP_LOGE(TAG,"u8ConfigVal:%X \r\n",u8ConfigVal);
    if (u8ConfigVal == Already_saved)
    {
        ESP_LOGI(TAG, "----Get parameter OK----");

        Len = sizeof(device_ID);
        nvs_get_str(config_get_handle, "device_ID", (char *)device_ID, &Len);

        Len = sizeof(mqtt_password);
        nvs_get_str(config_get_handle, "password", (char *)mqtt_password, &Len);

        Len = sizeof(QRcode);
        nvs_get_str(config_get_handle, "QRcode", (char *)QRcode, &Len);


        nvs_get_u8(config_get_handle, "shortcut_F1", &shortcut_F1);
        nvs_get_u8(config_get_handle, "shortcut_F2", &shortcut_F2);

        nvs_get_i32(config_get_handle, "Zero_dcv0", &Zero_dcv_data[0]);
        nvs_get_i32(config_get_handle, "Zero_dcv1", &Zero_dcv_data[1]);
        nvs_get_i32(config_get_handle, "Zero_dcv2", &Zero_dcv_data[2]);
        nvs_get_i32(config_get_handle, "Zero_acv0", &Zero_acv_data[0]);
        nvs_get_i32(config_get_handle, "Zero_acv1", &Zero_acv_data[1]);
        nvs_get_i32(config_get_handle, "Zero_acv2", &Zero_acv_data[2]);
        nvs_get_i32(config_get_handle, "Zero_dca0", &Zero_dca_data[0]);
        nvs_get_i32(config_get_handle, "Zero_dca1", &Zero_dca_data[1]);
        nvs_get_i32(config_get_handle, "Zero_dca2", &Zero_dca_data[2]);
        nvs_get_i32(config_get_handle, "Zero_aca0", &Zero_aca_data[0]);
        nvs_get_i32(config_get_handle, "Zero_aca1", &Zero_aca_data[1]);
        nvs_get_i32(config_get_handle, "Zero_2wr0", &Zero_2wr_data[0]);
        nvs_get_i32(config_get_handle, "Zero_2wr1", &Zero_2wr_data[1]);
        nvs_get_i32(config_get_handle, "Zero_2wr2", &Zero_2wr_data[2]);
        nvs_get_i32(config_get_handle, "Zero_2wr3", &Zero_2wr_data[3]);

        nvs_get_u32(config_get_handle, "Slope_dcv0", &union32.uint32val);
        Slope_dcv_data[0]=union32.floatval;
        nvs_get_u32(config_get_handle, "Slope_dcv1", &union32.uint32val);
        Slope_dcv_data[1]=union32.floatval;
        nvs_get_u32(config_get_handle, "Slope_dcv2", &union32.uint32val);
        Slope_dcv_data[2]=union32.floatval;
    
        nvs_get_u32(config_get_handle, "Slope_dca0", &union32.uint32val);
        Slope_dca_data[0]=union32.floatval;
        nvs_get_u32(config_get_handle, "Slope_dca1", &union32.uint32val);
        Slope_dca_data[1]=union32.floatval;
        nvs_get_u32(config_get_handle, "Slope_dca2", &union32.uint32val);
        Slope_dca_data[2]=union32.floatval;
        
        nvs_get_u32(config_get_handle, "Current_2wr0", &union32.uint32val);
        Current_2wr_data[0]=union32.floatval;
        nvs_get_u32(config_get_handle, "Current_2wr1", &union32.uint32val);
        Current_2wr_data[1]=union32.floatval;
        nvs_get_u32(config_get_handle, "Current_2wr2", &union32.uint32val);
        Current_2wr_data[2]=union32.floatval;
        nvs_get_u32(config_get_handle, "Current_2wr3", &union32.uint32val);
        Current_2wr_data[3]=union32.floatval;

        nvs_get_u16(config_get_handle, "Slope_dcvout", &Slope_dcvout);
        nvs_get_u16(config_get_handle, "Slope_dcaout", &Slope_dcaout);

        nvs_get_u8(config_get_handle, "r_buzzer", &r_buzzer_threshold);
        if((r_buzzer_threshold<1)||(r_buzzer_threshold>100)) r_buzzer_threshold=30;

        nvs_get_u8(config_get_handle, "d_buzzer", &d_buzzer_threshold);
        if((d_buzzer_threshold<1)||(d_buzzer_threshold>100)) d_buzzer_threshold=30;

        nvs_get_u8(config_get_handle, "choice_temp", &choice_temp);
        if(choice_temp>4) choice_temp=0;

        nvs_get_u8(config_get_handle, "VI_select", &setup_VI_select);
        if(setup_VI_select>2) setup_VI_select=0;

        nvs_get_u8(config_get_handle, "VI_REV1", &vi_reverse1);
        if(vi_reverse1>1) vi_reverse1=0;

        nvs_get_u8(config_get_handle, "VI_REV2", &vi_reverse2);
        if(vi_reverse2>1) vi_reverse2=0;

        nvs_get_u8(config_get_handle, "VI_REV3", &vi_reverse3);
        if(vi_reverse3>1) vi_reverse3=0;

        nvs_get_i32(config_get_handle, "baud_rate", &uart_config.baud_rate);
       if((uart_config.baud_rate<1200)||(uart_config.baud_rate>256000)) 
       {
          uart_config.baud_rate=115200;
       }
       else
       {
          nvs_get_u8(config_get_handle, "data_bits", &uart_config.data_bits);
          nvs_get_u8(config_get_handle, "stop_bits", &uart_config.stop_bits);
          nvs_get_u8(config_get_handle, "parity", &uart_config.parity);
          nvs_get_u8(config_get_handle, "data_hex", &uart_data_hex);
       }

       nvs_get_u8(config_get_handle, "backlight", &backlight);
       if((backlight<5)||(backlight>100)) backlight=80;

       nvs_get_u8(config_get_handle, "backtime", &backtime);
       if(backtime>99) backtime=5;

       nvs_get_u8(config_get_handle, "auto_off", &auto_off);
       if(auto_off>1) auto_off=0;

       nvs_get_u8(config_get_handle, "k_sound", &key_sound);
        if(key_sound>1) key_sound=0;

        nvs_get_u8(config_get_handle, "o_sound", &on_off_sound);
        if(on_off_sound>1) on_off_sound=0;

        nvs_get_u8(config_get_handle, "p_sound", &low_power_sound);
        if(low_power_sound>1) low_power_sound=0;

        nvs_get_u8(config_get_handle, "wifi_on", &wifi_on);
        if(wifi_on>1) wifi_on=0;

        nvs_get_u16(config_get_handle, "c_freq", &current_freq);
        if((current_freq<2)||(current_freq>60000)) current_freq=5;

        nvs_get_u8(config_get_handle, "choice_dcv", &choice_dcv);
        if(choice_dcv>3) choice_dcv=VOL_AUTO;

        nvs_get_u8(config_get_handle, "choice_acv", &choice_acv);
        if(choice_acv>3) choice_acv=VOL_AUTO;

        nvs_get_u8(config_get_handle, "choice_dca", &choice_dca);
        if(choice_dca>2) choice_dca=CUR_A;

        nvs_get_u8(config_get_handle, "choice_2wr", &choice_2wr);
        if(choice_2wr>4) choice_2wr=RES_AUTO;

        nvs_get_u8(config_get_handle, "choice_wdc", &choice_wdc);
        if(choice_wdc>2) choice_wdc=CUR_A;

        nvs_get_u8(config_get_handle, "choice_aca", &choice_aca);
        if(choice_aca>2) choice_aca=CUR_A;

        nvs_get_u8(config_get_handle, "choice_wac", &choice_wac);
        if(choice_wac>2) choice_wac=CUR_A;

        nvs_get_u8(config_get_handle, "sd_onoff", &sd_onoff);
        if(sd_onoff>1) sd_onoff=0;

       nvs_close(config_get_handle);

         //ESP_LOGI(TAG, "Slope_dcv_data[2] = %f",Slope_dcv_data[2]);

       //  ESP_LOGI(TAG, "device_ID = %s",device_ID);

        // ESP_LOGI(TAG, "Passward = %s",Passward);
        // ESP_LOGI(TAG, "Backlight = %d",Backlight);
        // ESP_LOGI(TAG, "Volume = %d",Volume);
        // ESP_LOGI(TAG, "WorkMode = %d",WorkMode);
        // ESP_LOGI(TAG, "Speed = %d",Speed);
        // ESP_LOGI(TAG, "OffPower = %d",OffPower); 
        // ESP_LOGI(TAG, "AutoPlay = %d",AutoPlay); 
    }
    else
    {
       // ESP_LOGI(TAG, "----Get parameter Fail----");
        nvs_close(config_get_handle);
        write_config_in_nvs(CONFIG_CAL);  
        write_config_in_nvs(CONFIG_FUN);  
        write_config_in_nvs(CONFIG_OTHER);  
    }

}


void app_main(void)
{
    gpio_init();
    //pwm_ledc_init();
    usart0_init();
    
    /// 检查NVS是否初始化成功，如果不成功就检查是不是没有多余空间了或者是不是找到了新版本，如果是，擦除NVS重新初始化
      esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
    read_config_in_nvs();//读取配置，目前不需要
    
    
    // 从上往下，优先级由低至高
    // xTaskCreate(进程函数指针, 别名, 分配堆栈大小, NULL, 优先级, NULL);
    xTaskCreate(key_task, "KEY", 1024*8, NULL, 2, NULL);
    xTaskCreate(adc_task, "ADC", 1024*4, NULL, 3, NULL);
    
    //xTaskCreate(canbus_task, "CANBUS", 1024*2, NULL, 5, NULL);
    
    xTaskCreate(lcd_ui_task, "LCD_UI", 1024*16, NULL, 6, NULL);
    xTaskCreate(wifinet_task, "WIFINET", 1024*8, NULL, 7, NULL);
    xTaskCreate(micro_sd_task, "SD", 1024*4, NULL, 8, NULL);
    xTaskCreate(switch_fun_task, "SWITCH", 1024*2, NULL, 14, NULL);
    xTaskCreate(other_task, "other", 1024*2, NULL, 15, NULL);
    xTaskCreate(usart0_task, "usart0", 1024*4, NULL, 16, NULL);

    // 相对延时函数，执行后延时使得其他进程得以运行
    vTaskDelay(pdMS_TO_TICKS(5000));

    uint32_t strapping = (*((volatile uint32_t *)(GPIO_STRAP_REG)));
    //("strapping=%04x\n", strapping);

    //printf("Version=%s\n", ver);
    //printf(" ------------------esp_get_free_heap_size : %d \n", esp_get_free_heap_size());

   // char* p = (char*)malloc(10240);
   // free(p);

   //获取剩余的dram大小
	size_t dram = heap_caps_get_free_size(MALLOC_CAP_8BIT);

	//获取剩余的iram大小
	size_t iram = heap_caps_get_free_size(MALLOC_CAP_32BIT) - heap_caps_get_free_size(MALLOC_CAP_8BIT);

	//获取剩余的dram大小与heap_caps_get_free_size函数一样
	uint32_t data = xPortGetFreeHeapSize();


	//获取最大的连续的堆区空间
	size_t heapmax = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

	//获取栈区的最高水位线(也就是栈顶，意味着最大栈区空间)
	int stackmark=uxTaskGetStackHighWaterMark(NULL);
	
	//("data=%d\n", data);
	//printf("dram=%d\n", dram);
	//printf("iram=%d\n", iram);
	//printf("max=%d\n", heapmax);
	//printf("stackmark=%d\n", stackmark);



    


    
}
