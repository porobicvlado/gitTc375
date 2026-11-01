#ifndef APP_UART_TASKS_H_
#define APP_UART_TASKS_H_

#include "Ifx_Types.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

/*-------------------------------------------------- Config -----------------------------------------------------*/
#define UART_RX_RINGBUF_SIZE    128u
#define UART_BAUDRATE           115200

#define UART_TX_MSG_MAX_LEN     64u
#define UART_TX_QUEUE_LENGTH    8u

/*-------------------------------------------------- Types -------------------------------------------------------*/
typedef struct
{
    uint16_t len;
    char     data[UART_TX_MSG_MAX_LEN];
} UartTxMsg_t;

/*-------------------------------------------------- Handles ----------------------------------------------------*/
extern SemaphoreHandle_t g_uartRxSem;
extern QueueHandle_t     g_uartTxQueue;

/*-------------------------------------------------- API ---------------------------------------------------------*/
void    init_uart(void);
void    task_uart_rx(void *arg);
void    task_uart_tx(void *arg);

void    uart_send_char(uint8_t ch);
uint8_t uart_receive_char(void);

/* Formatted output helpers - each builds its full output locally, then sends it
 * as ONE message so it can never be interleaved with another task's output. */
void    uart_send_string(const char *str);
void    uart_send_uint(uint32 value);
void    uart_send_uint_padded(uint32 value, uint8 width);

void    uart_process_command(const char *cmd);

#endif /* APP_UART_TASKS_H_ */
