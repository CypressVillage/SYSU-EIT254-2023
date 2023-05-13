#ifndef _LCD_UI_H
#define _LCD_UI_H


typedef struct {
	unsigned char const *text_p[20];//指向每个菜单项的字模数组
    uint16_t width[20];       //字模位图宽度
    uint16_t height[20];      //字模位图高度
    uint16_t textColor[20];   //文字颜色
    uint16_t backColor[20];   //背景颜色
    uint8_t  select;          //当前选中的位置
    uint8_t  start;           //屏幕显示的菜单起始位置
    uint8_t  end;            //屏幕显示的菜单结束位置
} MENU_ST;

typedef struct {
	unsigned char const *text_p[30];//指向每个图标底部字模数组
    unsigned char const *icon_p[30];//指向每个图标取模数组
    uint8_t  page;           //页，每页1-8个图标
    uint8_t  icon_total;   //图标总数
    uint8_t  select;         //当前选中的位置
} FUNCHOICE_ST;

extern uint8_t shortcut_F1;//功能选择页面 快捷键1
extern uint8_t shortcut_F2;//功能选择页面 快捷键2

/* 波形图 Y轴比例 */
enum y_scale
{
    SCALE_X0_001=0, //Y轴 乘以0.001
    SCALE_X0_01,    //Y轴 乘以0.01
    SCALE_X0_1,    
    SCALE_X1,
    SCALE_X10,
    SCALE_X100,
    SCALE_X1000,
    SCALE_X10000,
    

};

/* 当前功能 */
enum function
{
    FUN_DCV=0,  // 直流电压
    FUN_ACV,    // 交流电压
    FUN_DCA,    // 直流电流
    FUN_ACA,    // 交流电流
    FUN_2WR,    // 两线电阻
    FUN_4WR,    // 四线低阻
    FUN_DIODE,  // 二极管
    FUN_BEEP,   // 通断档
    FUN_DCW,    // 直流功率
    FUN_ACW,    // 交流功率
    FUN_TEMP,   // 测量温度
    FUN_VTOA,   // 电压电流转换
    FUN_UART,   // UART调试
    FUN_RS485,  // RS485调试 
    FUN_CAN,    // CAN调试 
    FUN_SIGNAL, // 直流信号源
    FUN_PWM,    // PWM信号源
    FUN_SERVO,   //舵机控制
    FUN_NULL=0xFF, 
};

/* 当前显示的页面 */
enum display
{
    DISPLAY_DCV=0,  // 直流电压 面板
    DISPLAY_ACV,    // 交流电压 面板
    DISPLAY_DCA,    // 直流电流 面板
    DISPLAY_ACA,    // 交流电流 面板
    DISPLAY_2WR,    // 两线电阻 面板
    DISPLAY_4WR,    // 四线低阻 面板
    DISPLAY_DIODE,    // 二极管 面板
    DISPLAY_BEEP,    // 通断档 面板
    DISPLAY_DCW,    //直流功率 面板
    DISPLAY_ACW,    //交流功率 面板
    DISPLAY_TEMP,    //测量温度 面板
    DISPLAY_VTOA,   //电压电流转换 面板
    DISPLAY_UART,    //UART调试 面板
    DISPLAY_RS485,    //RS485调试  面板
    DISPLAY_CAN,    //CAN调试  面板
    DISPLAY_SIGNAL,  //直流信号源 面板
    DISPLAY_PWM,      //PWM信号源 面板
    DISPLAY_SERVO,     //舵机控制 面板
    DISPLAY_FUNCHOICE,//功能选择
    DISPLAY_MENU,   // 设置菜单
    DISPLAY_SETUP_RANGE,    //量程设置
    DISPLAY_SETUP_SAVE,     //存储设置
    DISPLAY_SETUP_BUZZER,     //蜂鸣器阈值
    DISPLAY_SETUP_TEMP,     //温度传感器类型
    DISPLAY_SETUP_V_I,     //V/I转换设置
    DISPLAY_SETUP_PORT,     //串口参数设置
    DISPLAY_SETUP_BACKLIGHT, //背光设置
    DISPLAY_SETUP_VOICE,     //声音开关
    DISPLAY_SETUP_ROUTER,     //路由器配置
    DISPLAY_SETUP_NET,      //网络设置
    DISPLAY_SETUP_INFO,     //设备信息
    DISPLAY_INSTRUCTIONS,  //使用说明
    DISPLAY_PWROFF,   //关机
};

/* 刷新页面 */
enum refresh
{
    REFRESH_VALUE=0x20,  // 刷新测量值
    REFRESH_LEFT_ICON=0x21,  // 刷新测量页面左侧图标
    REFRESH_TOP_ICON=0x22,  // 刷新顶部图标
    REFRESH_CAN=0x23,  // 刷新CAN页面参数
};

extern uint8_t ui_display_st; //当前显示的页面 
extern uint8_t ui_exit;

extern uint16_t current_freq;//测量频率 0.1秒
extern uint8_t current_fun; //当前功能  
extern uint8_t measure_stop;//测量停止

extern uint8_t backlight; //背光亮度 5-100%
extern uint8_t backtime;   //自动息屏时间 1-99分钟
extern uint16_t backtime_add; //息屏时间，单位 10秒
extern uint8_t auto_off;      //息屏自动关机

extern uint8_t key_sound;      //按键声音开关
extern uint8_t on_off_sound;   //开关机声音开关
extern uint8_t low_power_sound; //低电量声音开关

extern uint8_t wifi_on;        //wifi开关
extern uint8_t net_state;   //网络状态， 0 路由器未连接，1 路由器已连接，2 服务器已连接

extern uint8_t setup_VI_select;//为0 是0-5V  为1是1-5V   为2是0-10V
extern uint8_t vi_reverse1; //为0是电压转电流，为1是电流转电压  0-5V
extern uint8_t vi_reverse2; //为0是电压转电流，为1是电流转电压  1-5V 
extern uint8_t vi_reverse3; //为0是电压转电流，为1是电流转电压  0-10V



extern xQueueHandle gui_evt_queue; 

void lcd_ui_task(void *arg);
void uart_485_clean_rx();
void can_clean_rx();


void set_bl(uint16_t duty,int time_ms);
#endif
