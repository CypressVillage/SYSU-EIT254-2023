#include "main.h"
#include "switch_fun.h"

const char *SWITCH_FUN = "SWITCH_FUN";

uint8_t gear_sw=0;//当前档位状态

uint8_t OHM=0; //电流源输出值  0.5uA 10uA 250uA 5mA
uint8_t JDQ1=0; //继电器1的状态 0-(V A) 1-(R)
uint8_t JDQ2=0; //继电器2的状态 0-uA 1-mA
uint8_t JDQ3=0; //继电器3的状态 A
uint8_t K_DIV=0; //分压电阻  10K  100K  1000K  0R
uint8_t K_RMS=0; //交流测量 0电压 1电流
uint8_t AIN_MUX=1; //模拟量输入通道选择 0-7 : uA mA 0V 10A  VHS VLS  RMS  DIV



uint8_t choice_dcv=VOL_AUTO; //直流电压档量程
uint8_t choice_dcv_old=0xFF;

uint8_t choice_acv=VOL_AUTO;//交流电压档量程
uint8_t choice_acv_old=0xFF;

uint8_t choice_dca=CUR_A; //直流电流档量程
uint8_t choice_dca_old=0xFF;

uint8_t choice_2wr=RES_AUTO; //电阻档量程
uint8_t choice_2wr_old=0xFF;

uint8_t choice_wdc=CUR_A; //直流功率档电流量程
uint8_t choice_wdc_old=0xFF;

uint8_t choice_aca=CUR_A; //交流电流档量程
uint8_t choice_aca_old=0xFF;

uint8_t choice_wac=CUR_A; //交流功率档电流量程
uint8_t choice_wac_old=0xFF;

uint8_t choice_temp=TEMP_PT100; //温度传感器类型
uint8_t choice_temp_old=0xFF;


void I2C_Init()
{	
   esp_err_t ret;
   i2c_config_t conf = {};
   conf.mode = I2C_MODE_MASTER;
   conf.sda_io_num = I2C_SDA;
   conf.scl_io_num = I2C_SCL;
   conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
   conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
   conf.master.clk_speed = 50000;
   i2c_param_config(I2C_NUM_0, &conf);
   i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

	i2c_cmd_handle_t cmd = i2c_cmd_link_create(); // 在执行i2c之前，必须执行此函数 创建一个i2c 命令 链接，为之后的i2c操作执行，在执行完成之后需要销毁
   i2c_master_start(cmd);//i2c运行开始函数。注意这不是真的开始，只是是一个开始标记，初始化cmd 
    //以下是i2c真正的读写操作
   i2c_master_write_byte(cmd, WRITECommand, 0x01);
   i2c_master_write_byte(cmd, (VIN | R55 | FAST), 0x01);// 
   i2c_master_stop(cmd);//i2c停止运行。并不是真正的停止，因为此时i2c还没有真正的运行，我认为这是一个标识，当时i2c运行的时候读取到此标志就停止运行。
   ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_RATE_MS);
   i2c_cmd_link_delete(cmd); // 操作完成 删除cmd
   if (ret != ESP_OK) {
     //  ESP_LOGI(SWITCH_FUN, "I2C ERROR =%d",ret);
   }else{
     //  ESP_LOGI(SWITCH_FUN, "I2C OK");
   }
}

void AW9523_Init()
{
   i2c_cmd_handle_t cmd = i2c_cmd_link_create();
   i2c_master_start(cmd);
   i2c_master_write_byte(cmd, 0xB0, 1);
	i2c_master_write_byte(cmd, 0x11, 1);
   i2c_master_write_byte(cmd, 0x10, 1);//P0口配置为推挽输出
   i2c_master_stop(cmd);
   
   esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
   if (ret != ESP_OK) {
    //   ESP_LOGI(SWITCH_FUN, "AW9523=ERROR =%d",ret);
   }
   i2c_cmd_link_delete(cmd);
}

void AW9523_Write(uint8_t data_L , uint8_t data_H)
{
   i2c_cmd_handle_t cmd = i2c_cmd_link_create();
   i2c_master_start(cmd);
   i2c_master_write_byte(cmd, 0xB0, 1);
	i2c_master_write_byte(cmd, 0x02, 1);  //P0口
   i2c_master_write_byte(cmd, data_L, 1);
   i2c_master_stop(cmd);
   esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
   if (ret != ESP_OK) {
    //   ESP_LOGI(SWITCH_FUN, "AW9523=ERROR =%d",ret);
   }
   i2c_cmd_link_delete(cmd);

   cmd = i2c_cmd_link_create();
   i2c_master_start(cmd);
   i2c_master_write_byte(cmd, 0xB0, 1);
	i2c_master_write_byte(cmd, 0x03, 1); //P1口
   i2c_master_write_byte(cmd, data_H, 1);
   i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
   if (ret != ESP_OK) {
   //    ESP_LOGI(SWITCH_FUN, "AW9523=ERROR =%d",ret);
   }
   i2c_cmd_link_delete(cmd);
}


uint8_t JDQ1_OLD=0xff; //继电器1的状态 
uint8_t JDQ2_OLD=0xff; //继电器2的状态 mA uA
void control_sw()
{
   uint8_t data_H=0;
   uint8_t data_L=0;

   
   if(AIN_MUX&0x01) data_L|=0x08;
   if(AIN_MUX&0x02) data_L|=0x04;
   if(AIN_MUX&0x04) data_L|=0x02;
   if(K_RMS==0) data_L|=0x10;
   if(K_DIV&0x02) data_L|=0x20;
   if(K_DIV&0x01) data_L|=0x40;
   if(OHM&0x02) data_L|=0x80;
   if(OHM&0x01) data_H|=0x10;
   if(JDQ3) data_H|=0x01;
   

   if(JDQ1_OLD!=JDQ1)
   {
       //ESP_LOGI(SWITCH_FUN, "JDQ1_OLD!=JDQ1");
       if(JDQ1==0)
       {
           data_H|=0x04;
       }
       else 
       {
           data_H|=0x02;
       }
   }

   if(JDQ2_OLD!=JDQ2)
   { 
       //ESP_LOGI(SWITCH_FUN, "JDQ2_OLD!=JDQ2");
       if(JDQ2==0)
       {
           data_L|=0x01;
       }
       else 
       {
           data_H|=0x08;
       }
   }

   if((JDQ1_OLD!=JDQ1)|(JDQ2_OLD!=JDQ2))
   {
       JDQ1_OLD=JDQ1;
       JDQ2_OLD=JDQ2;
       AW9523_Write(data_L,data_H);
       vTaskDelay(pdMS_TO_TICKS(200));
       
      // ESP_LOGI(SWITCH_FUN, "JDQ1_OLD!=JDQ1  2222"); 
   }
   data_L&=0xFE;
   data_H&=0xF1;
   AW9523_Write(data_L,data_H);
}



//切换模拟开关
void analog_switch(uint8_t sw)
{
	if(gear_sw!=sw)
	{
		gear_sw=sw;
		switch(sw)
		{
			case ASW_OFF:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=0;
            K_DIV=3;
            K_RMS=0;
            AIN_MUX=2;
			   break;
         case ASW_CIR:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=0;
            K_DIV=3;
            K_RMS=0;
            AIN_MUX=7;
			   break;
         case ASW_DCV0:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=0;
            K_DIV=0;
            K_RMS=0;
            AIN_MUX=7;
			   break;
         case ASW_DCV1:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=0;
            K_DIV=1;
            K_RMS=0;
            AIN_MUX=7;
			   break;
         case ASW_DCV2:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=7;
			   break;
         case ASW_ACV0:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=0;
            K_DIV=0;
            K_RMS=0;
            AIN_MUX=6;
			   break;
         case ASW_ACV1:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=0;
            K_DIV=1;
            K_RMS=0;
            AIN_MUX=6;
			   break;
         case ASW_ACV2:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=6;
			   break;
         case ASW_DCUA:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=0;
			   break;  
         case ASW_DCMA:
				OHM=0;
            JDQ1=0;
            JDQ2=1;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=1;
			   break; 
         case ASW_DCA:
				OHM=0;
            JDQ1=0;
            JDQ2=1;
            JDQ3=1;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=3;
			   break;
         case ASW_ACMA:
				OHM=0;
            JDQ1=0;
            JDQ2=1;
            JDQ3=0;
            K_DIV=2;
            K_RMS=1;
            AIN_MUX=6;
			   break; 
         case ASW_ACA:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=1;
            K_DIV=2;
            K_RMS=1;
            AIN_MUX=6;
			   break;
         case ASW_2WR0:
				OHM=0;
            JDQ1=1;
            JDQ2=0;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=7;
			   break;
          case ASW_2WR1:
				OHM=1;
            JDQ1=1;
            JDQ2=0;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=7;
			   break;
          case ASW_2WR2:
				OHM=2;
            JDQ1=1;
            JDQ2=0;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=7;
			   break;
          case ASW_2WR3:
				OHM=3;
            JDQ1=1;
            JDQ2=0;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=7;
			   break;
          case ASW_4WHS:
				OHM=0;
            JDQ1=1;
            JDQ2=0;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=4;
			   break;
          case ASW_4WLS:
				OHM=0;
            JDQ1=1;
            JDQ2=0;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=5;
			   break;
          case ASW_DIODE:
				OHM=0;
            JDQ1=1;
            JDQ2=0;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=7;
			   break;
         case ASW_BEEP:
				OHM=0;
            JDQ1=1;
            JDQ2=0;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=7;
			   break;
         case ASW_DCW_UA:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=0;
			   break; 
         case ASW_DCW_UAV0:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=0;
            K_DIV=0;
            K_RMS=0;
            AIN_MUX=7;
			   break;
         case ASW_DCW_UAV1:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=0;
            K_DIV=1;
            K_RMS=0;
            AIN_MUX=7;
            break; 
         case ASW_DCW_UAV2:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=7;
			   break;  
         case ASW_DCW_MA:
				OHM=0;
            JDQ1=0;
            JDQ2=1;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=1;
			   break;
         case ASW_DCW_MAV0:
				OHM=0;
            JDQ1=0;
            JDQ2=1;
            JDQ3=0;
            K_DIV=0;
            K_RMS=0;
            AIN_MUX=7;
			   break;
         case ASW_DCW_MAV1:
				OHM=0;
            JDQ1=0;
            JDQ2=1;
            JDQ3=0;
            K_DIV=1;
            K_RMS=0;
            AIN_MUX=7;
            break; 
         case ASW_DCW_MAV2:
				OHM=0;
            JDQ1=0;
            JDQ2=1;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=7;
			   break; 
         case ASW_DCW_A:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=1;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=3;
			   break; 
         case ASW_DCW_AV0:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=1;
            K_DIV=0;
            K_RMS=0;
            AIN_MUX=7;
			   break;
         case ASW_DCW_AV1:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=1;
            K_DIV=1;
            K_RMS=0;
            AIN_MUX=7;
            break; 
         case ASW_DCW_AV2:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=1;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=7;
			   break; 
         case ASW_ACW_MA:
				OHM=0;
            JDQ1=0;
            JDQ2=1;
            JDQ3=0;
            K_DIV=2;
            K_RMS=1;
            AIN_MUX=6;
			   break; 
         case ASW_ACW_MAV0:
				OHM=0;
            JDQ1=0;
            JDQ2=1;
            JDQ3=0;
            K_DIV=0;
            K_RMS=0;
            AIN_MUX=6;
			   break;
         case ASW_ACW_MAV1:
				OHM=0;
            JDQ1=0;
            JDQ2=1;
            JDQ3=0;
            K_DIV=1;
            K_RMS=0;
            AIN_MUX=6;
			   break;
         case ASW_ACW_MAV2:
				OHM=0;
            JDQ1=0;
            JDQ2=1;
            JDQ3=0;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=6;
			   break;
         case ASW_ACW_A:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=1;
            K_DIV=2;
            K_RMS=1;
            AIN_MUX=6;
			   break;
         case ASW_ACW_AV0:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=1;
            K_DIV=0;
            K_RMS=0;
            AIN_MUX=6;
			   break;
         case ASW_ACW_AV1:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=1;
            K_DIV=1;
            K_RMS=0;
            AIN_MUX=6;
			   break;
         case ASW_ACW_AV2:
				OHM=0;
            JDQ1=0;
            JDQ2=0;
            JDQ3=1;
            K_DIV=2;
            K_RMS=0;
            AIN_MUX=6;
			   break; 
         
			default:
            break;

		}
      control_sw();
	}
}






void switch_fun_task(void *arg)
{
   //gpio_set_level(V16_EN, 0);
   JDQ1=1;
   JDQ2=1;
   ESP_LOGI(SWITCH_FUN, "SWITCH_FUN");
   
   I2C_Init();
   AW9523_Init();
   AW9523_Write(0,0);
   vTaskDelay(pdMS_TO_TICKS(100));
   gpio_set_level(V16_EN, 1);
   vTaskDelay(pdMS_TO_TICKS(100));
   signal_V=50;
   signal_A=150;
   GP8403_init();
   GP8403_WriteReg();
   
   //analog_switch(ASW_DCV0);
    
    while (1) {
      vTaskDelay(pdMS_TO_TICKS(1000));
   }

}
