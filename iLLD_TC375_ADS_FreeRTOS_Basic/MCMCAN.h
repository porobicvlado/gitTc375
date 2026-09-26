/********************************************************************************************************************
 * \file MCMCAN.h
 * \copyright Copyright (C) Infineon Technologies AG 2019
 *********************************************************************************************************************/

#ifndef MCMCAN_H_
#define MCMCAN_H_ 1

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include "Ifx_Types.h"
#include "Can/Can/IfxCan_Can.h"
#include "Can/Std/IfxCan.h"
#include "IfxCpu_Irq.h"
#include "IfxPort.h"
#include "Stm/Std/IfxStm.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*********************************************************************************************************************/
/*--------------------------------------MODE SELECTION MACRO---------------------------------------------------------*/
/*********************************************************************************************************************/
/* Mode definitions */
#define LOOPBACK                    0
#define TWO_CONTROLLER_MODE_1       1
#define TWO_CONTROLLER_MODE_2       2

/* Select mode here */
#define CAN_MODE                    TWO_CONTROLLER_MODE_1

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/

/* Message ID configuration based on selected mode */
#if (CAN_MODE == LOOPBACK)
    #define CAN_MESSAGE_ID1         (uint32)0x777
    #define CAN_MESSAGE_ID2         (uint32)0x777

#elif (CAN_MODE == TWO_CONTROLLER_MODE_1)
    #define CAN_MESSAGE_ID1         (uint32)0x777
    #define CAN_MESSAGE_ID2         (uint32)0x888

#elif (CAN_MODE == TWO_CONTROLLER_MODE_2)
    #define CAN_MESSAGE_ID1         (uint32)0x888
    #define CAN_MESSAGE_ID2         (uint32)0x777
#endif

/* Common macros */
#define PIN5                        5
#define PIN6                        6
#define INVALID_RX_DATA_VALUE       0xA5
#define INVALID_ID_VALUE            (uint32)0xFFFFFFFF
#define ISR_PRIORITY_CAN_TX         12
#define ISR_PRIORITY_CAN_RX         13
#define MAXIMUM_CAN_DATA_PAYLOAD    2
#define STM_FREQ_HZ                 100000000ULL
#define CAN_RX_QUEUE_LENGTH         50

/*********************************************************************************************************************/
/*--------------------------------------------------Data Structures--------------------------------------------------*/
/*********************************************************************************************************************/

/* Structure for time data (4 bytes total) */
typedef struct
{
    uint8 hours;      // 0-23
    uint8 minutes;    // 0-59
    uint8 seconds;    // 0-59
    uint8 reserved;   // Padding to make it 4 bytes
} TimeData_t;

/* Structure for CAN message in Queue */
typedef struct
{
    uint32 messageId;
    uint32 counter;        // First 4 bytes
    TimeData_t timeData;   // Next 4 bytes
    uint8 dataLength;
} CanRxMessage_t;

/* MCMCAN module structure */
typedef struct
{
    IfxCan_Can_Config canConfig;
    IfxCan_Can canModule;
    IfxCan_Can_Node canSrcNode;
    IfxCan_Can_Node canDstNode;
    IfxCan_Can_NodeConfig canNodeConfig;
    IfxCan_Filter canFilter;
    IfxCan_Message txMsg;
    IfxCan_Message rxMsg;
    uint32 txData[MAXIMUM_CAN_DATA_PAYLOAD];
    uint32 rxData[MAXIMUM_CAN_DATA_PAYLOAD];
    uint32 txCounter;
    TimeData_t currentTime;  // Current time to send
} McmcanType;

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
extern McmcanType g_mcmcan;
extern QueueHandle_t g_canRxQueue;
extern IfxPort_Pin_Config g_led1;
extern IfxPort_Pin_Config g_led2;

/* ISR Debug counters */
extern volatile uint32 g_txIsrCount;
extern volatile uint32 g_rxIsrCount;
extern volatile uint32 g_queueSendSuccessCount;
extern volatile uint32 g_queueSendFailCount;

/* Function call counters */
extern volatile uint32 g_initMcmcanCalled;
extern volatile uint32 g_initLedsCalled;
extern volatile uint32 g_transmitCanMessageCalled;
extern volatile uint32 g_transmitCanMessageSuccess;
extern volatile uint32 g_transmitCanMessageFail;

#if (CAN_MODE != LOOPBACK)
extern volatile uint32 g_driverPortInitCalled;
#endif

/* Task counters */
extern volatile uint32 g_txTaskLoopCount;
extern volatile uint32 g_rxTaskLoopCount;
extern volatile uint32 g_rxTaskMessageCount;
extern volatile uint32 g_txTaskSendCount;

/*********************************************************************************************************************/
/*-----------------------------------------------Function Prototypes-------------------------------------------------*/
/*********************************************************************************************************************/
void initMcmcan(void);
void transmitCanMessage(void);
void initLeds(void);

#if (CAN_MODE != LOOPBACK)
void Transciever_Port_Init(void);
#endif

#endif /* MCMCAN_H_ */
