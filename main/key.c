#include "main.h"
#include "key.h"


const char *KEY = "KEY";

xQueueHandle key_evt_queue; // FreeRTOS里的一个消息队列，这里用来接收按键的按动情况

// 旋转编码器的控制，定义了两个静态变量Left和Right，
// 检测到按键左右旋转后就会将按键消息送入消息队列执行中断
// 旋转编码器的中断和按键的中断不是一个中断
static void IRAM_ATTR rotary_isr_handler(void* arg)
{
    static uint8_t Left,Right;  //左 右旋转方向静态变量
    uint8_t evt;

   //  evt=KEY_ROTARY_L;
   //      xQueueSendFromISR(key_evt_queue, &evt, NULL);	 
    if((gpio_get_level(SWA)==0)&(gpio_get_level(SWB)==0))
	 {
        Left=0XFF;Right=0X00;
    } // 左旋转的变量赋值有效 右旋转无效
	 else if((gpio_get_level(SWA)!=0)&(gpio_get_level(SWB)==0))            //判断A.B电平 如果A是低电平 B是高电平表示向左旋转
    {
        Right=0XFF;Left=0X00;
    } //右旋转的变量赋值有效  左旋转无效
	 else if((Right!=0)&(gpio_get_level(SWA)==0)&(gpio_get_level(SWB)!=0))  //将有效的右旋再次与A.B电平对比  确认是右旋
    {
		  Right=0X00;
        evt=KEY_ROTARY_R;
        xQueueSendFromISR(key_evt_queue, &evt, NULL);
	 }				
	 else if((Left!=0)&(gpio_get_level(SWA)!=0)&(gpio_get_level(SWB)!=0))    //将有效的左旋再次与A.B电平对比  确认是左旋
    {			
		  Left=0X00;	
        evt=KEY_ROTARY_L;
        xQueueSendFromISR(key_evt_queue, &evt, NULL);	 
	}     
}

//IO中断配置
void gpio_exti_init(void)
{
   gpio_config_t io_conf;
   io_conf.intr_type = GPIO_INTR_ANYEDGE;
   io_conf.pin_bit_mask = (1ULL<<SWB);
   io_conf.mode = GPIO_MODE_INPUT;
   io_conf.pull_up_en = 1;
   gpio_config(&io_conf);
   gpio_install_isr_service(0);
   gpio_isr_handler_add(SWB, rotary_isr_handler, (void*) SWB);
   gpio_intr_enable(SWB);

}



//复位息屏计数器，并且返回当前是否是息屏状态
uint8_t screen_status()
{
   if(backtime==0) return 1;
   if(backtime_add<=1) 
   {
      set_bl(81.92*backlight,0);
      backtime_add=backtime*6;
      return 0;
   }
   backtime_add=backtime*6;
   return 1;
}


// 读取其他按键的状态，复杂一点的送入消息队列，简单一点的当场处理（音量大小）
uint8_t K1Set,K1Cnt,K2Set,K2Cnt,K3Set,K3Cnt,K4Set,K4Cnt;
void KeyScan()
{	
   uint8_t evt;

		if(gpio_get_level(SWK)==0) 
		{		
         if(screen_status()==1)
         {
            if(++K1Cnt==3) 
            {
                  K1Set = 1;			
            }
            else if(K1Cnt==50)
            { 
                 // ESP_LOGI(KEY, "Press and hold the key");
                  K1Set=0; 
            }
            else if(K1Cnt>50)
            {
                  K1Cnt=100;
            }
         }
         else
         {
            K1Cnt=100;
         }	
		}
		else
		{
			 K1Cnt = 0;
			 if(K1Set==1)
			 {
					K1Set = 0;
               evt=KEY_ENTER;
               xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
               if(key_sound!=0) beep_start(2);
			 }
		}

      if(gpio_get_level(PWR_KEY)==0) 
		{		
         if(screen_status()==1)
         {	
				if(++K2Cnt==3) 
				{
						K2Set = 1;			
				}
				else if(K2Cnt==30)
				{ 
                //  ESP_LOGI(KEY, "Press and hold the key");
                  current_fun=FUN_NULL;
                  //gpio_set_level(PWR_EN, 0);
						K2Set=0; 
                  evt=KEY_PWROFF;
                  xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                  if(on_off_sound!=0)//关机提示音
                  {
                     gpio_set_level(BEEP, 1);
                     vTaskDelay(pdMS_TO_TICKS(100));
                     gpio_set_level(BEEP, 0);
                     vTaskDelay(pdMS_TO_TICKS(80));
                     gpio_set_level(BEEP, 1);
                     vTaskDelay(pdMS_TO_TICKS(100));
                     gpio_set_level(BEEP, 0);
                  } 
				}
				else if(K2Cnt>30)
				{
						K2Cnt=100;
				}
         }
         else
         {
            K1Cnt=100;
         }
		}
		else
		{
			 K2Cnt = 0;
			 if(K2Set==1)
			 {
					K2Set = 0;
               evt=KEY_PWR_BACK;
               xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
               if(key_sound!=0) beep_start(2);
			 }
		}

      if(gpio_get_level(KEY2)==0) 
		{		
         if(screen_status()==1)
         {	
				if(++K3Cnt==3) 
				{
						K3Set = 1;
                  evt=KEY_F1_DOWN;
                  xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
                  if(key_sound!=0) beep_start(2);		

				}
				else if(K3Cnt==50)
				{ 
						K3Set=0; 
                  evt=KEY_F1_LONG;
                  xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
				}
				else if(K3Cnt>50)
				{
						K3Cnt=100;
				}
         }
         else
         {
            K1Cnt=100;
         }
		}
		else
		{
			 K3Cnt = 0;
			 if(K3Set==1)
			 {
					K3Set = 0;
               evt=KEY_F1_UP;
               xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
			 }
		}

      if(gpio_get_level(KEY1)==0) 
		{			
         if(screen_status()==1)
         {
				if(++K4Cnt==3) 
				{
						K4Set = 1;
                  evt=KEY_F2_DOWN;
                  xQueueSendFromISR(gui_evt_queue, &evt, NULL); 	
                  if(key_sound!=0) beep_start(2);		
				}
				else if(K4Cnt==50)
				{ 
						K4Set=0; 
                  evt=KEY_F2_LONG;
                  xQueueSendFromISR(gui_evt_queue, &evt, NULL);

				}
				else if(K4Cnt>50)
				{
						K4Cnt=100;
				}
         }
         else
         {
            K1Cnt=100;
         }
		}
		else
		{
			 K4Cnt = 0;
			 if(K4Set==1)
			 {
					K4Set = 0;
               evt=KEY_F2_UP;
               xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
			 }
		}
}

uint8_t beep_add=0;
uint8_t beep_k=0;
uint8_t beep_d=0;

// 蜂鸣器响一会
void beep_start(uint8_t duration)
{
   beep_d=duration; // 蜂鸣器持续时间
   beep_add=0;
   beep_k=1;
}

// 根据beep_k和beep_add的状态决定蜂鸣器的状态
void Beep_drive()
{
   if(beep_k==1)
   {
      if(++beep_add>beep_d)
      {
         gpio_set_level(BEEP, 0);
         beep_k=0;
      }
      else if(beep_add==1)
      {
         gpio_set_level(BEEP, 1);
      }
   }
}

esp_timer_handle_t esp_timer_handle_t1 = 0;
/*定时器中断函数*/
void esp_timer_cb(void *arg){

   KeyScan();
   Beep_drive();

}

void key_task(void *arg)
{
   uint8_t key_add = 0;// 管理长按短按的
   uint8_t evt;
   key_evt_queue = xQueueCreate(5, sizeof(uint8_t)); // 创建按键消息队列
   gpio_exti_init();

   
   
   while(1)//短按开机 ？？？ 我觉得是长按跳出循环开机
   {
      vTaskDelay(pdMS_TO_TICKS(90));
      if(++key_add > 9) break; //长按 开机
      if((gpio_get_level(PWR_KEY)!=0)) key_add = 0; //可以看出这个按键是低电平有效，按下为0
   }
   if(on_off_sound!=0)//开机提示音
   {
      gpio_set_level(BEEP, 1); // 可以看出执行后蜂鸣器会一直叫
      vTaskDelay(pdMS_TO_TICKS(200));
      gpio_set_level(BEEP, 0);
   }
   gpio_set_level(PWR_EN, 1);
   key_add = 0;
  
   while(1)//长按开机
   {
      if((gpio_get_level(PWR_KEY)!=0)) break;;
      vTaskDelay(pdMS_TO_TICKS(100));

      if(key_add < 10)
      {
         key_add++;
      }
      else //长按时间大于1秒
      {
        // ESP_LOGI(KEY, "LONG KEY");
         break;
      }
   }
		
      // 这里是开机的时候长按会进入出厂设置
	 key_add = 0;
    while((gpio_get_level(PWR_KEY)==0))
    {
        vTaskDelay(pdMS_TO_TICKS(100));

        if(key_add < 70)
        {
            key_add++;
        }
        else //长按时间大于10秒，恢复出厂设置
        {
         //   ESP_LOGI(KEY, "LONG LONG KEY"); 
            while((gpio_get_level(PWR_KEY)==0))
            {
               vTaskDelay(pdMS_TO_TICKS(100));
            }			
        }
    }



    //定时器结构体初始化
    esp_timer_create_args_t esp_timer_create_args_t1 = {
        .callback = &esp_timer_cb, //定时器回调函数
        .arg = NULL, //传递给回调函数的参数
        .name = "esp_timer" //定时器名称
    };

    /*创建定时器*/       
    esp_err_t err = esp_timer_create(&esp_timer_create_args_t1, &esp_timer_handle_t1);
    err = esp_timer_start_periodic(esp_timer_handle_t1, 30 * 1000);


    

    while (1) {
      if(xQueueReceive(key_evt_queue, &evt, portMAX_DELAY))
      {
        // ESP_LOGI(KEY, "evt=%d",evt);
         switch (evt)
         {
         case KEY_ROTARY_R: //旋转编码器 右旋
         //   ESP_LOGI(KEY, "ROTARY Right");
            if(key_sound!=0) beep_start(1);
            if(screen_status()==1)
            {
               xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
            }
            break;

         case KEY_ROTARY_L: //旋转编码器 左旋
         //   ESP_LOGI(KEY, "ROTARY Left");
            if(key_sound!=0) beep_start(1);
            if(screen_status()==1)
            {
               xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
            }
            break;
         
         default:
            break;
         }
         
           
      }
      
   }

}

