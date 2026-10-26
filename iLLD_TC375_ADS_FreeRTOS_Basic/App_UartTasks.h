#ifndef APP_UART_TASKS_H_
#define APP_UART_TASKS_H_

#include "Ifx_Types.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

/*-------------------------------------------------- Config -----------------------------------------------------*/
#define UART_RX_RINGBUF_SIZE    128u
#define UART_TX_QUEUE_LENGTH    128u
#define UART_BAUDRATE           115200

/*-------------------------------------------------- Handles ----------------------------------------------------*/
extern SemaphoreHandle_t g_uartRxSem;
extern QueueHandle_t     g_uartTxQueue;

/*-------------------------------------------------- API ---------------------------------------------------------*/
void    init_uart(void);                 /* hardverski init ASCLIN + interrupt enable, poziva se iz init_drivers() */
void    task_uart_rx(void *arg);
void    task_uart_tx(void *arg);

void    uart_send_char(uint8_t ch);      /* non-blocking-ish, zove se iz bilo kog taska (i iz printf retargeta)   */
uint8_t uart_receive_char(void);         /* blokira TASK dok ne stigne karakter (zove se iz scanf retargeta)      */

/* Novo - helper funkcije za formatiran ispis bez printf-a */
void    uart_send_string(const char *str);
void    uart_send_uint(uint32 value);
void    uart_send_uint_padded(uint32 value, uint8 width);
void uart_process_command(const char *cmd);

#endif /* APP_UART_TASKS_H_ */
