/********************************************************************************************************************
 * \file MCMCAN.c
 * \copyright Copyright (C) Infineon Technologies AG 2020
 *********************************************************************************************************************/

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "MCMCAN.h"

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
McmcanType                  g_mcmcan;
IfxPort_Pin_Config          g_led1;
IfxPort_Pin_Config          g_led2;

/* Debug counters for ISR */
volatile uint32 g_txIsrCount = 0;
volatile uint32 g_rxIsrCount = 0;
volatile uint32 g_queueSendSuccessCount = 0;
volatile uint32 g_queueSendFailCount = 0;

/* Debug counters for functions */
volatile uint32 g_initMcmcanCalled = 0;
volatile uint32 g_initLedsCalled = 0;
volatile uint32 g_transmitCanMessageCalled = 0;
volatile uint32 g_transmitCanMessageSuccess = 0;
volatile uint32 g_transmitCanMessageFail = 0;

#if (CAN_MODE != LOOPBACK)
volatile uint32 g_driverPortInitCalled = 0;
#endif

/* External queue handle */
extern QueueHandle_t g_canRxQueue;

#if (CAN_MODE != LOOPBACK)
/* CAN Pin configuration for external communication */
IFX_CONST IfxCan_Can_Pins Can00_pins = {
       &IfxCan_TXD00_P20_8_OUT,   IfxPort_OutputMode_pushPull,
       &IfxCan_RXD00B_P20_7_IN,   IfxPort_InputMode_pullUp,
       IfxPort_PadDriver_cmosAutomotiveSpeed4
};
#endif

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
IFX_INTERRUPT(canIsrTxHandler, 0, ISR_PRIORITY_CAN_TX);
IFX_INTERRUPT(canIsrRxHandler, 0, ISR_PRIORITY_CAN_RX);

/* Interrupt Service Routine (ISR) called once the TX interrupt has been generated */
void canIsrTxHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    g_txIsrCount++;

    IfxCan_Node_clearInterruptFlag(g_mcmcan.canSrcNode.node, IfxCan_Interrupt_transmissionCompleted);

    IfxPort_togglePin(g_led1.port, g_led1.pinIndex);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* RX ISR - Extract Counter and Time */
void canIsrRxHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    CanRxMessage_t rxMessage;

    g_rxIsrCount++;

    IfxCan_Can_readMessage(&g_mcmcan.canDstNode, &g_mcmcan.rxMsg, g_mcmcan.rxData);

    IfxCan_Node_clearInterruptFlag(g_mcmcan.canDstNode.node,
                                   IfxCan_Interrupt_messageStoredToDedicatedRxBuffer);

    /* Extract received data */
    rxMessage.messageId = g_mcmcan.rxMsg.messageId;
    rxMessage.counter = g_mcmcan.rxData[0];  // First 4 bytes = Counter

    /* Extract time data from second 4 bytes */
    uint32 timeRaw = g_mcmcan.rxData[1];
    rxMessage.timeData.hours = (timeRaw >> 24) & 0xFF;
    rxMessage.timeData.minutes = (timeRaw >> 16) & 0xFF;
    rxMessage.timeData.seconds = (timeRaw >> 8) & 0xFF;
    rxMessage.timeData.reserved = timeRaw & 0xFF;

    rxMessage.dataLength = MAXIMUM_CAN_DATA_PAYLOAD;

    /* Send to queue */
    if (g_canRxQueue != NULL)
    {
        if (xQueueSendFromISR(g_canRxQueue, &rxMessage, &xHigherPriorityTaskWoken) == pdTRUE)
        {
            g_queueSendSuccessCount++;
            IfxPort_togglePin(g_led2.port, g_led2.pinIndex);
        }
        else
        {
            g_queueSendFailCount++;
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

#if (CAN_MODE != LOOPBACK)
/* Initialize CAN transceiver (P20.6 - STB pin) */
void Transciever_Port_Init(void)
{
    IfxPort_Pin_Config  pin26;

    g_driverPortInitCalled++;

    pin26.port=&MODULE_P20;
    pin26.pinIndex=6;

    IfxPort_setPinModeOutput(pin26.port, pin26.pinIndex, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinLow(pin26.port, pin26.pinIndex);
}
#endif

/* Function to initialize MCMCAN module and nodes */
void initMcmcan(void)
{
    g_initMcmcanCalled++;

    /* Initialize time to 00:00:00 */
    g_mcmcan.currentTime.hours = 0;
    g_mcmcan.currentTime.minutes = 0;
    g_mcmcan.currentTime.seconds = 0;
    g_mcmcan.currentTime.reserved = 0;

    /* Initialize CAN module */
    IfxCan_Can_initModuleConfig(&g_mcmcan.canConfig, &MODULE_CAN0);
    IfxCan_Can_initModule(&g_mcmcan.canModule, &g_mcmcan.canConfig);

#if (CAN_MODE == LOOPBACK)
    /* LOOPBACK MODE - Source CAN node configuration */
    IfxCan_Can_initNodeConfig(&g_mcmcan.canNodeConfig, &g_mcmcan.canModule);

    g_mcmcan.canNodeConfig.busLoopbackEnabled = TRUE;
    g_mcmcan.canNodeConfig.nodeId = IfxCan_NodeId_0;
    g_mcmcan.canNodeConfig.frame.type = IfxCan_FrameType_transmit;

    g_mcmcan.canNodeConfig.interruptConfig.transmissionCompletedEnabled = TRUE;
    g_mcmcan.canNodeConfig.interruptConfig.traco.priority = ISR_PRIORITY_CAN_TX;
    g_mcmcan.canNodeConfig.interruptConfig.traco.interruptLine = IfxCan_InterruptLine_0;
    g_mcmcan.canNodeConfig.interruptConfig.traco.typeOfService = IfxSrc_Tos_cpu0;

    IfxCan_Can_initNode(&g_mcmcan.canSrcNode, &g_mcmcan.canNodeConfig);

    /* LOOPBACK MODE - Destination CAN node configuration */
    IfxCan_Can_initNodeConfig(&g_mcmcan.canNodeConfig, &g_mcmcan.canModule);

    g_mcmcan.canNodeConfig.busLoopbackEnabled = TRUE;
    g_mcmcan.canNodeConfig.nodeId = IfxCan_NodeId_1;
    g_mcmcan.canNodeConfig.frame.type = IfxCan_FrameType_receive;

    g_mcmcan.canNodeConfig.interruptConfig.messageStoredToDedicatedRxBufferEnabled = TRUE;
    g_mcmcan.canNodeConfig.interruptConfig.reint.priority = ISR_PRIORITY_CAN_RX;
    g_mcmcan.canNodeConfig.interruptConfig.reint.interruptLine = IfxCan_InterruptLine_1;
    g_mcmcan.canNodeConfig.interruptConfig.reint.typeOfService = IfxSrc_Tos_cpu0;

    IfxCan_Can_initNode(&g_mcmcan.canDstNode, &g_mcmcan.canNodeConfig);

#else  /* TWO_CONTROLLER_MODE */
    /* NORMAL MODE - CAN node configuration */
    IfxCan_Can_initNodeConfig(&g_mcmcan.canNodeConfig, &g_mcmcan.canModule);

    g_mcmcan.canNodeConfig.busLoopbackEnabled = FALSE;
    g_mcmcan.canNodeConfig.nodeId = IfxCan_NodeId_0;
    g_mcmcan.canNodeConfig.frame.type = IfxCan_FrameType_transmitAndReceive;

    /* TX Interrupt Configuration */
    g_mcmcan.canNodeConfig.interruptConfig.transmissionCompletedEnabled = TRUE;
    g_mcmcan.canNodeConfig.interruptConfig.traco.priority = ISR_PRIORITY_CAN_TX;
    g_mcmcan.canNodeConfig.interruptConfig.traco.interruptLine = IfxCan_InterruptLine_0;
    g_mcmcan.canNodeConfig.interruptConfig.traco.typeOfService = IfxSrc_Tos_cpu0;

    /* RX Interrupt Configuration */
    g_mcmcan.canNodeConfig.interruptConfig.messageStoredToDedicatedRxBufferEnabled = TRUE;
    g_mcmcan.canNodeConfig.interruptConfig.reint.priority = ISR_PRIORITY_CAN_RX;
    g_mcmcan.canNodeConfig.interruptConfig.reint.interruptLine = IfxCan_InterruptLine_1;
    g_mcmcan.canNodeConfig.interruptConfig.reint.typeOfService = IfxSrc_Tos_cpu0;

    /* Assign CAN pins */
    g_mcmcan.canNodeConfig.pins = &Can00_pins;

    /* Initialize node */
    IfxCan_Can_initNode(&g_mcmcan.canSrcNode, &g_mcmcan.canNodeConfig);

    /* Use same node for both TX and RX */
    g_mcmcan.canDstNode = g_mcmcan.canSrcNode;
#endif

    /* CAN filter configuration */
    g_mcmcan.canFilter.number = 0;
    g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
    g_mcmcan.canFilter.id1 = CAN_MESSAGE_ID2;
    g_mcmcan.canFilter.rxBufferOffset = IfxCan_RxBufferId_0;

    IfxCan_Can_setStandardFilter(&g_mcmcan.canDstNode, &g_mcmcan.canFilter);

    /* Initialize TX counter */
    g_mcmcan.txCounter = 0;
}

/* Function to transmit CAN message with Counter and Time */
void transmitCanMessage(void)
{
    g_transmitCanMessageCalled++;

    /* Initialize TX message */
    IfxCan_Can_initMessage(&g_mcmcan.txMsg);

    /* First 4 bytes: Counter */
    g_mcmcan.txData[0] = g_mcmcan.txCounter;

    /* Second 4 bytes: Time (H:M:S + reserved) */
    g_mcmcan.txData[1] = ((uint32)g_mcmcan.currentTime.hours << 24) |
                         ((uint32)g_mcmcan.currentTime.minutes << 16) |
                         ((uint32)g_mcmcan.currentTime.seconds << 8) |
                         ((uint32)g_mcmcan.currentTime.reserved);

    g_mcmcan.txMsg.messageId = CAN_MESSAGE_ID1;

    IfxCan_Status txStatus = IfxCan_Can_sendMessage(&g_mcmcan.canSrcNode,
                                                    &g_mcmcan.txMsg,
                                                    &g_mcmcan.txData[0]);

    if (txStatus != IfxCan_Status_notSentBusy)
    {
        g_mcmcan.txCounter++;
        g_transmitCanMessageSuccess++;
    }
    else
    {
        g_transmitCanMessageFail++;
    }
}

/* Function to initialize the LEDs */
void initLeds(void)
{
    g_initLedsCalled++;

    /* LED1 configuration */
    g_led1.port      = &MODULE_P00;
    g_led1.pinIndex  = PIN5;
    g_led1.mode      = IfxPort_OutputIdx_general;
    g_led1.padDriver = IfxPort_PadDriver_cmosAutomotiveSpeed1;

    /* LED2 configuration */
    g_led2.port      = &MODULE_P00;
    g_led2.pinIndex  = PIN6;
    g_led2.mode      = IfxPort_OutputIdx_general;
    g_led2.padDriver = IfxPort_PadDriver_cmosAutomotiveSpeed1;

    /* Initialize LEDs to HIGH (OFF) */
    IfxPort_setPinHigh(g_led1.port, g_led1.pinIndex);
    IfxPort_setPinHigh(g_led2.port, g_led2.pinIndex);

    /* Set pin mode to output */
    IfxPort_setPinModeOutput(g_led1.port, g_led1.pinIndex, IfxPort_OutputMode_pushPull, g_led1.mode);
    IfxPort_setPinModeOutput(g_led2.port, g_led2.pinIndex, IfxPort_OutputMode_pushPull, g_led2.mode);

    /* Set pad driver */
    IfxPort_setPinPadDriver(g_led1.port, g_led1.pinIndex, g_led1.padDriver);
    IfxPort_setPinPadDriver(g_led2.port, g_led2.pinIndex, g_led2.padDriver);
}
