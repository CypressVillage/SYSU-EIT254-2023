#ifndef _CANBUS_H
#define _CANBUS_H

/* --------------------- Definitions and static variables ------------------ */
//Example Configuration
#define NO_OF_ITERS                     3
#define RX_TASK_PRIO                    9

#define ID_MASTER_STOP_CMD              0x0A0
#define ID_MASTER_START_CMD             0x0A1
#define ID_MASTER_PING                  0x0A2
#define ID_SLAVE_STOP_RESP              0x0B0
#define ID_SLAVE_DATA                   0x0B1
#define ID_SLAVE_PING_RESP              0x0B2

extern uint8_t can_close;
extern uint8_t can_baud;
extern uint8_t can_stop;
extern uint8_t can_filter;
extern uint8_t can_extd;   //过滤标准帧/扩展帧
extern uint8_t can_rtr;    //过滤数据帧/远程帧
extern uint32_t can_filter_id;
extern uint32_t can_filter_mask;

extern uint32_t can_rx_len;//接收的包数
extern uint32_t can_tx_len;//发送的包数

typedef struct {
	uint8_t can_data[8];
    uint8_t can_extd; //标准帧/扩展帧
    uint8_t can_rtr; //数据帧/远程帧
    uint32_t identifier; //帧ID
} CAN_DATA;

extern CAN_DATA can_rx_data;


/* CAN 波特率 */
enum TWAI_BAUD
{
    TWAI_16KBITS= 4,
    TWAI_20KBITS,
    TWAI_25KBITS,
    TWAI_50KBITS,
    TWAI_100KBITS,
    TWAI_125KBITS,
    TWAI_250KBITS,
    TWAI_500KBITS,
    TWAI_800KBITS,
    TWAI_1MBITS ,
};



extern TaskHandle_t CAN_Handle_task;

void twai_tx_data(uint32_t identifier,uint8_t extd,uint8_t rtr,uint8_t *data);
void twai_set_baud(uint8_t baud);
void twai_set_filter(uint8_t filter_on,uint8_t extd,uint32_t code,uint32_t mask);

void canbus_task(void *arg);
#endif
