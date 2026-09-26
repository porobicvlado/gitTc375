/**********************************************************************************************************************
 * \file App_SystemInit.c
 * \copyright Copyright (C) Infineon Technologies AG 2023
 *********************************************************************************************************************/

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#include "App_Config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "MCMCAN.h"

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
extern IFX_ALIGN(4) IfxCpu_syncEvent g_cpuSyncEvent;
QueueHandle_t g_canRxQueue = NULL;

/* Debug counters */
volatile uint32 g_debugInitDriversCalled = 0;
volatile uint32 g_debugQueueCreated = 0;
volatile uint32 g_debugTxTaskCreated = 0;
volatile uint32 g_debugRxTaskCreated = 0;
volatile uint32 g_debugInitTaskDeleted = 0;

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
/* Low-level hardware initialization */
void hw_init_minimal(void)
{
    /* Enable interrupts */
    IfxCpu_enableInterrupts();

    /* !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1);
}

/* Initialize all drivers and peripherals */
static void init_drivers(void)
{
    g_debugInitDriversCalled++;

    /* Initialize LEDs */
    initLeds();

#if (CAN_MODE != LOOPBACK)
    /* Initialize CAN transceiver (P20.6 - STB pin) */
    Transciever_Port_Init();
#endif

    /* Initialize MCMCAN module */
    initMcmcan();
}

/* System initialization task */
void task_system_init(void *arg)
{
    BaseType_t txResult, rxResult;

    /* Full system initialization */
    init_drivers();

    /* Create CAN RX Queue */
    g_canRxQueue = xQueueCreate(CAN_RX_QUEUE_LENGTH, sizeof(CanRxMessage_t));

    /* Check if queue creation was successful */
    if (g_canRxQueue == NULL)
    {
        /* Queue creation failed - indicate error with LED1 blinking */
        while(1)
        {
            IfxPort_togglePin(g_led1.port, g_led1.pinIndex);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    g_debugQueueCreated++;

    /* Initialize RX message structure */
    IfxCan_Can_initMessage(&g_mcmcan.rxMsg);
    memset((void *)(&g_mcmcan.rxData[0]), INVALID_RX_DATA_VALUE, MAXIMUM_CAN_DATA_PAYLOAD * sizeof(uint32));

    /* Create application tasks */
    txResult = xTaskCreate(task_can_tx, "CAN_TX", 2048, NULL, tskIDLE_PRIORITY + 2, NULL);
    if (txResult == pdPASS)
    {
        g_debugTxTaskCreated++;
    }

    rxResult = xTaskCreate(task_can_rx, "CAN_RX", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
    if (rxResult == pdPASS)
    {
        g_debugRxTaskCreated++;
    }

    /* Delete self - initialization task is no longer needed */
    g_debugInitTaskDeleted++;
    vTaskDelay(pdMS_TO_TICKS(500));
    vTaskDelete(NULL);
}
