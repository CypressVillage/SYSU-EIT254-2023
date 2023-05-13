#ifndef _USART_485_H
#define _USART_485_H

#define UDATA_MAX 100
extern char uart_data[UDATA_MAX][39];
extern uint16_t uart_data_len;
extern uint16_t uart_data_rows;
extern uint16_t uart_data_start;
extern uint16_t uart_data_end;
extern uint8_t  uart_data_hex;
extern uint8_t  uart_data_stop;

extern uart_config_t uart_config;

extern xQueueHandle usart_evt_queue;

extern uint8_t sd_uart_data[1024];
extern uint16_t sd_uart_data_len;

extern uint8_t tx_uart_data[1024];
extern uint16_t tx_uart_data_len;

extern uint8_t rx_uart_data[1024];
extern uint16_t rx_uart_data_len;

enum UART_485
{
    UART_TX_DATA = 0, //发送UART数据
};


void usart0_init();
void usart1_init();

void usart_485_task(void *arg);
void usart0_task(void *arg);
#endif
