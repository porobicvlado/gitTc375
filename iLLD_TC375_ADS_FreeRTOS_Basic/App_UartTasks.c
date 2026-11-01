#include <string.h>
#include <stdio.h>

#include "App_UartTasks.h"
#include "Asclin/Asc/IfxAsclin_Asc.h"
#include "IfxAsclin_PinMap.h"
#include "App_Config.h"

/*-------------------------------------------------- Globals -----------------------------------------------------*/
SemaphoreHandle_t g_uartRxSem   = NULL;
QueueHandle_t     g_uartTxQueue = NULL;

static IfxAsclin_Asc     g_ascHandle;
static uint8_t           g_rxBuf[UART_RX_RINGBUF_SIZE];
static volatile uint16_t g_rxHead = 0, g_rxTail = 0;

#define UART_MODULE   (&MODULE_ASCLIN0)
#define UART_TX_PIN   (&IfxAsclin0_TX_P14_0_OUT)
#define UART_RX_PIN   (&IfxAsclin0_RXA_P14_1_IN)

volatile sint32 g_uartVar = 0;

#define UART_CMD_BUF_SIZE 32
static char    g_cmdBuf[UART_CMD_BUF_SIZE];
static uint8_t g_cmdLen = 0;

static void uart_rx_buffer_push(uint8_t ch)
{
    uint16_t next = (uint16_t)((g_rxHead + 1u) & (UART_RX_RINGBUF_SIZE - 1u));
    if (next != g_rxTail)
    {
        g_rxBuf[g_rxHead] = ch;
        g_rxHead = next;
    }
}

static uint8_t uart_rx_buffer_pop(void)
{
    uint8_t ch = 0;
    if (g_rxHead != g_rxTail)
    {
        ch = g_rxBuf[g_rxTail];
        g_rxTail = (uint16_t)((g_rxTail + 1u) & (UART_RX_RINGBUF_SIZE - 1u));
    }
    return ch;
}

static void uart_send_msg(const char *data, uint16_t len)
{
    UartTxMsg_t msg;

    if (len > UART_TX_MSG_MAX_LEN)
    {
        len = UART_TX_MSG_MAX_LEN;
    }

    memcpy(msg.data, data, len);
    msg.len = len;

    (void)xQueueSend(g_uartTxQueue, &msg, portMAX_DELAY);
}

/*-------------------------------------------------- Hardware init ------------------------------------------------*/
void init_uart(void)
{
    IfxAsclin_Asc_Config ascConfig;
    IfxAsclin_Asc_initModuleConfig(&ascConfig, UART_MODULE);

    ascConfig.baudrate.baudrate = (float32)UART_BAUDRATE;

    const IfxAsclin_Asc_Pins pins = {
        NULL_PTR, IfxPort_InputMode_pullUp,
        UART_RX_PIN, IfxPort_InputMode_pullUp,
        NULL_PTR, IfxPort_OutputMode_pushPull,
        UART_TX_PIN, IfxPort_OutputMode_pushPull,
        IfxPort_PadDriver_cmosAutomotiveSpeed1
    };
    ascConfig.pins = &pins;

    ascConfig.interrupt.txPriority = 0;
    ascConfig.interrupt.rxPriority = ISR_PRIORITY_UART_RX;
    ascConfig.interrupt.erPriority = 0;
    ascConfig.interrupt.typeOfService = IfxCpu_Irq_getTos(IfxCpu_getCoreIndex());

    IfxAsclin_Asc_initModule(&g_ascHandle, &ascConfig);

    UART_MODULE->FLAGSSET.B.TFLS = 1;

    g_uartRxSem   = xSemaphoreCreateBinary();
    g_uartTxQueue = xQueueCreate(UART_TX_QUEUE_LENGTH, sizeof(UartTxMsg_t));
}

/*-------------------------------------------------- ISR -----------------------------------------------------------*/
IFX_INTERRUPT(UART0_RxISR, 0, ISR_PRIORITY_UART_RX);
void UART0_RxISR(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t ch = (uint8_t)IfxAsclin_readRxData(UART_MODULE);

    IfxAsclin_clearRxFifoFillLevelFlag(UART_MODULE);
    uart_rx_buffer_push(ch);

    xSemaphoreGiveFromISR(g_uartRxSem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*-------------------------------------------------- Tasks ---------------------------------------------------------*/
void task_uart_rx(void *arg)
{
    (void)arg;

    for (;;)
    {
        xSemaphoreTake(g_uartRxSem, portMAX_DELAY);
        while (g_rxHead != g_rxTail)
        {
            uint8_t ch = uart_rx_buffer_pop();

            if (ch == '\r' || ch == '\n')
            {
                if (g_cmdLen > 0)
                {
                    g_cmdBuf[g_cmdLen] = '\0';
                    uart_process_command(g_cmdBuf);
                    g_cmdLen = 0;
                }
            }
            else
            {
                if (g_cmdLen < (UART_CMD_BUF_SIZE - 1u))
                {
                    g_cmdBuf[g_cmdLen++] = (char)ch;
                }
            }
        }
    }
}

void task_uart_tx(void *arg)
{
    UartTxMsg_t msg;
    (void)arg;

    for (;;)
    {
        xQueueReceive(g_uartTxQueue, &msg, portMAX_DELAY);
        for (uint16_t i = 0; i < msg.len; i++)
        {
            while (IfxAsclin_getTxFifoFillLevelFlagStatus(UART_MODULE) != TRUE)
            {
            }
            IfxAsclin_clearTxFifoFillLevelFlag(UART_MODULE);
            IfxAsclin_writeTxData(UART_MODULE, (uint8_t)msg.data[i]);
        }
    }
}

/*-------------------------------------------------- Public API ----------------------------------------------------*/
void uart_send_char(uint8_t ch)
{
    char c = (char)ch;
    uart_send_msg(&c, 1u);
}

uint8_t uart_receive_char(void)
{
    uint8_t ch;
    while (g_rxHead == g_rxTail)
    {
        xSemaphoreTake(g_uartRxSem, portMAX_DELAY);
    }
    ch = uart_rx_buffer_pop();
    return ch;
}

void uart_send_string(const char *str)
{
    uart_send_msg(str, (uint16_t)strlen(str));
}

void uart_send_uint(uint32 value)
{
    uart_send_uint_padded(value, 0);
}

void uart_send_uint_padded(uint32 value, uint8 width)
{
    char     buf[UART_TX_MSG_MAX_LEN];
    char     digits[10];
    uint8    count = 0;
    uint32   v = value;
    uint16_t idx = 0;

    if (v == 0)
    {
        digits[count++] = '0';
    }
    else
    {
        while (v > 0 && count < sizeof(digits))
        {
            digits[count++] = (char)('0' + (v % 10u));
            v /= 10u;
        }
    }

    for (uint8 p = count; (p < width) && (idx < sizeof(buf)); p++)
    {
        buf[idx++] = '0';
    }

    while ((count > 0) && (idx < sizeof(buf)))
    {
        count--;
        buf[idx++] = digits[count];
    }

    uart_send_msg(buf, idx);
}

void uart_process_command(const char *cmd)
{
    static const char prefix[] = "testvar=";
    const size_t prefixLen = sizeof(prefix) - 1u;

    if (strncmp(cmd, prefix, prefixLen) == 0)
    {
        const char *numStr = cmd + prefixLen;
        sint32 sign   = 1;
        sint32 result = 0;
        char   resp[UART_TX_MSG_MAX_LEN];
        int    n;

        if (*numStr == '-')
        {
            sign = -1;
            numStr++;
        }

        while (*numStr >= '0' && *numStr <= '9')
        {
            result = result * 10 + (sint32)(*numStr - '0');
            numStr++;
        }

        g_uartVar = sign * result;

        n = snprintf(resp, sizeof(resp), "testvar=%ld\r\n", (long)g_uartVar);
        if (n > 0)
        {
            uart_send_msg(resp, (uint16_t)n);
        }
    }
}
