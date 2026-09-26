/**********************************************************************************************************************
 * \file App_CanTasks.c
 * \copyright Copyright (C) Infineon Technologies AG 2023
 *********************************************************************************************************************/

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "MCMCAN.h"
#include "App_Config.h"

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
volatile uint32 g_txTaskLoopCount = 0;
volatile uint32 g_rxTaskLoopCount = 0;
volatile uint32 g_rxTaskMessageCount = 0;
volatile uint32 g_txTaskSendCount = 0;

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
        g_txTaskLoopCount++;

        /* Update time (increment every second) */
        updateTime(&g_mcmcan.currentTime);

        /* Transmit message with Counter and Time */
        transmitCanMessage();

        g_txTaskSendCount++;

        /* Send every 1 second */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* CAN RX Task - Receives and processes messages */
void task_can_rx(void *arg)
{
    CanRxMessage_t rxMessage;

    while (1)
    {
        g_rxTaskLoopCount++;

        /* Wait for message from Queue (blocking indefinitely) */
        if (xQueueReceive(g_canRxQueue, &rxMessage, portMAX_DELAY) == pdTRUE)
        {
            g_rxTaskMessageCount++;

            /* Process received message */
            if (rxMessage.messageId == CAN_MESSAGE_ID2)
            {
                /* Access received data */
                uint32 receivedCounter = rxMessage.counter;
                uint8 receivedHours = rxMessage.timeData.hours;
                uint8 receivedMinutes = rxMessage.timeData.minutes;
                uint8 receivedSeconds = rxMessage.timeData.seconds;

                /* Debug: You can set breakpoint here to inspect values */
                /* Or use the values for further processing */
                __nop();
            }
        }
    }
}
