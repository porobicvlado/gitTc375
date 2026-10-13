/**********************************************************************************************************************
 * \file App_CanTasks.c
 * \copyright Copyright (C) Infineon Technologies AG 2023
 *********************************************************************************************************************/

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "App_SystemInit.h"
#include "App_Config.h"
#include "MCMCAN.h"

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

/* Helper function to update time */
void updateTime(TimeData_t *time)
{
    time->seconds++;

    if (time->seconds >= 60)
    {
        time->seconds = 0;
        time->minutes++;

        if (time->minutes >= 60)
        {
            time->minutes = 0;
            time->hours++;

            if (time->hours >= 24)
            {
                time->hours = 0;
            }
        }
    }
}

/* CAN TX Task - Sends Counter and Time every second */
void task_can_tx(void *arg)
{
    while (1)
    {
        /* Update time (increment every second) */
        updateTime(&g_currentTime.currentTimeTx);

        /* Increment Tx counter before sending */
        g_counter.counterTx++;

        /* Transmit message with Counter and Time - data is now passed explicitly,
         * transmitCanMessage() no longer reads g_counter / g_currentTime on its own */
        transmitCanMessage(g_counter.counterTx, g_currentTime.currentTimeTx);

        /* Send every 1 second */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* CAN RX Task - Receives raw message from queue and performs ALL parsing */
void task_can_rx(void *arg)
{
    CanRxMessage_forward_t rxMessage;

    while (1)
    {
        /* Wait for message from Queue (blocking indefinitely) */
        if (xQueueReceive(g_canRxQueue, &rxMessage, portMAX_DELAY) == pdTRUE)
        {
            /* Process received message */
            if (rxMessage.messageId == CAN_MESSAGE_ID2)
            {
                /* Parse Counter from first word */
                g_counter.counterRx = rxMessage.data[0];

                /* Parse Time data from second word directly into global Rx time state
                 * (removed the intermediate local TimeData_t rxTime copy - not needed) */
                uint32 timeRaw = rxMessage.data[1];
                g_currentTime.currentTimeRx.hours    = (timeRaw >> 24) & 0xFF;
                g_currentTime.currentTimeRx.minutes  = (timeRaw >> 16) & 0xFF;
                g_currentTime.currentTimeRx.seconds  = (timeRaw >> 8)  & 0xFF;
                g_currentTime.currentTimeRx.reserved = timeRaw & 0xFF;
                __nop();
            }
        }
    }
}
