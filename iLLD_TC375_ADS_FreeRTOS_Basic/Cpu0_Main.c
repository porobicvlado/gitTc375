/**********************************************************************************************************************
 * \file Cpu0_Main.c
 * \copyright Copyright (C) Infineon Technologies AG 2023
 *********************************************************************************************************************/

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "IfxCpu.h"

#include "App_Config.h"
#include "FreeRTOS.h"
#include "task.h"

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
IFX_ALIGN(4) IfxCpu_syncEvent g_cpuSyncEvent = 0;

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
void core0_main(void)
{
    /* Low-level HW init (interrupts, watchdog, CPU sync) */
    hw_init_minimal();
    
    /* Create system initialization task with high priority */
    xTaskCreate(
        task_system_init,
        "SYS_INIT",
        1024,
        NULL,
        tskIDLE_PRIORITY + 3,
        NULL
    );

    /* Start the scheduler */
    vTaskStartScheduler();
    
    /* Should never get here */
    while (1)
    {
        __nop();
    }
}

/* Required FreeRTOS callback, called in case of a stack overflow */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    while (1)
    {
        __nop();
    }
}

/* FreeRTOS malloc failed hook */
void vApplicationMallocFailedHook(void)
{
    while (1)
    {
        __nop();
    }
}

volatile uint32 g_tickCount = 0;

void vApplicationTickHook(void)
{
    g_tickCount++;
}
