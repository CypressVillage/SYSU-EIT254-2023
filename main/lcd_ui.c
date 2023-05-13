#include "main.h"
#include "lcd_ui.h"
#include "image.h"
const char *LCD_UI_TAG = "LCD_TASK";
TFT_t dev;

xQueueHandle gui_evt_queue; 

uint8_t ui_display_st=DISPLAY_FUNCHOICE; //当前显示的页面 
uint8_t ui_display_old;

uint8_t ui_exit=0;

uint16_t current_freq=5;//测量频率 0.1秒  2-60000
uint8_t current_fun=FUN_NULL;//当前功能  
uint8_t measure_stop=0;//测量停止


uint8_t backlight=80; //背光亮度 5-100%
uint8_t backtime=5;   //自动息屏时间 1-99分钟
uint16_t backtime_add=0; //息屏时间，单位 10秒
uint8_t auto_off=0;      //息屏自动关机

uint8_t key_sound=1;      //按键声音开关
uint8_t on_off_sound=1;   //开关机声音开关
uint8_t low_power_sound=1; //低电量声音开关

uint32_t line_y_dcv=SCALE_X100; //波形图Y轴比例，直流电压
uint32_t line_y_acv=SCALE_X1000; //波形图Y轴比例，交流电压
uint32_t line_y_dca=SCALE_X0_1; //波形图Y轴比例，直流电流
uint32_t line_y_aca=SCALE_X1; //波形图Y轴比例，交流电流
uint32_t line_y_2wr=SCALE_X10; //波形图Y轴比例，两线电阻
uint32_t line_y_4wr=SCALE_X1; //波形图Y轴比例，四线电阻
uint32_t line_y_dcw=SCALE_X10; //波形图Y轴比例，直流功率
uint32_t line_y_acw=SCALE_X10; //波形图Y轴比例，交流功率
uint32_t line_y_temp=SCALE_X100; //波形图Y轴比例，温度
uint8_t  line_y_old=0xFF;
uint32_t line_y_scale_cal=1;   //参与计算的比例

uint8_t shortcut_F1=0xFF;//功能选择页面 快捷键1
uint8_t shortcut_F2=0XFF;//功能选择页面 快捷键2

uint8_t wifi_on=1;        //wifi开关

uint8_t net_state=0;   //网络状态， 0 路由器未连接，1 路由器已连接，2 服务器已连接

uint8_t setup_VI_select=0;//为0 是0-5V  为1是1-5V   为2是0-10V
uint8_t vi_reverse1=0; //为0是电压转电流，为1是电流转电压  0-5V
uint8_t vi_reverse2=0; //为0是电压转电流，为1是电流转电压  1-5V 
uint8_t vi_reverse3=0; //为0是电压转电流，为1是电流转电压  0-10V


MENU_ST ST_frame_menu;
FUNCHOICE_ST ST_funchoice;

#define BL_TIMER          LEDC_TIMER_1
#define BL_MODE           LEDC_LOW_SPEED_MODE


ledc_channel_config_t  bl_channel0= {
        .channel    = LEDC_CHANNEL_0,
        .duty       = 0,
        .gpio_num   = CONFIG_BL_GPIO, 
        .speed_mode = BL_MODE,
        .hpoint     = 0,
        .timer_sel  = BL_TIMER   
    };

//设置屏幕背光亮度 //0-0x1FFF  8191
void set_bl(uint16_t duty,int time_ms){
   ledc_set_fade_with_time(bl_channel0.speed_mode,bl_channel0.channel, duty, time_ms);
   ledc_fade_start(bl_channel0.speed_mode,bl_channel0.channel, LEDC_FADE_NO_WAIT);
}

void bl_pwm_init(void){

     //1. PWM: 定时器配置
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
        .freq_hz = 200,                      // frequency of PWM signal
        .speed_mode = BL_MODE,           // timer mode
        .timer_num = BL_TIMER,            // timer index
        .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
    };
    ledc_timer_config(&ledc_timer);
    //2. PWM:通道配置
    ledc_channel_config(&bl_channel0); 
    //3.PWM:使用硬件渐变
    ledc_fade_func_install(0);
    set_bl(8192,0);

}


esp_timer_handle_t esp_timer_handle_screenoff = 0;

//使屏幕变暗，10秒钟后息屏
void esp_timer_screenoff_cb(void *arg){
    uint8_t evt;
   if(backtime_add>0)
   {
        if(--backtime_add==1) //剩余10秒 变暗
        {
            set_bl(81.92*5,2000);
        }
        else if(backtime_add==0)
        {
            set_bl(0,0);
            if(auto_off==1)
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
   }
}



//刷新数值条 满量程  V: 0-100;
void ui_value_bar(uint8_t v)
{
    uint8_t i=0;
    uint16_t color=0;

    lcdDrawFillRect(&dev,40,119,41+(2.3*v),121,GREEN);
    lcdDrawFillRect(&dev,271-(2.3*(100-v)),119,271,121,0x2965);
    for(i=0;i<11;i++)
    {
        if((v/10)>=i) color=GREEN;
        else color=0x2965;

        if(i%2==0) lcdDrawFillRect(&dev,40+(i*23),112,41+(i*23),119,color);
        else lcdDrawFillRect(&dev,40+(i*23),114,40+(i*23),119,color);
    }
}

uint16_t line_add=0;
signed char line_buf[290];
signed char line_buf_old[290];
//波形图
//V范围：0-100  127为空
//sign:正负号
void ui_line_chart(uint32_t indata,uint8_t sign)
{
    uint16_t i=0;
    signed char line_cache[290];
    signed char v=0;
    //画网格
    for(i=0;i<59;i++)
    {
        lcdDrawPixel(&dev,29+(i*5),158,DGRAY);
        lcdDrawPixel(&dev,29+(i*5),198,DGRAY);
    }
    for(i=0;i<17;i++)
    {
        lcdDrawPixel(&dev,69,158+(i*5),DGRAY);
        lcdDrawPixel(&dev,109,158+(i*5),DGRAY);
        lcdDrawPixel(&dev,149,158+(i*5),DGRAY);
        lcdDrawPixel(&dev,189,158+(i*5),DGRAY);
        lcdDrawPixel(&dev,229,158+(i*5),DGRAY);
        lcdDrawPixel(&dev,269,158+(i*5),DGRAY);
        lcdDrawPixel(&dev,309,158+(i*5),DGRAY);
    } 
    memcpy(line_cache,line_buf,290);
    if(indata!=127)//按比例缩放，实际屏幕留给波形图的是 +-40点
    {
        if(indata>100) indata=100;
        indata=indata/2.564;
        if(sign==1) //负数
        {
            v=indata-2*indata;
        }
        else
        {
            v=indata;
        }
    }
    else v=indata;
    //画到最后一个点 开始移位
    if(++line_add>289)
    {
        line_add=289;
        for(i=0;i<289;i++)
        {
            line_buf[i]=line_buf[i+1];
        }  
    }
    line_buf[line_add]=v;  
    //画到最后一个点，开始位移波形，用黑色擦除旧的波形
    if(line_add==289)
    {
        for(i=0;i<290;i++)
        {
           if(line_buf_old[i]!=line_buf[i]) //如果擦除的点与准备要画的点重叠，那就不擦除了，防止闪烁
           {
               lcdDrawPixel(&dev,30+i,198-line_buf_old[i],BLACK); 
           }
        }  
    }
    memcpy(line_buf_old,line_cache,290);//记录旧波形
    for(i=0;i<290;i++)
    {
       if(line_buf[i]!=127)
       {
           lcdDrawPixel(&dev,30+i,198-line_buf[i],GREEN);
       }
    }
}

uint8_t fun2_key=0;

//刷新顶部按钮
void top_button_refresh()
{
    if(measure_stop==0)
    {
        lcdDrawFillRect(&dev,50,3,150,28,DGRAY);
        LCD_monochrome_any(&dev,55,3,ztcl_96x24,96,24,WHITE,DGRAY);
    }
    else
    {
        lcdDrawFillRect(&dev,50,3,150,28,0x4228);
        LCD_monochrome_any(&dev,55,3,jxcl_96x24,96,24,0xD69A,0x4228);
    }
    
    if(fun2_key==0)
    {
        lcdDrawFillRect(&dev,170,3,270,28,DGRAY);
        LCD_monochrome_any(&dev,173,3,biaojiceliangzhi,96,24,WHITE,DGRAY);
    }
    else
    {
        lcdDrawFillRect(&dev,170,3,270,28,0x4228);
        LCD_monochrome_any(&dev,173,3,biaojiceliangzhi,96,24,0xD69A,0x4228);
    }  
}


//刷新万用表页面左侧图标
void left_icon_refresh()
{
    if((sd_onoff==1)&(sd_state==1))
    {
        LCD_monochrome_24X24(&dev,2,38,Icon3,GREEN,DGRAY);
    }
    else
    {
        LCD_monochrome_24X24(&dev,2,38,Icon3,0x4228,DGRAY);
    }

    if((key_sound==0)&(on_off_sound==0)&(low_power_sound==0))
    {
        LCD_monochrome_24X24(&dev,4,65,Icon4,0x4228,DGRAY);
    }
    else
    {
        LCD_monochrome_24X24(&dev,4,65,Icon5,GREEN,DGRAY);
    }
}

//顶部图标
void top_icon_refresh()
{
    if(electricity_st==254)//充电中
    {
        LCD_monochrome_32X32(&dev,286,0,Icon_batt_ch,RED,BLACK);
    }
    else if(electricity_st==1)// 1%
    {
        LCD_monochrome_32X32(&dev,286,0,Icon_batt_1,RED,BLACK);
    }
    else if(electricity_st==25)// 25%
    {
        LCD_monochrome_32X32(&dev,286,0,Icon_batt_25,WHITE,BLACK);
    }
    else if(electricity_st==50)// 50%
    {
        LCD_monochrome_32X32(&dev,286,0,Icon_batt_50,WHITE,BLACK);
    }
    else if(electricity_st==75)// 75%
    {
        LCD_monochrome_32X32(&dev,286,0,Icon_batt_75,WHITE,BLACK);
    }
    else if(electricity_st==100)// 100%
    {
        LCD_monochrome_32X32(&dev,286,0,Icon_batt_100,WHITE,BLACK);
    }

    if(wifi_on==0)//wifi关闭
    {
        LCD_monochrome_32X32(&dev,1,0,Icon_wifi_off,WHITE,BLACK);
    }
    else if(net_state==0)
    {
        LCD_monochrome_32X32(&dev,1,0,Icon_wifi_no,WHITE,BLACK);
    }
    else if(net_state==1)
    {
        LCD_monochrome_32X32(&dev,1,0,Icon_wifi_lu,WHITE,BLACK);
    }
    else if(net_state==2)
    {
        LCD_monochrome_32X32(&dev,1,0,Icon_wifi,WHITE,BLACK);
    }

}


//绘制万用表UI 初始化
void vol_frame_init(uint8_t select)
{
    int i=0;

    char d_str[]="U < 0.30";
    char r_str[]="R < 030";


    lcdFillScreen(&dev, BLACK);

    for(i=0;i<290;i++)
    {
        line_buf[i]=127;
    }
    line_add=0;


    //左侧底色和图标
    lcdDrawFillRect(&dev,0,33,28,130,DGRAY);
    lcdDrawFillRect(&dev,0,33,315,36,DGRAY);
    left_icon_refresh();

    //顶部图标
    top_icon_refresh();
    
    
    //顶部按钮
    top_button_refresh();
 
    //波形图
    lcdDrawFillRect(&dev,28,158,29,239,DGRAY);
    lcdDrawFillRect(&dev,28,238,319,239,DGRAY);
    for(i=0;i<59;i++)
    {
        lcdDrawPixel(&dev,29+(i*5),158,DGRAY);
        lcdDrawPixel(&dev,29+(i*5),198,DGRAY);
    }
    for(i=0;i<17;i++)
    {
        lcdDrawPixel(&dev,69,158+(i*5),DGRAY);
        lcdDrawPixel(&dev,109,158+(i*5),DGRAY);
        lcdDrawPixel(&dev,149,158+(i*5),DGRAY);
        lcdDrawPixel(&dev,189,158+(i*5),DGRAY);
        lcdDrawPixel(&dev,229,158+(i*5),DGRAY);
        lcdDrawPixel(&dev,269,158+(i*5),DGRAY);
        lcdDrawPixel(&dev,309,158+(i*5),DGRAY);
    }
    LCD_ASCII_8X16_str(&dev,7,156," 1",2,WHITE,BLACK);
    LCD_ASCII_8X16_str(&dev,7,191," 0",2,WHITE,BLACK);
    LCD_ASCII_8X16_str(&dev,7,225,"-1",2,WHITE,BLACK);

    //上下限指示条
    // lcdDrawRect(&dev,300,58,310,135,GREEN);
    // lcdDrawFillRect(&dev,300,90,310,135,GREEN);
    // LCD_monochrome_any(&dev,291,43,shangxian,24,12,WHITE,BLACK);
    // LCD_monochrome_any(&dev,291,140,xiaxian,24,12,WHITE,BLACK);

    //右侧底色
    lcdDrawFillRect(&dev,287,38,315,150,DGRAY);


    ui_value_bar(0);
    LCD_ASCII_8X16(&dev,37,95,'0',LGRAY,BLACK);
    LCD_ASCII_8X16(&dev,83,95,'2',LGRAY,BLACK);
    LCD_ASCII_8X16(&dev,129,95,'4',LGRAY,BLACK);
    LCD_ASCII_8X16(&dev,175,95,'6',LGRAY,BLACK);
    LCD_ASCII_8X16(&dev,221,95,'8',LGRAY,BLACK);
    LCD_ASCII_8X16_str(&dev,263,95,"10",2,LGRAY,BLACK);

    //中间的Y轴比例和档位
    lcdDrawFillRect(&dev,0,132,82,150,DGRAY);
    LCD_monochrome_any(&dev,5,133,Y_32x16,32,16,WHITE,DGRAY);
    //LCD_ASCII_8X16_str(&dev,38,134,"0.01V",5,WHITE,DGRAY);
    lcdDrawFillRect(&dev,84,132,170,150,DGRAY);
    
    lcdDrawFillRect(&dev,172,132,285,150,DGRAY);

    if(current_fun==FUN_DCA)//直流电流档 显示采样电阻和最大测量范围
    {
        if(choice_dca==CUR_UA)
        {
            LCD_monochrome_any(&dev,174,133,R200_MAX5mA,112,16,WHITE,DGRAY);
        }
        else if(choice_dca==CUR_MA)
        {
            LCD_monochrome_any(&dev,174,133,R2_MAX500mA,112,16,WHITE,DGRAY);
        }
        else if(choice_dca==CUR_A)
        {
            LCD_monochrome_any(&dev,174,133,mR10_MAX10A,112,16,WHITE,DGRAY);
        }
    }
    else if(current_fun==FUN_ACA)//交流电流档 显示采样电阻和最大测量范围
    {
         if(choice_aca==CUR_MA)
        {
            LCD_monochrome_any(&dev,174,133,R2_MAX500mA,112,16,WHITE,DGRAY);
        }
        else if(choice_aca==CUR_A)
        {
            LCD_monochrome_any(&dev,174,133,mR10_MAX10A,112,16,WHITE,DGRAY);
        }
    }
    else if(current_fun==FUN_DIODE)//二极管
    {
        LCD_ASCII_8X16_str(&dev,40,134,"5V",2,WHITE,DGRAY);
        d_str[4]=d_buzzer_threshold/100%10+'0';
        d_str[6]=d_buzzer_threshold/10%10+'0';
        d_str[7]=d_buzzer_threshold%10+'0';
        LCD_ASCII_8X16_str(&dev,180,134,d_str,8,WHITE,DGRAY);
    }
    else if(current_fun==FUN_BEEP)//蜂鸣器
    {
        LCD_ASCII_8X16_str(&dev,38,134,"1000R",5,WHITE,DGRAY);
        r_str[4]=r_buzzer_threshold/100%10+'0';
        r_str[5]=r_buzzer_threshold/10%10+'0';
        r_str[6]=r_buzzer_threshold%10+'0';
        LCD_ASCII_8X16_str(&dev,180,134,r_str,7,WHITE,DGRAY);
    }
    else if(current_fun==FUN_DCW)//直流功率
    {
        if(choice_wdc==CUR_UA)
        {
            LCD_monochrome_any(&dev,174,133,R200_MAX5mA,112,16,WHITE,DGRAY);
        }
        else if(choice_wdc==CUR_MA)
        {
            LCD_monochrome_any(&dev,174,133,R2_MAX500mA,112,16,WHITE,DGRAY);
        }
        else if(choice_wdc==CUR_A)
        {
            LCD_monochrome_any(&dev,174,133,mR10_MAX10A,112,16,WHITE,DGRAY);
        }
    }
    else if(current_fun==FUN_ACW)//交流功率
    {
         if(choice_wac==CUR_MA)
        {
            LCD_monochrome_any(&dev,174,133,R2_MAX500mA,112,16,WHITE,DGRAY);
        }
        else if(choice_wac==CUR_A)
        {
            LCD_monochrome_any(&dev,174,133,mR10_MAX10A,112,16,WHITE,DGRAY);
        }
    }
    

    LCD_monochrome_any(&dev,91,133,ST_funchoice.text_p[select],72,16,WHITE,DGRAY);

    line_y_old=0xFF;
    //测量值和单位
    //LCD_ASCII_24X30_str(&dev,35,52,"-12.45678",9,YELLOW,BLACK);
    //LCD_monochrome_any(&dev,245,59,VDC,40,24,YELLOW,BLACK);
}



void vol_menu_refresh()
{
    int i=0;
    uint8_t start=0;
    uint8_t select=0;
    char num=0;
    char ch[]="  .";


    if(ST_frame_menu.select<ST_frame_menu.start)
    {
        ST_frame_menu.start--;
        ST_frame_menu.end--;
    }
    else if(ST_frame_menu.select>ST_frame_menu.end)
    {
        ST_frame_menu.start++;
        ST_frame_menu.end++;
    }

    start=ST_frame_menu.start;
    select=ST_frame_menu.select;
    for(i=0;i<(ST_frame_menu.end-ST_frame_menu.start+1);i++)//共显示几个菜单
    {
        ST_frame_menu.backColor[i+start]=DGRAY;
        ST_frame_menu.textColor[i+start]=LGRAY;
        if((i+start)==select)
        {
           ST_frame_menu.backColor[i+start]=CYAN;
           ST_frame_menu.textColor[i+start]=NAVY; 
        }
        lcdDrawFillRect(&dev,70,20+(i*45),250,54+(i*45),ST_frame_menu.backColor[i+start]);

        num=ST_frame_menu.start+i+1;
        
        ch[0]=num/10+'0';
        if(ch[0]=='0') ch[0]=' ';
        ch[1]=num%10+'0';
        LCD_ASCII_8X16_str(&dev,80,30+(i*45),ch,3,ST_frame_menu.textColor[i+start],ST_frame_menu.backColor[i+start]);
        LCD_monochrome_any(&dev,110,23+(i*45),ST_frame_menu.text_p[i+start],ST_frame_menu.width[i+start],ST_frame_menu.height[i+start],ST_frame_menu.textColor[i+start],ST_frame_menu.backColor[i+start]);
    }
}
//绘制菜单UI
void vol_menu_init()
{
    int i=0;
    ST_frame_menu.text_p[0]=lcsz_120x24;
    ST_frame_menu.text_p[1]=ccsz_120x24;
    ST_frame_menu.text_p[2]=fmqyz_120x24;
    ST_frame_menu.text_p[3]=wdcgqlx_120x24;
    ST_frame_menu.text_p[4]=vizhsz_120x24;
    ST_frame_menu.text_p[5]=ckcssz_120x24;
    ST_frame_menu.text_p[6]=bgsz_120x24;
    ST_frame_menu.text_p[7]=sykg_120x24;
    ST_frame_menu.text_p[8]=lyqpz_120x24;
    ST_frame_menu.text_p[9]=wlsz_120x24;
    //ST_frame_menu.text_p[10]=gjgx_120x24;
    ST_frame_menu.text_p[10]=sbxx_120x24;
    ST_frame_menu.text_p[11]=sysm_120x24;
 //   ST_frame_menu.select=0;
//    ST_frame_menu.start=0;
 //   ST_frame_menu.end=4;
    if(ST_frame_menu.end<4) ST_frame_menu.end=4;

    for(i=0;i<12;i++)
    {
        ST_frame_menu.height[i]=24;
        ST_frame_menu.width[i]=120;
        ST_frame_menu.backColor[i]=DGRAY;
        ST_frame_menu.textColor[i]=LGRAY;
    }
    LCD_ShowPicture(&dev,0,0,320,240,gImage_BL_320x240);
    vol_menu_refresh();
   // lcdFillScreen(&dev, BLACK); 
}

// n ：图标显示位置 0-7
// sel：是否被选中
// black：将整块区域填充黑色
// icon：图标取模数组 48x48
// text：文字取模数组 72x16
void funchoice_icon(uint8_t n,uint8_t sel,uint8_t black,const unsigned char *icon,const unsigned char *text)
{
    uint16_t Color1;
    uint16_t Color2;
    unsigned char const * bl;

    Color1=sel?WHITE:0X022D;
    Color2=sel?0X969D:LGRAY;
    bl=sel?gImage_menu_BL_on:gImage_menu_BL;
 
    if(n<4)
    {
        if(black==1)
        {
            lcdDrawFillRect(&dev,18+(n*73),45,90+(n*73),130,BLACK);
        }
        else
        {
            LCD_ShowPicture(&dev,20+(n*73),45,64,64,bl);
            LCD_monochrome_any(&dev,28+(n*73),53,icon,48,48,Color1,0X969D);
            LCD_monochrome_any(&dev,18+(n*73),113,text,72,16,Color2,BLACK);
        } 
    }
    else
    {
        n-=4;
        if(black==1)
        {
            lcdDrawFillRect(&dev,18+(n*73),145,90+(n*73),230,BLACK);
        }
        else
        {
            LCD_ShowPicture(&dev,20+(n*73),145,64,64,bl);
            LCD_monochrome_any(&dev,28+(n*73),153,icon,48,48,Color1,0X969D);
            LCD_monochrome_any(&dev,18+(n*73),213,text,72,16,Color2,BLACK);
        }
    }    
}

//刷新功能选择图标
void funchoice_refresh()
{
    int i=0;
    uint8_t page=0;
    uint8_t select=0;
    uint8_t icon_total=0;//图标总数

  //  uint8_t  start=0;       //起始位置
    uint8_t page_icon_n=0;//当前页图标数量

    page=ST_funchoice.page;
    select=ST_funchoice.select;
    icon_total=ST_funchoice.icon_total;
    page=select/8;

     //顶部快捷按钮

    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    if(shortcut_F1==0xFF)
    {
        LCD_monochrome_any(&dev,67,5,changanshezhi_72x16,72,16,LGRAY,0X022D);    
    }
    else
    {
        LCD_monochrome_any(&dev,67,5,ST_funchoice.text_p[shortcut_F1],72,16,LGRAY,0X022D);
    }

    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    if(shortcut_F2==0xFF)
    {
        LCD_monochrome_any(&dev,182,5,kuaijiegongneng_72x16,72,16,LGRAY,0X022D);
    }
    else
    {
        LCD_monochrome_any(&dev,182,5,ST_funchoice.text_p[shortcut_F2],72,16,LGRAY,0X022D);
    }
 

   //左侧 页条
    if(page==0)
    {
        lcdDrawRect(&dev,0,40,7,230,0X022D);
        lcdDrawFillRect(&dev,2,42,5,105,0X969D);
        lcdDrawFillRect(&dev,2,105,5,168,BLACK);
        lcdDrawFillRect(&dev,2,168,5,229,BLACK);
    }
    else if(page==1)
    {
        lcdDrawRect(&dev,0,40,7,230,0X022D);
        lcdDrawFillRect(&dev,2,42,5,105,BLACK);
        lcdDrawFillRect(&dev,2,105,5,168,0X969D);
        lcdDrawFillRect(&dev,2,168,5,229,BLACK);
    }
    else if(page==2)
    {
        lcdDrawRect(&dev,0,40,7,231,0X022D);
        lcdDrawFillRect(&dev,2,42,5,105,BLACK);
        lcdDrawFillRect(&dev,2,105,5,168,BLACK);
        lcdDrawFillRect(&dev,2,168,5,229,0X969D);
    }

    if(icon_total<=8)//只有一页
    {
        page_icon_n=icon_total;
  //      start=0;
    }
    else if(icon_total<=16)//有两页
    {
        if(page==0)
        {
            page_icon_n=8;
   //         start=0;
        }
        else if(page==1)
        {
            page_icon_n=icon_total-8;
    //        start=8;
        }
    }
    else if(icon_total<=24)//有三页
    {
        if(page==0)
        {
            page_icon_n=8;
    //        start=0;
        }
        else if(page==1)
        {
            page_icon_n=8;
     //       start=8;
        }
        else if(page==2)
        {
            page_icon_n=icon_total-16;
      //      start=16;
        }
    }
    
    for(i=0;i<8;i++)
    {
        if(i<page_icon_n)
        {
            if(select==((page*8)+i))//选中的图标
            {
                funchoice_icon(i,1,0,ST_funchoice.icon_p[((page*8)+i)],ST_funchoice.text_p[((page*8)+i)]);
            }
            else
            {
                funchoice_icon(i,0,0,ST_funchoice.icon_p[((page*8)+i)],ST_funchoice.text_p[((page*8)+i)]);
            } 
        }
        else//填充黑色
        {
            funchoice_icon(i,0,1,NULL,NULL);
        } 

    }

}

//绘制功能选择UI
void funchoice_init()
{
    ST_funchoice.icon_total=18;
    
    ST_funchoice.icon_p[0]=Icon_menu_zldy ;
    ST_funchoice.icon_p[1]=Icon_menu_jldy ;
    ST_funchoice.icon_p[2]=Icon_menu_zldl ;
    ST_funchoice.icon_p[3]=Icon_menu_jldl ;
    ST_funchoice.icon_p[4]=Icon_menu_lxdz ;
    ST_funchoice.icon_p[5]=Icon_menu_sxdz ;
    ST_funchoice.icon_p[6]=Icon_menu_ejg ;
    ST_funchoice.icon_p[7]=Icon_menu_tdd ;
    ST_funchoice.icon_p[8]=Icon_menu_zlgl ;
    ST_funchoice.icon_p[9]=Icon_menu_jlgl ;
    ST_funchoice.icon_p[10]=Icon_menu_clwd ;
    ST_funchoice.icon_p[11]=Icon_menu_dydlzh ;
    ST_funchoice.icon_p[12]=Icon_menu_uart ;
    ST_funchoice.icon_p[13]=Icon_menu_rs485 ;
    ST_funchoice.icon_p[14]=Icon_menu_can ;
    ST_funchoice.icon_p[15]=Icon_menu_zlxhy ;
    ST_funchoice.icon_p[16]=Icon_menu_pwm ;
    ST_funchoice.icon_p[17]=Icon_menu_djkz ;

    ST_funchoice.text_p[0]=zldy_72x16;
    ST_funchoice.text_p[1]=jldy_72x16;
    ST_funchoice.text_p[2]=zldl_72x16;
    ST_funchoice.text_p[3]=jldl_72x16;
    ST_funchoice.text_p[4]=lxdz_72x16;
    ST_funchoice.text_p[5]=sxdz_72x16;
    ST_funchoice.text_p[6]=ejg_72x16;
    ST_funchoice.text_p[7]=tdd_72x16;
    ST_funchoice.text_p[8]=zlgl_72x16;
    ST_funchoice.text_p[9]=jlgl_72x16;
    ST_funchoice.text_p[10]=clwd_72x16;
    ST_funchoice.text_p[11]=dydlzh_72x16;
    ST_funchoice.text_p[12]=uart_72x16;
    ST_funchoice.text_p[13]=rs485_72x16;
    ST_funchoice.text_p[14]=can_72x16;
    ST_funchoice.text_p[15]=zlxhy_72x16;
    ST_funchoice.text_p[16]=pwm_72x16;
    ST_funchoice.text_p[17]=djkz_72x16;

    
    ST_funchoice.page=0;
    //ST_funchoice.select=0;
    //LCD_ShowPicture(&dev,0,0,320,240,gImage_fun_ui);
    lcdFillScreen(&dev, BLACK);
    

    funchoice_refresh();
}


//功能选择
void display_funchoice()
{
    uint8_t evt;
     
    funchoice_init();
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(ST_funchoice.select<17) 
                {
                    ST_funchoice.select++;
                    funchoice_refresh();
                }
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(ST_funchoice.select>0) 
                {
                    ST_funchoice.select--;
                    funchoice_refresh();
                }
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_st=ST_funchoice.select;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //功能选择页面按返回，回到之前的面板页面
                //ui_display_st=ui_display_old;
                // ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(shortcut_F1!=0xFF)
                {
                    ui_display_st=shortcut_F1;
                    ST_funchoice.select=shortcut_F1;
                    ui_exit=1;
                }
                break;
            case KEY_F2_UP: //功能键2
                if(shortcut_F2!=0xFF)
                {
                    ui_display_st=shortcut_F2;
                    ST_funchoice.select=shortcut_F2;
                    ui_exit=1;
                }
                break;
            case KEY_F1_LONG: //长按功能键1
                shortcut_F1=ST_funchoice.select;
                write_config_in_nvs(CONFIG_OTHER); 
                funchoice_refresh();
                break;
            case KEY_F2_LONG: //长按功能键2
                shortcut_F2=ST_funchoice.select;
                write_config_in_nvs(CONFIG_OTHER); 
                funchoice_refresh();
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            }
        }
        if(ui_exit==1) break;
    }
}

//菜单选择
void display_menu()
{
    uint8_t evt;
     

    vol_menu_init();
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(ST_frame_menu.select>0) 
                {
                    ST_frame_menu.select--;
                    vol_menu_refresh();
                }
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(ST_frame_menu.select<11) 
                {
                    ST_frame_menu.select++;
                    vol_menu_refresh();
                }
                break;
            case KEY_ENTER: //旋转编码器 按下
                switch (ST_frame_menu.select)
                {
                case 0:
                    ui_display_st=DISPLAY_SETUP_RANGE;
                    break;
                case 1:
                    ui_display_st=DISPLAY_SETUP_SAVE;
                    break;
                case 2:
                    ui_display_st=DISPLAY_SETUP_BUZZER;
                    break;
                case 3:
                    ui_display_st=DISPLAY_SETUP_TEMP;
                    break;
                case 4:
                    ui_display_st=DISPLAY_SETUP_V_I;
                    break;
                case 5:
                    ui_display_st=DISPLAY_SETUP_PORT;
                    break;
                case 6:
                    ui_display_st=DISPLAY_SETUP_BACKLIGHT;
                    break;
                case 7:
                    ui_display_st=DISPLAY_SETUP_VOICE;
                    break;
                case 8:
                    ui_display_st=DISPLAY_SETUP_ROUTER;
                    break;
                case 9:
                    ui_display_st=DISPLAY_SETUP_NET;
                    break;
                case 10:
                    ui_display_st=DISPLAY_SETUP_INFO;
                    break;
                case 11:
                    ui_display_st=DISPLAY_INSTRUCTIONS;
                    break;
                default:
                    break;
                }
                ui_exit=1;
                break;    
            case KEY_PWR_BACK: //开关机键  返回键
                ui_display_st=ui_display_old;
                ui_exit=1;
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            }
        }
        if(ui_exit==1) break;
    }
}



// 直流电压 面板
void display_dcv(uint8_t select)
{
    uint8_t evt;
    
    char strbuf[10]; 
    choice_dcv_old=0xFF;
    vol_frame_init(select);
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case REFRESH_VALUE: //刷新测量值

                strbuf[0]=sign?'-':' ';
                if(unit==0x01)//单位uV
                {
                    ui_line_chart(measured_value/line_y_scale_cal,sign);
                    if(measured_value<1000000)//小于1V 999.99mV
                    {
                        ui_value_bar(measured_value/10000);
                        strbuf[1]=measured_value/100000%10+'0';
                        strbuf[2]=measured_value/10000%10+'0';
                        strbuf[3]=measured_value/1000%10+'0';
                        strbuf[4]='.';
                        strbuf[5]=measured_value/100%10+'0';
                        strbuf[6]=measured_value/10%10+'0'; 
                        LCD_monochrome_any(&dev,215,59,mVDC,56,24,BLACK,YELLOW);
                    }
                    else if(measured_value<10000000)//小于10V 9.9999V
                    {
                        ui_value_bar(measured_value/100000);
                        strbuf[1]=measured_value/1000000%10+'0';
                        strbuf[2]='.';
                        strbuf[3]=measured_value/100000%10+'0';
                        strbuf[4]=measured_value/10000%10+'0';
                        strbuf[5]=measured_value/1000%10+'0';
                        strbuf[6]=measured_value/100%10+'0'; 
                        
                        LCD_monochrome_any(&dev,215,59,VDC,56,24,BLACK,YELLOW);
                    }
                    else
                    {
                        ui_value_bar(0);
                        strbuf[1]='-';
                        strbuf[2]='-';;
                        strbuf[3]='.';
                        strbuf[4]='-';;
                        strbuf[5]='-';;
                        strbuf[6]=' ';;
                    }
                }
                else if(unit==0x02)//单位mV
                {
                    ui_line_chart(measured_value*1000/line_y_scale_cal,sign);
                     if(measured_value<100000)//小于100V 99.999V
                    {
                        ui_value_bar(measured_value/1000);
                        strbuf[1]=measured_value/10000%10+'0';
                        strbuf[2]=measured_value/1000%10+'0';
                        strbuf[3]='.';
                        strbuf[4]=measured_value/100%10+'0';
                        strbuf[5]=measured_value/10%10+'0';
                        strbuf[6]=measured_value%10+'0'; 
                        LCD_monochrome_any(&dev,215,59,VDC,56,24,BLACK,YELLOW);
                    }
                    else if(measured_value<1000000)//小于1000V 999.99V
                    {
                        ui_value_bar(measured_value/10000);
                        strbuf[1]=measured_value/100000%10+'0';
                        strbuf[2]=measured_value/10000%10+'0';
                        strbuf[3]=measured_value/1000%10+'0';
                        strbuf[4]='.';
                        strbuf[5]=measured_value/100%10+'0';
                        strbuf[6]=measured_value/10%10+'0'; 
                        LCD_monochrome_any(&dev,215,59,VDC,56,24,BLACK,YELLOW);
                    }
                    else
                    {
                        ui_value_bar(0);
                        strbuf[1]='-';
                        strbuf[2]='-';
                        strbuf[3]='.';
                        strbuf[4]='-';
                        strbuf[5]='-';
                        strbuf[6]=' ';
                    }
                }
                else
                {
                    ui_value_bar(0);
                    strbuf[1]=' ';
                    strbuf[2]=' ';
                    strbuf[3]=' ';
                    strbuf[4]=' ';
                    strbuf[5]=' ';
                    strbuf[6]=' ';
                }
                

                LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,YELLOW,BLACK);
                if(line_y_old!=line_y_dcv)
                {
                    line_y_old=line_y_dcv;
                    
                    switch (line_y_dcv)
                    {
                    case SCALE_X0_001:
                        LCD_ASCII_8X16_str(&dev,38,134,"1mV  ",5,WHITE,DGRAY);
                        line_y_scale_cal=10;
                        break;    
                    case SCALE_X0_01:
                        LCD_ASCII_8X16_str(&dev,38,134,"10mV ",5,WHITE,DGRAY);
                        line_y_scale_cal=100;
                        break;
                    case SCALE_X0_1:
                        LCD_ASCII_8X16_str(&dev,38,134,"100mV ",5,WHITE,DGRAY);
                        line_y_scale_cal=1000;
                        break;
                    case SCALE_X1:
                        LCD_ASCII_8X16_str(&dev,38,134,"1V   ",5,WHITE,DGRAY);
                        line_y_scale_cal=10000;
                        break;
                    case SCALE_X10:
                        LCD_ASCII_8X16_str(&dev,38,134,"10V  ",5,WHITE,DGRAY);
                        line_y_scale_cal=100000;
                        break;
                    case SCALE_X100:
                        LCD_ASCII_8X16_str(&dev,38,134,"100V ",5,WHITE,DGRAY);
                        line_y_scale_cal=1000000;
                        break;
                    case SCALE_X1000:
                        LCD_ASCII_8X16_str(&dev,38,134,"1000V",5,WHITE,DGRAY);
                        line_y_scale_cal=10000000;
                        break;
                    default:
                        break;
                    }  
                }

                if(choice_dcv!=choice_dcv_old) //切换量程
                {
                    choice_dcv_old=choice_dcv;
                    switch (choice_dcv)
                    {
                    case VOL_AUTO:
                        analog_switch(ASW_DCV0);
                        LCD_monochrome_any(&dev,173,133,dyzd_112x16,112,16,WHITE,DGRAY);
                        break;
                    case VOL_10V:
                        analog_switch(ASW_DCV2);
                        LCD_monochrome_any(&dev,173,133,dy10v_112x16,112,16,WHITE,DGRAY);
                        break;
                    case VOL_100V:
                        analog_switch(ASW_DCV1);
                        LCD_monochrome_any(&dev,173,133,dy100v_112x16,112,16,WHITE,DGRAY);
                        break;
                    case VOL_1000V:
                        analog_switch(ASW_DCV0);
                        LCD_monochrome_any(&dev,173,133,dy1000v_112x16,112,16,WHITE,DGRAY);
                        break;
                    
                    default:
                        break;
                    }
                }
                break;
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(line_y_dcv<SCALE_X1000) line_y_dcv++;
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(line_y_dcv>0) line_y_dcv--;
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(measure_stop==0) 
                {
                    measure_stop=1;
                    top_button_refresh();
                    vTaskDelay(pdMS_TO_TICKS(100));//给ADC最后一次刷新数值留出时间
                    LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,0xB500,BLACK);//暂停测量后，测量值为灰黄色
                }
                else 
                {
                    measure_stop=0;
                    top_button_refresh();
                }
                break;
            case KEY_F2_DOWN: //功能键2 按下
                fun2_key=1;
                evt=WIFINET_MARK;
                xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                top_button_refresh();
                break;
            case KEY_F2_UP: //功能键2 短按抬起
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_F2_LONG: //功能键2 长按
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_PWROFF: //关机
            ui_display_st=DISPLAY_PWROFF;
                ui_exit=1;
                break;
            case REFRESH_LEFT_ICON: //刷新左侧图标
                left_icon_refresh();
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
            
        }
        if(ui_exit==1) break;
    }
}


// 交流电压 面板
void display_acv(uint8_t select)
{
    uint8_t evt;
    
    char strbuf[10]; 
    choice_acv_old=0xFF;
    vol_frame_init(select);
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case REFRESH_VALUE: //刷新测量值
            
                strbuf[0]=sign?'-':' ';
                if(unit==0x01)//单位uV
                {
                    ui_line_chart(measured_value/line_y_scale_cal,sign);
                    if(measured_value<1000000)//小于1V 999.99mV
                    {
                        ui_value_bar(measured_value/10000);
                        strbuf[1]=measured_value/100000%10+'0';
                        strbuf[2]=measured_value/10000%10+'0';
                        strbuf[3]=measured_value/1000%10+'0';
                        strbuf[4]='.';
                        strbuf[5]=measured_value/100%10+'0';
                        strbuf[6]=measured_value/10%10+'0'; 
                        LCD_monochrome_any(&dev,215,59,mVAC,56,24,BLACK,YELLOW);
                    }
                    else if(measured_value<10000000)//小于10V 9.9999V
                    {
                        ui_value_bar(measured_value/100000);
                        strbuf[1]=measured_value/1000000%10+'0';
                        strbuf[2]='.';
                        strbuf[3]=measured_value/100000%10+'0';
                        strbuf[4]=measured_value/10000%10+'0';
                        strbuf[5]=measured_value/1000%10+'0';
                        strbuf[6]=measured_value/100%10+'0'; 
                        
                        LCD_monochrome_any(&dev,215,59,VAC,56,24,BLACK,YELLOW);
                    }
                    else
                    {
                        ui_value_bar(0);
                        strbuf[1]='-';
                        strbuf[2]='-';;
                        strbuf[3]='.';
                        strbuf[4]='-';;
                        strbuf[5]='-';;
                        strbuf[6]=' ';;
                    }
                }
                else if(unit==0x02)//单位mV
                {
                    ui_line_chart(measured_value*1000/line_y_scale_cal,sign);
                     if(measured_value<100000)//小于100V 99.999V
                    {
                        ui_value_bar(measured_value/1000);
                        strbuf[1]=measured_value/10000%10+'0';
                        strbuf[2]=measured_value/1000%10+'0';
                        strbuf[3]='.';
                        strbuf[4]=measured_value/100%10+'0';
                        strbuf[5]=measured_value/10%10+'0';
                        strbuf[6]=measured_value%10+'0'; 
                        LCD_monochrome_any(&dev,215,59,VAC,56,24,BLACK,YELLOW);
                    }
                    else if(measured_value<1000000)//小于1000V 999.99V
                    {
                        ui_value_bar(measured_value/10000);
                        strbuf[1]=measured_value/100000%10+'0';
                        strbuf[2]=measured_value/10000%10+'0';
                        strbuf[3]=measured_value/1000%10+'0';
                        strbuf[4]='.';
                        strbuf[5]=measured_value/100%10+'0';
                        strbuf[6]=measured_value/10%10+'0'; 
                        LCD_monochrome_any(&dev,215,59,VAC,56,24,BLACK,YELLOW);
                    }
                    else
                    {
                        ui_value_bar(0);
                        strbuf[1]='-';
                        strbuf[2]='-';;
                        strbuf[3]='.';
                        strbuf[4]='-';;
                        strbuf[5]='-';;
                        strbuf[6]=' ';;
                    }
                }
                else
                {
                    ui_value_bar(0);
                    strbuf[1]=' ';
                    strbuf[2]=' ';
                    strbuf[3]=' ';
                    strbuf[4]=' ';
                    strbuf[5]=' ';
                    strbuf[6]=' ';
                }

                LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,YELLOW,BLACK);

                if(line_y_old!=line_y_acv)
                {
                    line_y_old=line_y_acv;
                    LCD_monochrome_any(&dev,173,133,dyzd_112x16,112,16,DGRAY,WHITE);
                    switch (line_y_acv)
                    {
                    case SCALE_X0_001:
                        LCD_ASCII_8X16_str(&dev,38,134,"1mV  ",5,WHITE,DGRAY);
                        line_y_scale_cal=10;
                        break;    
                    case SCALE_X0_01:
                        LCD_ASCII_8X16_str(&dev,38,134,"10mV ",5,WHITE,DGRAY);
                        line_y_scale_cal=100;
                        break;
                    case SCALE_X0_1:
                        LCD_ASCII_8X16_str(&dev,38,134,"100mV ",5,WHITE,DGRAY);
                        line_y_scale_cal=1000;
                        break;
                    case SCALE_X1:
                        LCD_ASCII_8X16_str(&dev,38,134,"1V   ",5,WHITE,DGRAY);
                        line_y_scale_cal=10000;
                        break;
                    case SCALE_X10:
                        LCD_ASCII_8X16_str(&dev,38,134,"10V  ",5,WHITE,DGRAY);
                        line_y_scale_cal=100000;
                        break;
                    case SCALE_X100:
                        LCD_ASCII_8X16_str(&dev,38,134,"100V ",5,WHITE,DGRAY);
                        line_y_scale_cal=1000000;
                        break;
                    case SCALE_X1000:
                        LCD_ASCII_8X16_str(&dev,38,134,"1000V",5,WHITE,DGRAY);
                        line_y_scale_cal=10000000;
                        break;
                    default:
                        break;
                    }  
                }

                if(choice_acv!=choice_acv_old) //切换量程
                {
                    choice_acv_old=choice_acv;
                    switch (choice_acv)
                    {
                    case VOL_AUTO:
                        analog_switch(ASW_ACV0);
                        LCD_monochrome_any(&dev,173,133,dyzd_112x16,112,16,WHITE,DGRAY);
                        break;
                    case VOL_10V:
                        analog_switch(ASW_ACV2);
                        LCD_monochrome_any(&dev,173,133,dy10v_112x16,112,16,WHITE,DGRAY);
                        break;
                    case VOL_100V:
                        analog_switch(ASW_ACV1);
                        LCD_monochrome_any(&dev,173,133,dy100v_112x16,112,16,WHITE,DGRAY);
                        break;
                    case VOL_1000V:
                        analog_switch(ASW_ACV0);
                        LCD_monochrome_any(&dev,173,133,dy1000v_112x16,112,16,WHITE,DGRAY);
                        break;
                    
                    default:
                        break;
                    }
                }

                break;
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(line_y_acv<SCALE_X1000) line_y_acv++;
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(line_y_acv>0) line_y_acv--;
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(measure_stop==0) 
                {
                    measure_stop=1;
                    top_button_refresh();
                    vTaskDelay(pdMS_TO_TICKS(100));//给ADC最后一次刷新数值留出时间
                    LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,0xB500,BLACK);//暂停测量后，测量值为灰黄色
                }
                else 
                {
                    measure_stop=0;
                    top_button_refresh();
                }
                break;
           case KEY_F2_DOWN: //功能键2 按下
                fun2_key=1;
                evt=WIFINET_MARK;
                xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                top_button_refresh();
                break;
            case KEY_F2_UP: //功能键2 短按抬起
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_F2_LONG: //功能键2 长按
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_LEFT_ICON: //刷新左侧图标
                left_icon_refresh();
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }

}



// 直流电流 面板
void display_dca(uint8_t select)
{
    uint8_t evt;
    
    char strbuf[10]; 

    vol_frame_init(select);
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case REFRESH_VALUE: //刷新测量值
  
                strbuf[0]=sign?'-':' ';
                if(measured_value<1000000)//小于1mA  1A
                {
                    ui_value_bar(measured_value/10000);
                    strbuf[1]=measured_value/100000%10+'0';
                    strbuf[2]=measured_value/10000%10+'0';
                    strbuf[3]=measured_value/1000%10+'0';
                    strbuf[4]='.';
                    strbuf[5]=measured_value/100%10+'0';
                    strbuf[6]=measured_value/10%10+'0';
                    if(choice_dca==CUR_UA)
                    {
                        ui_line_chart(measured_value/line_y_scale_cal/1000,sign);
                        LCD_monochrome_any(&dev,215,59,uAdc,56,24,BLACK,YELLOW);
                    }
                    else
                    {
                        ui_line_chart(measured_value/line_y_scale_cal,sign);
                        LCD_monochrome_any(&dev,215,59,mAdc,56,24,BLACK,YELLOW);
                    }
                    
                }
                else if(measured_value<10000000)//小于10V 9.9999V
                {
                    ui_value_bar(measured_value/100000);
                    strbuf[1]=measured_value/1000000%10+'0';
                    strbuf[2]='.';
                    strbuf[3]=measured_value/100000%10+'0';
                    strbuf[4]=measured_value/10000%10+'0';
                    strbuf[5]=measured_value/1000%10+'0';
                    strbuf[6]=measured_value/100%10+'0'; 
                    if(choice_dca==CUR_UA)
                    {
                        ui_line_chart(measured_value/line_y_scale_cal/1000,sign);
                        LCD_monochrome_any(&dev,215,59,mAdc,56,24,BLACK,YELLOW);
                    }
                    else
                    {
                        ui_line_chart(measured_value/line_y_scale_cal,sign);
                        LCD_monochrome_any(&dev,215,59,Adc,56,24,BLACK,YELLOW);
                    }
                }
                else if(measured_value<100000000)//小于100V 99.999V
                {
                    ui_line_chart(measured_value/line_y_scale_cal,sign);

                    ui_value_bar(measured_value/1000000);
                    strbuf[1]=measured_value/10000000%10+'0';
                    strbuf[2]=measured_value/1000000%10+'0';
                    strbuf[3]='.';
                    strbuf[4]=measured_value/100000%10+'0';
                    strbuf[5]=measured_value/10000%10+'0';
                    strbuf[6]=measured_value/1000%10+'0'; 
                    LCD_monochrome_any(&dev,215,59,Adc,56,24,BLACK,YELLOW);
                }
                else
                {
                    ui_line_chart(measured_value/line_y_scale_cal,sign);
                    
                    strbuf[1]='-';
                    strbuf[2]='-';;
                    strbuf[3]='.';
                    strbuf[4]='-';;
                    strbuf[5]='-';;
                    strbuf[6]=' ';;
                }
                LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,YELLOW,BLACK);

                if(line_y_old!=line_y_dca)
                {
                    line_y_old=line_y_dca;
                    switch (line_y_dca)
                    {
                    case SCALE_X0_001:
                        LCD_ASCII_8X16_str(&dev,38,134,"1mA  ",5,WHITE,DGRAY);
                        line_y_scale_cal=10;
                        break;
                    case SCALE_X0_01:
                        LCD_ASCII_8X16_str(&dev,38,134,"10mA ",5,WHITE,DGRAY);
                        line_y_scale_cal=100;
                        break;
                    case SCALE_X0_1:
                        LCD_ASCII_8X16_str(&dev,38,134,"100mA",5,WHITE,DGRAY);
                        line_y_scale_cal=1000;
                        break;
                    case SCALE_X1:
                        LCD_ASCII_8X16_str(&dev,38,134,"1A   ",5,WHITE,DGRAY);
                        line_y_scale_cal=10000;
                        break;
                    case SCALE_X10:
                        LCD_ASCII_8X16_str(&dev,38,134,"10A  ",5,WHITE,DGRAY);
                        line_y_scale_cal=100000;
                        break;
                    
                    default:
                        break;
                    }  
                }

                if(choice_dca_old!=choice_dca)
                {
                    choice_dca_old=choice_dca;
                    switch (choice_dca)
                    {
                    case CUR_UA:
                        LCD_monochrome_any(&dev,174,133,R200_MAX5mA,112,16,WHITE,DGRAY);
                        break;
                    case CUR_MA:
                        LCD_monochrome_any(&dev,174,133,R2_MAX500mA,112,16,WHITE,DGRAY);
                        break;
                    case CUR_A:
                        LCD_monochrome_any(&dev,174,133,mR10_MAX10A,112,16,WHITE,DGRAY);
                        break; 
                    default:
                        break;
                    }
                }

                break;
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(line_y_dca<SCALE_X10) line_y_dca++;
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(line_y_dca>0) line_y_dca--;
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(measure_stop==0) 
                {
                    measure_stop=1;
                    top_button_refresh();
                    vTaskDelay(pdMS_TO_TICKS(100));//给ADC最后一次刷新数值留出时间
                    LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,0xB500,BLACK);//暂停测量后，测量值为灰黄色
                }
                else 
                {
                    measure_stop=0;
                    top_button_refresh();
                }
                // choice_dca=CUR_UA;
                // analog_switch(ASW_DCUA);
                break;
           case KEY_F2_DOWN: //功能键2 按下
                fun2_key=1;
                evt=WIFINET_MARK;
                xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                top_button_refresh();
                break;
            case KEY_F2_UP: //功能键2 短按抬起
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_F2_LONG: //功能键2 长按
                fun2_key=0;
                top_button_refresh();
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_LEFT_ICON: //刷新左侧图标
                left_icon_refresh();
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}

// 交流电流 面板
void display_aca(uint8_t select)
{
    uint8_t evt;
    
    char strbuf[10]; 

    vol_frame_init(select);
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case REFRESH_VALUE: //刷新测量值

                ui_line_chart(measured_value/line_y_scale_cal,sign);
                
                strbuf[0]=sign?'-':' ';
                if(measured_value<1000000)//小于1mA  1A
                {
                    ui_value_bar(measured_value/10000);
                    strbuf[1]=measured_value/100000%10+'0';
                    strbuf[2]=measured_value/10000%10+'0';
                    strbuf[3]=measured_value/1000%10+'0';
                    strbuf[4]='.';
                    strbuf[5]=measured_value/100%10+'0';
                    strbuf[6]=measured_value/10%10+'0';
                    LCD_monochrome_any(&dev,215,59,mAac,56,24,BLACK,YELLOW);
                }
                else if(measured_value<10000000)//小于10V 9.9999V
                {
                    ui_value_bar(measured_value/100000);
                    strbuf[1]=measured_value/1000000%10+'0';
                    strbuf[2]='.';
                    strbuf[3]=measured_value/100000%10+'0';
                    strbuf[4]=measured_value/10000%10+'0';
                    strbuf[5]=measured_value/1000%10+'0';
                    strbuf[6]=measured_value/100%10+'0'; 
                    LCD_monochrome_any(&dev,215,59,Aac,56,24,BLACK,YELLOW); 
                }
                else if(measured_value<100000000)//小于100V 99.999V
                {
                    ui_line_chart(measured_value/line_y_scale_cal,sign);

                    ui_value_bar(measured_value/1000000);
                    strbuf[1]=measured_value/10000000%10+'0';
                    strbuf[2]=measured_value/1000000%10+'0';
                    strbuf[3]='.';
                    strbuf[4]=measured_value/100000%10+'0';
                    strbuf[5]=measured_value/10000%10+'0';
                    strbuf[6]=measured_value/1000%10+'0'; 
                    LCD_monochrome_any(&dev,215,59,Aac,56,24,BLACK,YELLOW);
                }
                else
                {
                   
                    strbuf[1]='-';
                    strbuf[2]='-';;
                    strbuf[3]='.';
                    strbuf[4]='-';;
                    strbuf[5]='-';;
                    strbuf[6]=' ';;
                }
                LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,YELLOW,BLACK);

                if(line_y_old!=line_y_aca)
                {
                    line_y_old=line_y_aca;
                    switch (line_y_aca)
                    {
                    case SCALE_X0_001:
                        LCD_ASCII_8X16_str(&dev,38,134,"1mA  ",5,WHITE,DGRAY);
                        line_y_scale_cal=10;
                        break;
                    case SCALE_X0_01:
                        LCD_ASCII_8X16_str(&dev,38,134,"10mA ",5,WHITE,DGRAY);
                        line_y_scale_cal=100;
                        break;
                    case SCALE_X0_1:
                        LCD_ASCII_8X16_str(&dev,38,134,"100mA",5,WHITE,DGRAY);
                        line_y_scale_cal=1000;
                        break;
                    case SCALE_X1:
                        LCD_ASCII_8X16_str(&dev,38,134,"1A   ",5,WHITE,DGRAY);
                        line_y_scale_cal=10000;
                        break;
                    case SCALE_X10:
                        LCD_ASCII_8X16_str(&dev,38,134,"10A  ",5,WHITE,DGRAY);
                        line_y_scale_cal=100000;
                        break;
                    
                    default:
                        break;
                    }  
                }

                if(choice_aca_old!=choice_aca)
                {
                    choice_aca_old=choice_aca;
                    switch (choice_aca)
                    {
                    case CUR_UA:
                        LCD_monochrome_any(&dev,174,133,R200_MAX5mA,112,16,WHITE,DGRAY);
                        break;
                    case CUR_MA:
                        LCD_monochrome_any(&dev,174,133,R2_MAX500mA,112,16,WHITE,DGRAY);
                        break;
                    case CUR_A:
                        LCD_monochrome_any(&dev,174,133,mR10_MAX10A,112,16,WHITE,DGRAY);
                        break; 
                    default:
                        break;
                    }
                }

                break;
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(line_y_aca<SCALE_X10) line_y_aca++;
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(line_y_aca>0) line_y_aca--;
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(measure_stop==0) 
                {
                    measure_stop=1;
                    top_button_refresh();
                    vTaskDelay(pdMS_TO_TICKS(100));//给ADC最后一次刷新数值留出时间
                    LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,0xB500,BLACK);//暂停测量后，测量值为灰黄色
                }
                else 
                {
                    measure_stop=0;
                    top_button_refresh();
                }
                break;
            case KEY_F2_DOWN: //功能键2 按下
                fun2_key=1;
                evt=WIFINET_MARK;
                xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                top_button_refresh();
                break;
            case KEY_F2_UP: //功能键2 短按抬起
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_F2_LONG: //功能键2 长按
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_LEFT_ICON: //刷新左侧图标
                left_icon_refresh();
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}


// 两线电阻 面板
void display_2wr(uint8_t select)
{
    uint8_t evt;
    
    char strbuf[10];
    choice_2wr_old=0xFF;
    vol_frame_init(select);
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case REFRESH_VALUE: //刷新测量值

                //波形显示
                if(measured_value==0xFFFFFFFF)
                {

                }
                else if(unit==0x0A)//单位 Ω
                {
                    if(line_y_scale_cal/1000!=0)
                    {
                        ui_line_chart(measured_value/(line_y_scale_cal/1000),sign);
                    } 
                }
                else if(unit==0x09) //单位mΩ
                {
                    if(line_y_scale_cal!=0)
                    {
                        ui_line_chart(measured_value/line_y_scale_cal,sign);
                    }
                    
                }
                else if(unit==0x08)//单位uΩ
                {
                    if(line_y_scale_cal!=0)
                    {
                        ui_line_chart(measured_value/line_y_scale_cal/1000,sign);
                    }
                }

                strbuf[0]=sign?'-':' ';
                if(measured_value<10000000)//小于10 9.9999
                {
                    ui_value_bar(measured_value/100000);
                    strbuf[1]=measured_value/1000000%10+'0';
                    strbuf[2]='.';
                    strbuf[3]=measured_value/100000%10+'0';
                    strbuf[4]=measured_value/10000%10+'0';
                    strbuf[5]=measured_value/1000%10+'0';
                    strbuf[6]=measured_value/100%10+'0'; 
                    if(unit==0x0A)
                    {
                        LCD_monochrome_any(&dev,215,59,MMR,56,24,BLACK,YELLOW);
                    }
                    else if(unit==0x09)
                    {
                        LCD_monochrome_any(&dev,215,59,KR,56,24,BLACK,YELLOW);
                    }
                    else if(unit==0x08)
                    {
                        LCD_monochrome_any(&dev,215,59,R,56,24,BLACK,YELLOW);
                    }
                    
                }
                else if(measured_value<100000000)//小于100 99.999
                {
                    ui_value_bar(measured_value/1000000);
                    strbuf[1]=measured_value/10000000%10+'0';
                    strbuf[2]=measured_value/1000000%10+'0';
                    strbuf[3]='.';
                    strbuf[4]=measured_value/100000%10+'0';
                    strbuf[5]=measured_value/10000%10+'0';
                    strbuf[6]=measured_value/1000%10+'0'; 
                    if(unit==0x0A)
                    {
                        LCD_monochrome_any(&dev,215,59,MMR,56,24,BLACK,YELLOW);
                    }
                    else if(unit==0x09)
                    {
                        LCD_monochrome_any(&dev,215,59,KR,56,24,BLACK,YELLOW);
                    }
                    else if(unit==0x08)
                    {
                        LCD_monochrome_any(&dev,215,59,R,56,24,BLACK,YELLOW);
                    }
                }
                else if(measured_value<1000000000)//小于1000 999.99
                {
                    ui_value_bar(measured_value/10000000);
                    strbuf[1]=measured_value/100000000%10+'0';
                    strbuf[2]=measured_value/10000000%10+'0';
                    strbuf[3]=measured_value/1000000%10+'0';
                    strbuf[4]='.';
                    strbuf[5]=measured_value/100000%10+'0';
                    strbuf[6]=measured_value/10000%10+'0'; 
                    if(unit==0x0A)
                    {
                        LCD_monochrome_any(&dev,215,59,MMR,56,24,BLACK,YELLOW);
                    }
                    else if(unit==0x09)
                    {
                        LCD_monochrome_any(&dev,215,59,KR,56,24,BLACK,YELLOW);
                    }
                    else if(unit==0x08)
                    {
                        LCD_monochrome_any(&dev,215,59,R,56,24,BLACK,YELLOW);
                    }
                }
                else
                {
                    ui_value_bar(0);
                    strbuf[1]='-';
                    strbuf[2]='-';
                    strbuf[3]='.';
                    strbuf[4]='-';
                    strbuf[5]='-';
                    strbuf[6]=' ';
                    LCD_monochrome_any(&dev,215,59,MMR,56,24,BLACK,YELLOW);
                }
                LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,YELLOW,BLACK);

                if(line_y_old!=line_y_2wr)
                {
                    line_y_old=line_y_2wr;
                    switch (line_y_2wr)
                    {
                    case SCALE_X0_1:
                        LCD_ASCII_8X16_str(&dev,38,134,"100mR   ",5,WHITE,DGRAY);
                        line_y_scale_cal=1;
                        break;
                    case SCALE_X1:
                        LCD_ASCII_8X16_str(&dev,38,134,"10R  ",5,WHITE,DGRAY);
                        line_y_scale_cal=100;
                        break;
                    case SCALE_X10:
                        LCD_ASCII_8X16_str(&dev,38,134,"1KR  ",5,WHITE,DGRAY);
                        line_y_scale_cal=10000;
                        break;
                    case SCALE_X100:
                        LCD_ASCII_8X16_str(&dev,38,134,"100KR",5,WHITE,DGRAY);
                        line_y_scale_cal=1000000;
                        break;
                    case SCALE_X1000:
                        LCD_ASCII_8X16_str(&dev,38,134,"10MR ",5,WHITE,DGRAY);
                        line_y_scale_cal=100000000;
                        break;           
                    default:
                        line_y_scale_cal=1;
                        break;
                    }  
                }

                if(choice_2wr_old!=choice_2wr)
                {
                    choice_2wr_old=choice_2wr;
                    switch (choice_2wr)
                    {
                    case RES_AUTO:
                        analog_switch(ASW_2WR3);
                        LCD_monochrome_any(&dev,173,133,dzzd_112x16,112,16,DGRAY,WHITE);
                        break;
                    case RES_1K:
                        analog_switch(ASW_2WR0);
                        LCD_monochrome_any(&dev,174,133,dz1k_112x16,112,16,WHITE,DGRAY);
                        break;
                    case RES_20K:
                        analog_switch(ASW_2WR1);
                        LCD_monochrome_any(&dev,174,133,dz20k_112x16,112,16,WHITE,DGRAY);
                        break; 
                    case RES_500K:
                        analog_switch(ASW_2WR2);
                        LCD_monochrome_any(&dev,174,133,dz500k_112x16,112,16,WHITE,DGRAY);
                        break;
                    case RES_100M:
                        analog_switch(ASW_2WR3);
                        LCD_monochrome_any(&dev,174,133,dz100M_112x16,112,16,WHITE,DGRAY);
                        break;
                    default:
                        break;
                    }
                }
                

                break;
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(line_y_2wr<SCALE_X1000) line_y_2wr++;
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(line_y_2wr>0) line_y_2wr--;
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(measure_stop==0) 
                {
                    measure_stop=1;
                    top_button_refresh();
                    vTaskDelay(pdMS_TO_TICKS(100));//给ADC最后一次刷新数值留出时间
                    LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,0xB500,BLACK);//暂停测量后，测量值为灰黄色
                }
                else 
                {
                    measure_stop=0;
                    top_button_refresh();
                }
                break;
            case KEY_F2_DOWN: //功能键2 按下
                fun2_key=1;
                evt=WIFINET_MARK;
                xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                top_button_refresh();
                break;
            case KEY_F2_UP: //功能键2 短按抬起
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_F2_LONG: //功能键2 长按
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_LEFT_ICON: //刷新左侧图标
                left_icon_refresh();
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}

// 四线低阻 面板
void display_4wr(uint8_t select)
{
    uint8_t evt;
    
    char strbuf[10]; 

    vol_frame_init(select);
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case REFRESH_VALUE: //刷新测量值
            
                //波形显示
                if(measured_value==0xFFFFFFFF)
                {

                }
                else if(unit==0x0A)
                {
                    if(line_y_scale_cal/1000!=0)
                    {
                        ui_line_chart(measured_value/(line_y_scale_cal/1000),sign);
                    } 
                }
                else if(unit==0x09)
                {
                    if(line_y_scale_cal!=0)
                    {
                        ui_line_chart(measured_value/line_y_scale_cal,sign);
                    }
                    
                }
                else if(unit==0x08)
                {
                    if(line_y_scale_cal!=0)
                    {
                        ui_line_chart(measured_value/line_y_scale_cal/1000,sign);
                    }
                }

                strbuf[0]=sign?'-':' ';
                if(measured_value<10000000)//小于10 9.9999
                {
                    ui_value_bar(measured_value/100000);
                    strbuf[1]=measured_value/1000000%10+'0';
                    strbuf[2]='.';
                    strbuf[3]=measured_value/100000%10+'0';
                    strbuf[4]=measured_value/10000%10+'0';
                    strbuf[5]=measured_value/1000%10+'0';
                    strbuf[6]=measured_value/100%10+'0';    
                    LCD_monochrome_any(&dev,215,59,R,56,24,BLACK,YELLOW);    
                }
                else if(measured_value<100000000)//小于100 99.999
                {
                    ui_value_bar(measured_value/1000000);
                    strbuf[1]=measured_value/10000000%10+'0';
                    strbuf[2]=measured_value/1000000%10+'0';
                    strbuf[3]='.';
                    strbuf[4]=measured_value/100000%10+'0';
                    strbuf[5]=measured_value/10000%10+'0';
                    strbuf[6]=measured_value/1000%10+'0'; 
                    LCD_monochrome_any(&dev,215,59,R,56,24,BLACK,YELLOW);
                    
                }
                else if(measured_value<1000000000)//小于1000 999.99
                {
                    ui_value_bar(measured_value/10000000);
                    strbuf[1]=measured_value/100000000%10+'0';
                    strbuf[2]=measured_value/10000000%10+'0';
                    strbuf[3]=measured_value/1000000%10+'0';
                    strbuf[4]='.';
                    strbuf[5]=measured_value/100000%10+'0';
                    strbuf[6]=measured_value/10000%10+'0'; 
                    LCD_monochrome_any(&dev,215,59,R,56,24,BLACK,YELLOW);
                }
                else
                {
                    ui_value_bar(0);
                    strbuf[1]='-';
                    strbuf[2]='-';
                    strbuf[3]='.';
                    strbuf[4]='-';
                    strbuf[5]='-';
                    strbuf[6]=' ';
                    LCD_monochrome_any(&dev,215,59,R,56,24,BLACK,YELLOW);
                }
                LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,YELLOW,BLACK);

                if(line_y_old!=line_y_4wr)
                {
                    LCD_monochrome_any(&dev,173,133,dz5mA_112x16,112,16,DGRAY,WHITE);
                    line_y_old=line_y_4wr;
                    switch (line_y_4wr)
                    {
                    case SCALE_X0_1:
                        LCD_ASCII_8X16_str(&dev,38,134,"100mR   ",5,WHITE,DGRAY);
                        line_y_scale_cal=1;
                        break;
                    case SCALE_X1:
                        LCD_ASCII_8X16_str(&dev,38,134,"1R   ",5,WHITE,DGRAY);
                        line_y_scale_cal=10;
                        break;
                    case SCALE_X10:
                        LCD_ASCII_8X16_str(&dev,38,134,"10R  ",5,WHITE,DGRAY);
                        line_y_scale_cal=100;
                        break;
                    case SCALE_X100:
                        LCD_ASCII_8X16_str(&dev,38,134,"100R ",5,WHITE,DGRAY);
                        line_y_scale_cal=1000;
                        break;
                    case SCALE_X1000:
                        LCD_ASCII_8X16_str(&dev,38,134,"1000R",5,WHITE,DGRAY);
                        line_y_scale_cal=10000;
                        break;           
                    default:
                        line_y_scale_cal=1;
                        break;
                    }  
                }
                

                break;
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(line_y_4wr<SCALE_X1000) line_y_4wr++;
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(line_y_4wr>0) line_y_4wr--;
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(measure_stop==0) 
                {
                    measure_stop=1;
                    top_button_refresh();
                    vTaskDelay(pdMS_TO_TICKS(100));//给ADC最后一次刷新数值留出时间
                    LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,0xB500,BLACK);//暂停测量后，测量值为灰黄色
                }
                else 
                {
                    measure_stop=0;
                    top_button_refresh();
                }
                break;
            case KEY_F2_DOWN: //功能键2 按下
                fun2_key=1;
                evt=WIFINET_MARK;
                xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                top_button_refresh();
                break;
            case KEY_F2_UP: //功能键2 短按抬起
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_F2_LONG: //功能键2 长按
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_LEFT_ICON: //刷新左侧图标
                left_icon_refresh();
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}

// 二极管 面板
void display_diode(uint8_t select)
{
    uint8_t evt;
    
    char strbuf[10]; 

    vol_frame_init(select);
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case REFRESH_VALUE: //刷新测量值 
                ui_line_chart(measured_value/50000,sign);
                strbuf[0]=sign?'-':' ';
                if(measured_value<1000000)//小于1V 999.99mV
                {
                    ui_value_bar(measured_value/10000);
                    strbuf[1]=measured_value/100000%10+'0';
                    strbuf[2]=measured_value/10000%10+'0';
                    strbuf[3]=measured_value/1000%10+'0';
                    strbuf[4]='.';
                    strbuf[5]=measured_value/100%10+'0';
                    strbuf[6]=measured_value/10%10+'0';
                    LCD_monochrome_any(&dev,215,59,mVDC,56,24,BLACK,YELLOW);
                }
                else if(measured_value<10000000)//小于10V 9.9999V
                {
                    ui_value_bar(measured_value/100000);
                    strbuf[1]=measured_value/1000000%10+'0';
                    strbuf[2]='.';
                    strbuf[3]=measured_value/100000%10+'0';
                    strbuf[4]=measured_value/10000%10+'0';
                    strbuf[5]=measured_value/1000%10+'0';
                    strbuf[6]=measured_value/100%10+'0'; 
                    LCD_monochrome_any(&dev,215,59,VDC,56,24,BLACK,YELLOW);
                }
                else
                {
                    ui_value_bar(0);
                    strbuf[1]='-';
                    strbuf[2]='-';;
                    strbuf[3]='.';
                    strbuf[4]='-';;
                    strbuf[5]='-';;
                    strbuf[6]=' ';;
                }
                LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,YELLOW,BLACK);

                break;
            case KEY_ROTARY_R: //旋转编码器 右旋
                
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
 
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(measure_stop==0) 
                {
                    measure_stop=1;
                    top_button_refresh();
                    vTaskDelay(pdMS_TO_TICKS(100));//给ADC最后一次刷新数值留出时间
                    LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,0xB500,BLACK);//暂停测量后，测量值为灰黄色
                }
                else 
                {
                    measure_stop=0;
                    top_button_refresh();
                }
                break;
            case KEY_F2_DOWN: //功能键2 按下
                fun2_key=1;
                evt=WIFINET_MARK;
                xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                top_button_refresh();
                break;
            case KEY_F2_UP: //功能键2 短按抬起
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_F2_LONG: //功能键2 长按
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_LEFT_ICON: //刷新左侧图标
                left_icon_refresh();
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}



// 通断档 面板
void display_beep(uint8_t select)
{
    uint8_t evt;
    
    char strbuf[10]; 
    analog_switch(ASW_2WR0);
    vol_frame_init(select);
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case REFRESH_VALUE: //刷新测量值
            
                ui_line_chart(measured_value/10000000,sign);
                strbuf[0]=sign?'-':' ';
                 if(measured_value<10000000)//小于10 9.9999
                {
                    ui_value_bar(measured_value/100000);
                    strbuf[1]=measured_value/1000000%10+'0';
                    strbuf[2]='.';
                    strbuf[3]=measured_value/100000%10+'0';
                    strbuf[4]=measured_value/10000%10+'0';
                    strbuf[5]=measured_value/1000%10+'0';
                    strbuf[6]=measured_value/100%10+'0';    
                    LCD_monochrome_any(&dev,215,59,R,56,24,BLACK,YELLOW);    
                }
                else if(measured_value<100000000)//小于100 99.999
                {
                    ui_value_bar(measured_value/1000000);
                    strbuf[1]=measured_value/10000000%10+'0';
                    strbuf[2]=measured_value/1000000%10+'0';
                    strbuf[3]='.';
                    strbuf[4]=measured_value/100000%10+'0';
                    strbuf[5]=measured_value/10000%10+'0';
                    strbuf[6]=measured_value/1000%10+'0'; 
                    LCD_monochrome_any(&dev,215,59,R,56,24,BLACK,YELLOW);
                    
                }
                else if(measured_value<1000000000)//小于1000 999.99
                {
                    ui_value_bar(measured_value/10000000);
                    strbuf[1]=measured_value/100000000%10+'0';
                    strbuf[2]=measured_value/10000000%10+'0';
                    strbuf[3]=measured_value/1000000%10+'0';
                    strbuf[4]='.';
                    strbuf[5]=measured_value/100000%10+'0';
                    strbuf[6]=measured_value/10000%10+'0'; 
                    LCD_monochrome_any(&dev,215,59,R,56,24,BLACK,YELLOW);
                }
                else
                {
                    ui_value_bar(0);
                    strbuf[1]='-';
                    strbuf[2]='-';
                    strbuf[3]='.';
                    strbuf[4]='-';
                    strbuf[5]='-';
                    strbuf[6]=' ';
                    LCD_monochrome_any(&dev,215,59,R,56,24,BLACK,YELLOW);
                }
                LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,YELLOW,BLACK);

            
                break;
            case KEY_ROTARY_R: //旋转编码器 右旋
 
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
  
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(measure_stop==0) 
                {
                    measure_stop=1;
                    top_button_refresh();
                    vTaskDelay(pdMS_TO_TICKS(100));//给ADC最后一次刷新数值留出时间
                    LCD_ASCII_24X30_str(&dev,55,52,strbuf,7,0xB500,BLACK);//暂停测量后，测量值为灰黄色
                }
                else 
                {
                    measure_stop=0;
                    top_button_refresh();
                }
                break;
            case KEY_F2_DOWN: //功能键2 按下
                fun2_key=1;
                evt=WIFINET_MARK;
                xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                top_button_refresh();
                break;
            case KEY_F2_UP: //功能键2 短按抬起
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_F2_LONG: //功能键2 长按
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_LEFT_ICON: //刷新左侧图标
                left_icon_refresh();
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}


//直流功率 面板
void display_dcw(uint8_t select)
{
    uint8_t evt;
    
    char strbuf[10]; 
    char strbuf2[10]; 

    vol_frame_init(select);
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case REFRESH_VALUE: //刷新测量值
            
                ui_line_chart(measured_value/line_y_scale_cal,sign);
                strbuf[0]=sign?'-':' ';
                if(measured_value<100000)//小于0.1V 99.999mV
                {
                    ui_value_bar(measured_value/1000);
                    strbuf[1]=measured_value/10000%10+'0';
                    strbuf[2]=measured_value/1000%10+'0';
                    strbuf[3]='.';
                    strbuf[4]=measured_value/100%10+'0';
                    strbuf[5]=measured_value/10%10+'0';
                    strbuf[6]=measured_value%10+'0';
                    LCD_monochrome_any(&dev,215,67,mWDC,56,24,BLACK,YELLOW);
                }
                else if(measured_value<1000000)//小于1V 999.99mV
                {
                    ui_value_bar(measured_value/10000);
                    strbuf[1]=measured_value/100000%10+'0';
                    strbuf[2]=measured_value/10000%10+'0';
                    strbuf[3]=measured_value/1000%10+'0';
                    strbuf[4]='.';
                    strbuf[5]=measured_value/100%10+'0';
                    strbuf[6]=measured_value/10%10+'0';
                    LCD_monochrome_any(&dev,215,67,mWDC,56,24,BLACK,YELLOW);  
                }
                else if(measured_value<10000000)//小于10V 9.9999V
                {
                    ui_value_bar(measured_value/100000);
                    strbuf[1]=measured_value/1000000%10+'0';
                    strbuf[2]='.';
                    strbuf[3]=measured_value/100000%10+'0';
                    strbuf[4]=measured_value/10000%10+'0';
                    strbuf[5]=measured_value/1000%10+'0';
                    strbuf[6]=measured_value/100%10+'0'; 
                    if(unit==0x0C) //uW
                    {
                        LCD_monochrome_any(&dev,215,67,WDC,56,24,BLACK,YELLOW);
                    }
                    else if(unit==0x0D) //mW
                    {
                        LCD_monochrome_any(&dev,215,67,kWDC,56,24,BLACK,YELLOW);
                    }
                }
                else if(measured_value<100000000)//小于100V 99.999V
                {
                    ui_value_bar(measured_value/1000000);
                    strbuf[1]=measured_value/10000000%10+'0';
                    strbuf[2]=measured_value/1000000%10+'0';
                    strbuf[3]='.';
                    strbuf[4]=measured_value/100000%10+'0';
                    strbuf[5]=measured_value/10000%10+'0';
                    strbuf[6]=measured_value/1000%10+'0'; 
                    if(unit==0x0C)
                    {
                        LCD_monochrome_any(&dev,215,67,WDC,56,24,BLACK,YELLOW);
                    }
                    else if(unit==0x0D)
                    {
                        LCD_monochrome_any(&dev,215,67,kWDC,56,24,BLACK,YELLOW);
                    }
                }
                else if(measured_value<1000000000)//小于1000V 999.99V
                {
                    ui_value_bar(measured_value/10000000);
                    strbuf[1]=measured_value/100000000%10+'0';
                    strbuf[2]=measured_value/10000000%10+'0';
                    strbuf[3]=measured_value/1000000%10+'0';
                    strbuf[4]='.';
                    strbuf[5]=measured_value/100000%10+'0';
                    strbuf[6]=measured_value/10000%10+'0'; 
                    if(unit==0x0C)
                    {
                        LCD_monochrome_any(&dev,215,67,WDC,56,24,BLACK,YELLOW);
                    }
                    else if(unit==0X0D)
                    {
                        LCD_monochrome_any(&dev,215,67,kWDC,56,24,BLACK,YELLOW);
                    }
                }
                else
                {
                    strbuf[1]='-';
                    strbuf[2]='-';
                    strbuf[3]='.';
                    strbuf[4]='-';
                    strbuf[5]='-';
                    strbuf[6]=' ';
                }

                LCD_ASCII_24X30_str(&dev,55,60,strbuf,7,YELLOW,BLACK);

                if(carried_value1<1000)//小于1V
                {
                    strbuf2[0]='0';
                    strbuf2[1]=carried_value1/100%10+'0';
                    strbuf2[2]=carried_value1/10%10+'0';
                    strbuf2[3]=carried_value1%10+'0';
                    strbuf2[4]='m';
                    strbuf2[5]='V';
                }
                else if(carried_value1<10000)//小于10V
                {
                    strbuf2[0]=carried_value1/1000%10+'0';
                    strbuf2[1]='.';
                    strbuf2[2]=carried_value1/100%10+'0';
                    strbuf2[3]=carried_value1/10%10+'0';
                    strbuf2[4]=carried_value1%10+'0';
                    strbuf2[5]='V';
                }
                else if(carried_value1<100000)//小于100V
                {
                    strbuf2[0]=carried_value1/10000%10+'0';
                    strbuf2[1]=carried_value1/1000%10+'0';
                    strbuf2[2]='.';
                    strbuf2[3]=carried_value1/100%10+'0';
                    strbuf2[4]=carried_value1/10%10+'0';
                    strbuf2[5]='V';
                }
                else if(carried_value1<1000000)//小于1000V
                {
                    strbuf2[0]=carried_value1/100000%10+'0';
                    strbuf2[1]=carried_value1/10000%10+'0';
                    strbuf2[2]=carried_value1/1000%10+'0';
                    strbuf2[3]='.';
                    strbuf2[4]=carried_value1/100%10+'0';
                    strbuf2[5]='V';
                }
                else
                {
                    strbuf2[1]='-';
                    strbuf2[2]='-';
                    strbuf2[3]='-';
                    strbuf2[4]='-';
                    strbuf2[5]='-';
                    strbuf2[6]='-';
                }
                LCD_ASCII_8X16_str(&dev,42,37,strbuf2,6,WHITE,BLACK);

                if(carried_value2<1000)//小于1mA
                {
                    
                    strbuf2[0]='0';
                    strbuf2[1]=carried_value2/100%10+'0';
                    strbuf2[2]=carried_value2/10%10+'0';
                    strbuf2[3]=carried_value2%10+'0';
                    strbuf2[4]='u';
                    strbuf2[5]='A';
                    strbuf2[6]=' ';
                }
                if(carried_value2<10000)//小于10mA
                {
                    strbuf2[0]=carried_value2/1000%10+'0';
                    strbuf2[1]='.';
                    strbuf2[2]=carried_value2/100%10+'0';
                    strbuf2[3]=carried_value2/10%10+'0';
                    strbuf2[4]=carried_value2%10+'0';
                    strbuf2[5]='m';
                    strbuf2[6]='A';
                }
                else if(carried_value2<100000)//小于100mA
                {
                    strbuf2[0]=carried_value2/10000%10+'0';
                    strbuf2[1]=carried_value2/1000%10+'0';
                    strbuf2[2]='.';
                    strbuf2[3]=carried_value2/100%10+'0';
                    strbuf2[4]=carried_value2/10%10+'0';
                    strbuf2[5]='m';
                    strbuf2[6]='A';
                }
                else if(carried_value2<1000000)//小于1A
                {
                    strbuf2[0]=carried_value2/100000%10+'0';
                    strbuf2[1]=carried_value2/10000%10+'0';
                    strbuf2[2]=carried_value2/1000%10+'0';
                    strbuf2[3]='.';
                    strbuf2[4]=carried_value2/100%10+'0';
                    strbuf2[5]='m';
                    strbuf2[6]='A';
                }
                else if(carried_value2<10000000)//小于10A
                {
                    strbuf2[0]=carried_value2/1000000%10+'0';
                    strbuf2[1]='.';
                    strbuf2[2]=carried_value2/100000%10+'0';
                    strbuf2[3]=carried_value2/10000%10+'0';
                    strbuf2[4]=carried_value2/1000%10+'0';
                    strbuf2[5]='A';
                    strbuf2[6]=' ';
                }
                else
                {
                    strbuf2[1]='-';
                    strbuf2[2]='-';
                    strbuf2[3]='-';
                    strbuf2[4]='-';
                    strbuf2[5]='-';
                    strbuf2[6]='-';
                    strbuf2[7]=' ';
                }
                
                LCD_ASCII_8X16_str(&dev,110,37,strbuf2,7,WHITE,BLACK);
                

                if(line_y_old!=line_y_dcw)
                {
                    line_y_old=line_y_dcw;
                    switch (line_y_dcw)
                    {
                    case SCALE_X0_01:
                        LCD_ASCII_8X16_str(&dev,38,134,"10mW ",5,WHITE,DGRAY);
                        line_y_scale_cal=100;
                        break;
                    case SCALE_X0_1:
                        LCD_ASCII_8X16_str(&dev,38,134,"100mW ",5,WHITE,DGRAY);
                        line_y_scale_cal=1000;
                        break;
                    case SCALE_X1:
                        LCD_ASCII_8X16_str(&dev,38,134,"1W   ",5,WHITE,DGRAY);
                        line_y_scale_cal=10000;
                        break;
                    case SCALE_X10:
                        LCD_ASCII_8X16_str(&dev,38,134,"10W  ",5,WHITE,DGRAY);
                        line_y_scale_cal=100000;
                        break;
                    case SCALE_X100:
                        LCD_ASCII_8X16_str(&dev,38,134,"100W ",5,WHITE,DGRAY);
                        line_y_scale_cal=1000000;
                        break;
                    case SCALE_X1000:
                        LCD_ASCII_8X16_str(&dev,38,134,"1000W",5,WHITE,DGRAY);
                        line_y_scale_cal=10000000;
                        break;
                    
                    default:
                        break;
                    }  
                }

                if(choice_wdc_old!=choice_wdc)
                {
                    choice_wdc_old=choice_wdc;
                    switch (choice_wdc)
                    {
                    case CUR_UA:
                        LCD_monochrome_any(&dev,174,133,R200_MAX5mA,112,16,WHITE,DGRAY);
                        break;
                    case CUR_MA:
                        LCD_monochrome_any(&dev,174,133,R2_MAX500mA,112,16,WHITE,DGRAY);
                        break;
                    case CUR_A:
                        LCD_monochrome_any(&dev,174,133,mR10_MAX10A,112,16,WHITE,DGRAY);
                        break; 
                    default:
                        break;
                    }
                }

                break;
            case KEY_ROTARY_R: //旋转编码器 右旋
                //if(choice_wdc<2) choice_wdc++;
                if(line_y_dcw<SCALE_X1000) line_y_dcw++;
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                //if(choice_wdc>0) choice_wdc--;
                if(line_y_dcw>SCALE_X0_01) line_y_dcw--;
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(measure_stop==0) 
                {
                    measure_stop=1;
                    top_button_refresh();
                    vTaskDelay(pdMS_TO_TICKS(100));//给ADC最后一次刷新数值留出时间
                    LCD_ASCII_24X30_str(&dev,55,60,strbuf,7,0xB500,BLACK);//暂停测量后，测量值为灰黄色
                }
                else 
                {
                    measure_stop=0;
                    top_button_refresh();
                }
                break;
            case KEY_F2_DOWN: //功能键2 按下
                fun2_key=1;
                evt=WIFINET_MARK;
                xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                top_button_refresh();
                break;
            case KEY_F2_UP: //功能键2 短按抬起
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_F2_LONG: //功能键2 长按
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_LEFT_ICON: //刷新左侧图标
                left_icon_refresh();
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}

//交流功率 面板
void display_acw(uint8_t select)
{
    uint8_t evt;
    
    char strbuf[10]; 
    char strbuf2[10]; 

    vol_frame_init(select);
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case REFRESH_VALUE: //刷新测量值
            
                ui_line_chart(measured_value/line_y_scale_cal,sign);
                strbuf[0]=sign?'-':' ';
                if(measured_value<100000)//小于0.1V 99.999mV
                {
                    ui_value_bar(measured_value/1000);
                    strbuf[1]=measured_value/10000%10+'0';
                    strbuf[2]=measured_value/1000%10+'0';
                    strbuf[3]='.';
                    strbuf[4]=measured_value/100%10+'0';
                    strbuf[5]=measured_value/10%10+'0';
                    strbuf[6]=measured_value%10+'0';
                    LCD_monochrome_any(&dev,215,67,mWAC,56,24,BLACK,YELLOW);
                }
                else if(measured_value<1000000)//小于1V 999.99mV
                {
                    ui_value_bar(measured_value/10000);
                    strbuf[1]=measured_value/100000%10+'0';
                    strbuf[2]=measured_value/10000%10+'0';
                    strbuf[3]=measured_value/1000%10+'0';
                    strbuf[4]='.';
                    strbuf[5]=measured_value/100%10+'0';
                    strbuf[6]=measured_value/10%10+'0';
                    LCD_monochrome_any(&dev,215,67,mWAC,56,24,BLACK,YELLOW);  
                }
                else if(measured_value<10000000)//小于10V 9.9999V
                {
                    ui_value_bar(measured_value/100000);
                    strbuf[1]=measured_value/1000000%10+'0';
                    strbuf[2]='.';
                    strbuf[3]=measured_value/100000%10+'0';
                    strbuf[4]=measured_value/10000%10+'0';
                    strbuf[5]=measured_value/1000%10+'0';
                    strbuf[6]=measured_value/100%10+'0'; 
                    if(unit==0)
                    {
                        LCD_monochrome_any(&dev,215,67,WAC,56,24,BLACK,YELLOW);
                    }
                    else if(unit==1)
                    {
                        LCD_monochrome_any(&dev,215,67,kWAC,56,24,BLACK,YELLOW);
                    }
                }
                else if(measured_value<100000000)//小于100V 99.999V
                {
                    ui_value_bar(measured_value/1000000);
                    strbuf[1]=measured_value/10000000%10+'0';
                    strbuf[2]=measured_value/1000000%10+'0';
                    strbuf[3]='.';
                    strbuf[4]=measured_value/100000%10+'0';
                    strbuf[5]=measured_value/10000%10+'0';
                    strbuf[6]=measured_value/1000%10+'0'; 
                    if(unit==0x0C)
                    {
                        LCD_monochrome_any(&dev,215,67,WAC,56,24,BLACK,YELLOW);
                    }
                    else if(unit==0x0D)
                    {
                        LCD_monochrome_any(&dev,215,67,kWAC,56,24,BLACK,YELLOW);
                    }
                }
                else if(measured_value<1000000000)//小于1000V 999.99V
                {
                    ui_value_bar(measured_value/10000000);
                    strbuf[1]=measured_value/100000000%10+'0';
                    strbuf[2]=measured_value/10000000%10+'0';
                    strbuf[3]=measured_value/1000000%10+'0';
                    strbuf[4]='.';
                    strbuf[5]=measured_value/100000%10+'0';
                    strbuf[6]=measured_value/10000%10+'0'; 
                    if(unit==0x0C)
                    {
                        LCD_monochrome_any(&dev,215,67,WAC,56,24,BLACK,YELLOW);
                    }
                    else if(unit==0x0D)
                    {
                        LCD_monochrome_any(&dev,215,67,kWAC,56,24,BLACK,YELLOW);
                    }
                }
                else
                {
                    strbuf[1]='-';
                    strbuf[2]='-';
                    strbuf[3]='.';
                    strbuf[4]='-';
                    strbuf[5]='-';
                    strbuf[6]=' ';
                }

                LCD_ASCII_24X30_str(&dev,55,60,strbuf,7,YELLOW,BLACK);

                if(carried_value1<1000)//小于1V
                {
                    strbuf2[0]='0';
                    strbuf2[1]=carried_value1/100%10+'0';
                    strbuf2[2]=carried_value1/10%10+'0';
                    strbuf2[3]=carried_value1%10+'0';
                    strbuf2[4]='m';
                    strbuf2[5]='V';
                }
                else if(carried_value1<10000)//小于10V
                {
                    strbuf2[0]=carried_value1/1000%10+'0';
                    strbuf2[1]='.';
                    strbuf2[2]=carried_value1/100%10+'0';
                    strbuf2[3]=carried_value1/10%10+'0';
                    strbuf2[4]=carried_value1%10+'0';
                    strbuf2[5]='V';
                }
                else if(carried_value1<100000)//小于100V
                {
                    strbuf2[0]=carried_value1/10000%10+'0';
                    strbuf2[1]=carried_value1/1000%10+'0';
                    strbuf2[2]='.';
                    strbuf2[3]=carried_value1/100%10+'0';
                    strbuf2[4]=carried_value1/10%10+'0';
                    strbuf2[5]='V';
                }
                else if(carried_value1<1000000)//小于1000V
                {
                    strbuf2[0]=carried_value1/100000%10+'0';
                    strbuf2[1]=carried_value1/10000%10+'0';
                    strbuf2[2]=carried_value1/1000%10+'0';
                    strbuf2[3]='.';
                    strbuf2[4]=carried_value1/100%10+'0';
                    strbuf2[5]='V';
                }
                else
                {
                    strbuf2[1]='-';
                    strbuf2[2]='-';
                    strbuf2[3]='-';
                    strbuf2[4]='-';
                    strbuf2[5]='-';
                    strbuf2[6]='-';
                }
                LCD_ASCII_8X16_str(&dev,42,37,strbuf2,6,WHITE,BLACK);

                if(carried_value2<1000)//小于1mA
                {
                    
                    strbuf2[0]='0';
                    strbuf2[1]=carried_value2/100%10+'0';
                    strbuf2[2]=carried_value2/10%10+'0';
                    strbuf2[3]=carried_value2%10+'0';
                    strbuf2[4]='u';
                    strbuf2[5]='A';
                    strbuf2[6]=' ';
                    
                }
                if(carried_value2<10000)//小于10mA
                {
                    strbuf2[0]=carried_value2/1000%10+'0';
                    strbuf2[1]='.';
                    strbuf2[2]=carried_value2/100%10+'0';
                    strbuf2[3]=carried_value2/10%10+'0';
                    strbuf2[4]=carried_value2%10+'0';
                    strbuf2[5]='m';
                    strbuf2[6]='A';
                }
                else if(carried_value2<100000)//小于100mA
                {
                    strbuf2[0]=carried_value2/10000%10+'0';
                    strbuf2[1]=carried_value2/1000%10+'0';
                    strbuf2[2]='.';
                    strbuf2[3]=carried_value2/100%10+'0';
                    strbuf2[4]=carried_value2/10%10+'0';
                    strbuf2[5]='m';
                    strbuf2[6]='A';
                }
                else if(carried_value2<1000000)//小于1A
                {
                    strbuf2[0]=carried_value2/100000%10+'0';
                    strbuf2[1]=carried_value2/10000%10+'0';
                    strbuf2[2]=carried_value2/1000%10+'0';
                    strbuf2[3]='.';
                    strbuf2[4]=carried_value2/100%10+'0';
                    strbuf2[5]='m';
                    strbuf2[6]='A';
                }
                else if(carried_value2<10000000)//小于10A
                {
                    strbuf2[0]=carried_value2/1000000%10+'0';
                    strbuf2[1]='.';
                    strbuf2[2]=carried_value2/100000%10+'0';
                    strbuf2[3]=carried_value2/10000%10+'0';
                    strbuf2[4]=carried_value2/1000%10+'0';
                    strbuf2[5]='A';
                    strbuf2[6]=' ';
                }
                else
                {
                    strbuf2[1]='-';
                    strbuf2[2]='-';
                    strbuf2[3]='-';
                    strbuf2[4]='-';
                    strbuf2[5]='-';
                    strbuf2[6]='-';
                    strbuf2[7]=' ';
                }
                
                LCD_ASCII_8X16_str(&dev,110,37,strbuf2,7,WHITE,BLACK);
                

                if(line_y_old!=line_y_acw)
                {
                    line_y_old=line_y_acw;
                    switch (line_y_acw)
                    {
                    case SCALE_X0_01:
                        LCD_ASCII_8X16_str(&dev,38,134,"10mW ",5,WHITE,DGRAY);
                        line_y_scale_cal=100;
                        break;
                    case SCALE_X0_1:
                        LCD_ASCII_8X16_str(&dev,38,134,"100mW ",5,WHITE,DGRAY);
                        line_y_scale_cal=1000;
                        break;
                    case SCALE_X1:
                        LCD_ASCII_8X16_str(&dev,38,134,"1W   ",5,WHITE,DGRAY);
                        line_y_scale_cal=10000;
                        break;
                    case SCALE_X10:
                        LCD_ASCII_8X16_str(&dev,38,134,"10W  ",5,WHITE,DGRAY);
                        line_y_scale_cal=100000;
                        break;
                    case SCALE_X100:
                        LCD_ASCII_8X16_str(&dev,38,134,"100W ",5,WHITE,DGRAY);
                        line_y_scale_cal=1000000;
                        break;
                    case SCALE_X1000:
                        LCD_ASCII_8X16_str(&dev,38,134,"1000W",5,WHITE,DGRAY);
                        line_y_scale_cal=10000000;
                        break;
                    
                    default:
                        break;
                    }  
                }

                if(choice_wac_old!=choice_wac)
                {
                    choice_wac_old=choice_wac;
                    switch (choice_wac)
                    {
                    case CUR_MA:
                        LCD_monochrome_any(&dev,174,133,R2_MAX500mA,112,16,WHITE,DGRAY);
                        break;
                    case CUR_A:
                        LCD_monochrome_any(&dev,174,133,mR10_MAX10A,112,16,WHITE,DGRAY);
                        break; 
                    default:
                        break;
                    }
                }

                break;
            case KEY_ROTARY_R: //旋转编码器 右旋
                //if(choice_wac==CUR_MA) choice_wac=CUR_A;
                if(line_y_acw<SCALE_X1000) line_y_acw++;
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                //if(choice_wac==CUR_A) choice_wac=CUR_MA;
                if(line_y_acw>SCALE_X0_01) line_y_acw--;
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(measure_stop==0) 
                {
                    measure_stop=1;
                    top_button_refresh();
                    vTaskDelay(pdMS_TO_TICKS(100));//给ADC最后一次刷新数值留出时间
                    LCD_ASCII_24X30_str(&dev,55,60,strbuf,7,0xB500,BLACK);//暂停测量后，测量值为灰黄色
                }
                else 
                {
                    measure_stop=0;
                    top_button_refresh();
                }
                break;
            case KEY_F2_DOWN: //功能键2 按下
                fun2_key=1;
                evt=WIFINET_MARK;
                xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                top_button_refresh();
                break;
            case KEY_F2_UP: //功能键2 短按抬起
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_F2_LONG: //功能键2 长按
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break; 
            case REFRESH_LEFT_ICON: //刷新左侧图标
                left_icon_refresh();
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}

//测量温度 面板
void display_temp(uint8_t select)
{
    uint8_t evt;
    
    char strbuf[10]; 
    char strbuf2[10]; 

    vol_frame_init(select);
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case REFRESH_VALUE: //刷新测量值
            
                ui_line_chart(measured_value/line_y_scale_cal,sign);
                strbuf[0]=sign?'-':' ';
                 if(measured_value<10000)//小于10℃
                {
                    ui_value_bar(measured_value/100);
                    strbuf[1]=' ';
                    strbuf[2]=measured_value/1000%10+'0';
                    strbuf[3]='.';
                    strbuf[4]=measured_value/100%10+'0';
                    strbuf[5]=measured_value/10%10+'0';
                }
                else if(measured_value<100000)//小于100℃
                {
                    ui_value_bar(measured_value/1000);
                    strbuf[1]=measured_value/10000%10+'0';
                    strbuf[2]=measured_value/1000%10+'0';
                    strbuf[3]='.';
                    strbuf[4]=measured_value/100%10+'0';
                    strbuf[5]=measured_value/10%10+'0';
                }
                else if(measured_value<1000000)//小于1000℃
                {
                    ui_value_bar(measured_value/10000);
                    strbuf[1]=measured_value/100000%10+'0';
                    strbuf[2]=measured_value/10000%10+'0';
                    strbuf[3]=measured_value/1000%10+'0';
                    strbuf[4]='.';
                    strbuf[5]=measured_value/100%10+'0';
                }
                else
                {
                    ui_value_bar(0);
                    strbuf[1]='-';
                    strbuf[2]='-';;
                    strbuf[3]='.';
                    strbuf[4]='-';;
                    strbuf[5]='-';;
                }
                LCD_ASCII_24X30_str(&dev,70,60,strbuf,6,YELLOW,BLACK);
                LCD_monochrome_any(&dev,200,59,TC,56,24,BLACK,YELLOW);


                if(carried_value1<1000)//小于10R
                {
                    strbuf2[0]=' ';
                    strbuf2[1]=carried_value1/100%10+'0';
                    strbuf2[2]='.';
                    strbuf2[3]=carried_value1/10%10+'0';
                    strbuf2[4]=carried_value1%10+'0';
                    strbuf2[5]=' ';
                    strbuf2[6]='R'; 
                }
                else if(carried_value1<10000)//小于100R
                {
                    strbuf2[0]=' ';
                    strbuf2[1]=carried_value1/1000%10+'0';
                    strbuf2[2]=carried_value1/100%10+'0';
                    strbuf2[3]='.';
                    strbuf2[4]=carried_value1/10%10+'0';
                    strbuf2[5]=carried_value1%10+'0';
                    strbuf2[6]='R';
                }
                else if(carried_value1<100000)//小于1k
                {
                    strbuf2[0]=' ';
                    strbuf2[1]=carried_value1/10000%10+'0';
                    strbuf2[2]=carried_value1/1000%10+'0';
                    strbuf2[3]=carried_value1/100%10+'0';
                    strbuf2[4]='.';
                    strbuf2[5]=carried_value1/10%10+'0';
                    strbuf2[6]='R';
                }
                else if(carried_value1<1000000)//小于10k
                {
                    strbuf2[0]=' ';
                    strbuf2[1]=carried_value1/100000%10+'0';
                    strbuf2[2]='.';
                    strbuf2[3]=carried_value1/10000%10+'0';
                    strbuf2[4]=carried_value1/1000%10+'0';
                    strbuf2[5]='K';
                    strbuf2[6]='R';
                }
                else if(carried_value1<10000000)//小于100k
                {
                    strbuf2[0]=' ';
                    strbuf2[1]=carried_value1/1000000%10+'0';
                    strbuf2[2]=carried_value1/100000%10+'0';
                    strbuf2[3]='.';
                    strbuf2[4]=carried_value1/1000%10+'0';
                    strbuf2[5]='K';
                    strbuf2[6]='R';
                }
                else if(carried_value1<50000000)//小于500k
                {
                    
                    strbuf2[0]=carried_value1/10000000%10+'0';
                    strbuf2[1]=carried_value1/1000000%10+'0';
                    strbuf2[2]=carried_value1/100000%10+'0';
                    strbuf2[3]='.';
                    strbuf2[4]=carried_value1/10000%10+'0';
                    strbuf2[5]='K';
                    strbuf2[6]='R';
                }
                else
                {
                    strbuf2[1]='-';
                    strbuf2[2]='-';
                    strbuf2[3]='-';
                    strbuf2[4]='-';
                    strbuf2[5]='-';
                    strbuf2[6]='-';
                }
                LCD_ASCII_8X16_str(&dev,42,37,strbuf2,7,WHITE,BLACK);

                strbuf2[0]=carried_value2_sign?'-':' ';
                 if(carried_value2<1000)//小于10℉
                {
                    strbuf2[1]='0';
                    strbuf2[2]=carried_value2/100%10+'0';
                    strbuf2[3]='.';
                    strbuf2[4]=carried_value2/10%10+'0';
                    strbuf2[5]=carried_value2%10+'0';
                    strbuf2[6]='F';
                }
                else if(carried_value2<10000)//小于100℉
                {
                    strbuf2[1]=carried_value2/1000%10+'0';
                    strbuf2[2]=carried_value2/100%10+'0';
                    strbuf2[3]='.';
                    strbuf2[4]=carried_value2/10%10+'0';
                    strbuf2[5]=carried_value2%10+'0';
                    strbuf2[6]='F';    
                }
                else if(carried_value2<100000)//小于1000℉
                {
                    strbuf2[1]=carried_value2/10000%10+'0';
                    strbuf2[2]=carried_value2/1000%10+'0';
                    strbuf2[3]=carried_value2/100%10+'0';
                    strbuf2[4]='.';
                    strbuf2[5]=carried_value2/10%10+'0';
                    strbuf2[6]='F';
                }
                else
                {
                    strbuf2[1]='-';
                    strbuf2[2]='-';;
                    strbuf2[3]='.';
                    strbuf2[4]='-';;
                    strbuf2[5]='-';;
                    strbuf2[6]=' ';
                }
                
                LCD_ASCII_8X16_str(&dev,120,37,strbuf2,7,WHITE,BLACK);


                

                if(line_y_old!=line_y_temp)
                {
                    line_y_old=line_y_temp;
                    switch (line_y_temp)
                    {

                    case SCALE_X1:
                        LCD_ASCII_8X16_str(&dev,38,134,"1C   ",5,WHITE,DGRAY);
                        line_y_scale_cal=1;
                        break;
                    case SCALE_X10:
                        LCD_ASCII_8X16_str(&dev,38,134,"10C  ",5,WHITE,DGRAY);
                        line_y_scale_cal=10;
                        break;
                    case SCALE_X100:
                        LCD_ASCII_8X16_str(&dev,38,134,"100C ",5,WHITE,DGRAY);
                        line_y_scale_cal=100;
                        break;
                    default:
                        break;
                    }  
                }

                if(choice_temp_old!=choice_temp)
                {
                    choice_temp_old=choice_temp;
                    switch (choice_temp)
                    {
                    case TEMP_PT100:
                        LCD_ASCII_8X16_str(&dev,178,134,"   PT100    ",12,WHITE,DGRAY);
                        break;
                    case TEMP_PT200:
                        LCD_ASCII_8X16_str(&dev,178,134,"   PT200    ",12,WHITE,DGRAY);
                        break;
                    case TEMP_PT1000:
                        LCD_ASCII_8X16_str(&dev,178,134,"   PT1000   ",12,WHITE,DGRAY);
                        break;
                    case TEMP_NTC_103KF_3950:
                        LCD_ASCII_8X16_str(&dev,178,134,"NTC103KF3950",12,WHITE,DGRAY);
                        break;
                    case TEMP_NTC_103KF_3435:
                        LCD_ASCII_8X16_str(&dev,178,134,"NTC103KF3435",12,WHITE,DGRAY);
                        break;     
                    default:
                        break;
                    }
                }

                break;
            case KEY_ROTARY_R: //旋转编码器 右旋
                //if(choice_temp<4) choice_temp++;
                if(line_y_temp<SCALE_X100) line_y_temp++;
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(line_y_temp>SCALE_X1) line_y_temp--;
                //if(choice_temp>0) choice_temp--;
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(measure_stop==0) 
                {
                    measure_stop=1;
                    top_button_refresh();
                    vTaskDelay(pdMS_TO_TICKS(100));//给ADC最后一次刷新数值留出时间
                    LCD_ASCII_24X30_str(&dev,70,60,strbuf,6,0xB500,BLACK);//暂停测量后，测量值为灰黄色
                }
                else 
                {
                    measure_stop=0;
                    top_button_refresh();
                }
                break;
            case KEY_F2_DOWN: //功能键2 按下
                fun2_key=1;
                evt=WIFINET_MARK;
                xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
                top_button_refresh();
                break;
            case KEY_F2_UP: //功能键2 短按抬起
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_F2_LONG: //功能键2 长按
                fun2_key=0;
                top_button_refresh();
                break; 
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_LEFT_ICON: //刷新左侧图标
                left_icon_refresh();
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}

uint16_t V_A_select=0;

//刷新电压电流转换显示的数值
void V_A_refresh()
{
    uint32_t v=0;
    uint32_t a=0;
    char strbuf[4]="--- ";  
    if(measured_value!=0xFFFFFFFF)
    {
        if(gear_sw==ASW_DCV2)
        {
            v=(measured_value+50000)/100000;
            strbuf[0]=v/100%10+'0';
            strbuf[1]=v/10%10+'0';
            strbuf[2]='.';
            strbuf[3]=v%10+'0';
        }
        else
        {
            a=(measured_value+50)/100;
            strbuf[0]=a/100%10+'0';
            strbuf[1]=a/10%10+'0';
            strbuf[2]='.';
            strbuf[3]=a%10+'0';
        }
    }

    LCD_ASCII_24X30_str(&dev,122,102,strbuf,4,YELLOW,BLACK);

    if(measured_value!=0xFFFFFFFF)
    {
        if(gear_sw==ASW_DCV2)
        {
            a=(carried_value1+50)/100;
            strbuf[0]=a/100%10+'0';
            strbuf[1]=a/10%10+'0';
            strbuf[2]='.';
            strbuf[3]=a%10+'0';
            signal_A=a;
            if(signal_A>250) signal_A=250;
        }
        else
        {
            v=(carried_value1+50)/100;
            strbuf[0]=v/100%10+'0';
            strbuf[1]=v/10%10+'0';
            strbuf[2]='.';
            strbuf[3]=v%10+'0';
            signal_V=v;
            if(signal_V>100) signal_A=100;
        }
        
        GP8403_WriteReg(); 
    }
    else
    {
        signal_V=0;
        signal_A=0;
        GP8403_WriteReg();  
    }
    LCD_ASCII_24X30_str(&dev,122,150,strbuf,4,YELLOW,BLACK);
    
}

//电压电流转换 面板
void display_V_A()
{
    uint8_t evt;
    
    
    lcdFillScreen(&dev, BLACK);

    //
    lcdDrawFillRect(&dev,0,33,319,36,DGRAY);

    //顶部图标和按钮
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,67,5,ztcl_72x16,72,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,182,5,cssz_72x16,72,16,LGRAY,0X022D);


    if(setup_VI_select==0)
    {
        LCD_ASCII_8X16_str(&dev,88,60,"0-5V",4,WHITE,BLACK);
        if(vi_reverse1==0)
        {
            LCD_monochrome_any(&dev,130,60,youjt_16x16,16,16,WHITE,BLACK);
            LCD_monochrome_any(&dev,201,109,VDC,56,24,BLACK,YELLOW);
            LCD_monochrome_any(&dev,216,157,mAdc,56,24,BLACK,YELLOW);
            analog_switch(ASW_DCV2);
        }
        else
        {
            LCD_monochrome_any(&dev,130,60,zuojt_16x16,16,16,WHITE,BLACK);
            LCD_monochrome_any(&dev,216,109,mAdc,56,24,BLACK,YELLOW);
            LCD_monochrome_any(&dev,201,157,VDC,56,24,BLACK,YELLOW);
            analog_switch(ASW_DCMA);
        }
    }
    else if(setup_VI_select==1)
    {
        LCD_ASCII_8X16_str(&dev,88,60,"1-5V",4,WHITE,BLACK);
        if(vi_reverse2==0)
        {
            LCD_monochrome_any(&dev,130,60,youjt_16x16,16,16,WHITE,BLACK);
            LCD_monochrome_any(&dev,201,109,VDC,56,24,BLACK,YELLOW);
            LCD_monochrome_any(&dev,216,157,mAdc,56,24,BLACK,YELLOW);
            analog_switch(ASW_DCV2);
        }
        else
        {
            LCD_monochrome_any(&dev,130,60,zuojt_16x16,16,16,WHITE,BLACK);
            LCD_monochrome_any(&dev,216,109,mAdc,56,24,BLACK,YELLOW);
            LCD_monochrome_any(&dev,201,157,VDC,56,24,BLACK,YELLOW);
            analog_switch(ASW_DCMA);
        }
    }
    if(setup_VI_select==2)
    {
        LCD_ASCII_8X16_str(&dev,80,60,"0-10V",5,WHITE,BLACK);
        if(vi_reverse3==0)
        {
            LCD_monochrome_any(&dev,130,60,youjt_16x16,16,16,WHITE,BLACK);
            LCD_monochrome_any(&dev,201,109,VDC,56,24,BLACK,YELLOW);
            LCD_monochrome_any(&dev,216,157,mAdc,56,24,BLACK,YELLOW);
            analog_switch(ASW_DCV2);
        }
        else
        {
            LCD_monochrome_any(&dev,130,60,zuojt_16x16,16,16,WHITE,BLACK);
            LCD_monochrome_any(&dev,216,109,mAdc,56,24,BLACK,YELLOW);
            LCD_monochrome_any(&dev,201,157,VDC,56,24,BLACK,YELLOW);
            analog_switch(ASW_DCMA);
        }
    }
    LCD_ASCII_8X16_str(&dev,154,60,"4-20mA",6,WHITE,BLACK);


    top_icon_refresh();

    LCD_monochrome_any(&dev,30,102,clz_88x30,88,30,LGRAY,BLACK);
    
    
    //LCD_monochrome_any(&dev,258,99,shou_48x30,48,30,LGRAY,BLACK);
    //lcdDrawFillRect(&dev,258,99,306,129,BLACK);

    LCD_monochrome_any(&dev,30,150,scz_88x30,88,30,LGRAY,BLACK);

    
    //LCD_monochrome_any(&dev,258,157,shou_48x30,48,30,LGRAY,BLACK);
    //lcdDrawFillRect(&dev,258,157,306,187,BLACK);






    //LCD_monochrome_any(&dev,130,100,zlxhy_72x16,72,16,LGRAY,BLACK);
    V_A_refresh();
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case REFRESH_VALUE: //刷新测量值
                V_A_refresh();
                break;
            case KEY_ROTARY_R: //旋转编码器 右旋
               
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                

                break;
            case KEY_F2_UP: //功能键2
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_SETUP_V_I;
                ui_exit=1;
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_LEFT_ICON: //刷新左侧图标
                left_icon_refresh();
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}



uint16_t uart_data_move=0;


//UART调试 刷新显示内容
void uart_refresh_text()
{
    uint16_t i=0;
    uint16_t m=uart_data_move;
    //uint16_t rows=uart_data_start-uart_data_move;
    uint16_t rows=uart_data_start;

    while(m--)
    {
        if(rows==0) rows=UDATA_MAX;
        else rows--;
    }

    // ESP_LOGI(LCD_UI_TAG,"uart_data_start=%d\r\n",uart_data_start);
    // ESP_LOGI(LCD_UI_TAG,"uart_data_move=%d\r\n",uart_data_move);
    for(i=0;i<11;i++)
    {
        if(rows>=UDATA_MAX)
        {
            rows=0;
        }
        LCD_ASCII_8X16_str(&dev,3,55+(i*16),uart_data[rows],39,WHITE,BLACK); 
        rows++;      
    }
}

//UART调试 刷新配置信息
void uart_refresh_config()
{
    char baud_str[6]="      ";
    if(uart_config.baud_rate>=100000)
    {
        baud_str[0]=uart_config.baud_rate/100000%10+'0';
    }
    if(uart_config.baud_rate>=10000)
    {
        baud_str[1]=uart_config.baud_rate/10000%10+'0';
    }
    baud_str[2]=uart_config.baud_rate/1000%10+'0';
    baud_str[3]=uart_config.baud_rate/100%10+'0';
    baud_str[4]=uart_config.baud_rate/10%10+'0';
    baud_str[5]=uart_config.baud_rate%10+'0';
    LCD_ASCII_8X16_str(&dev,85,34,baud_str,6,YELLOW,DGRAY);

    if(uart_config.parity==UART_PARITY_DISABLE)
    {
        LCD_ASCII_8X16_str(&dev,140,34,"N",1,YELLOW,DGRAY);
    }
    else if(uart_config.parity==UART_PARITY_EVEN)
    {
        LCD_ASCII_8X16_str(&dev,140,34,"E",1,YELLOW,DGRAY);
    }
    else if(uart_config.parity==UART_PARITY_ODD)
    {
        LCD_ASCII_8X16_str(&dev,140,34,"O",1,YELLOW,DGRAY);
    }

    if(uart_config.data_bits==UART_DATA_5_BITS)
    {
        LCD_ASCII_8X16_str(&dev,155,34,"5",1,YELLOW,DGRAY);
    }
    else if(uart_config.data_bits==UART_DATA_6_BITS)
    {
        LCD_ASCII_8X16_str(&dev,155,34,"6",1,YELLOW,DGRAY);
    }
    else if(uart_config.data_bits==UART_DATA_7_BITS)
    {
        LCD_ASCII_8X16_str(&dev,155,34,"7",1,YELLOW,DGRAY);
    }
    else if(uart_config.data_bits==UART_DATA_8_BITS)
    {
        LCD_ASCII_8X16_str(&dev,155,34,"8",1,YELLOW,DGRAY);
    }

    if(uart_config.stop_bits==UART_STOP_BITS_1)
    {
        LCD_ASCII_8X16_str(&dev,170,34,"1  ",3,YELLOW,DGRAY);
    }
    else if(uart_config.stop_bits==UART_STOP_BITS_1_5)
    {
        LCD_ASCII_8X16_str(&dev,170,34,"1.5",3,YELLOW,DGRAY);
    }
    else if(uart_config.stop_bits==UART_STOP_BITS_2)
    {
        LCD_ASCII_8X16_str(&dev,170,34,"2  ",3,YELLOW,DGRAY);
    }

    if(uart_data_hex==0)
    {
        LCD_ASCII_8X16_str(&dev,200,34,"Str",3,YELLOW,DGRAY);
    }
    else
    {
        LCD_ASCII_8X16_str(&dev,200,34,"Hex",3,YELLOW,DGRAY);
    }

}



//UART调试 刷新接收的长度
void uart_refresh_rxlen()
{
    char len_srt[]="R:00000";
    len_srt[2]=uart_data_len/10000%10+'0';
    len_srt[3]=uart_data_len/1000%10+'0';
    len_srt[4]=uart_data_len/100%10+'0';
    len_srt[5]=uart_data_len/10%10+'0';
    len_srt[6]=uart_data_len%10+'0';
    LCD_ASCII_8X16_str(&dev,236,34,len_srt,7,YELLOW,DGRAY);
}
void uart_485_clean_rx()
{
    uint8_t i=0;
    uint8_t j=0;
    uart_data_len=0;
    uart_data_rows=0;
    uart_data_start=0;
    uart_data_end=0;
    uart_data_move=0;
    for(i=0;i<11;i++)
    {
        for(j=0;j<39;j++)
        {
            uart_data[i][j]=' ';
        }
    }
    uart_refresh_text();
    uart_refresh_rxlen();
}
xTaskHandle xHandle;
//UART rs485调试 面板
void display_uart_485(uint8_t select)
{
    uint8_t evt;
     
    uint8_t i=0;
    uint8_t j=0;
    lcdFillScreen(&dev, BLACK);
    lcdDrawFillRect(&dev,0,33,319,49,DGRAY);

    if(current_fun==FUN_UART) LCD_monochrome_any(&dev,5,34,uart_72x16,72,16,YELLOW,DGRAY);
    else LCD_monochrome_any(&dev,5,34,rs485_72x16,72,16,YELLOW,DGRAY);
    
    //顶部图标和按钮
    top_icon_refresh();
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,67,5,ztjx_72x16,72,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,182,5,qkjs_72x16,72,16,LGRAY,0X022D);

    uart_refresh_config();
    uart_refresh_text();
    uart_refresh_rxlen();

    xTaskCreate(usart_485_task, "USART_485", 1024*2, NULL, 4, &xHandle);
    uart_data_stop=0;
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case REFRESH_VALUE://刷新数据
                uart_refresh_text();
                uart_refresh_rxlen();
                break;   
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(uart_data_move>0) uart_data_move--;
                uart_refresh_text();
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                
                // if(uart_data_start>uart_data_end)
                // {
                //     if((uart_data_start-uart_data_move)>uart_data_end) uart_data_move++;
                // }
                // else
                // {
                //     if(uart_data_move<uart_data_start) uart_data_move++;
                // }
                if(uart_data_rows==UDATA_MAX)
                {
                    if(uart_data_move<=(UDATA_MAX-11)) uart_data_move++;
                }
                else if(uart_data_rows>11)
                {
                    if(uart_data_move<uart_data_rows-11) uart_data_move++;
                }
                
                
                uart_refresh_text();
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                uart_data_stop=1;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                uart_data_stop=1;
                vTaskDelete(xHandle);
                break;
            case KEY_F1_UP: //功能键1

                //ESP_LOGI(LCD_UI_TAG,"uart_data_rows=%d\r\n",uart_data_rows);
                //ESP_LOGI(LCD_UI_TAG,"uart_data_start=%d\r\n",uart_data_start);
                //ESP_LOGI(LCD_UI_TAG,"uart_data_end=%d\r\n",uart_data_end);


                if(uart_data_stop==0)
                {
                    uart_data_stop=1;
                    LCD_monochrome_any(&dev,5,34,stop_72x16,72,16,MAROON,DGRAY);
                }
                else
                {
                    uart_data_stop=0;
                    if(current_fun==FUN_UART) LCD_monochrome_any(&dev,5,34,uart_72x16,72,16,YELLOW,DGRAY);
                    else LCD_monochrome_any(&dev,5,34,rs485_72x16,72,16,YELLOW,DGRAY);
                }
                break;
            case KEY_F2_UP: //功能键2
                uart_485_clean_rx();
                
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}





//CAN调试 刷新显示内容
void can_refresh_text()
{
    char str[11];

    if(can_filter==0)
    {
        LCD_monochrome_any(&dev,130,70,guanbi_32x16,32,16,YELLOW,BLACK);
    }
    else 
    {
        LCD_monochrome_any(&dev,130,70,kaiqi_32x16,32,16,YELLOW,BLACK);
    }

    if(can_extd==0) LCD_monochrome_any(&dev,130,95,bzz_56x16,56,16,YELLOW,BLACK);
    else LCD_monochrome_any(&dev,130,95,kzz_56x16,56,16,YELLOW,BLACK);

    if(can_rtr==0) LCD_monochrome_any(&dev,130,120,sjz_56x16,56,16,YELLOW,BLACK);
    else LCD_monochrome_any(&dev,130,120,ycz_56x16,56,16,YELLOW,BLACK);
   
    sprintf(str,"0x%08X",can_filter_id);
    LCD_ASCII_8X16_str(&dev,130,145,str,10,YELLOW,BLACK);
    sprintf(str,"0x%08X",can_filter_mask);
    LCD_ASCII_8X16_str(&dev,130,170,str,10,YELLOW,BLACK);  
}

//CAN调试 刷新收发长度
void can_refresh_rxtxlen()
{
    char len_srt[]="R:00000";
    len_srt[0]='R';
    len_srt[2]=can_rx_len/10000%10+'0';
    len_srt[3]=can_rx_len/1000%10+'0';
    len_srt[4]=can_rx_len/100%10+'0';
    len_srt[5]=can_rx_len/10%10+'0';
    len_srt[6]=can_rx_len%10+'0';
    LCD_ASCII_8X16_str(&dev,145,34,len_srt,7,YELLOW,DGRAY);
    len_srt[0]='T';
    len_srt[2]=can_tx_len/10000%10+'0';
    len_srt[3]=can_tx_len/1000%10+'0';
    len_srt[4]=can_tx_len/100%10+'0';
    len_srt[5]=can_tx_len/10%10+'0';
    len_srt[6]=can_tx_len%10+'0';
    LCD_ASCII_8X16_str(&dev,230,34,len_srt,7,YELLOW,DGRAY);
}

void can_clean_rx()
{
    can_rx_len=0;
    can_tx_len=0;
    can_refresh_rxtxlen();
}

//CAN调试 刷新波特率
void can_refresh_baud()
{
    char baud_srt[4];
    switch (can_baud)
   {
    case TWAI_16KBITS:
        memcpy(baud_srt," 16k",4);
        break;
    case TWAI_20KBITS:
        memcpy(baud_srt," 20k",4);
        break;
    case TWAI_25KBITS:
        memcpy(baud_srt," 25k",4);
        break;
    case TWAI_50KBITS:
        memcpy(baud_srt," 50k",4);
        break;
    case TWAI_100KBITS:
        memcpy(baud_srt,"100k",4);
        break;
    case TWAI_125KBITS:
        memcpy(baud_srt,"125k",4);
        break;
    case TWAI_250KBITS:
        memcpy(baud_srt,"250k",4);
        break;
    case TWAI_500KBITS:
        memcpy(baud_srt,"500k",4);
        break;
    case TWAI_800KBITS:
        memcpy(baud_srt,"800k",4);
        break;
    case TWAI_1MBITS:
        memcpy(baud_srt," 1M ",4);
        break; 
   }
   LCD_ASCII_8X16_str(&dev,90,34,baud_srt,4,YELLOW,DGRAY);
}

//CAN调试  面板
void display_can()
{
    uint8_t evt;
     
    lcdFillScreen(&dev, BLACK);
    //顶部图标和按钮
    top_icon_refresh();
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,67,5,ztjx_72x16,72,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,182,5,glkg_72x16,72,16,LGRAY,0X022D);
    lcdDrawFillRect(&dev,0,33,319,49,DGRAY);
    LCD_monochrome_any(&dev,5,34,can_72x16,72,16,YELLOW,DGRAY);
    can_refresh_baud();
    //can_refresh_rxtxlen();
    LCD_monochrome_any(&dev,70,70,glq_56x16,56,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,70,95,zgs_56x16,56,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,70,120,zlx_56x16,56,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,85,145,zid_56x16,56,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,72,170,mask_56x16,56,16,WHITE,BLACK);
    can_refresh_text();
    LCD_monochrome_any(&dev,40,217,qdapp_200x12,200,12,LGRAY,BLACK);
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case REFRESH_CAN://刷新参数
                can_refresh_baud();
                can_refresh_text();
                break;    
            case REFRESH_VALUE://刷新数据
                can_refresh_rxtxlen();
                break;     
            case KEY_ROTARY_R: //旋转编码器 右旋
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(can_stop==0)
                {
                    can_stop=1;
                    twai_stop();
                    LCD_monochrome_any(&dev,5,34,stop_72x16,72,16,MAROON,DGRAY);
                }
                else
                {
                    twai_start();
                    can_stop=0;
                    LCD_monochrome_any(&dev,5,34,can_72x16,72,16,YELLOW,DGRAY);
                }
                break;
            case KEY_F2_UP: //功能键2
                if(can_filter==0) 
                {
                    can_filter=1;
                    twai_set_filter(can_filter,can_extd,can_filter_id,can_filter_mask);
                }
                else 
                {
                    can_filter=0;
                    twai_set_filter(can_filter,can_extd,can_filter_id,can_filter_mask);
                } 
                can_refresh_text();
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}

uint16_t signal_select=0;

//刷新直流信号源显示的数值
void signal_refresh()
{
    char strbuf[4];  
    strbuf[0]=signal_V/100%10+'0';
    strbuf[1]=signal_V/10%10+'0';
    strbuf[2]='.';
    strbuf[3]=signal_V%10+'0';
    LCD_ASCII_24X30_str(&dev,105,92,strbuf,4,YELLOW,BLACK);

    strbuf[0]=signal_A/100%10+'0';
    strbuf[1]=signal_A/10%10+'0';
    strbuf[2]='.';
    strbuf[3]=signal_A%10+'0';
    LCD_ASCII_24X30_str(&dev,105,150,strbuf,4,YELLOW,BLACK);

    GP8403_WriteReg();
}

//直流信号源 面板
void display_signal()
{
    uint8_t evt;
    
    
    lcdFillScreen(&dev, BLACK);

    //
    lcdDrawFillRect(&dev,0,33,319,36,DGRAY);

    //顶部图标和按钮
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,67,5,tjdy_72x16,72,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,182,5,tjdl_72x16,72,16,LGRAY,0X022D);

    top_icon_refresh();

    LCD_monochrome_any(&dev,30,92,scdy_64x30,64,30,LGRAY,BLACK);
    LCD_monochrome_any(&dev,185,99,VDC,56,24,BLACK,YELLOW);
    
    //LCD_monochrome_any(&dev,258,99,shou_48x30,48,30,LGRAY,BLACK);
    //lcdDrawFillRect(&dev,258,99,306,129,BLACK);

    LCD_monochrome_any(&dev,30,150,scdl_64x30,64,30,LGRAY,BLACK); 
    LCD_monochrome_any(&dev,195,157,mAdc,56,24,BLACK,YELLOW);
    //LCD_monochrome_any(&dev,258,157,shou_48x30,48,30,LGRAY,BLACK);
    //lcdDrawFillRect(&dev,258,157,306,187,BLACK);

    if(signal_select==0)
    {
        LCD_monochrome_any(&dev,258,99,shou_48x30,48,30,LGRAY,BLACK);
    }
    else
    {
        LCD_monochrome_any(&dev,258,157,shou_48x30,48,30,LGRAY,BLACK);
    }

    //LCD_monochrome_any(&dev,130,100,zlxhy_72x16,72,16,LGRAY,BLACK);
    signal_refresh();
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
             case REFRESH_VALUE://刷新数据
                signal_refresh();
                break;     
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(signal_select==0)
                {
                    if(signal_V<100) signal_V++;
                }
                else
                {
                    if(signal_A<300) signal_A++;
                }
                signal_refresh();
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(signal_select==0)
                {
                    if(signal_V>0) signal_V--;
                }
                else
                {
                    if(signal_A>0) signal_A--;
                }
                signal_refresh();
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                signal_select=0;
                lcdDrawFillRect(&dev,258,157,306,187,BLACK);
                LCD_monochrome_any(&dev,258,99,shou_48x30,48,30,LGRAY,BLACK);

                break;
            case KEY_F2_UP: //功能键2
                signal_select=1;
                lcdDrawFillRect(&dev,258,99,306,129,BLACK);
                LCD_monochrome_any(&dev,258,157,shou_48x30,48,30,LGRAY,BLACK);
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_LEFT_ICON: //刷新左侧图标
                left_icon_refresh();
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}



uint16_t pwm_select=0;
uint16_t pwm_freq_step=10; //调节频率时的步进
//刷新PWM显示的数值
void pwm_duty_refresh()
{
    char strbuf[4];  

    strbuf[0]=pwm_freq_step/1000%10+'0';
    strbuf[1]=pwm_freq_step/100%10+'0';
    strbuf[2]=pwm_freq_step/10%10+'0';
    strbuf[3]=pwm_freq_step%10+'0';
    LCD_ASCII_24X30_str(&dev,130,50,strbuf,4,YELLOW,BLACK);

    strbuf[0]=pwm_freq/1000%10+'0';
    strbuf[1]=pwm_freq/100%10+'0';
    strbuf[2]=pwm_freq/10%10+'0';
    strbuf[3]=pwm_freq%10+'0';
    LCD_ASCII_24X30_str(&dev,105,100,strbuf,4,YELLOW,BLACK);

    strbuf[0]=pwm_duty1/100%10+'0';
    strbuf[1]=pwm_duty1/10%10+'0';
    strbuf[2]=pwm_duty1%10+'0';
    LCD_ASCII_24X30_str(&dev,105,150,strbuf,3,YELLOW,BLACK);

    strbuf[0]=pwm_duty2/100%10+'0';
    strbuf[1]=pwm_duty2/10%10+'0';
    strbuf[2]=pwm_duty2%10+'0';
    LCD_ASCII_24X30_str(&dev,105,200,strbuf,3,YELLOW,BLACK);

    pwm_ledc_init();
    
}

//PWM信号源 面板
void display_pwm()
{
    uint8_t evt;
    
    uint8_t i=0;
    
    lcdFillScreen(&dev, BLACK);

    //
    lcdDrawFillRect(&dev,0,33,319,36,DGRAY);

    //顶部图标和按钮
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,75,5,syx_48x16,48,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,190,5,xyx_48x16,48,16,LGRAY,0X022D);

    top_icon_refresh();

    LCD_monochrome_any(&dev,30,50,bujin_96x30,96,30,LGRAY,BLACK);

    LCD_monochrome_any(&dev,30,100,pinlv_64x30,64,30,LGRAY,BLACK);
    LCD_monochrome_any(&dev,205,103,hz_24x30,24,30,YELLOW,BLACK);

    LCD_monochrome_any(&dev,30,150,dyl_64x30,64,30,LGRAY,BLACK);
    LCD_monochrome_any(&dev,185,153,bfh_24x30,24,30,YELLOW,BLACK);

    LCD_monochrome_any(&dev,30,200,del_64x30,64,30,LGRAY,BLACK); 
    LCD_monochrome_any(&dev,185,203,bfh_24x30,24,30,YELLOW,BLACK);
    
    LCD_monochrome_any(&dev,238,50+pwm_select*50+5,shou_48x30,48,30,LGRAY,BLACK);

    pwm_duty_refresh();
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case REFRESH_VALUE://刷新数据
                pwm_duty_refresh();
                break;   
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(pwm_select==0)
                {
                    if(pwm_freq_step==1) pwm_freq_step=10;
                    else if(pwm_freq_step==10) pwm_freq_step=100;
                    else if(pwm_freq_step==100) pwm_freq_step=1000;
                    else if(pwm_freq_step==1000) pwm_freq_step=1000;
                    else pwm_freq_step=10;
                }
                else if(pwm_select==1)
                {
                    if((pwm_freq+pwm_freq_step)<=9000) pwm_freq+=pwm_freq_step;
                }
                else if(pwm_select==2)
                {
                    if(pwm_duty1<100) pwm_duty1++;
                }
                else if(pwm_select==3)
                {
                    if(pwm_duty2<100) pwm_duty2++;
                }
                pwm_duty_refresh();
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(pwm_select==0)
                {

                   if(pwm_freq_step==1) pwm_freq_step=1;
                    else if(pwm_freq_step==10) pwm_freq_step=1;
                    else if(pwm_freq_step==100) pwm_freq_step=10;
                    else if(pwm_freq_step==1000) pwm_freq_step=100;
                    else pwm_freq_step=10;
                }
                else if(pwm_select==1)
                {
                    if(pwm_freq>=(pwm_freq_step+1)) pwm_freq-=pwm_freq_step;
                }
                else if(pwm_select==2)
                {
                    if(pwm_duty1>0) pwm_duty1--;
                }
                else if(pwm_select==3)
                {
                    if(pwm_duty2>0) pwm_duty2--;
                }
                pwm_duty_refresh();
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(pwm_select>0) pwm_select--;
                for(i=0;i<4;i++)
                {
                    LCD_monochrome_any(&dev,238,50+i*50+5,cshou_48x30,48,30,LGRAY,BLACK);
                }
                LCD_monochrome_any(&dev,238,50+pwm_select*50+5,shou_48x30,48,30,LGRAY,BLACK);

                break;
            case KEY_F2_UP: //功能键2
                if(pwm_select<3) pwm_select++;
                for(i=0;i<4;i++)
                {
                    LCD_monochrome_any(&dev,238,50+i*50+5,cshou_48x30,48,30,LGRAY,BLACK);
                }
                LCD_monochrome_any(&dev,238,50+pwm_select*50+5,shou_48x30,48,30,LGRAY,BLACK);
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_LEFT_ICON: //刷新左侧图标
                left_icon_refresh();
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}

//刷新舵机显示的数值
void servo_duty_refresh()
{
    char strbuf[4];  

    strbuf[0]=servo_type/100%10+'0';
    strbuf[1]=servo_type/10%10+'0';
    strbuf[2]=servo_type%10+'0';
    LCD_ASCII_24X30_str(&dev,105,60,strbuf,3,LGRAY,BLACK);

    strbuf[0]=servo_angle1/100%10+'0';
    strbuf[1]=servo_angle1/10%10+'0';
    strbuf[2]=servo_angle1%10+'0';
    LCD_ASCII_24X30_str(&dev,105,115,strbuf,3,YELLOW,BLACK);

    strbuf[0]=servo_angle2/100%10+'0';
    strbuf[1]=servo_angle2/10%10+'0';
    strbuf[2]=servo_angle2%10+'0';
    LCD_ASCII_24X30_str(&dev,105,170,strbuf,3,YELLOW,BLACK);

    set_pwnch1(204.8+(4.5511*servo_angle1),0);
    set_pwnch2(204.8+(4.5511*servo_angle2),0);
    
}

uint8_t servo_select=0;

//舵机控制 面板
void display_servo()
{
    uint8_t evt;
    
    
    lcdFillScreen(&dev, BLACK);

    //
    lcdDrawFillRect(&dev,0,33,319,36,DGRAY);

    //顶部图标和按钮
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,67,5,tjdyl_72x16,72,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,182,5,tjdel_72x16,72,16,LGRAY,0X022D);

    top_icon_refresh();

    LCD_monochrome_any(&dev,30,60,leixing_64x30,64,30,LGRAY,BLACK);
    LCD_monochrome_any(&dev,185,63,du_24x30,24,30,LGRAY,BLACK);

    LCD_monochrome_any(&dev,30,115,dyl_64x30,64,30,LGRAY,BLACK);
    LCD_monochrome_any(&dev,185,118,du_24x30,24,30,YELLOW,BLACK);

    LCD_monochrome_any(&dev,30,170,del_64x30,64,30,LGRAY,BLACK); 
    LCD_monochrome_any(&dev,185,173,du_24x30,24,30,YELLOW,BLACK);
  

    if(servo_select==0)
    {
        LCD_monochrome_any(&dev,238,120,shou_48x30,48,30,LGRAY,BLACK);
    }
    else
    {
        LCD_monochrome_any(&dev,238,175,shou_48x30,48,30,LGRAY,BLACK);
    }

    //LCD_monochrome_any(&dev,130,100,zlxhy_72x16,72,16,LGRAY,BLACK);
    servo_duty_refresh();
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(servo_select==0)
                {
                    if(servo_angle1<180) servo_angle1++;
                }
                else
                {
                    if(servo_angle2<180) servo_angle2++;
                }
                servo_duty_refresh();
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(servo_select==0)
                {
                    if(servo_angle1>0) servo_angle1--;
                }
                else
                {
                    if(servo_angle2>0) servo_angle2--;
                }
                servo_duty_refresh();
                break;
            case KEY_ENTER: //旋转编码器 按下
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_old=ui_display_st;
                ui_display_st=DISPLAY_FUNCHOICE;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                servo_select=0;
                lcdDrawFillRect(&dev,238,175,286,205,BLACK);
                LCD_monochrome_any(&dev,238,120,shou_48x30,48,30,LGRAY,BLACK);

                break;
            case KEY_F2_UP: //功能键2
                servo_select=1;
                lcdDrawFillRect(&dev,238,120,286,150,BLACK);
                LCD_monochrome_any(&dev,238,175,shou_48x30,48,30,LGRAY,BLACK);
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_LEFT_ICON: //刷新左侧图标
                left_icon_refresh();
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                top_icon_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}


void setup_range_refresh()
{
    if(choice_dca==CUR_UA) //uA 档位 最大5000uA
    {
        lcdDrawFillRect(&dev,180,70,224,73,YELLOW);
        lcdDrawFillRect(&dev,120,70,179,73,BLACK);
        lcdDrawFillRect(&dev,78,70,118,73,BLACK);      
    }
    else if(choice_dca==CUR_MA)//mA 档位 最大 500mA
    {
        lcdDrawFillRect(&dev,180,70,224,73,BLACK);
        lcdDrawFillRect(&dev,120,70,179,73,YELLOW);
        lcdDrawFillRect(&dev,78,70,116,73,BLACK);   
    }
    else if(choice_dca==CUR_A)//A 档位
    {
        lcdDrawFillRect(&dev,180,70,224,73,BLACK);
        lcdDrawFillRect(&dev,120,70,179,73,BLACK);
        lcdDrawFillRect(&dev,78,70,116,73,YELLOW);     
    }

    if(choice_aca==CUR_MA)//mA 档位 最大 500mA
    {
        lcdDrawFillRect(&dev,120,109,179,112,YELLOW);
        lcdDrawFillRect(&dev,78,109,116,112,BLACK);   
    }
    else if(choice_aca==CUR_A)//A 档位
    {
        lcdDrawFillRect(&dev,120,109,179,112,BLACK);
        lcdDrawFillRect(&dev,78,109,116,112,YELLOW);     
    }
    
    if(choice_dcv==VOL_AUTO) //自动
    {
        lcdDrawFillRect(&dev,78,149,116,152,YELLOW);
        lcdDrawFillRect(&dev,123,149,176,152,BLACK); 
        lcdDrawFillRect(&dev,179,149,224,152,BLACK);
        lcdDrawFillRect(&dev,226,149,263,152,BLACK);    
    }
    else if(choice_dcv==VOL_1000V) //1000V
    {
        lcdDrawFillRect(&dev,78,149,116,152,BLACK);
        lcdDrawFillRect(&dev,123,149,176,152,YELLOW); 
        lcdDrawFillRect(&dev,179,149,224,152,BLACK);
        lcdDrawFillRect(&dev,226,149,263,152,BLACK);
    }
    else if(choice_dcv==VOL_100V) //100V
    {
        lcdDrawFillRect(&dev,78,149,116,152,BLACK);
        lcdDrawFillRect(&dev,123,149,176,152,BLACK); 
        lcdDrawFillRect(&dev,179,149,224,152,YELLOW);
        lcdDrawFillRect(&dev,226,149,263,152,BLACK);
    }
    else if(choice_dcv==VOL_10V) //10V
    {
        lcdDrawFillRect(&dev,78,149,116,152,BLACK);
        lcdDrawFillRect(&dev,123,149,176,152,BLACK); 
        lcdDrawFillRect(&dev,179,149,224,152,BLACK);
        lcdDrawFillRect(&dev,226,149,263,152,YELLOW);
    }

    if(choice_acv==VOL_AUTO) //自动
    {
        lcdDrawFillRect(&dev,78,188,116,191,YELLOW);
        lcdDrawFillRect(&dev,123,188,176,191,BLACK); 
        lcdDrawFillRect(&dev,179,188,224,191,BLACK);
        lcdDrawFillRect(&dev,226,188,263,191,BLACK);    
    }
    else if(choice_acv==VOL_1000V) //1000V
    {
        lcdDrawFillRect(&dev,78,188,116,191,BLACK);
        lcdDrawFillRect(&dev,123,188,176,191,YELLOW); 
        lcdDrawFillRect(&dev,179,188,224,191,BLACK);
        lcdDrawFillRect(&dev,226,188,263,191,BLACK);
    }
    else if(choice_acv==VOL_100V) //100V
    {
        lcdDrawFillRect(&dev,78,188,116,191,BLACK);
        lcdDrawFillRect(&dev,123,188,176,191,BLACK); 
        lcdDrawFillRect(&dev,179,188,224,191,YELLOW);
        lcdDrawFillRect(&dev,226,188,263,191,BLACK);
    }
    else if(choice_acv==VOL_10V) //10V
    {
        lcdDrawFillRect(&dev,78,188,116,191,BLACK);
        lcdDrawFillRect(&dev,123,188,176,191,BLACK); 
        lcdDrawFillRect(&dev,179,188,224,191,BLACK);
        lcdDrawFillRect(&dev,226,188,263,191,YELLOW);
    }

    if(choice_2wr==RES_AUTO)
    {
        lcdDrawFillRect(&dev,78,227,116,230,YELLOW);
        lcdDrawFillRect(&dev,122,227,169,230,BLACK);
        lcdDrawFillRect(&dev,173,227,217,230,BLACK);
        lcdDrawFillRect(&dev,218,227,256,230,BLACK);
        lcdDrawFillRect(&dev,257,227,288,230,BLACK);
    }
    else if(choice_2wr==RES_100M)
    {
        lcdDrawFillRect(&dev,78,227,116,230,BLACK);
        lcdDrawFillRect(&dev,122,227,169,230,YELLOW);
        lcdDrawFillRect(&dev,173,227,217,230,BLACK);
        lcdDrawFillRect(&dev,218,227,256,230,BLACK);
        lcdDrawFillRect(&dev,257,227,288,230,BLACK); 
    }
    else if(choice_2wr==RES_500K)
    {
        lcdDrawFillRect(&dev,78,227,116,230,BLACK);
        lcdDrawFillRect(&dev,122,227,169,230,BLACK);
        lcdDrawFillRect(&dev,173,227,217,230,YELLOW);
        lcdDrawFillRect(&dev,218,227,256,230,BLACK);
        lcdDrawFillRect(&dev,257,227,288,230,BLACK); 
    }
    else if(choice_2wr==RES_20K)
    {
        lcdDrawFillRect(&dev,78,227,116,230,BLACK);
        lcdDrawFillRect(&dev,122,227,169,230,BLACK);
        lcdDrawFillRect(&dev,173,227,217,230,BLACK);
        lcdDrawFillRect(&dev,218,227,256,230,YELLOW);
        lcdDrawFillRect(&dev,257,227,288,230,BLACK); 
    }
    else if(choice_2wr==RES_1K)
    {
        lcdDrawFillRect(&dev,78,227,116,230,BLACK);
        lcdDrawFillRect(&dev,122,227,169,230,BLACK);
        lcdDrawFillRect(&dev,173,227,217,230,BLACK);
        lcdDrawFillRect(&dev,218,227,256,230,BLACK);
        lcdDrawFillRect(&dev,257,227,288,230,YELLOW); 
    }
}

uint8_t setup_range_select=0;

//量程设置 面板
void display_setup_range()
{
    uint8_t evt;
    
    uint8_t i=0;
    
    lcdFillScreen(&dev, BLACK);

    //
    lcdDrawFillRect(&dev,0,33,319,36,DGRAY);

    //顶部图标和按钮
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,75,5,syx_48x16,48,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,190,5,xyx_48x16,48,16,LGRAY,0X022D);

    LCD_monochrome_any(&dev,1,50,zldl_224x16,224,16,WHITE,BLACK);

    LCD_monochrome_any(&dev,1,90,jldl_176x16,176,16,WHITE,BLACK);

    LCD_monochrome_any(&dev,1,130,zldy_112x16,112,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,123,130,v1000_136x16,136,16,WHITE,BLACK);

    LCD_monochrome_any(&dev,1,170,jldy_112x16,112,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,123,170,v1000_136x16,136,16,WHITE,BLACK);

    LCD_monochrome_any(&dev,1,210,lxdz1_112x16,112,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,121,210,lxdz2_160x16,160,16,WHITE,BLACK);

   
    LCD_monochrome_any(&dev,287,50+setup_range_select*40-5,shou_32x24,32,24,WHITE,BLACK);
    
    setup_range_refresh();

    //signal_refresh();
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case KEY_ROTARY_R: //旋转编码器 右旋
                switch (setup_range_select)
                {
                case 0:
                    if(choice_dca>0) choice_dca--;
                    break;
                case 1:
                    if(choice_aca>1) choice_aca--;
                    break;
                case 2:
                    if(choice_dcv<3) choice_dcv++;
                    break;
                case 3:
                    if(choice_acv<3) choice_acv++;
                    break;
                case 4:
                    if(choice_2wr<4) choice_2wr++;
                    break;
                default:
                    break;
                }
                
                setup_range_refresh();
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                switch (setup_range_select)
                {
                case 0:
                    if(choice_dca<2) choice_dca++;
                    break;
                case 1:
                    if(choice_aca<2) choice_aca++;
                    break;
                case 2:
                    if(choice_dcv>0) choice_dcv--;
                    break;
                case 3:
                    if(choice_acv>0) choice_acv--;
                    break;
                case 4:
                    if(choice_2wr>0) choice_2wr--;
                    break;
                default:
                    break;
                }
                setup_range_refresh();
                break;
            case KEY_ENTER: //旋转编码器 按下

                break;
            case KEY_PWR_BACK: //返回键
                ui_display_st=DISPLAY_MENU;
                write_config_in_nvs(CONFIG_OTHER); 
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(setup_range_select>0) setup_range_select--;
                for(i=0;i<5;i++)
                {
                    LCD_monochrome_any(&dev,287,50+i*40-5,cshou_32x24,32,24,WHITE,BLACK);
                }
                LCD_monochrome_any(&dev,287,50+setup_range_select*40-5,shou_32x24,32,24,WHITE,BLACK);

                break;
            case KEY_F2_UP: //功能键2
                if(setup_range_select<4) setup_range_select++;
                for(i=0;i<5;i++)
                {
                    LCD_monochrome_any(&dev,287,50+i*40-5,cshou_32x24,32,24,WHITE,BLACK);
                }
                LCD_monochrome_any(&dev,287,50+setup_range_select*40-5,shou_32x24,32,24,WHITE,BLACK);
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            }
        }
        if(ui_exit==1) break;
    }
}



void setup_save_refresh()
{
    char ch[10];
    time_t now;
	struct tm timeinfo;

    if(sd_state==0)//TF卡未挂载
    {
        LCD_monochrome_any(&dev,162,60,wuka_32x16,32,16,YELLOW,BLACK);
    }
    else if(sd_onoff==0)
    {
        LCD_monochrome_any(&dev,162,60,guanbi_32x16,32,16,YELLOW,BLACK);
    }
    else 
    {
        LCD_monochrome_any(&dev,162,60,kaiqi_32x16,32,16,YELLOW,BLACK);
    }

    if(time_state==0)//未校时
    {
        LCD_monochrome_any(&dev,162,92,wjs_48x16,48,16,WHITE,BLACK);
        LCD_monochrome_any(&dev,162,114,wjs_48x16,48,16,WHITE,BLACK);
    }
    else
    {
        time(&now);
        localtime_r(&now, &timeinfo);

        ch[0] = (timeinfo.tm_year + 1900) / 1000 % 10 + '0';
        ch[1] = (timeinfo.tm_year + 1900) / 100 % 10 + '0';
        ch[2] = (timeinfo.tm_year + 1900) / 10 % 10 + '0';
        ch[3] = (timeinfo.tm_year + 1900) % 10 + '0';
        ch[4] = '/';
        ch[5] = (timeinfo.tm_mon + 1) / 10 % 10 + '0';
        ch[6] = (timeinfo.tm_mon + 1) % 10 + '0';
        ch[7] = '/';
        ch[8] = (timeinfo.tm_mday) / 10 % 10 + '0';
        ch[9] = (timeinfo.tm_mday) % 10 + '0';
        LCD_ASCII_8X16_str(&dev,162,92,ch,10,WHITE,BLACK);
        ch[0] = (timeinfo.tm_hour) / 10 % 10 + '0';
        ch[1] = (timeinfo.tm_hour) % 10 + '0';
        ch[2] = ':';
        ch[3] = (timeinfo.tm_min) / 10 % 10 + '0';
        ch[4] = (timeinfo.tm_min) % 10 + '0';
        ch[5] = ':';
        ch[6] = (timeinfo.tm_sec) / 10 % 10 + '0';
        ch[7] = (timeinfo.tm_sec) % 10 + '0';
        LCD_ASCII_8X16_str(&dev,162,114,ch,8,WHITE,BLACK);

    }
}

esp_timer_handle_t esp_timer_handle_t2 = 0;

/*定时器中断函数*/
void esp_timer_s_cb(void *arg){
   setup_save_refresh(); 
}

//存储设置 面板
void display_setup_save()
{
    uint8_t evt;
    
    char ch[10];
    uint32_t data=0;
    
    lcdFillScreen(&dev, BLACK);

    //
    lcdDrawFillRect(&dev,0,33,319,36,DGRAY);

    //顶部图标和按钮
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,67,5,cckg_72x16,72,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    //LCD_monochrome_any(&dev,182,5,wljs_72x16,72,16,LGRAY,0X022D);

    LCD_monochrome_any(&dev,85,60,bdcc_72x16,72,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,85,92,bdrq_72x16,72,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,85,114,bdsj_72x16,72,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,40,136,tfzrl_112x16,112,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,23,158,rfsyrl_128x16,128,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,90,200,wjs_136x12,136,12,LGRAY,BLACK);
    LCD_monochrome_any(&dev,90,217,bzcrcb_120x12,120,12,LGRAY,BLACK);

   // ESP_LOGI(LCD_UI_TAG,"sd_state=%d\r\n",sd_state);

    if(sd_state!=0)//TF卡已挂载 显示容量
    {
        data=bytes_total/1024/10.24;
        ch[0]=data/100000%10+'0';
        ch[1]=data/10000%10+'0';
        ch[2]=data/1000%10+'0';
        ch[3]=data/100%10+'0';
        ch[4]='.';
        ch[5]=data/10%10+'0';
        ch[6]=data%10+'0';
        LCD_ASCII_8X16_str(&dev,162,136,ch,7,WHITE,BLACK);

        data=bytes_free/1024/10.24;
        ch[0]=data/100000%10+'0';
        ch[1]=data/10000%10+'0';
        ch[2]=data/1000%10+'0';
        ch[3]=data/100%10+'0';
        ch[4]='.';
        ch[5]=data/10%10+'0';
        ch[6]=data%10+'0';
        LCD_ASCII_8X16_str(&dev,162,158,ch,7,WHITE,BLACK);
    } 
    else
    {
        LCD_monochrome_any(&dev,162,136,wuka_32x16,32,16,WHITE,BLACK);
        LCD_monochrome_any(&dev,162,158,wuka_32x16,32,16,WHITE,BLACK);
    }


    setup_save_refresh();


    esp_timer_start_periodic(esp_timer_handle_t2,  1000*1000);
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case KEY_ROTARY_R: //旋转编码器 右旋
                
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                
                break;
            case KEY_ENTER: //旋转编码器 按下
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_st=DISPLAY_MENU;
                 write_config_in_nvs(CONFIG_OTHER);  
                ui_exit=1;
                esp_timer_stop(esp_timer_handle_t2);
                break;
            case KEY_F1_UP: //功能键1
                
                if(sd_state!=0)
                {
                    if(sd_onoff==0) sd_onoff=1;
                    else sd_onoff=0;
                }
                else
                {
                    sd_onoff=0;
                }
                if(time_state==0)//未校时
                {
                    sd_onoff=0;
                }
                
                setup_save_refresh();  
                break;
            case KEY_F2_UP: //功能键2
                
                break;
            case KEY_PWROFF: //关机
                esp_timer_stop(esp_timer_handle_t2);
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            }
        }
        if(ui_exit==1) break;
    }
}



uint16_t setup_buzzer_select=0;
//刷新蜂鸣器阈值显示的数值
void setup_buzzer_refresh()
{
    char strbuf[4];  
    strbuf[0]=r_buzzer_threshold/100%10+'0';
    strbuf[1]=r_buzzer_threshold/10%10+'0';
    strbuf[2]=r_buzzer_threshold%10+'0';
    LCD_ASCII_24X30_str(&dev,115,92,strbuf,3,YELLOW,BLACK);

    strbuf[0]=d_buzzer_threshold/100%10+'0';
    strbuf[1]='.';
    strbuf[2]=d_buzzer_threshold/10%10+'0';
    strbuf[3]=d_buzzer_threshold%10+'0';
    LCD_ASCII_24X30_str(&dev,115,150,strbuf,4,YELLOW,BLACK);
}
//蜂鸣器阈值 面板
void display_setup_buzzer()
{
    uint8_t evt;
    
    
    lcdFillScreen(&dev, BLACK);

    //
    lcdDrawFillRect(&dev,0,33,319,36,DGRAY);

    //顶部图标和按钮
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,75,5,tdd_48x16,48,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,190,5,ejg_48x16,48,16,LGRAY,0X022D);

    LCD_monochrome_any(&dev,30,92,tdd_80x30,80,30,LGRAY,BLACK);
    LCD_monochrome_any(&dev,205,95,R_24x24,24,24,YELLOW,BLACK);


    LCD_monochrome_any(&dev,30,150,ejg_80x30,80,30,LGRAY,BLACK); 
    LCD_monochrome_any(&dev,205,153,V_24x24,24,24,YELLOW,BLACK);


    if(setup_buzzer_select==0)
    {
        LCD_monochrome_any(&dev,258,99,shou_48x30,48,30,LGRAY,BLACK);
    }
    else
    {
        LCD_monochrome_any(&dev,258,157,shou_48x30,48,30,LGRAY,BLACK);
    }

    //LCD_monochrome_any(&dev,130,100,zlxhy_72x16,72,16,LGRAY,BLACK);
    setup_buzzer_refresh();
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(setup_buzzer_select==0)
                {
                    if(r_buzzer_threshold<100) r_buzzer_threshold++;
                }
                else
                {
                    if(d_buzzer_threshold<100) d_buzzer_threshold++;
                }
                setup_buzzer_refresh();
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(setup_buzzer_select==0)
                {
                    if(r_buzzer_threshold>1) r_buzzer_threshold--;
                }
                else
                {
                    if(d_buzzer_threshold>1) d_buzzer_threshold--;
                }
                setup_buzzer_refresh();
                break;
            case KEY_ENTER: //旋转编码器 按下

                break;
            case KEY_PWR_BACK: //返回键
                ui_display_st=DISPLAY_MENU;
                write_config_in_nvs(CONFIG_OTHER);  
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                setup_buzzer_select=0;
                lcdDrawFillRect(&dev,258,157,306,187,BLACK);
                LCD_monochrome_any(&dev,258,99,shou_48x30,48,30,LGRAY,BLACK);

                break;
            case KEY_F2_UP: //功能键2
                setup_buzzer_select=1;
                lcdDrawFillRect(&dev,258,99,306,129,BLACK);
                LCD_monochrome_any(&dev,258,157,shou_48x30,48,30,LGRAY,BLACK);
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            }
        }
        if(ui_exit==1) break;
    }
}

//温度传感器类型 面板
void display_setup_temp()
{
    uint8_t evt;
    
    uint8_t i=0;
    
    lcdFillScreen(&dev, BLACK);

    //
    lcdDrawFillRect(&dev,0,33,319,36,DGRAY);

    //顶部图标和按钮
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,75,5,syx_48x16,48,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,190,5,xyx_48x16,48,16,LGRAY,0X022D);

    if(choice_temp==0) LCD_monochrome_any(&dev,60,80,pt100_64x16,64,16,YELLOW,BLACK);
    else LCD_monochrome_any(&dev,60,80,pt100_64x16,64,16,WHITE,BLACK);

    if(choice_temp==1) LCD_monochrome_any(&dev,60,105,pt200_64x16,64,16,YELLOW,BLACK);
    else LCD_monochrome_any(&dev,60,105,pt200_64x16,64,16,WHITE,BLACK);

    if(choice_temp==2) LCD_monochrome_any(&dev,60,130,pt1000_72x16,72,16,YELLOW,BLACK);
    else LCD_monochrome_any(&dev,60,130,pt1000_72x16,72,16,WHITE,BLACK);

    if(choice_temp==3) LCD_monochrome_any(&dev,60,155,NTC_103KF_3950_160x16,160,16,YELLOW,BLACK);
    else LCD_monochrome_any(&dev,60,155,NTC_103KF_3950_160x16,160,16,WHITE,BLACK);

    if(choice_temp==4) LCD_monochrome_any(&dev,60,180,NTC_103KF_3435_160x16,160,16,YELLOW,BLACK);
    else LCD_monochrome_any(&dev,60,180,NTC_103KF_3435_160x16,160,16,WHITE,BLACK);

    LCD_monochrome_any(&dev,250,80+choice_temp*25-5,shou_32x24,32,24,WHITE,BLACK);

    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(choice_temp>0) choice_temp--;
                for(i=0;i<5;i++)
                {
                    LCD_monochrome_any(&dev,250,80+i*25-5,cshou_32x24,32,24,WHITE,BLACK);
                }
                LCD_monochrome_any(&dev,250,80+choice_temp*25-5,shou_32x24,32,24,WHITE,BLACK);

                if(choice_temp==0) LCD_monochrome_any(&dev,60,80,pt100_64x16,64,16,YELLOW,BLACK);
                else LCD_monochrome_any(&dev,60,80,pt100_64x16,64,16,WHITE,BLACK);

                if(choice_temp==1) LCD_monochrome_any(&dev,60,105,pt200_64x16,64,16,YELLOW,BLACK);
                else LCD_monochrome_any(&dev,60,105,pt200_64x16,64,16,WHITE,BLACK);

                if(choice_temp==2) LCD_monochrome_any(&dev,60,130,pt1000_72x16,72,16,YELLOW,BLACK);
                else LCD_monochrome_any(&dev,60,130,pt1000_72x16,72,16,WHITE,BLACK);

                if(choice_temp==3) LCD_monochrome_any(&dev,60,155,NTC_103KF_3950_160x16,160,16,YELLOW,BLACK);
                else LCD_monochrome_any(&dev,60,155,NTC_103KF_3950_160x16,160,16,WHITE,BLACK);

                if(choice_temp==4) LCD_monochrome_any(&dev,60,180,NTC_103KF_3435_160x16,160,16,YELLOW,BLACK);
                else LCD_monochrome_any(&dev,60,180,NTC_103KF_3435_160x16,160,16,WHITE,BLACK);

                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(choice_temp<4) choice_temp++;
                for(i=0;i<5;i++)
                {
                    LCD_monochrome_any(&dev,250,80+i*25-5,cshou_32x24,32,24,WHITE,BLACK);
                }
                LCD_monochrome_any(&dev,250,80+choice_temp*25-5,shou_32x24,32,24,WHITE,BLACK);

                if(choice_temp==0) LCD_monochrome_any(&dev,60,80,pt100_64x16,64,16,YELLOW,BLACK);
                else LCD_monochrome_any(&dev,60,80,pt100_64x16,64,16,WHITE,BLACK);

                if(choice_temp==1) LCD_monochrome_any(&dev,60,105,pt200_64x16,64,16,YELLOW,BLACK);
                else LCD_monochrome_any(&dev,60,105,pt200_64x16,64,16,WHITE,BLACK);

                if(choice_temp==2) LCD_monochrome_any(&dev,60,130,pt1000_72x16,72,16,YELLOW,BLACK);
                else LCD_monochrome_any(&dev,60,130,pt1000_72x16,72,16,WHITE,BLACK);

                if(choice_temp==3) LCD_monochrome_any(&dev,60,155,NTC_103KF_3950_160x16,160,16,YELLOW,BLACK);
                else LCD_monochrome_any(&dev,60,155,NTC_103KF_3950_160x16,160,16,WHITE,BLACK);

                if(choice_temp==4) LCD_monochrome_any(&dev,60,180,NTC_103KF_3435_160x16,160,16,YELLOW,BLACK);
                else LCD_monochrome_any(&dev,60,180,NTC_103KF_3435_160x16,160,16,WHITE,BLACK);

                break; 
            case KEY_ENTER: //旋转编码器 按下
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_st=DISPLAY_MENU;
                write_config_in_nvs(CONFIG_OTHER);  
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(choice_temp>0) choice_temp--;
                for(i=0;i<5;i++)
                {
                    LCD_monochrome_any(&dev,250,80+i*25-5,cshou_32x24,32,24,WHITE,BLACK);
                }
                LCD_monochrome_any(&dev,250,80+choice_temp*25-5,shou_32x24,32,24,WHITE,BLACK);
                break;
            case KEY_F2_UP: //功能键2
                if(choice_temp<4) choice_temp++;
                for(i=0;i<5;i++)
                {
                    LCD_monochrome_any(&dev,250,80+i*25-5,cshou_32x24,32,24,WHITE,BLACK);
                }
                LCD_monochrome_any(&dev,250,80+choice_temp*25-5,shou_32x24,32,24,WHITE,BLACK);
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            }
        }
        if(ui_exit==1) break;
    }
}




void setup_VI_refresh()
{
    if(vi_reverse1==0)
    {
        LCD_monochrome_any(&dev,110,90,youjt_16x16,16,16,BLACK,YELLOW);
    }
    else
    {
        LCD_monochrome_any(&dev,110,90,zuojt_16x16,16,16,BLACK,YELLOW);
    }

    if(vi_reverse2==0)
    {
        LCD_monochrome_any(&dev,110,130,youjt_16x16,16,16,BLACK,YELLOW);
    }
    else
    {
        LCD_monochrome_any(&dev,110,130,zuojt_16x16,16,16,BLACK,YELLOW);
    }

    if(vi_reverse3==0)
    {
        LCD_monochrome_any(&dev,110,170,youjt_16x16,16,16,BLACK,YELLOW);
    }
    else
    {
        LCD_monochrome_any(&dev,110,170,zuojt_16x16,16,16,BLACK,YELLOW);
    }
}

//V/I转换设置
void display_setup_V_I()
{
    uint8_t evt;
    
    uint8_t i=0;
    
    lcdFillScreen(&dev, BLACK);

    //
    lcdDrawFillRect(&dev,0,33,319,36,DGRAY);

    //顶部图标和按钮
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,75,5,syx_48x16,48,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,190,5,xyx_48x16,48,16,LGRAY,0X022D);

    LCD_ASCII_8X16_str(&dev,68,90,"0-5V",4,WHITE,BLACK);
    LCD_ASCII_8X16_str(&dev,134,90,"4-20mA",6,WHITE,BLACK);

    LCD_ASCII_8X16_str(&dev,68,130,"1-5V",4,WHITE,BLACK);
    LCD_ASCII_8X16_str(&dev,134,130,"4-20mA",6,WHITE,BLACK);

    LCD_ASCII_8X16_str(&dev,60,170,"0-10V",5,WHITE,BLACK);
    LCD_ASCII_8X16_str(&dev,134,170,"4-20mA",6,WHITE,BLACK);

    setup_VI_refresh();

    LCD_monochrome_any(&dev,230,90+setup_VI_select*40-5,shou_32x24,32,24,WHITE,BLACK);
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case KEY_ROTARY_R: //旋转编码器 右旋
                switch (setup_VI_select)
                {
                case 0:
                    if(vi_reverse1==1) vi_reverse1=0;
                    break;
                case 1:
                    if(vi_reverse2==1) vi_reverse2=0;
                    break;
                case 2:
                    if(vi_reverse3==1) vi_reverse3=0;
                    break;
                default:
                    break;
                }
                setup_VI_refresh();
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                switch (setup_VI_select)
                {
                case 0:
                    if(vi_reverse1==0) vi_reverse1=1;
                    break;
                case 1:
                    if(vi_reverse2==0) vi_reverse2=1;
                    break;
                case 2:
                    if(vi_reverse3==0) vi_reverse3=1;
                    break;
                default:
                    break;
                }
                setup_VI_refresh();
                break;
            case KEY_ENTER: //旋转编码器 按下
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_st=DISPLAY_MENU;
                write_config_in_nvs(CONFIG_OTHER);  
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(setup_VI_select>0) setup_VI_select--;
                for(i=0;i<3;i++)
                {
                    LCD_monochrome_any(&dev,230,90+i*40-5,cshou_32x24,32,24,WHITE,BLACK);
                }
                LCD_monochrome_any(&dev,230,90+setup_VI_select*40-5,shou_32x24,32,24,WHITE,BLACK);
                break;
            case KEY_F2_UP: //功能键2
                if(setup_VI_select<2) setup_VI_select++;
                for(i=0;i<3;i++)
                {
                    LCD_monochrome_any(&dev,230,90+i*40-5,cshou_32x24,32,24,WHITE,BLACK);
                }
                LCD_monochrome_any(&dev,230,90+setup_VI_select*40-5,shou_32x24,32,24,WHITE,BLACK);
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            }
        }
        if(ui_exit==1) break;
    }
}

uint16_t setup_port_select=0;
//刷新串口设置显示的数值
void setup_port_refresh()
{
    char baud_str[6]="      ";

    if(uart_config.baud_rate>=100000)
    {
        baud_str[0]=uart_config.baud_rate/100000%10+'0';
    }
    if(uart_config.baud_rate>=10000)
    {
        baud_str[1]=uart_config.baud_rate/10000%10+'0';
    }
    baud_str[2]=uart_config.baud_rate/1000%10+'0';
    baud_str[3]=uart_config.baud_rate/100%10+'0';
    baud_str[4]=uart_config.baud_rate/10%10+'0';
    baud_str[5]=uart_config.baud_rate%10+'0';
    LCD_ASCII_8X16_str(&dev,170,60,baud_str,6,YELLOW,BLACK);

    if(uart_config.data_bits==UART_DATA_5_BITS)
    {
        LCD_ASCII_8X16_str(&dev,170,90,"5",1,YELLOW,BLACK);
    }
    else if(uart_config.data_bits==UART_DATA_6_BITS)
    {
        LCD_ASCII_8X16_str(&dev,170,90,"6",1,YELLOW,BLACK);
    }
    else if(uart_config.data_bits==UART_DATA_7_BITS)
    {
        LCD_ASCII_8X16_str(&dev,170,90,"7",1,YELLOW,BLACK);
    }
    else if(uart_config.data_bits==UART_DATA_8_BITS)
    {
        LCD_ASCII_8X16_str(&dev,170,90,"8",1,YELLOW,BLACK);
    }

    if(uart_config.stop_bits==UART_STOP_BITS_1)
    {
        LCD_ASCII_8X16_str(&dev,170,120,"1  ",3,YELLOW,BLACK);
    }
    else if(uart_config.stop_bits==UART_STOP_BITS_1_5)
    {
        LCD_ASCII_8X16_str(&dev,170,120,"1.5",3,YELLOW,BLACK);
    }
    else if(uart_config.stop_bits==UART_STOP_BITS_2)
    {
        LCD_ASCII_8X16_str(&dev,170,120,"2  ",3,YELLOW,BLACK);
    }

    if(uart_config.parity==UART_PARITY_DISABLE)
    {
        LCD_ASCII_8X16_str(&dev,170,150,"None",4,YELLOW,BLACK);
    }
    else if(uart_config.parity==UART_PARITY_EVEN)
    {
        LCD_ASCII_8X16_str(&dev,170,150,"Even",4,YELLOW,BLACK);
    }
    else if(uart_config.parity==UART_PARITY_ODD)
    {
        LCD_ASCII_8X16_str(&dev,170,150,"Odd ",4,YELLOW,BLACK);
    }

    if(uart_data_hex==0)
    {
        LCD_ASCII_8X16_str(&dev,170,180,"Str",3,YELLOW,BLACK);
    }
    else
    {
        LCD_ASCII_8X16_str(&dev,170,180,"Hex",3,YELLOW,BLACK);
    }

    

}

//串口设置 面板
void display_setup_port()
{
    uint8_t evt;
    
    uint8_t i=0;
    
    lcdFillScreen(&dev, BLACK);

    //
    lcdDrawFillRect(&dev,0,33,319,36,DGRAY);

    //顶部图标和按钮
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,75,5,syx_48x16,48,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,190,5,xyx_48x16,48,16,LGRAY,0X022D);

    LCD_monochrome_any(&dev,110,60,btl_56x16,56,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,110,90,sjw_56x16,56,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,110,120,tzw_56x16,56,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,94,150,jojy_72x16,72,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,94,180,xsfs_72x16,72,16,WHITE,BLACK);

    LCD_monochrome_any(&dev,70,210,gsz_184x16,184,16,LGRAY,BLACK);

    LCD_monochrome_any(&dev,230,60+setup_port_select*30-5,shou_32x24,32,24,WHITE,BLACK);

    setup_port_refresh();

    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case KEY_ROTARY_R: //旋转编码器 右旋
                switch (setup_port_select)
                {
                case 0:
                    if(uart_config.baud_rate==1200)   uart_config.baud_rate=2400;
                    else if(uart_config.baud_rate==2400)   uart_config.baud_rate=4800;
                    else if(uart_config.baud_rate==4800)   uart_config.baud_rate=9600;
                    else if(uart_config.baud_rate==9600)   uart_config.baud_rate=14400;
                    else if(uart_config.baud_rate==14400)  uart_config.baud_rate=19200;
                    else if(uart_config.baud_rate==19200)  uart_config.baud_rate=38400;
                    else if(uart_config.baud_rate==38400)  uart_config.baud_rate=56000;
                    else if(uart_config.baud_rate==56000)  uart_config.baud_rate=115200;
                    else if(uart_config.baud_rate==115200) uart_config.baud_rate=230400;
                    else if(uart_config.baud_rate==230400) uart_config.baud_rate=256000; 
                    else if(uart_config.baud_rate==256000) uart_config.baud_rate=256000;  
                    else uart_config.baud_rate=115200;       
                    break;
                case 1:
                    if(uart_config.data_bits==UART_DATA_5_BITS)   uart_config.data_bits=UART_DATA_6_BITS;
                    else if(uart_config.data_bits==UART_DATA_6_BITS)   uart_config.data_bits=UART_DATA_7_BITS;
                    else if(uart_config.data_bits==UART_DATA_7_BITS)   uart_config.data_bits=UART_DATA_8_BITS;
                    else if(uart_config.data_bits==UART_DATA_8_BITS)   uart_config.data_bits=UART_DATA_8_BITS;
                    else uart_config.data_bits=UART_DATA_8_BITS;       
                    break;
                case 2:
                    if(uart_config.stop_bits==UART_STOP_BITS_1)   uart_config.stop_bits=UART_STOP_BITS_1_5;
                    else if(uart_config.stop_bits==UART_STOP_BITS_1_5)   uart_config.stop_bits=UART_STOP_BITS_2;
                    else if(uart_config.stop_bits==UART_STOP_BITS_2)   uart_config.stop_bits=UART_STOP_BITS_2;
                    else uart_config.stop_bits=UART_STOP_BITS_1;       
                    break;
                case 3:
                    if(uart_config.parity==UART_PARITY_DISABLE)   uart_config.parity=UART_PARITY_EVEN;
                    else if(uart_config.parity==UART_PARITY_EVEN)   uart_config.parity=UART_PARITY_ODD;
                    else if(uart_config.parity==UART_PARITY_ODD)   uart_config.parity=UART_PARITY_ODD;
                    else uart_config.parity=UART_PARITY_DISABLE;       
                    break;
                case 4:
                    if(uart_data_hex==0) uart_data_hex=1;
                    else uart_data_hex=0; 
                    break;
                
                default:
                    break;
                }
                setup_port_refresh();
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                switch (setup_port_select)
                {
                case 0:
                    if(uart_config.baud_rate==1200)   uart_config.baud_rate=1200;
                    else if(uart_config.baud_rate==2400)   uart_config.baud_rate=1200;
                    else if(uart_config.baud_rate==4800)   uart_config.baud_rate=2400;
                    else if(uart_config.baud_rate==9600)   uart_config.baud_rate=4800;
                    else if(uart_config.baud_rate==14400)  uart_config.baud_rate=9600;
                    else if(uart_config.baud_rate==19200)  uart_config.baud_rate=14400;
                    else if(uart_config.baud_rate==38400)  uart_config.baud_rate=19200;
                    else if(uart_config.baud_rate==56000)  uart_config.baud_rate=38400;
                    else if(uart_config.baud_rate==115200) uart_config.baud_rate=56000;
                    else if(uart_config.baud_rate==230400) uart_config.baud_rate=115200;   
                    else if(uart_config.baud_rate==256000) uart_config.baud_rate=230400; 
                    else uart_config.baud_rate=115200;       
                    break;
                case 1:
                    if(uart_config.data_bits==UART_DATA_5_BITS)   uart_config.data_bits=UART_DATA_5_BITS;
                    else if(uart_config.data_bits==UART_DATA_6_BITS)   uart_config.data_bits=UART_DATA_5_BITS;
                    else if(uart_config.data_bits==UART_DATA_7_BITS)   uart_config.data_bits=UART_DATA_6_BITS;
                    else if(uart_config.data_bits==UART_DATA_8_BITS)   uart_config.data_bits=UART_DATA_7_BITS;
                    else uart_config.data_bits=UART_DATA_8_BITS;       
                    break;
                 case 2:
                    if(uart_config.stop_bits==UART_STOP_BITS_1)   uart_config.stop_bits=UART_STOP_BITS_1;
                    else if(uart_config.stop_bits==UART_STOP_BITS_1_5)   uart_config.stop_bits=UART_STOP_BITS_1;
                    else if(uart_config.stop_bits==UART_STOP_BITS_2)   uart_config.stop_bits=UART_STOP_BITS_1_5;
                    else uart_config.stop_bits=UART_STOP_BITS_1;       
                    break;
                case 3:
                    if(uart_config.parity==UART_PARITY_DISABLE)   uart_config.parity=UART_PARITY_DISABLE;
                    else if(uart_config.parity==UART_PARITY_EVEN)   uart_config.parity=UART_PARITY_DISABLE;
                    else if(uart_config.parity==UART_PARITY_ODD)   uart_config.parity=UART_PARITY_EVEN;
                    else uart_config.parity=UART_PARITY_DISABLE;       
                    break;
                case 4:
                    if(uart_data_hex==0) uart_data_hex=1;
                    else uart_data_hex=0; 
                    break;
                
                default:
                    break;
                }
                setup_port_refresh();
                break;
            case KEY_ENTER: //旋转编码器 按下
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_st=DISPLAY_MENU;
                write_config_in_nvs(CONFIG_OTHER);  
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(setup_port_select>0) setup_port_select--;
                for(i=0;i<5;i++)
                {
                    LCD_monochrome_any(&dev,230,60+i*30-5,cshou_32x24,32,24,WHITE,BLACK);
                }
                LCD_monochrome_any(&dev,230,60+setup_port_select*30-5,shou_32x24,32,24,WHITE,BLACK);
                break;
            case KEY_F2_UP: //功能键2
                if(setup_port_select<4) setup_port_select++;
                for(i=0;i<5;i++)
                {
                    LCD_monochrome_any(&dev,230,60+i*30-5,cshou_32x24,32,24,WHITE,BLACK);
                }
                LCD_monochrome_any(&dev,230,60+setup_port_select*30-5,shou_32x24,32,24,WHITE,BLACK);
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            }
        }
        if(ui_exit==1) break;
    }
}

uint16_t setup_backlight_select=0;


//刷新背光设置显示的数值
void setup_backlight_refresh()
{
    char strbuf[4];  
    strbuf[0]=backlight/100%10+'0';
    strbuf[1]=backlight/10%10+'0';
    strbuf[2]=backlight%10+'0';
    LCD_ASCII_24X30_str(&dev,150,70,strbuf,3,YELLOW,BLACK);

    if(backtime==0)
    {
        LCD_monochrome_any(&dev,150,120,guan_48x30,48,30,YELLOW,BLACK);
    }
    else
    {
        strbuf[0]=backtime/10%10+'0';
        strbuf[1]=backtime%10+'0';
        LCD_ASCII_24X30_str(&dev,150,120,strbuf,2,YELLOW,BLACK);
        
    }

    if(auto_off==0)
    {
        LCD_monochrome_any(&dev,150,170,guan_48x30,48,30,YELLOW,BLACK);
    }
    else
    {
        LCD_monochrome_any(&dev,150,170,kai_48x30,48,30,YELLOW,BLACK);
    }

    set_bl(81.92*backlight,0);
    backtime_add=backtime*6;
    
}

//背光设置 面板
void display_setup_backlight()
{
    uint8_t evt;
    int i=0;
    
    lcdFillScreen(&dev, BLACK);

    //
    lcdDrawFillRect(&dev,0,33,319,36,DGRAY);

    //顶部图标和按钮
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,75,5,syx_48x16,48,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,190,5,xyx_48x16,48,16,LGRAY,0X022D);

    LCD_monochrome_any(&dev,50,70,ldtj_96x30,96,30,WHITE,BLACK);
    LCD_monochrome_any(&dev,225,70,bfh_24x30,24,30,WHITE,BLACK);

    LCD_monochrome_any(&dev,50,120,zdxp_96x30,96,30,WHITE,BLACK); 
    LCD_monochrome_any(&dev,225,120,fen_24x30,24,30,WHITE,BLACK);

    LCD_monochrome_any(&dev,50,170,xpgj_96x30,96,30,WHITE,BLACK); 
    
    

    setup_backlight_refresh();

    // if(setup_backlight_select==0)
    // {
    //     LCD_monochrome_any(&dev,258,99,shou_48x30,48,30,WHITE,BLACK);
    // }
    // else
    // {
    //     LCD_monochrome_any(&dev,258,157,shou_48x30,48,30,WHITE,BLACK);
    // }

    LCD_monochrome_any(&dev,258,70+50*setup_backlight_select+3,shou_48x30,48,30,WHITE,BLACK);

    
    
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(setup_backlight_select==0)
                {
                    if(backlight<100) backlight+=5;
                }
                else if(setup_backlight_select==1)
                {
                    if(backtime<99) backtime++;
                }
                else if(setup_backlight_select==2)
                {
                    if(auto_off==0) auto_off=1;
                    else auto_off=0;
                }
                setup_backlight_refresh();
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(setup_backlight_select==0)
                {
                    if(backlight>5) backlight-=5;
                }
                else if(setup_backlight_select==1)
                {
                    if(backtime>0) backtime--;
                }
                else if(setup_backlight_select==2)
                {
                    if(auto_off==0) auto_off=1;
                    else auto_off=0;
                }
                setup_backlight_refresh();
                break;
            case KEY_ENTER: //旋转编码器 按下

                break;
            case KEY_PWR_BACK: //返回键
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                write_config_in_nvs(CONFIG_OTHER);  
                break;
            case KEY_F1_UP: //功能键1
                if(setup_backlight_select>0) setup_backlight_select--;
                for(i=0;i<3;i++)
                {
                    LCD_monochrome_any(&dev,258,70+50*i+3,cshou_48x30,48,30,WHITE,BLACK);
                }
                LCD_monochrome_any(&dev,258,70+50*setup_backlight_select+3,shou_48x30,48,30,WHITE,BLACK);

                break;
            case KEY_F2_UP: //功能键2
                if(setup_backlight_select<2) setup_backlight_select++;
                for(i=0;i<3;i++)
                {
                    LCD_monochrome_any(&dev,258,70+50*i+3,cshou_48x30,48,30,WHITE,BLACK);
                }  
                LCD_monochrome_any(&dev,258,70+50*setup_backlight_select+3,shou_48x30,48,30,WHITE,BLACK);
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            }
        }
        if(ui_exit==1) break;
    }
}

uint16_t setup_voice_select=0;


//刷新声音设置显示的数值
void setup_voice_refresh()
{
    if(key_sound==0)
    {
        LCD_monochrome_any(&dev,193,70,guan_24x20,24,20,YELLOW,BLACK);
    }
    else
    {
        LCD_monochrome_any(&dev,193,70,kai_24x20,24,20,YELLOW,BLACK);
    }

    if(on_off_sound==0)
    {
        LCD_monochrome_any(&dev,193,120,guan_24x20,24,20,YELLOW,BLACK);
    }
    else
    {
        LCD_monochrome_any(&dev,193,120,kai_24x20,24,20,YELLOW,BLACK);
    }

    if(low_power_sound==0)
    {
        LCD_monochrome_any(&dev,193,170,guan_24x20,24,20,YELLOW,BLACK);
    }
    else
    {
        LCD_monochrome_any(&dev,193,170,kai_24x20,24,20,YELLOW,BLACK);
    }
    
}

//声音设置 面板
void display_setup_voice()
{
    uint8_t evt;
    
    uint8_t i=0;
    
    lcdFillScreen(&dev, BLACK);

    //
    lcdDrawFillRect(&dev,0,33,319,36,DGRAY);

    //顶部图标和按钮
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,75,5,syx_48x16,48,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,190,5,xyx_48x16,48,16,LGRAY,0X022D);

    LCD_monochrome_any(&dev,71,70,ajtsy_120x20,120,20,WHITE,BLACK);
    LCD_monochrome_any(&dev,50,120,kgjtsy_136x20,136,20,WHITE,BLACK); 
    LCD_monochrome_any(&dev,50,170,dldtsy_136x20,136,20,WHITE,BLACK); 

    

    setup_voice_refresh();

    LCD_monochrome_any(&dev,258,70+50*setup_voice_select+3,shou_48x30,48,30,WHITE,BLACK);
    
    
    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(setup_voice_select==0)
                {
                    if(key_sound==0) key_sound=1;
                    else key_sound=0;
                }
                else if(setup_voice_select==1)
                {
                    if(on_off_sound==0) on_off_sound=1;
                    else on_off_sound=0;
                }
                else if(setup_voice_select==2)
                {
                    if(low_power_sound==0) low_power_sound=1;
                    else low_power_sound=0;
                }
                setup_voice_refresh();
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(setup_voice_select==0)
                {
                    if(key_sound==0) key_sound=1;
                    else key_sound=0;
                }
                else if(setup_voice_select==1)
                {
                    if(on_off_sound==0) on_off_sound=1;
                    else on_off_sound=0;
                }
                else if(setup_voice_select==2)
                {
                    if(low_power_sound==0) low_power_sound=1;
                    else low_power_sound=0;
                }
                setup_voice_refresh();
                break;
            case KEY_ENTER: //旋转编码器 按下

                break;
            case KEY_PWR_BACK: //返回键
                ui_display_st=DISPLAY_MENU;
                write_config_in_nvs(CONFIG_OTHER);  
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                if(setup_voice_select>0) setup_voice_select--;
                for(i=0;i<3;i++)
                {
                    LCD_monochrome_any(&dev,258,70+50*i+3,cshou_48x30,48,30,WHITE,BLACK);
                }
                LCD_monochrome_any(&dev,258,70+50*setup_voice_select+3,shou_48x30,48,30,WHITE,BLACK);

                break;
            case KEY_F2_UP: //功能键2
                if(setup_voice_select<2) setup_voice_select++;
                for(i=0;i<3;i++)
                {
                    LCD_monochrome_any(&dev,258,70+50*i+3,cshou_48x30,48,30,WHITE,BLACK);
                }
                LCD_monochrome_any(&dev,258,70+50*setup_voice_select+3,shou_48x30,48,30,WHITE,BLACK);
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            }
        }
        if(ui_exit==1) break;
    }
}


uint8_t err_add=0;
void setup_router_refresh()
{
    LCD_ASCII_8X16_str(&dev,94,160,"                    ",20,YELLOW,BLACK);

    LCD_ASCII_8X16_str(&dev,95,90, "                           ",20,YELLOW,BLACK);
    LCD_ASCII_8X16_str(&dev,95,120,"                           ",20,YELLOW,BLACK);
    

    if(progress==PROG_START)
    {
        LCD_monochrome_any(&dev,94,160,ddqsxh_112x16,112,16,YELLOW,BLACK);

        LCD_monochrome_any(&dev,95,90,djs_64x16,64,16,WHITE,BLACK);
        LCD_monochrome_any(&dev,95,120,djs_64x16,64,16,WHITE,BLACK);
    }
    else if(progress==PROG_DATA)
    {
        LCD_monochrome_any(&dev,94,160,sjjsz_96x16,96,16,YELLOW,BLACK);

        LCD_monochrome_any(&dev,95,90,djs_64x16,64,16,WHITE,BLACK);
        LCD_monochrome_any(&dev,95,120,djs_64x16,64,16,WHITE,BLACK);
    }
    else if(progress==PROG_OVER)
    {
        LCD_monochrome_any(&dev,115,160,jswc_72x16,72,16,YELLOW,BLACK);

        LCD_ASCII_8X16_str(&dev,95,90,ls_wifi_ssid,strlen(ls_wifi_ssid),YELLOW,BLACK);
        LCD_ASCII_8X16_str(&dev,95,120,ls_wifi_pass,strlen(ls_wifi_pass),YELLOW,BLACK);
    }
    else if(progress==PROG_ERROR)
    {
        LCD_monochrome_any(&dev,94,160,jscw_136x16,136,16,RED,BLACK);

        LCD_monochrome_any(&dev,95,90,djs_64x16,64,16,WHITE,BLACK);
        LCD_monochrome_any(&dev,95,120,djs_64x16,64,16,WHITE,BLACK);
    }
    else if(progress==PROG_CONNECTED_OK)
    {
        LCD_ASCII_8X16_str(&dev,95,90,ls_wifi_ssid,strlen(ls_wifi_ssid),YELLOW,BLACK);
        LCD_ASCII_8X16_str(&dev,95,120,ls_wifi_pass,strlen(ls_wifi_pass),YELLOW,BLACK);
        LCD_monochrome_any(&dev,115,160,ljcg_64x16,64,16,YELLOW,BLACK);
        LCD_ASCII_8X16_str(&dev,135,60, "                    ",20,WHITE,BLACK);
        LCD_ASCII_8X16_str(&dev,135,60, wifi_ssid,strlen(wifi_ssid),WHITE,BLACK);
    }
    else if(progress==PROG_CONNECTED_ERR)
    {
        LCD_ASCII_8X16_str(&dev,95,90,ls_wifi_ssid,strlen(ls_wifi_ssid),YELLOW,BLACK);
        LCD_ASCII_8X16_str(&dev,95,120,ls_wifi_pass,strlen(ls_wifi_pass),YELLOW,BLACK);

        if(++err_add>2)
        {
            err_add=0;
            LCD_monochrome_any(&dev,115,160,ljsb_64x16,64,16,RED,BLACK);
        }
        else
        {
            progress=PROG_CONNECTED;
        }
        
    }
}

//路由器配置 面板
void display_setup_router()
{
    uint8_t evt;
    
    uint8_t i=0;

    
    
    
    lcdFillScreen(&dev, BLACK);

    //
    lcdDrawFillRect(&dev,0,33,319,36,DGRAY);

    //顶部图标和按钮
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,67,5,ksjs_72x16,72,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,182,5,kslj_72x16,72,16,LGRAY,0X022D);

    LCD_monochrome_any(&dev,20,60,dqwifi_112x16,112,16,WHITE,BLACK);
    LCD_ASCII_8X16_str(&dev,135,60, wifi_ssid,strlen(wifi_ssid),WHITE,BLACK);

    LCD_monochrome_any(&dev,20,90,wifimc_72x16,72,16,WHITE,BLACK);

    LCD_monochrome_any(&dev,20,120,wifimm_72x16,72,16,WHITE,BLACK);
    

    LCD_monochrome_any(&dev,20,210,qdk_144x12,144,12,LGRAY,BLACK);
    LCD_monochrome_any(&dev,165,210,azts_136x12,136,12,LGRAY,BLACK);


    for(i=0;i<32;i++)
    {
        ls_wifi_ssid[i]=0;
        ls_wifi_pass[i]=0;
    }

    progress=PROG_STOP;

    setup_router_refresh();

    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case KEY_ROTARY_R: //旋转编码器 右旋
                
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                
                break;
            case KEY_ENTER: //旋转编码器 按下
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                start_config_router();
                setup_router_refresh();
                break;
            case KEY_F2_UP: //功能键2
                if(strlen(ls_wifi_ssid)==0)
                {
                    LCD_ASCII_8X16_str(&dev,94,160,"                    ",20,YELLOW,BLACK);
                    LCD_monochrome_any(&dev,94,160,qxjs_128x16,128,16,YELLOW,BLACK);
                }
                else
                {
                    LCD_ASCII_8X16_str(&dev,94,160,"                    ",20,YELLOW,BLACK);
                    LCD_monochrome_any(&dev,94,160,kslj_80x16,80,16,YELLOW,BLACK);
                    
                    wifi_connecting_routers();
                    progress=PROG_CONNECTED;
                    err_add=0;
                }
                
                break;
            case KEY_CONFIG: //配网
                setup_router_refresh();
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            }
        }
        if(ui_exit==1) break;
    }
}

uint16_t setup_net_select=0;

//刷新网络设置显示的数值
void setup_net_refresh()
{
    char up_str[3];

    if(wifi_on==0)
    {
        LCD_monochrome_any(&dev,165,90,guanbi_32x16,32,16,YELLOW,BLACK);
        net_state=0;
    }
    else 
    {
        LCD_monochrome_any(&dev,165,90,kaiqi_32x16,32,16,YELLOW,BLACK);
    }
    


    if(net_state==0)
    {
        LCD_monochrome_any(&dev,165,150,wlj_48x16,48,16,WHITE,BLACK);
        LCD_monochrome_any(&dev,165,180,wlj_48x16,48,16,WHITE,BLACK);
    }
    else if(net_state==1)
    {
        LCD_monochrome_any(&dev,165,150,ylj_48x16,48,16,WHITE,BLACK);
        LCD_monochrome_any(&dev,165,180,wlj_48x16,48,16,WHITE,BLACK);
    }
    else if(net_state==2)
    {
        LCD_monochrome_any(&dev,165,150,ylj_48x16,48,16,WHITE,BLACK);
        LCD_monochrome_any(&dev,165,180,ylj_48x16,48,16,WHITE,BLACK);
    }  

}

//网络设置 面板
void display_setup_net()
{
    uint8_t evt;
    
    uint8_t i=0;
    
    lcdFillScreen(&dev, BLACK);

    //
    lcdDrawFillRect(&dev,0,33,319,36,DGRAY);

    //顶部图标和按钮
    LCD_monochrome_any(&dev,55,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,75,5,syx_48x16,48,16,LGRAY,0X022D);    
    LCD_monochrome_any(&dev,170,1,butten_bl_96x26,96,26,BLACK,0X022D);
    LCD_monochrome_any(&dev,190,5,xyx_48x16,48,16,LGRAY,0X022D);


    LCD_monochrome_any(&dev,85,90,wifikg_80x16,80,16,WHITE,BLACK);
    //LCD_monochrome_any(&dev,56,120,sjscjg_104x16,104,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,40,150,ylyqdlj_120x16,120,16,WHITE,BLACK);
    LCD_monochrome_any(&dev,40,180,yfwqdlj_120x16,120,16,WHITE,BLACK);


    LCD_monochrome_any(&dev,230,90+setup_net_select*30-5,shou_32x24,32,24,WHITE,BLACK);

    setup_net_refresh();

    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case KEY_ROTARY_R: //旋转编码器 右旋
                if(wifi_on==0)
                {
                    wifi_on=1; 
                    net_state=0; 
                    ESP_ERROR_CHECK( esp_wifi_connect() ); 
                }

                setup_net_refresh();
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                if(wifi_on!=0)
                {  
                    wifi_on=0;
                    ESP_ERROR_CHECK( esp_wifi_disconnect() );
                }
                setup_net_refresh();
                break;
            case KEY_ENTER: //旋转编码器 按下
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_st=DISPLAY_MENU;
                write_config_in_nvs(CONFIG_OTHER); 
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
                // if(setup_net_select>0) setup_net_select--;
                // for(i=0;i<2;i++)
                // {
                //     LCD_monochrome_any(&dev,230,90+i*30-5,cshou_32x24,32,24,WHITE,BLACK);
                // }
                // LCD_monochrome_any(&dev,230,90+setup_net_select*30-5,shou_32x24,32,24,WHITE,BLACK);
                break;
            case KEY_F2_UP: //功能键2
                // if(setup_net_select<1) setup_net_select++;
                // for(i=0;i<2;i++)
                // {
                //     LCD_monochrome_any(&dev,230,90+i*30-5,cshou_32x24,32,24,WHITE,BLACK);
                // }
                // LCD_monochrome_any(&dev,230,90+setup_net_select*30-5,shou_32x24,32,24,WHITE,BLACK);
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            case REFRESH_TOP_ICON: //刷新顶部图标
                setup_net_refresh();
                break;
            }
        }
        if(ui_exit==1) break;
    }
}


void displayQRCode(int side, uint8_t *bitdata,int deviation_x,int deviation_y,int prescaler)
{
	//int deviation_x=185;
    //int deviation_y=70;
	int i=0;
	int j=0;
	int a=0;
//	int l=0;
//	int n=0;
	int OUT_FILE_PIXEL_PRESCALER=prescaler;

		for (i = 0; i < side; i++) {

			for (j = 0; j < side; j++) {
				a = j * side + i;

				if ((bitdata[a / 8] & (1 << (7 - a % 8))))
				{
					// for (l = 0; l < OUT_FILE_PIXEL_PRESCALER; l++)
					// {
					// 	for (n = 0; n < OUT_FILE_PIXEL_PRESCALER; n++)
					// 	{
                    //         lcdDrawPixel(&dev,OUT_FILE_PIXEL_PRESCALER*i+l+deviation_x,OUT_FILE_PIXEL_PRESCALER*(j)+n+deviation_y,WHITE);
					// 	}
					// }
                    lcdDrawFillRect(&dev,OUT_FILE_PIXEL_PRESCALER*i+deviation_x,OUT_FILE_PIXEL_PRESCALER*j+deviation_y,OUT_FILE_PIXEL_PRESCALER*i+OUT_FILE_PIXEL_PRESCALER+deviation_x-1,OUT_FILE_PIXEL_PRESCALER*j+OUT_FILE_PIXEL_PRESCALER+deviation_y-1,WHITE);
				}
			}
		}

}
void QRGenerator(char *input,int deviation_x,int deviation_y,int prescaler)
{
	int side;
	uint8_t bitdata[QR_MAX_BITDATA];

	// remove newline
	if (input[strlen(input) - 1] == '\n')
	{
		input[strlen(input) - 1] = 0;
	}

	side = qr_encode(QR_LEVEL_M, 0, input, 0, bitdata);
   // ESP_LOGI(LCD_UI_TAG,"side=%d\r\n",side);
   // ESP_LOG_BUFFER_HEX(LCD_UI_TAG, bitdata, 10); 
	displayQRCode(side, bitdata,deviation_x,deviation_y,prescaler);
    //LCD_monochrome_any(&dev,50,50,bitdata,25,25,WHITE,BLACK);
}

//设备信息 面板
void display_setup_info()
{
    uint8_t evt;
    
    
    lcdFillScreen(&dev, BLACK);
    LCD_ShowPicture(&dev,2,70,150,47,gImage_logo);

    LCD_ASCII_8X16_str(&dev,15,140,"ID:",3,WHITE,BLACK);
    LCD_ASCII_8X16_str(&dev,40,140,device_ID,10,WHITE,BLACK);

    LCD_ASCII_8X16_str(&dev,15,160,"Version:",8,WHITE,BLACK);
    LCD_ASCII_8X16_str(&dev,88,160,ver,2,WHITE,BLACK);

    QRGenerator(QRcode,170,40,3);
    LCD_monochrome_any(&dev,190,205,sbewm_80x16,80,16,WHITE,BLACK);


    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case KEY_ROTARY_R: //旋转编码器 右旋
                
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                
                break;
            case KEY_ENTER: //旋转编码器 按下
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
              
                break;
            case KEY_F2_UP: //功能键2
           
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            }
        }
        if(ui_exit==1) break;
    }
}


//使用说明
void display_instructions()
{
    uint8_t evt;
    
    lcdFillScreen(&dev, BLACK);
    LCD_monochrome_any(&dev,88,20,wdzx_128x16,128,16,WHITE,BLACK);
    QRGenerator("http://www.iot7.cn/wiki/#/w2a-sysm",80,60,6);

    while(1)
    {
        if(xQueueReceive(gui_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
            case KEY_ROTARY_R: //旋转编码器 右旋
                
                break;
            case KEY_ROTARY_L: //旋转编码器 左旋
                
                break;
            case KEY_ENTER: //旋转编码器 按下
                break;
            case KEY_PWR_BACK: //返回键
                ui_display_st=DISPLAY_MENU;
                ui_exit=1;
                break;
            case KEY_F1_UP: //功能键1
              
                break;
            case KEY_F2_UP: //功能键2
           
                break;
            case KEY_PWROFF: //关机
                ui_display_st=DISPLAY_PWROFF;
                 ui_exit=1;
                break;
            }
        }
        if(ui_exit==1) break;
    }
}




//关机 面板
void display_pwroff()
{
    uint8_t evt;
    evt=WIFINET_MQTTSTOP;
    xQueueSendFromISR(wifinet_evt_queue, &evt, NULL);
    lcdFillScreen(&dev, BLACK);

    LCD_ShowPicture(&dev,80,50,150,47,gImage_logo);

    LCD_monochrome_any(&dev,75,150,ygj_168x20,168,20,WHITE,BLACK);
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
        gpio_set_level(PWR_EN, 0);
    }

}


//定时器结构体初始化
esp_timer_create_args_t esp_timer_create_args_t2 = {
    .callback = &esp_timer_s_cb, //定时器回调函数
    .arg = NULL, //传递给回调函数的参数
    .name = "esp_timer_s" //定时器名称
};


//定时器结构体初始化
esp_timer_create_args_t esp_timer_create_args_screenoff = {
    .callback = &esp_timer_screenoff_cb, //定时器回调函数
    .arg = NULL, //传递给回调函数的参数
    .name = "esp_timer_screenoff" //定时器名称
};


uint8_t fz=0;
void lcd_ui_task(void *arg)
{
   int i=0;
   uint8_t evt;
   gui_evt_queue = xQueueCreate(5, sizeof(uint8_t));

           /*创建定时器*/       
    esp_timer_create(&esp_timer_create_args_t2, &esp_timer_handle_t2);
    esp_timer_create(&esp_timer_create_args_screenoff, &esp_timer_handle_screenoff);

    esp_timer_start_periodic(esp_timer_handle_screenoff, 10000 * 1000);


   spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO);
	lcdInit(&dev, CONFIG_WIDTH, CONFIG_HEIGHT);
	lcdFillScreen(&dev, BLACK);
    bl_pwm_init();
    esp_timer_create(&config_router_periodic_arg, &config_router_timer_handle);
	//esp_timer_start_periodic(config_router_timer_handle, 10 * 1000);

    LCD_monochrome_any(&dev,120,150,qdz_96x24,96,24,WHITE,BLACK);
    lcdDrawRect(&dev,50,70,270,100,WHITE);
    for(i=0;i<6;i++)
    {
        lcdDrawFillRect(&dev,50,70,50+i*44,100,WHITE);
        vTaskDelay(pdMS_TO_TICKS(150));
    }  

  
    set_bl(81.92*backlight,0);
    backtime_add=backtime*6;
   while (1) {
        ui_exit=0;
       switch (ui_display_st)
       {
        case DISPLAY_DCV:
           current_fun=FUN_DCV;

           evt=SD_SWFUN;
           xQueueSendFromISR(sd_evt_queue, &evt, NULL); 

           analog_switch(ASW_DCV1);
           display_dcv(current_fun);       
           break;
        case DISPLAY_ACV:
           current_fun=FUN_ACV;

           evt=SD_SWFUN;
           xQueueSendFromISR(sd_evt_queue, &evt, NULL); 

           analog_switch(ASW_ACV0);
           display_acv(current_fun);     
           break;
        case DISPLAY_DCA:
           current_fun=FUN_DCA;
           
           evt=SD_SWFUN;
           xQueueSendFromISR(sd_evt_queue, &evt, NULL); 
           
           if(choice_dca==CUR_UA) //uA 档位 最大5000uA
            {
                analog_switch(ASW_DCUA);   
            }
            else if(choice_dca==CUR_MA)//mA 档位 最大 500mA
            {
                analog_switch(ASW_DCMA);
            }
            else if(choice_dca==CUR_A)//A 档位
            {
                analog_switch(ASW_DCA);
            }
           
           display_dca(current_fun);      
           break;
        case DISPLAY_ACA:
           current_fun=FUN_ACA;
           
           evt=SD_SWFUN;
           xQueueSendFromISR(sd_evt_queue, &evt, NULL); 
           
            if(choice_aca==CUR_MA)//mA 档位 最大 500mA
            {
                analog_switch(ASW_ACMA);
            }
            else if(choice_aca==CUR_A)//A 档位
            {
                analog_switch(ASW_ACA);
            }
           display_aca(current_fun);
           break;
        case DISPLAY_2WR:
           current_fun=FUN_2WR;
           
           evt=SD_SWFUN;
           xQueueSendFromISR(sd_evt_queue, &evt, NULL); 
           
           analog_switch(ASW_2WR2);
           display_2wr(current_fun);
           break;
        case DISPLAY_4WR:
           current_fun=FUN_4WR;
           
           evt=SD_SWFUN;
           xQueueSendFromISR(sd_evt_queue, &evt, NULL); 
           
           analog_switch(ASW_4WHS);
           display_4wr(current_fun);
           break;
        case DISPLAY_DIODE:
           current_fun=FUN_DIODE;
           
           evt=SD_SWFUN;
           xQueueSendFromISR(sd_evt_queue, &evt, NULL); 
           
           analog_switch(ASW_DIODE);
           display_diode(current_fun);
           break;
        case DISPLAY_BEEP:
           current_fun=FUN_BEEP;
           
           evt=SD_SWFUN;
           xQueueSendFromISR(sd_evt_queue, &evt, NULL); 
           
           analog_switch(ASW_BEEP);
           display_beep(current_fun);
           break; 
        case DISPLAY_DCW:
           current_fun=FUN_DCW;
           
           evt=SD_SWFUN;
           xQueueSendFromISR(sd_evt_queue, &evt, NULL); 
           
            switch (choice_wdc)
            {
            case CUR_UA:
                analog_switch(ASW_DCW_UAV0);
                break;
            case CUR_MA:
                analog_switch(ASW_DCW_MAV0);
                break;
            case CUR_A:
                analog_switch(ASW_DCW_AV0);
                break; 
            default:
                break;
            }
           display_dcw(current_fun);
           break; 
        case DISPLAY_ACW:
           current_fun=FUN_ACW;
           
           evt=SD_SWFUN;
           xQueueSendFromISR(sd_evt_queue, &evt, NULL); 
           
           switch (choice_wac)
            {
            case CUR_MA:
                analog_switch(ASW_ACW_MAV0);
                break;
            case CUR_A:
                analog_switch(ASW_ACW_AV0);
                break; 
            default:
                break;
            }
           display_acw(current_fun);
           break; 
        case DISPLAY_TEMP:
           current_fun=FUN_TEMP;
           
           evt=SD_SWFUN;
           xQueueSendFromISR(sd_evt_queue, &evt, NULL); 
           
           analog_switch(ASW_2WR0);
           display_temp(current_fun);       
           break;  
        case DISPLAY_VTOA:
           current_fun=FUN_VTOA;
           display_V_A();       
           break; 
        case DISPLAY_UART:
           current_fun=FUN_UART;
           
           evt=SD_SWFUN;
           xQueueSendFromISR(sd_evt_queue, &evt, NULL); 
           pwm_ledc_off();
           usart1_init();
           analog_switch(ASW_OFF);
           display_uart_485(current_fun);       
           break; 
        case DISPLAY_RS485:
           current_fun=FUN_RS485;
           
           evt=SD_SWFUN;
           xQueueSendFromISR(sd_evt_queue, &evt, NULL); 
           pwm_ledc_off();
           usart1_init();
           analog_switch(ASW_OFF);
           display_uart_485(current_fun);       
           break; 
        case DISPLAY_CAN: 
            current_fun=FUN_CAN;
            evt=SD_SWFUN;
            xQueueSendFromISR(sd_evt_queue, &evt, NULL); 
            analog_switch(ASW_OFF);
            xTaskCreate(canbus_task, "CANBUS", 1024*6, NULL, 5, &CAN_Handle_task);
            display_can();
            can_close=1;   
           break; 
        case DISPLAY_SIGNAL:
           current_fun=FUN_SIGNAL;
           analog_switch(ASW_OFF);
           display_signal();       
           break; 
        case DISPLAY_PWM:
           current_fun=FUN_PWM;
           pwm_ledc_init();
           analog_switch(ASW_OFF);
           display_pwm();       
           break; 
        case DISPLAY_SERVO:
           current_fun=FUN_SERVO;
           pwm_ledc_init();
           analog_switch(ASW_OFF);
           display_servo();       
           break;
        case DISPLAY_FUNCHOICE: //功能选择
           display_funchoice();
           break;
        case DISPLAY_MENU://菜单
           display_menu();
           break; 
        case DISPLAY_SETUP_RANGE://量程设置
           display_setup_range();
           break; 
        case DISPLAY_SETUP_SAVE: //存储设置
           display_setup_save();
           break;       
        case DISPLAY_SETUP_BUZZER://蜂鸣器阈值
           display_setup_buzzer();
           break; 
        case DISPLAY_SETUP_TEMP://温度传感器类型
           display_setup_temp();
           break; 
        case DISPLAY_SETUP_V_I://V/I转换设置
           display_setup_V_I();
           break;
        case DISPLAY_SETUP_PORT://串口设置
           display_setup_port();
           break; 
        case DISPLAY_SETUP_BACKLIGHT://背光设置
           display_setup_backlight();
           break; 
        case DISPLAY_SETUP_VOICE://声音开关
           display_setup_voice();
           break; 
        case DISPLAY_SETUP_ROUTER://路由器配置
           display_setup_router();
           break; 
        case DISPLAY_SETUP_NET: //网络设置
           display_setup_net();
           break; 
        case DISPLAY_SETUP_INFO://设备信息
           display_setup_info();
           break; 
        case DISPLAY_INSTRUCTIONS://使用说明
           display_instructions();
           break; 
        case DISPLAY_PWROFF://关机
           display_pwroff();
           break;
       default:
           break;
       }
   }

}

