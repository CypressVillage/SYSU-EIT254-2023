#ifndef _SWITCH_FUN_H
#define _SWITCH_FUN_H

//对外功能接口
#define SW_FUN_USART_PWM	0x00
#define SW_FUN_CAN 	0x01
#define SW_FUN_RS485	0x02

/* 档位 */
enum asw
{
    ASW_OFF  = 0,
    ASW_CIR, //分压电阻短路，调零
    ASW_DCV0,
    ASW_DCV1,
    ASW_DCV2,
    ASW_ACV0,
    ASW_ACV1,
    ASW_ACV2,
    ASW_DCUA,
    ASW_DCMA,
    ASW_DCA,
    ASW_ACMA,
    ASW_ACA,
    ASW_2WR0,
    ASW_2WR1,
    ASW_2WR2,
    ASW_2WR3,
    ASW_4WHS,
    ASW_4WLS,
    ASW_DIODE,
    ASW_BEEP,
    ASW_DCW_UA,
    ASW_DCW_UAV0,
    ASW_DCW_UAV1,
    ASW_DCW_UAV2,
    ASW_DCW_MA,
    ASW_DCW_MAV0,
    ASW_DCW_MAV1,
    ASW_DCW_MAV2,
    ASW_DCW_A,
    ASW_DCW_AV0,
    ASW_DCW_AV1,
    ASW_DCW_AV2,
    ASW_ACW_UA,
    ASW_ACW_UAV0,
    ASW_ACW_UAV1,
    ASW_ACW_UAV2,
    ASW_ACW_MA,
    ASW_ACW_MAV0,
    ASW_ACW_MAV1,
    ASW_ACW_MAV2,
    ASW_ACW_A,
    ASW_ACW_AV0,
    ASW_ACW_AV1,
    ASW_ACW_AV2,
};


/* 电压档 */
enum voltage
{
    VOL_AUTO= 0,
    VOL_1000V,
    VOL_100V,
    VOL_10V,   
};

/* 电流档 */
enum current
{
    CUR_UA  = 0,
    CUR_MA,
    CUR_A,
};

/* 电阻档 */
enum resistance
{
    RES_AUTO= 0,
    RES_100M, 
    RES_500K,
    RES_20K,  
    RES_1K,   
};

/*温度档 */
enum temp
{
    TEMP_PT100  = 0,
    TEMP_PT200,
    TEMP_PT1000,
    TEMP_NTC_103KF_3950,
    TEMP_NTC_103KF_3435,
};


extern uint8_t choice_2wr; 
extern uint8_t choice_2wr_old; 
extern uint8_t choice_dcv;
extern uint8_t choice_dcv_old;
extern uint8_t choice_acv;
extern uint8_t choice_acv_old;
extern uint8_t choice_dca;
extern uint8_t choice_dca_old;
extern uint8_t choice_wdc;
extern uint8_t choice_wdc_old;
extern uint8_t choice_aca;
extern uint8_t choice_aca_old;
extern uint8_t choice_wac;
extern uint8_t choice_wac_old;
extern uint8_t choice_temp; 
extern uint8_t choice_temp_old;

extern uint8_t gear_sw;

void analog_switch(uint8_t sw);

void switch_fun_task(void *arg);
#endif
