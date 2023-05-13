#ifndef _KEY_H
#define _KEY_H

/* 档位 */
enum key
{
    KEY_ROTARY_R = 0,//旋转编码器 右旋
    KEY_ROTARY_L,    //旋转编码器 左旋
    KEY_ENTER,        //旋转编码器 按下
    KEY_PWR_BACK,    //开关机键  返回键
    KEY_F1_UP,       //功能键1短按抬起
    KEY_F1_DOWN,     //功能键1按下
    KEY_F1_LONG,     //长按功能键1
    KEY_F2_UP,       //功能键2短按抬起
    KEY_F2_DOWN,     //功能键2按下
    KEY_F2_LONG,     //长按功能键2
    KEY_CONFIG,
    KEY_PWROFF,
};

void key_task(void *arg);
void beep_start(uint8_t duration);
uint8_t screen_status();
#endif
