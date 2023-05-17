
#include "main.h"
#include "adc_dac.h"

#define    PRINTF_VALUE    0  //是否打印测量值


const char *ADC_TASK_TAG = "ADC_TASK";

uint32_t measured_value = 0; //测量值 全F为超量程
uint32_t carried_value1 = 0; //携带的值1，功率档时为测量电压 温度档时为电阻值
uint32_t carried_value2 = 0; //携带的值2，功率档时为测量电流 温度档时为华氏度
uint8_t carried_value2_sign = 0; 

uint8_t sign = 0; //正负号 为1负数
uint8_t unit = 0; //测量单位

uint8_t r_buzzer_threshold=30;//通断档蜂鸣器阈值 单位R  范围1-100
uint8_t d_buzzer_threshold=30;//通断档蜂鸣器阈值 单位0.01V 范围0.01V-1V

struct tm ADC_timeinfo;

uint8_t electricity_st = 100; //电池电量：0为电量低，255为正常，254为充电状态，1-100为百分比

uint8_t lsdata[3]={0,0,0};
uint8_t lsdata2[1]={0};

uint8_t no_need_up=0;   // 0 需要上传  1 不需要上传，这样看来是不是强制设置为1就可以了，不可以。。。

uint8_t de_dcout=0; //标定直流源
uint16_t Slope_dcvout=10000;//直流电压源斜率校准数值
uint16_t Slope_dcaout=30000;//直流电压源斜率校准数值

/**************************************************/
/*        读AD7791                                */
/*                                                */
/**************************************************/
void write_ad7791(unsigned char byte)   /*  写一个字节   7791*/
{
   char count;
   gpio_set_level(ADC_CS, 0);
   ets_delay_us(30);
   for(count=0;count<=7;count++)
   {     
      gpio_set_level(ADC_SCLK, 0);
      ets_delay_us(30);
      if((byte & 0x80)==0)
         gpio_set_level(ADC_DIN, 0);
      else
         gpio_set_level(ADC_DIN, 1);         	
      byte=byte<<1;
      
      gpio_set_level(ADC_SCLK, 1);  
      ets_delay_us(30);     
   }
   ets_delay_us(30);
   gpio_set_level(ADC_CS, 1); 
     
}




int read_ad7791() /*从 AD7791读3个字节数据，24位数据*/
{

   char count;
   long adval;
   adval=0x00000000;
   gpio_set_level(ADC_CS, 0);   
   
   while (gpio_get_level(ADC_DOUT));
   ets_delay_us(5);
   
   for(count=0;count<=23;count++)
   {
      gpio_set_level(ADC_SCLK, 0);
      ets_delay_us(5);
      adval=adval<<1;
      gpio_set_level(ADC_SCLK, 1);
      ets_delay_us(5);
      if(gpio_get_level(ADC_DOUT)!=0) 
      {
         adval=adval+1;
      }
   }
   while (gpio_get_level(ADC_DOUT))
   {
      
   }
	gpio_set_level(ADC_CS, 0); 

   return(adval-0x7FFFFF);
}  


void spi_adc_init(uint8_t velocity)
{
   char i;
   gpio_set_level(ADC_CS, 0);  
   gpio_set_level(ADC_DIN, 1); 
   for(i=0;i<32;i++)
   {
      gpio_set_level(ADC_SCLK, 0);
      ets_delay_us(10);
      gpio_set_level(ADC_SCLK, 1);
      ets_delay_us(10);
   }
   gpio_set_level(ADC_CS, 1);  
   vTaskDelay(pdMS_TO_TICKS(50));

  write_ad7791(0x10); 
  ets_delay_us(10);
  //write_ad7791(0x02); //带缓冲
  write_ad7791(0x00); //不带缓冲
  ets_delay_us(10);
  if(velocity!=0)
  {
     write_ad7791(0x20); 
     ets_delay_us(10); 
     write_ad7791(0x01); //100Hz
  }
  else
  {
     write_ad7791(0x20); 
     ets_delay_us(10); 
     write_ad7791(0x04); //
  }

  ets_delay_us(10);
  write_ad7791(0x3C); //开始连续转换
  gpio_set_level(ADC_DIN, 0);
}



uint16_t signal_V=50; //0-100  10V
uint16_t signal_A=100;//0-300  30mA

/// DAC
void GP8403_init()
{
   i2c_cmd_handle_t cmd = i2c_cmd_link_create();
   i2c_master_start(cmd);
   i2c_master_write_byte(cmd, 0xB8, 1);
	i2c_master_write_byte(cmd, 0x01, 1);
   i2c_master_write_byte(cmd, 0x11, 1);
   i2c_master_stop(cmd);
   esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
   if (ret != ESP_OK) {
       //ESP_LOGI(ADC_TASK_TAG, "GP8403INIT=ERROR =%d",ret);
   }
   i2c_cmd_link_delete(cmd);
}



void GP8403_WriteReg()
{
	  uint8_t dat1_L;
	  uint8_t dat1_H;
	  uint8_t dat2_L;
	  uint8_t dat2_H;
	

	  uint16_t dat1=40.95*(float)signal_V*(10000.0/Slope_dcvout);
	  uint16_t dat2=(40.95*(float)signal_A*(30000.0/Slope_dcaout))/3;
	  
	  dat1<<=4;
	  dat2<<=4;
	  
	  dat1_L=dat1&0xFF;
	  dat1_H=(dat1>>8);
	
	  dat2_L=dat2&0xFF;
	  dat2_H=(dat2>>8);

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
   i2c_master_start(cmd);
   i2c_master_write_byte(cmd, 0xB8, 1);
	i2c_master_write_byte(cmd, 0x02, 1);
   i2c_master_write_byte(cmd, dat1_L, 1);
   i2c_master_write_byte(cmd, dat1_H, 1);
   i2c_master_write_byte(cmd, dat2_L, 1);
   i2c_master_write_byte(cmd, dat2_H, 1);
   i2c_master_stop(cmd);
   esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
   if (ret != ESP_OK) {
       //(ADC_TASK_TAG, "GP8403=ERROR =%d",ret);
   }
   i2c_cmd_link_delete(cmd);
}






//测量直流电压
void measure_dcv(float voltage)
{
	uint32_t Vol=0;

   if(voltage>=0)
   {
      sign=0;
   }
   else
   {
      sign=1;
   }

   if(gear_sw==ASW_DCV0)//最大1000V
   {
         Vol=calibration_dcv(voltage,0,99349000);//100V标定
         //Vol= abs(voltage);  //求绝对值
         //Vol*=903.96;
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DCV0 Vol=%d\r\n",Vol);
         if((Vol<100000000)&(choice_dcv==VOL_AUTO))//电压小于100V 切换档位
         {
               analog_switch(ASW_DCV1);
               
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               //Read_LTC();
         }
   }
   else if(gear_sw==ASW_DCV1)//最大100V
   {                                  
         Vol=calibration_dcv(voltage,1,99349000);//100V标定
         //Vol= abs(voltage);  //求绝对值
         //Vol*=91.54;  
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DCV1 Vol=%d\r\n",Vol);
         if((Vol<10000000)&(choice_dcv==VOL_AUTO))//电压小于10V 切换档位
         {
               analog_switch(ASW_DCV2);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               //Read_LTC();
         }
         else if((Vol>110000000)&(choice_dcv==VOL_AUTO))//电压大于110V 切换档位
         {
               analog_switch(ASW_DCV0);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               //Read_LTC();
         }
   }
   else if(gear_sw==ASW_DCV2)//最大10V
   {
         Vol=calibration_dcv(voltage,2,9992000);//10V标定
         //Vol= abs(voltage);  //求绝对值
         //Vol*=10.04;
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DCV2 Vol=%d\r\n",Vol);
         if((Vol>11000000)&(choice_dcv==VOL_AUTO))//电压大于11V 切换档位
         {
               analog_switch(ASW_DCV1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               //Read_LTC();
         }
   }
   else
   {
         analog_switch(ASW_DCV2);
   }

   if(Vol<10000000)//uV 小于10V
   {
         unit=0x01;
         measured_value=Vol;
   }
   else //mV
   {
         unit=0x02;
         measured_value=Vol/1000;
   }

}

//测量交流电压
void measure_acv(float voltage)
{
	uint32_t Vol;
   
   sign=0; //交流电压不能为负
   if(voltage>=0)
   {
      Vol= voltage; 
   }
   else
   {
      Vol=0;
   }

   if(gear_sw==ASW_ACV0)//最大1000V
   {
         Vol=calibration_acv(voltage,0);//100V标定
         //Vol*=1139;
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ACV0 Vol=%d\r\n",Vol);
         
         if((Vol<100000000)&(choice_acv==VOL_AUTO))//电压小于100V 切换档位
         {
               analog_switch(ASW_ACV1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
         }
   }
   else if(gear_sw==ASW_ACV1)//最大100V
   {
         Vol=calibration_acv(voltage,1);//50V标定
         //Vol*=115.6;
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ACV1 Vol=%d\r\n",Vol);
         if((Vol<10000000)&(choice_acv==VOL_AUTO))//电压小于10V 切换档位
         {
               analog_switch(ASW_ACV2);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               
         }
         else if((Vol>110000000)&(choice_acv==VOL_AUTO))//电压大于110V 切换档位
         {
               analog_switch(ASW_ACV0);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
         }
   }
   else if(gear_sw==ASW_ACV2)//最大10V
   {
         Vol=calibration_acv(voltage,2);//10V标定
         //Vol*=12.65;
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ACV2 Vol=%d\r\n",Vol);
         if((Vol>11000000)&(choice_acv==VOL_AUTO))//电压大于11V 切换档位
         {
               analog_switch(ASW_ACV1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
         }
   }
   else
   {
         analog_switch(ASW_ACV0);
   }
   if(Vol<10000000)//uV 小于10V
   {
         unit=0x01;
         measured_value=Vol;
   }
   else //mV
   {
         unit=0x02;
         measured_value=Vol/1000;
   }
}



//测量直流电流
void measure_dca(float voltage)
{
	uint32_t Vol;
   
   if(voltage>=0)
   {
      sign=0;
      Vol= voltage; 
   }
   else
   {
      sign=1;
      Vol= abs(voltage);  //求a的绝对值
   }

   if(choice_dca==CUR_UA) //uA 档位 最大5000uA
   {
         Vol=calibration_dca(voltage,0,2500000);//2.5mA标定
         //Vol*=4.963;
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DCuA Vol=%d\r\n",Vol);   
         unit=0x04;//nA
         measured_value=Vol;
   }
   else if(choice_dca==CUR_MA)//mA 档位 最大 500mA
   {
         Vol=calibration_dca(voltage,1,250000);//250mA标定
         //Vol*=0.493;
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DCmA Vol=%d\r\n",Vol);  
         unit=0x05;//uA
         measured_value=Vol;  
   }
   else if(choice_dca==CUR_A)//A 档位
   {
         Vol=calibration_dca(voltage,2,1002000);//1A标定
         //Vol*=102.2;
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DCA Vol=%d\r\n",Vol);
         unit=0x05;//uA
         measured_value=Vol;
   }
}

//测量交流电流
void measure_aca(float voltage)
{
	uint32_t Vol;
   
   sign=0; //交流不能为负
   if(voltage>=0)
   {
      Vol= voltage; 
   }
   else
   {
      Vol= 0;
   }

   if(choice_aca==CUR_MA)//mA 档位 最大 500mA
   {
         //Vol*=0.618;
         Vol=calibration_aca(voltage,0);
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ACmA Vol=%d\r\n",Vol);    
   }
   else if(choice_aca==CUR_A)//A 档位
   {
         //Vol*=125;
         Vol=calibration_aca(voltage,1);
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ACA Vol=%d\r\n",Vol);
   }

   unit=0x05;//uA
   measured_value=Vol;

}


//测量两线电阻
void measure_2wr(float voltage)
{
	//uint32_t Vol=0;
   float resistance=0;
   
   sign=0; //电阻不能为负
   // if(voltage<0)
   // {
   //    voltage=0; 
   // }

   if(gear_sw==ASW_2WR0)//最大 1KΩ
   {
         resistance=calibration_2wr(voltage,0,999.9);
         if(resistance>=10000000) resistance=10000000-1;             
         measured_value=((10000000*resistance)/(10000000-resistance))*1000000;  //最后乘以1000000是为了得到uΩ

         if(measured_value>=1200000000)//如果大于1.2KΩ,//单位是mΩ
         {
            if(choice_2wr==RES_AUTO)
            {
               measured_value/=1000;
               unit=0x09; //单位是mΩ
               analog_switch(ASW_2WR1);//切换档位
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               //Read_LTC();
            }
            else
            {
               measured_value=0xFFFFFFFF;
            } 
         }
         else 
         {
            if(measured_value>=1000000000)
            {
               measured_value/=1000;
               unit=0x09; //单位是mΩ
            }
            else
            {
               unit=0x08; //单位是uΩ
            }
            
         }

         
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ASW_2WR0 Vol=%d\r\n",measured_value);
   }
   else if(gear_sw==ASW_2WR1)//最大20KΩ
   {   
         resistance=calibration_2wr(voltage,1,19960.1);
         if(resistance>=10000000) resistance=10000000-1;             
         measured_value=((10000000*resistance)/(10000000-resistance))*1000;  //最后乘以1000是为了得到mΩ
         unit=0x09; //单位是mΩ

         if(measured_value<1100000)//电阻小于1100Ω 切换档位
         {
            if(choice_2wr==RES_AUTO)
            {
               analog_switch(ASW_2WR0);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               //Read_LTC();
            }
         }
         else if(measured_value>22000000)//电阻大于22KΩ 切换档位
         {
            if(choice_2wr==RES_AUTO)
            {
               analog_switch(ASW_2WR2);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               //Read_LTC();
            }
            else
            {
               measured_value=0xFFFFFFFF;
            }
         }

         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ASW_2WR1 Vol=%d\r\n",measured_value);   
   }
   else if(gear_sw==ASW_2WR2)//最大500KΩ
   {
         resistance=calibration_2wr(voltage,2,476190);
         if(resistance>=10000000) resistance=10000000-1;             
         measured_value=((10000000*resistance)/(10000000-resistance))*1000;  //最后乘以1000是为了得到mΩ
         unit=0x09; //单位是mΩ

         if(measured_value<21000000)//电阻小于20KΩ 切换档位
         {
            if(choice_2wr==RES_AUTO)
            {
               analog_switch(ASW_2WR1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               //Read_LTC();
            }
         }
         else if(measured_value>600000000)//电阻大于600KΩ 切换档位
         {
            if(choice_2wr==RES_AUTO)
            {
               analog_switch(ASW_2WR3);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               //Read_LTC();
            }
            else
            {
               measured_value=0xFFFFFFFF;
            }
         }
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ASW_2WR2 Vol=%d\r\n",measured_value);    
   }
   else if(gear_sw==ASW_2WR3)//最大 100MΩ  
   {
         resistance=calibration_2wr(voltage,3,5000000);
         if(resistance>=10000000) resistance=10000000-1; 
         measured_value=((10000000*resistance)/(10000000-resistance));
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ASW_2WR3 Vol=%d\r\n",measured_value);
         unit=0x0A; //单位是Ω
         if(measured_value<590000)//电阻小于590KΩ 切换档位
         {
            if(choice_2wr==RES_AUTO)
            {
               analog_switch(ASW_2WR2);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               vTaskDelay(pdMS_TO_TICKS(66));
               read_adc_uv(1);
               //Read_LTC();
            }
         }
         else if(measured_value>110000000)//电阻大于110MΩ 
         {
             measured_value=0xFFFFFFFF;
         }
   }
   else
   {
         analog_switch(ASW_2WR3);
   } 
}


//测量四线电阻
void measure_4wr(float voltage)
{
   int i=0;
   float resistance=0;
   static uint8_t r4w_add=0;
   static int32_t Vol_H=0;
   static int32_t Vol_L=0;
	int32_t Vol=0;
   static float v_buf[4];

   sign=0; //电阻不能为负
   // if(voltage<0)
   // {
   //    voltage=0; 
   // }
   unit=0x08; //单位是uΩ
   
   

   if(++r4w_add>=10)
   {
      v_buf[r4w_add-7]=voltage;
      r4w_add=0;
      voltage=0;
      for(i=0;i<4;i++)
      {
         voltage+=v_buf[i];
         ESP_LOGI(ADC_TASK_TAG,"v_buf[%d]=%f\r\n",i,v_buf[i]);
      }

      voltage/=4;

      resistance=calibration_4wr(voltage);
      //voltage= voltage/5000;//分压10倍，电压除以5000uA得到被测电阻单位Ω
      if(resistance>=10000000) resistance=10000000-1;             
      Vol=((10000000*resistance)/(10000000-resistance))*1000000;  //最后乘以1000000是为了得到uΩ

      if(gear_sw==ASW_4WHS)
      {
         Vol_H=Vol;
         analog_switch(ASW_4WLS);
         
      }
      else if(gear_sw==ASW_4WLS)
      {
         Vol_L=Vol;
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"Vol_H=%d\r\n",Vol_H);
         if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"Vol_L=%d\r\n",Vol_L);

         //ESP_LOGI(ADC_TASK_TAG,"Vol_H=%d\r\n",Vol_H);
         //ESP_LOGI(ADC_TASK_TAG,"Vol_L=%d\r\n",Vol_L);
         analog_switch(ASW_4WHS);
         if((Vol_H>250000000)|(Vol_H<-300000)|(Vol_L>250000000)|(Vol_L<-300000))
         {
            measured_value=0xFFFFFFFF;
         }
         else if(Vol_H>Vol_L)
         {
            measured_value=Vol_H-Vol_L;
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ASW_4WR Vol=%d\r\n",measured_value);
         }
         else
         {
            measured_value=0;
         }
         if(measured_value>=250000000)
         {
            measured_value=0xFFFFFFFF;
         }
      }  
   }
   else if(r4w_add>=7)
   {
      v_buf[r4w_add-7]=voltage;
   }
   
}

//测量二极管
void measure_diode(float voltage)
{
   static uint8_t beep_b=0;
   sign=0;
   if(voltage<0)
   {
      voltage=0; 
   }
   unit=0x01;//uV
   measured_value=calibration_dcv(voltage,2,10000000);//10V标定
  // measured_value=voltage*10.04;
   if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DIODE Vol=%d\r\n",measured_value);
   if(measured_value>5000000)//电压大于5V
   {
      measured_value=0xFFFFFFFF;
      if(beep_b==1)
      {
         beep_b=0;
         gpio_set_level(BEEP, 0);
      }
   }
   else
   {
      if(measured_value<=(d_buzzer_threshold*10000))
      {
         if(beep_b==0)
         {
            beep_b=1;
            gpio_set_level(BEEP, 1);
         }
      }
      else
      {
         if(beep_b==1)
         {
            beep_b=0;
            gpio_set_level(BEEP, 0);
         }
      }
   }
  
  
}

//通断档
void measure_beep(float voltage)
{
   float resistance=0;
   static uint8_t beep_b=0;
   sign=0; //电阻不能为负
   if(voltage<0)
   {
      voltage=0; 
   }
   resistance=calibration_2wr(voltage,0,999.9);
   //resistance= (voltage)/500;//分压10倍，电压除以5000uA得到被测电阻单位Ω
   if(resistance>=10000000) resistance=10000000-1;             
   measured_value=((10000000*resistance)/(10000000-resistance))*1000000;  //最后乘以1000000是为了得到uΩ

   unit=0x08; //单位是uΩ
   if(measured_value>=1000000000)//如果大于1KΩ,//单位是mΩ
   {
      measured_value=0xFFFFFFFF;
      if(beep_b==1)
      {
         beep_b=0;
         gpio_set_level(BEEP, 0);
      }
   }
   else
   {
      if(measured_value<=(r_buzzer_threshold*1000000))
      {
         if(beep_b==0)
         {
            beep_b=1;
            gpio_set_level(BEEP, 1);
         }
      }
      else
      {
         if(beep_b==1)
         {
            beep_b=0;
            gpio_set_level(BEEP, 0);
         }
      }
   }

   if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ASW_2WR0 Vol=%d\r\n",measured_value); 
}

//测量直流功率
void measure_dcw(float voltage)
{
   static uint8_t dcw_add=0;
   static uint32_t Vol_V=0;
   static float Vol_A=0;
   static uint8_t dcwv=ASW_DCW_MAV0;

   sign=0;
   if(voltage<0)
   {
      voltage= abs(voltage); 
   }
   if(++dcw_add>6)
   {
      dcw_add=0;

      if(choice_wdc==CUR_UA)
      {
         if(gear_sw==ASW_DCW_UAV0)//最大1000V
         {
               dcwv=gear_sw;
               //voltage*=903.96;
               voltage=calibration_dcv(voltage,0,100000000);//100V标定
               if(voltage<100000000)//电压小于100V 切换档位
               {
                  analog_switch(ASW_DCW_UAV1); 
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else
               {
                  Vol_V=voltage/1000; //转换为mV
                  analog_switch(ASW_DCW_UA);  
               }
         }
         else if(gear_sw==ASW_DCW_UAV1)//最大100V
         {
               dcwv=gear_sw;
               //voltage*=91.54;
               voltage=calibration_dcv(voltage,1,100000000);//100V标定
               if(voltage<10000000)//电压小于10V 切换档位
               {
                  analog_switch(ASW_DCW_UAV2);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else if(voltage>110000000)//电压大于110V 切换档位
               {
                  analog_switch(ASW_DCW_UAV0);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else
               {
                  Vol_V=voltage/1000; //转换为mV
                  analog_switch(ASW_DCW_UA);
               }
         }
         else if(gear_sw==ASW_DCW_UAV2)//最大10V
         {
               dcwv=gear_sw;
               //voltage*=10.04;
               voltage=calibration_dcv(voltage,2,10000000);//10V标定
               if(voltage>11000000)//电压大于11V 切换档位
               {
                  analog_switch(ASW_DCW_UAV1);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else
               {
                  Vol_V=voltage/1000; //转换为mV
                  analog_switch(ASW_DCW_UA);
               }      
         }
         else if(gear_sw==ASW_DCW_UA)//uA 档位 最大5000uA
         {
           // Vol_A=voltage*4.963/1000000;//除1000000转换成mA
            Vol_A=calibration_dca(voltage,0,5000000)/1000000;//5mA标定
            carried_value1=Vol_V;
            carried_value2=Vol_A*1000;//uA 
            analog_switch(dcwv);
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DCV=%d\r\n",Vol_V);
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DCUA=%f\r\n",Vol_A);
            unit=0x0C; //单位 uW
            measured_value=Vol_A*Vol_V; 
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DCW=%d\r\n",measured_value);
         }
         else
         {
            if(choice_wdc==CUR_UA)
            {
               analog_switch(ASW_DCW_UAV0);
            }
            else if(choice_wdc==CUR_MA)
            {
               analog_switch(ASW_DCW_MAV0);
            }
            else if(choice_wdc==CUR_A)
            {
               analog_switch(ASW_DCW_AV0);
            }
         }
      }
      else if(choice_wdc==CUR_MA)
      {
         if(gear_sw==ASW_DCW_MAV0)//最大1000V
         {
               dcwv=gear_sw;
               //voltage*=903.96;
               voltage=calibration_dcv(voltage,0,100000000);//100V标定
               if(voltage<100000000)//电压小于100V 切换档位
               {
                  analog_switch(ASW_DCW_MAV1);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else
               {
                  Vol_V=voltage/1000; //转换为mV
                  analog_switch(ASW_DCW_MA);
               }
         }
         else if(gear_sw==ASW_DCW_MAV1)//最大100V
         {
               dcwv=gear_sw;
               //voltage*=91.54;
               voltage=calibration_dcv(voltage,1,100000000);//100V标定
               if(voltage<10000000)//电压小于10V 切换档位
               {
                  analog_switch(ASW_DCW_MAV2);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else if(voltage>110000000)//电压大于110V 切换档位
               {
                  analog_switch(ASW_DCW_MAV0);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else
               {
                  Vol_V=voltage/1000; //转换为mV
                  analog_switch(ASW_DCW_MA);
               }
         }
         else if(gear_sw==ASW_DCW_MAV2)//最大10V
         {
               dcwv=gear_sw;
               //voltage*=10.04;
               voltage=calibration_dcv(voltage,2,10000000);//10V标定
               if(voltage>11000000)//电压大于11V 切换档位
               {
                  analog_switch(ASW_DCW_MAV1);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else
               {
                  Vol_V=voltage/1000; //转换为mV
                  analog_switch(ASW_DCW_MA);
               }      
         }
         else if(gear_sw==ASW_DCW_MA)//mA 档位 最大 500mA
         {
            //Vol_A=voltage*0.493/1000;//除1000转换成mA
            Vol_A=calibration_dca(voltage,0,500000)/1000;//500mA标定   
            carried_value1=Vol_V;
            carried_value2=Vol_A*1000;//uA    
            analog_switch(dcwv);
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DCV=%d\r\n",Vol_V);
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DCMA=%f\r\n",Vol_A);
            unit=0x0C; //单位 uW
            measured_value=Vol_A*Vol_V; 
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DCW=%d\r\n",measured_value);
         }
         else
         {
            if(choice_wdc==CUR_UA)
            {
               analog_switch(ASW_DCW_UAV0);
            }
            else if(choice_wdc==CUR_MA)
            {
               analog_switch(ASW_DCW_MAV0);
            }
            else if(choice_wdc==CUR_A)
            {
               analog_switch(ASW_DCW_AV0);
            }
         }
      }
      else if(choice_wdc==CUR_A)
      {
         if(gear_sw==ASW_DCW_AV0)//最大1000V
         {
               dcwv=gear_sw;
               //voltage*=903.96;
               voltage=calibration_dcv(voltage,0,100000000);//100V标定
               if(voltage<100000000)//电压小于100V 切换档位
               {
                  analog_switch(ASW_DCW_AV1);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else
               {
                  Vol_V=voltage/1000; //转换为mV
                  analog_switch(ASW_DCW_A);
               }
         }
         else if(gear_sw==ASW_DCW_AV1)//最大100V
         {
               dcwv=gear_sw;
               //voltage*=91.54;
               voltage=calibration_dcv(voltage,1,100000000);//100V标定
               if(voltage<10000000)//电压小于10V 切换档位
               {
                  analog_switch(ASW_DCW_AV2);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else if(voltage>110000000)//电压大于110V 切换档位
               {
                  analog_switch(ASW_DCW_AV0);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else
               {
                  Vol_V=voltage/1000; //转换为mV
                  analog_switch(ASW_DCW_A);
               }
         }
         else if(gear_sw==ASW_DCW_AV2)//最大10V
         {
               dcwv=gear_sw;
               //voltage*=10.04;
               voltage=calibration_dcv(voltage,2,10000000);//10V标定
               if(voltage>11000000)//电压大于11V 切换档位
               {
                  analog_switch(ASW_DCW_AV1);           
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else
               {
                  Vol_V=voltage/1000; //转换为mV
                  analog_switch(ASW_DCW_A);

               }      
         }
         else if(gear_sw==ASW_DCW_A)//A 档位 最大 10A
         {
  
            Vol_A=calibration_dca(voltage,2,1000000)/1000;//1A标定
            carried_value1=Vol_V;
            carried_value2=Vol_A*1000;//uA     
            analog_switch(dcwv);
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"10A DCV=%d\r\n",Vol_V);
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"10A DCA=%f\r\n",Vol_A);
            if(voltage*Vol_V<1000000000) //小于1KW
            {
               unit=0x0C; //单位 uW
               measured_value=Vol_A*Vol_V; 
            }
            else
            {
               unit=0x0D; //单位 mW
               measured_value=Vol_A*Vol_V/1000; 
            }
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DCW=%d\r\n",measured_value);
         }
         else
         {
            if(choice_wdc==CUR_UA)
            {
               analog_switch(ASW_DCW_UAV0);
            }
            else if(choice_wdc==CUR_MA)
            {
               analog_switch(ASW_DCW_MAV0);
            }
            else if(choice_wdc==CUR_A)
            {
               analog_switch(ASW_DCW_AV0);
            }
         }
      }   
   }
}

//测量交流功率
void measure_acw(float voltage)
{
   static uint8_t acw_add=0;
   static uint32_t Vol_V=0;
   static float Vol_A=0;
   static uint8_t acwv=ASW_ACW_MAV0;

   sign=0;
   if(voltage<0)
   {
      voltage= abs(voltage); 
   }
   if(++acw_add>5)
   {
      acw_add=0;

      if(choice_wac==CUR_MA)
      {
         if(gear_sw==ASW_ACW_MAV0)//最大1000V
         {
               acwv=gear_sw;
               //voltage*=1139;
               voltage=calibration_acv(voltage,0);
               if(voltage<100000000)//电压小于100V 切换档位
               {
                  analog_switch(ASW_ACW_MAV1);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else
               {
                  Vol_V=voltage/1000; //转换为mV
                  analog_switch(ASW_ACW_MA);
               }
         }
         else if(gear_sw==ASW_ACW_MAV1)//最大100V
         {
               acwv=gear_sw;
               //voltage*=115.6;
               voltage=calibration_acv(voltage,1);
               if(voltage<10000000)//电压小于10V 切换档位
               {
                  analog_switch(ASW_ACW_MAV2);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else if(voltage>110000000)//电压大于110V 切换档位
               {
                  analog_switch(ASW_ACW_MAV0);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else
               {
                  Vol_V=voltage/1000; //转换为mV
                  analog_switch(ASW_ACW_MA);
               }
         }
         else if(gear_sw==ASW_ACW_MAV2)//最大10V
         {
               acwv=gear_sw;
               //voltage*=12.65;
               voltage=calibration_acv(voltage,2);
               if(voltage>11000000)//电压大于11V 切换档位
               {
                  analog_switch(ASW_ACW_MAV1);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else
               {
                  Vol_V=voltage/1000; //转换为mV
                  analog_switch(ASW_ACW_MA);
               }      
         }
         else if(gear_sw==ASW_ACW_MA)//mA 档位 最大 500mA
         {
            //Vol_A=voltage*0.618/1000;//除1000转换成mA 
            Vol_A=calibration_aca(voltage,0)/1000;
            carried_value1=Vol_V;
            carried_value2=Vol_A*1000;//uA    
            analog_switch(acwv);
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ACV=%d\r\n",Vol_V);
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ACMA=%f\r\n",Vol_A);
            unit=0x0C; //单位 uW
            measured_value=Vol_A*Vol_V; 
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ACW=%d\r\n",measured_value);
         }
         else
         {
            if(choice_wac==CUR_MA)
            {
               analog_switch(ASW_ACW_MAV0);
            }
            else if(choice_wac==CUR_A)
            {
               analog_switch(ASW_ACW_AV0);
            }
         }
      }
      else if(choice_wac==CUR_A)
      {
         if(gear_sw==ASW_ACW_AV0)//最大1000V
         {
               acwv=gear_sw;
               //voltage*=1139;
               voltage=calibration_acv(voltage,0);
               if(voltage<100000000)//电压小于100V 切换档位
               {
                  analog_switch(ASW_ACW_AV1);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else
               {
                  Vol_V=voltage/1000; //转换为mV
                  analog_switch(ASW_ACW_A);
               }
         }
         else if(gear_sw==ASW_ACW_AV1)//最大100V
         {
               acwv=gear_sw;
               //voltage*=115.6;
               voltage=calibration_acv(voltage,1);
               if(voltage<10000000)//电压小于10V 切换档位
               {
                  analog_switch(ASW_ACW_AV2);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else if(voltage>110000000)//电压大于110V 切换档位
               {
                  analog_switch(ASW_ACW_AV0);
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else
               {
                  Vol_V=voltage/1000; //转换为mV
                  analog_switch(ASW_ACW_A);
               }
         }
         else if(gear_sw==ASW_ACW_AV2)//最大10V
         {
               acwv=gear_sw;
               //voltage*=12.65;
               voltage=calibration_acv(voltage,2);
               if(voltage>11000000)//电压大于11V 切换档位
               {
                  analog_switch(ASW_ACW_AV1);           
                  vTaskDelay(pdMS_TO_TICKS(300));
                  //Read_LTC();
               }
               else
               {
                  Vol_V=voltage/1000; //转换为mV
                  analog_switch(ASW_ACW_A);
               }      
         }
         else if(gear_sw==ASW_ACW_A)//A 档位 最大 10A
         {
            //Vol_A=voltage*125/1000; //除1000转换成mA   
            Vol_A=calibration_dca(voltage,0,5000000)/1000;//5A标定
            carried_value1=Vol_V;
            carried_value2=Vol_A*1000;//uA     
            analog_switch(acwv);
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ACV=%d\r\n",Vol_V);
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ACA=%f\r\n",Vol_A);
            if(voltage*Vol_V<1000000000) //小于1KW
            {
               unit=0x0C; //单位 uW
               measured_value=Vol_A*Vol_V; 
            }
            else
            {
               unit=0x0D; //单位 mW
               measured_value=Vol_A*Vol_V/1000; 
            }
            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ACW=%d\r\n",measured_value);
         }
         else
         {
            if(choice_wac==CUR_MA)
            {
               analog_switch(ASW_ACW_MAV0);
            }
            else if(choice_wac==CUR_A)
            {
               analog_switch(ASW_ACW_AV0);
            }
         }
      }   
   }
}

void celsiusToFahrenheit(float celsius)
{
   celsius = ( 9.0 / 5.0 ) * celsius + 32; //摄氏度转华氏度
   if(celsius<0)//华氏为负数
   {
      carried_value2_sign=1;
      carried_value2=abs(celsius)*100; 
   }
   else
   {
      carried_value2_sign=0;
      carried_value2=celsius*100;
   }
}

//测量温度
void measure_temp(float voltage)
{
   float resistance=0;
 //  uint32_t Vol=0; 
   sign=0; //电阻不能为负
   if(voltage<0)
   {
      voltage=0; 
   }

   if(gear_sw==ASW_2WR0)//最大 1KΩ
   {
         resistance=calibration_2wr(voltage,0,999.9);
         //resistance= (voltage)/500;//分压10倍，电压除以5000uA得到被测电阻单位Ω
         if(resistance>=10000000) resistance=10000000-1;             
         resistance=((10000000*resistance)/(10000000-resistance));  //最后乘以1000000是为了得到uΩ

         if(resistance>=1100)//如果大于1KΩ,//单位是mΩ
         {
            analog_switch(ASW_2WR1);//切换档位
            vTaskDelay(pdMS_TO_TICKS(100));
            //Read_LTC();
         }

         //if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ASW_2WR0 Vol=%d\r\n",resistance);
   }
   else if(gear_sw==ASW_2WR1)//最大20KΩ
   {   
         resistance=calibration_2wr(voltage,1,19960.1);
         //resistance= voltage/25;//分压10倍，电压除以250uA得到被测电阻单位Ω
         if(resistance>=10000000) resistance=10000000-1;             
         resistance=((10000000*resistance)/(10000000-resistance));  //最后乘以1000是为了得到mΩ

         if(resistance<9000)//电阻小于900Ω 切换档位
         {
            analog_switch(ASW_2WR0);
            vTaskDelay(pdMS_TO_TICKS(100));
            //Read_LTC(); 
         }
         else if(resistance>21000)//电阻大于20KΩ 切换档位
         {
            analog_switch(ASW_2WR2);
            vTaskDelay(pdMS_TO_TICKS(100));
            //Read_LTC();
         }

        // if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ASW_2WR1 Vol=%d\r\n",resistance);   
   }
   else if(gear_sw==ASW_2WR2)//最大500KΩ
   {
         resistance=calibration_2wr(voltage,2,476190);
         //resistance= voltage/1;//分压10倍，电压除以10uA得到被测电阻单位Ω
         if(resistance>=10000000) resistance=10000000-1;             
         resistance=((10000000*resistance)/(10000000-resistance));  //最后乘以1000是为了得到mΩ

         if(resistance<19000)//电阻小于19KΩ 切换档位
         {
            analog_switch(ASW_2WR1);
            vTaskDelay(pdMS_TO_TICKS(100));
            //Read_LTC();
         }
         else if(resistance>501000)//电阻大于500KΩ 切换档位
         {
            
            resistance=0xFFFFFFFF;
        
         }
        // if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"ASW_2WR2 Vol=%d\r\n",measured_value);    
   }
   else
   {
         analog_switch(ASW_2WR2);
   }

   carried_value1=resistance*100;//单位0.01R

   
   if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"R=%f\r\n",resistance); 
   if(choice_temp==TEMP_PT200)
   {
      resistance/=2;
   }
   else if(choice_temp==TEMP_PT1000)
   {
      resistance/=10;
   }
   switch (choice_temp)
   {
   case TEMP_PT100:
   case TEMP_PT200:
   case TEMP_PT1000:
      if ((resistance < 60.256) | (resistance > 138.505)) {
         sign=0; 
         measured_value=0xFFFFFFFF;

         carried_value2_sign=0;
         carried_value2=0xFFFFFFFF;

      } else if (resistance > 100) { //正温度
         sign=0; 
         unit=0x0E;
         resistance=PT100_ResToTemp(resistance);
         measured_value = resistance*1000;
         celsiusToFahrenheit(resistance);
      } else { //负温度
         sign=1; 
         unit=0x0E;
         resistance = (100-PT100_ResToTemp_L(resistance)); 
         measured_value = resistance*1000;
         voltage=voltage-resistance*2;//转换为负数
         celsiusToFahrenheit(resistance);
      }
      break;
   case TEMP_NTC_103KF_3950:
       if((resistance<=32754.7)&(resistance>667.6))
      {
         sign=0; 
         unit=0x0E;
         resistance = NTC_103KF_3950_ResToTemp(resistance);
         measured_value = resistance*1000;
         celsiusToFahrenheit(resistance);
      }
      else if((resistance>32754.7)&(resistance<693073.6))
      {
         sign=1; 
         unit=0x0E;
         resistance = 50 - NTC_103KF_3950_ResToTemp_L(resistance);
         measured_value = resistance*1000;
         resistance=resistance-resistance*2;//转换为负数
         celsiusToFahrenheit(resistance);
      }
      else
      {
         sign=0; 
         measured_value=0xFFFFFFFF;

         carried_value2_sign=0;
         carried_value2=0xFFFFFFFF;
      } 
      break;
   case TEMP_NTC_103KF_3435:
      if((resistance<=27280)&(resistance>973))
      {
         sign=0;
         unit=0x0E; 
         resistance = NTC_103KF_3435_ResToTemp(resistance);
         measured_value = resistance*1000;
         celsiusToFahrenheit(resistance);
      }
      else if((resistance>27280)&(resistance<437139))
      {
         sign=1;
         unit=0x0E; 
         resistance = 55 - NTC_103KF_3435_ResToTemp_L(resistance);
         measured_value = resistance*1000;
         resistance=resistance-resistance*2;//转换为负数
         celsiusToFahrenheit(resistance);
      }
      else
      {
         sign=0; 
         measured_value=0xFFFFFFFF;

         carried_value2_sign=0;
         carried_value2=0xFFFFFFFF;
      } 
      break;
   default:
      break;
   }

   

   if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"TEMP=%d\r\n",measured_value); 
}

//电压电流转换
void measure_V_A(float voltage)
{
	int32_t Vol=0;
   uint8_t atov=0;
   uint16_t v_min=0;
   uint16_t v_max=0;


   if(setup_VI_select==0)
    {
        v_min=0;
        v_max=5000;
        if(vi_reverse1==0)
        {
            atov=0;
        }
        else
        {
            atov=1;
        }
    }
    else if(setup_VI_select==1)
    {
        v_min=1000;
        v_max=5000;
        if(vi_reverse2==0)
        {
            atov=0;

        }
        else
        {
            atov=1;
        }
    }
    else if(setup_VI_select==2)
    {
        v_min=0;
        v_max=10000;
        if(vi_reverse3==0)
        {
            atov=0;
        }
        else
        {
            atov=1;
        }
    }

    if(atov==0)//测量电压
    {
       
      if(voltage>=0)
      {
         if(gear_sw==ASW_DCV2)//最大10V
         {
            Vol=calibration_dcv(voltage,2,9991470);//10V标定

            if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DCV2 Vol=%d\r\n",Vol);
            if(Vol>11000000)//电压大于11V 
            {
               measured_value=0xFFFFFFFF;
               carried_value1=0xFFFFFFFF;
            }
            else
            {
               unit=0x01;
               measured_value=Vol;
               Vol/=1000;

               carried_value1=(20000.0-4000.0)/(v_max-v_min)*(Vol-v_min)+4000.0;

               
            }
         }
         else
         {
            analog_switch(ASW_DCV2);
         }
      }
      else
      {
         measured_value=0xFFFFFFFF;
         carried_value1=0xFFFFFFFF;
      }
    }
    else//测量电流
    {
      sign=0;
      Vol=calibration_dca(voltage,1,250000);//250mA标定
      //Vol*=0.493;
      if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"DCmA Vol=%d\r\n",Vol);  
      unit=0x01;
      measured_value=Vol; 
      Vol=(v_max-v_min)/(20000.0-4000.0)*(Vol-4000.0)+v_min;
     
      if(Vol<0) carried_value1=0;
      else
      {
         carried_value1=Vol;
      }
       if(PRINTF_VALUE) ESP_LOGI(ADC_TASK_TAG,"carried_value1=%d\r\n",carried_value1);
    }
   
}

float read_adc_uv(uint8_t filter)
{
   float voltage;
   static float filter_buf[10];
   static uint8_t filter_add=0;
   float sum=0;
   int i=0;
   long adc_data=0;

   adc_data=read_ad7791();
   voltage=adc_data*0.29802324164;

   if(filter==1)
   {
      filter_buf[filter_add]=voltage;
      if(++filter_add>=5) filter_add=0;
      sum=0;
      for(i=0;i<5;i++)
      {
            sum+=filter_buf[i];
      }
      voltage=sum/5;
   }
 
   return voltage;
}

//定时向网络发送测量值
esp_timer_handle_t esp_timer_handle_txdata = 0;
/*定时器中断函数*/
void esp_timer_txdata_cb(void *arg){
    uint8_t evt;
    if(no_need_up==0)
    {
        evt=WIFINET_MVOM;
        xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);

        evt=SD_RECEIVE_ADC;
        xQueueSendFromISR(sd_evt_queue, &evt, NULL); 
        //ESP_LOGI(ADC_TASK_TAG, "TX DATA");
    }
}

void set_current_freq(){
    esp_timer_stop(esp_timer_handle_txdata);
    esp_timer_start_periodic(esp_timer_handle_txdata, current_freq * 100* 1000);
}


uint8_t electricity_st_old=0;
uint16_t pwroff_t_add=0;
int readadc=0;
void adc_task(void *arg)
{

   time_t now;
   float voltage=0;
   int val;
   int batt_v;
   uint8_t evt;
   int b_add=0;
   int b_add_c=100;

   int s_add=1;
   int s_add_c=2;

   uint8_t fun_old=FUN_NULL; // 上一次的功能设置为无
   // 在ADC上设置特定通道的衰减，并配置其关联的GPIO引脚复用器
   // 这里的意思呢，就是什么也没干
   adc2_config_channel_atten( ADC2_CHANNEL_7, ADC_ATTEN_0db );
	vTaskDelay(pdMS_TO_TICKS(100));
   spi_adc_init(0);

   /*
      这一段是初始化了一个定时器，用来定时向网络发送测量值
      中断函数的定义并没有删除（1642行），但是我把初始化注释掉了应该就没有问题了
   */
   //     //定时器结构体初始化
   //  esp_timer_create_args_t esp_timer_create_args_txdata = {
   //      .callback = &esp_timer_txdata_cb, //定时器回调函数
   //      .arg = NULL, //传递给回调函数的参数
   //      .name = "esp_timer_txdata" //定时器名称
   //  };
   // /*创建定时器*/       
   //  esp_err_t err = esp_timer_create(&esp_timer_create_args_txdata, &esp_timer_handle_txdata);
   //  err = esp_timer_start_periodic(esp_timer_handle_txdata, current_freq * 100* 1000);

   // ESP_LOGI(ADC_TASK_TAG, "----ADC_TASK_TAG----"); 
   // vTaskDelay(pdMS_TO_TICKS(100));
   // voltage=read_adc_uv(0);
   // ESP_LOGI(ADC_TASK_TAG, "voltage = %f", voltage);
   // vTaskDelay(pdMS_TO_TICKS(100));
   // voltage=read_adc_uv(0);
   // ESP_LOGI(ADC_TASK_TAG, "voltage = %f", voltage);
   // vTaskDelay(pdMS_TO_TICKS(100));
   // voltage=read_adc_uv(0);
   // ESP_LOGI(ADC_TASK_TAG, "voltage = %f", voltage);

    
   while (1) {

      // 如果是通断档，就设置两个阈值
     if(current_fun==FUN_BEEP)
     {
         vTaskDelay(pdMS_TO_TICKS(20));
         b_add_c=100;
         s_add_c=5;
     }
     else
     {
         vTaskDelay(pdMS_TO_TICKS(100));
         b_add_c=20;
         s_add_c=1;
     }

     
     
     //write_ad7791(0x3c);/*将7791设置成连续读*/
     //write_ad7791(0x38);

      // 当测量没有停止时，就一直等待；measure_stop=0表示测量停止，出自lcd_ui.h
     while(measure_stop!=0)
     {
        vTaskDelay(pdMS_TO_TICKS(100));
     }
     
       // 电源管理部分，读取电池电压
      if(++b_add>b_add_c)
      {
         b_add=0;
         esp_err_t r=-1;
         r = adc2_get_raw( ADC2_CHANNEL_7, ADC_WIDTH_12Bit, &val); // ADC2获取原始数据，r为返回状态
         //ESP_LOGI(ADC_TASK_TAG, "ADC = %d", val);
         if ( r == ESP_OK ) {
            batt_v = (1000.0/4095.0)*val/0.234;
             //ESP_LOGI(ADC_TASK_TAG, "batt_v = %d", batt_v);
            if(gpio_get_level(CHRG)==0) //充电状态
            {
               electricity_st=254;
            }
            else
            {
               if(batt_v<3200) // 电量过低关机
               {
                  if(on_off_sound!=0)//关机提示音
                  {
                     evt=WIFINET_MQTTSTOP;
                     xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                     gpio_set_level(BEEP, 1);
                     vTaskDelay(pdMS_TO_TICKS(100));
                     gpio_set_level(BEEP, 0);
                     vTaskDelay(pdMS_TO_TICKS(80));
                     gpio_set_level(BEEP, 1);
                     vTaskDelay(pdMS_TO_TICKS(100));
                     gpio_set_level(BEEP, 0);
                     gpio_set_level(PWR_EN, 0);
                  }
               }
               if(electricity_st==254)
               {
                  // 电池电量只会显示几个固定值
                  if(batt_v<3300)
                  {
                     electricity_st=1;
                  }
                  else if(batt_v<3650)
                  {
                     electricity_st=25;
                  }
                  else if(batt_v<3750)
                  {
                     electricity_st=50;
                  }
                  else if(batt_v<3850)
                  {
                     electricity_st=75;
                  }
                  else 
                  {
                     electricity_st=100;
                  }

                  pwroff_t_add=600;
               }
               else
               {
                  if(electricity_st==1)
                  {
                     if(batt_v>(3300+50))
                     {
                        electricity_st=25;
                     }
                     else
                     {
                        if(pwroff_t_add%60==0)//每分钟 低电量警报
                        {
                           if(low_power_sound!=0)
                           {
                              gpio_set_level(BEEP, 1);
                              vTaskDelay(pdMS_TO_TICKS(100));
                              gpio_set_level(BEEP, 0);
                              vTaskDelay(pdMS_TO_TICKS(80));
                              gpio_set_level(BEEP, 1);
                              vTaskDelay(pdMS_TO_TICKS(100));
                              gpio_set_level(BEEP, 0);
                              vTaskDelay(pdMS_TO_TICKS(80));
                              gpio_set_level(BEEP, 1);
                              vTaskDelay(pdMS_TO_TICKS(100));
                              gpio_set_level(BEEP, 0);
                           }  
                        }
                        if(--pwroff_t_add==0) //低电量10分钟后关机
                        {
                           if(on_off_sound!=0)//关机提示音
                           {
                              evt=WIFINET_MQTTSTOP;
                              xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                              gpio_set_level(BEEP, 1);
                              vTaskDelay(pdMS_TO_TICKS(100));
                              gpio_set_level(BEEP, 0);
                              vTaskDelay(pdMS_TO_TICKS(80));
                              gpio_set_level(BEEP, 1);
                              vTaskDelay(pdMS_TO_TICKS(100));
                              gpio_set_level(BEEP, 0);
                              gpio_set_level(PWR_EN, 0);
                           }
                           gpio_set_level(PWR_EN, 0);
                        }  
                     }
                  }
                  else if(electricity_st==25)
                  {
                     if(batt_v>(3650+50))
                     {
                        electricity_st=50;
                     }
                     else if(batt_v<3350)
                     {
                        electricity_st=1;
                     }
                     pwroff_t_add=600;
                  }
                  else if(electricity_st==50)
                  {
                     if(batt_v>(3750+50))
                     {
                        electricity_st=75;
                     }
                     else if(batt_v<3650)
                     {
                        electricity_st=25;
                     }
                     pwroff_t_add=600;
                  }
                  else if(electricity_st==75)
                  {
                     if(batt_v>(3850+50))
                     {
                        electricity_st=100;
                     }
                     else if(batt_v<3750)
                     {
                        electricity_st=50;
                     }
                     pwroff_t_add=600;
                  }
                  else if(electricity_st==100)
                  {
                      if(batt_v<3850)
                     {
                        electricity_st=50;
                     }
                     pwroff_t_add=600;
                  }
               } 
            }

            if(electricity_st_old!=electricity_st)
            {
               electricity_st_old=electricity_st;
               evt=REFRESH_TOP_ICON;
               xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
            } 
         }
      }

      // 储存上一次的功能，如果是通断档（下一次就不是通断档了），ADC置1
      if(fun_old!=current_fun)
      {
         fun_old=current_fun;
         if(current_fun==FUN_BEEP)
         {
            spi_adc_init(1);
         }
         else
         {
            spi_adc_init(0);
         }
      }
      
      // 喂喂喂，这里才是主函数啊啊啊
      no_need_up=0;
      switch (current_fun)
      {
         case FUN_DCV://直流电压
            voltage=read_adc_uv(1);
            measure_dcv(voltage);
            break;
         case FUN_ACV://交流电压
            voltage=read_adc_uv(1);
            measure_acv(voltage);
            break;
         case FUN_DCA://直流电流
            voltage=read_adc_uv(1);
            measure_dca(voltage);
            break;
         case FUN_ACA://交流电压
            voltage=read_adc_uv(1);
            measure_aca(voltage);
            break;
         case FUN_2WR://两线电阻
            voltage=read_adc_uv(1);
            measure_2wr(voltage);
            break;
         case FUN_4WR://四线电阻
            voltage=read_adc_uv(1);
            measure_4wr(voltage);
            break;
         case FUN_DIODE://二极管
            voltage=read_adc_uv(0);
            measure_diode(voltage);
            break;
         case FUN_BEEP://通断档
            voltage=read_adc_uv(0);
            measure_beep(voltage);
            break;
         case FUN_DCW://直流功率
            voltage=read_adc_uv(1);
            measure_dcw(voltage);
            break;
         case FUN_ACW://交流功率
            voltage=read_adc_uv(1);
            measure_acw(voltage);
            break;
         case FUN_TEMP://测量温度
            voltage=read_adc_uv(1);
            measure_temp(voltage);
            break;
         case FUN_VTOA://电压电流转换
            voltage=read_adc_uv(1);
            measure_V_A(voltage);
            break;
         
         default:
         no_need_up=1;
            break;
      }
      
      
         //  voltage+=0.18664;
        
         
         //voltage=((float)(10-1))/(0.04945-0.00909)*(voltage-0.00909)+1;
         //voltage-=0.0039;
         // 这里再次上传数据，可是数据是同步发送到网络和本地的
         if((no_need_up==0)&((measure_stop==0)))
         {
            if(++s_add>s_add_c)
            {
               s_add=0;

               time(&now);
               localtime_r(&now, &ADC_timeinfo);

               evt=REFRESH_VALUE;
               xQueueSendFromISR(gui_evt_queue, &evt, NULL); 

            }
         }

   }

}

