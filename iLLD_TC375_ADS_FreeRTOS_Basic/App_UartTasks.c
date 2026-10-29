#include "App_UartTasks.h"
#include "Asclin/Asc/IfxAsclin_Asc.h"
#include "IfxAsclin_PinMap.h"
#include "App_Config.h"

/*-------------------------------------------------- Globals -----------------------------------------------------*/
SemaphoreHandle_t g_uartRxSem   = NULL;
QueueHandle_t     g_uartTxQueue = NULL;

static IfxAsclin_Asc   g_ascHandle;
static uint8_t         g_rxBuf[UART_RX_RINGBUF_SIZE];
static volatile uint16_t g_rxHead = 0, g_rxTail = 0;

#define UART_MODULE   (&MODULE_ASCLIN0)
#define UART_TX_PIN   (&IfxAsclin0_TX_P14_0_OUT)
#define UART_RX_PIN   (&IfxAsclin0_RXA_P14_1_IN)

volatile sint32 g_uartVar = 0;

#define UART_CMD_BUF_SIZE 32
static char    g_cmdBuf[UART_CMD_BUF_SIZE];
static uint8_t g_cmdLen = 0;

/*-------------------------------------------------- Ring buffer --------------------------------------------------*/
static void uart_rx_buffer_push(uint8_t ch)
{
    uint16_t next = (uint16_t)((g_rxHead + 1u) % UART_RX_RINGBUF_SIZE);
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
        g_rxTail = (uint16_t)((g_rxTail + 1u) % UART_RX_RINGBUF_SIZE);
    }
    return ch;
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
    uint8_t ch;
    for (;;)
    {
        xQueueReceive(g_uartTxQueue, &ch, portMAX_DELAY);

        while (IfxAsclin_getTxFifoFillLevelFlagStatus(UART_MODULE) != TRUE)
        {
        }
        IfxAsclin_clearTxFifoFillLevelFlag(UART_MODULE);
        IfxAsclin_writeTxData(UART_MODULE, ch);
    }
}

/*-------------------------------------------------- Public API ----------------------------------------------------*/
void uart_send_char(uint8_t ch)
{
    xQueueSend(g_uartTxQueue, &ch, portMAX_DELAY);
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

/*-------------------------------------------------- Formatted output helpers --------------------------------------*/

void uart_send_string(const char *str)
{
    while (*str != '\0')
    {
        uart_send_char((uint8_t)*str);
        str++;
    }
}

void uart_send_uint(uint32 value)
{
    uart_send_uint_padded(value, 0);
}

void uart_send_uint_padded(uint32 value, uint8 width)
{
    char  digits[10];
    uint8 count = 0;
    uint32 v = value;

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

    for (uint8 p = count; p < width; p++)
    {
        uart_send_char((uint8_t)'0');
    }

    while (count > 0)
    {
        count--;
        uart_send_char((uint8_t)digits[count]);
    }
}


static void uart_process_command(const char *cmd)
{
    static const char prefix[] = "testvar=";
    const size_t prefixLen = sizeof(prefix) - 1u;

    if (strncmp(cmd, prefix, prefixLen) == 0)
    {
        const char *numStr = cmd + prefixLen;
        sint32 sign   = 1;
        sint32 result = 0;

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

        uart_send_string("testvar=");
        uart_send_uint((uint32)g_uartVar);
        uart_send_string("\r\n");
    }
}
