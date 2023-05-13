#ifndef _ADC_DAC_H
#define _ADC_DAC_H



#define LTC_SDA_OUT _TRISF4 = 0;
#define LTC_SDA_IN _TRISF4 = 1;
#define LTC_SDA _RF4
#define LTC_SCL _RF5
#define LTC_SCL_DIR _TRISF5

//#define RTD_POWER_ON _RF3 = 0
//#define RTD_POWER_OFF _RF3 = 1
//#define RTD_POWER_DIR _TRISF3


//#define READCommand 0x2B
//#define WRITECommand 0x2A

#define READCommand 0x2B
#define WRITECommand 0x2A

// Select gain - 1 to 256 (also depends on speed setting)
// Does NOT apply to LTC2485.
#define GAIN1 0b00000000 // G = 1 (SPD = 0), G = 1 (SPD = 1)
#define GAIN2 0b00100000 // G = 4 (SPD = 0), G = 2 (SPD = 1)
#define GAIN3 0b01000000 // G = 8 (SPD = 0), G = 4 (SPD = 1)
#define GAIN4 0b01100000 // G = 16 (SPD = 0), G = 8 (SPD = 1)
#define GAIN5 0b10000000 // G = 32 (SPD = 0), G = 16 (SPD = 1)
#define GAIN6 0b10100000 // G = 64 (SPD = 0), G = 32 (SPD = 1)
#define GAIN7 0b11000000 // G = 128 (SPD = 0), G = 64 (SPD = 1)
#define GAIN8 0b11100000 // G = 256 (SPD = 0), G = 128 (SPD = 1)
// Select ADC source - differential input or PTAT circuit
#define VIN 0b00000000
#define PTAT 0b00001000
// Select rejection frequency - 50, 55, or 60Hz
#define R50 0b00000010
#define R55 0b00000000
#define R60 0b00000100
#define R2X 0b00000001
// Select speed mode
#define SLOW 0b00000000 // slow output rate with autozero
#define FAST 0b00000001 // fast output rate with no autozero

#define A 1.12255E-03
#define B 2.35156E-04
#define C 8.34823E-08

#define RefRES 249000.0


void GP8403_init();
void GP8403_WriteReg();


void adc_task(void *arg);


typedef struct
{
	float B0;		
	float B1;
	float B2;
	float B3;
}STRU_BPars;

extern STRU_BPars *pBParsN;	
extern STRU_BPars *pBParsP;	

extern uint32_t measured_value; //测量值 全F为超量程
extern uint32_t carried_value1; //携带的值1，功率档时为测量电压
extern uint32_t carried_value2; //携带的值2，功率档时为测量电流
extern uint8_t carried_value2_sign ; 
extern uint8_t sign; //正负号 为1负数
extern uint8_t unit; //测量单位

extern uint16_t signal_V; //直流电压源 0-100
extern uint16_t signal_A; //直流电压源 0-300

uint8_t de_dcout; //标定直流源
uint16_t Slope_dcvout;//直流电压源斜率校准数值
uint16_t Slope_dcaout;//直流电压源斜率校准数值

extern uint8_t r_buzzer_threshold;//通断档蜂鸣器阈值 单位R  范围1-100
extern uint8_t d_buzzer_threshold;//通断档蜂鸣器阈值 单位0.01V 范围0.01V-1V

extern struct tm ADC_timeinfo;

extern uint8_t electricity_st; //电池电量：0为电量低，255为正常，254为充电状态，1-100为百分比

void set_current_freq();
float read_adc_uv(uint8_t filter);

#endif
