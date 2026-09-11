/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f1xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f1xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "gpio.h"
#include <stdio.h>
#include "usart.h"
#include "modbus.h"
#include "fifo.h"
#include "wifi4g.h"
#include "mqtt.h"
#include "canfestival.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
void USER_UART_IRQHandler(UART_HandleTypeDef *huart);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern CAN_HandleTypeDef hcan;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim4;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart3_rx;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart3;
extern TIM_HandleTypeDef htim1;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M3 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  /* 故障诊断：打印栈帧 PC/LR（寄存器级直发） */
  {
    uint32_t msp = __get_MSP();
    uint32_t *stk = (uint32_t *)msp;
    static const char hex[] = "0123456789ABCDEF";
    char line[40];
    int i;
    uint32_t pc = stk[6];
    uint32_t lr = stk[5];
    for (i = 0; i < 8; i++)  line[i] = "[FAULT] "[i];
    line[8] = 'P'; line[9] = 'C'; line[10] = '=';
    for (i = 0; i < 8; i++) line[11 + i] = hex[(pc >> (28 - 4 * i)) & 0xF];
    line[19] = ' '; line[20] = 'L'; line[21] = 'R'; line[22] = '=';
    for (i = 0; i < 8; i++) line[23 + i] = hex[(lr >> (28 - 4 * i)) & 0xF];
    line[31] = '\r'; line[32] = '\n'; line[33] = 0;
    for (i = 0; line[i]; i++)
    {
      while (!(USART1->SR & USART_SR_TXE));
      USART1->DR = line[i];
    }
  }
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32F1xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f1xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles DMA1 channel3 global interrupt.
  */
void DMA1_Channel3_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel3_IRQn 0 */

  /* USER CODE END DMA1_Channel3_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart3_rx);
  /* USER CODE BEGIN DMA1_Channel3_IRQn 1 */

  /* USER CODE END DMA1_Channel3_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel5 global interrupt.
  */
void DMA1_Channel5_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel5_IRQn 0 */

  /* USER CODE END DMA1_Channel5_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart1_rx);
  /* USER CODE BEGIN DMA1_Channel5_IRQn 1 */

  /* USER CODE END DMA1_Channel5_IRQn 1 */
}

/**
  * @brief This function handles USB low priority or CAN RX0 interrupts.
  */
void USB_LP_CAN1_RX0_IRQHandler(void)
{
  /* USER CODE BEGIN USB_LP_CAN1_RX0_IRQn 0 */

  /* USER CODE END USB_LP_CAN1_RX0_IRQn 0 */
  HAL_CAN_IRQHandler(&hcan);
  /* USER CODE BEGIN USB_LP_CAN1_RX0_IRQn 1 */

  /* USER CODE END USB_LP_CAN1_RX0_IRQn 1 */
}

/**
  * @brief This function handles EXTI line[9:5] interrupts.
  */
void EXTI9_5_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI9_5_IRQn 0 */

  /* USER CODE END EXTI9_5_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(KEY_LEFT_Pin);
  HAL_GPIO_EXTI_IRQHandler(KEY_ESC_Pin);
  /* USER CODE BEGIN EXTI9_5_IRQn 1 */

  /* USER CODE END EXTI9_5_IRQn 1 */
}

/**
  * @brief This function handles TIM1 update interrupt.
  */
void TIM1_UP_IRQHandler(void)
{
  /* USER CODE BEGIN TIM1_UP_IRQn 0 */

  /* USER CODE END TIM1_UP_IRQn 0 */
  HAL_TIM_IRQHandler(&htim1);
  /* USER CODE BEGIN TIM1_UP_IRQn 1 */

  /* USER CODE END TIM1_UP_IRQn 1 */
}

/**
  * @brief This function handles TIM2 global interrupt.
  */
void TIM2_IRQHandler(void)
{
  /* USER CODE BEGIN TIM2_IRQn 0 */

  /* USER CODE END TIM2_IRQn 0 */
  HAL_TIM_IRQHandler(&htim2);
  /* USER CODE BEGIN TIM2_IRQn 1 */
	TimeDispatch();
  /* USER CODE END TIM2_IRQn 1 */
}

/**
  * @brief This function handles TIM4 global interrupt.
  */
void TIM4_IRQHandler(void)
{
  /* USER CODE BEGIN TIM4_IRQn 0 */

  /* USER CODE END TIM4_IRQn 0 */
  HAL_TIM_IRQHandler(&htim4);
  /* USER CODE BEGIN TIM4_IRQn 1 */

  /* USER CODE END TIM4_IRQn 1 */
}

/**
  * @brief This function handles USART1 global interrupt.
  */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */

  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);
  /* USER CODE BEGIN USART1_IRQn 1 */
	USER_UART_IRQHandler(&huart1);       //新添加的函数，用来处理串口空闲中断
  /* USER CODE END USART1_IRQn 1 */
}

/**
  * @brief This function handles USART3 global interrupt.
  */
void USART3_IRQHandler(void)
{
  /* USER CODE BEGIN USART3_IRQn 0 */

  /* USER CODE END USART3_IRQn 0 */
  HAL_UART_IRQHandler(&huart3);
  /* USER CODE BEGIN USART3_IRQn 1 */
	USER_UART_IRQHandler(&huart3);       //新添加的函数，用来处理串口空闲中断
  /* USER CODE END USART3_IRQn 1 */
}

/**
  * @brief This function handles EXTI line[15:10] interrupts.
  */
void EXTI15_10_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI15_10_IRQn 0 */

  /* USER CODE END EXTI15_10_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(KEY_OK_Pin);
  HAL_GPIO_EXTI_IRQHandler(KEY_UP_Pin);
  HAL_GPIO_EXTI_IRQHandler(KEY_DOWN_Pin);
  HAL_GPIO_EXTI_IRQHandler(KEY_RIGHT_Pin);
  /* USER CODE BEGIN EXTI15_10_IRQn 1 */

  /* USER CODE END EXTI15_10_IRQn 1 */
}

/* USER CODE BEGIN 1 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	static uint32_t old_uwTick =0; 
	if( (uwTick - old_uwTick) < 200 )  return ; 	
	// uwTick 是全局变量, 每隔1ms 累加一次
	// 第1次进入时 , 时间差会大于200 , if( (uwTick - old_uwTick) < 200 ) 这个条件不成立 
	// 第2次进入时 小时200ms时 不满足条件, 会直接返回 
	// 等时间差大于200ms时, 可以再次触发中断
	old_uwTick = uwTick ; 
	
	// PB13  按键的UP 
	if( GPIO_Pin ==  KEY_UP_Pin)  // 表示 Up 按键按下后触发的中断 
	{
			LED_Control(LED1,ON); // led1 on
			Key_Value = KEY_UP;
			printf("Key Up   Pressed\n");
	}
	else if( GPIO_Pin ==  KEY_DOWN_Pin) 
	{
			LED_Control(LED1,OFF); // led1 off
			Key_Value = KEY_DOWN;
			printf("Key Down Pressed\n");
	}
	else if( GPIO_Pin ==  KEY_LEFT_Pin) 
	{
			LED_Control(LED2,ON); // led2 on
			Key_Value = KEY_LEFT;
			printf("Key Left Pressed\n");
	}
	else if( GPIO_Pin ==  KEY_RIGHT_Pin) 
	{
			LED_Control(LED2,OFF); // led2 off
			Key_Value = KEY_RIGHT;
			printf("Key Right Pressed\n");
	}
	else if( GPIO_Pin ==  KEY_OK_Pin) 
	{
			BEEP_Control(ON); // beep on
			Key_Value = KEY_OK;
			printf("Key OK    Pressed\n");
	}
	else if( GPIO_Pin ==  KEY_ESC_Pin) 
	{
			BEEP_Control(OFF); // beep off
			Key_Value = KEY_ESC;
			printf("Key ESC   Pressed\n");
	}
}


// DMA 接收到一半的中断 
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
		if(huart->Instance == USART1)
		{
			uint16_t Length =  DMA_BUF_SIZE/2 - RX1_Offset ; 
			//printf("HLength=%d\n",Length);
			for(uint16_t _k=0;_k<Length;_k++){ while(!(USART3->SR & USART_SR_TXE)); USART3->DR = RX1_Buf[RX1_Offset+_k]; } 		
			RX1_Offset += Length; 		
		}
		else if(huart->Instance == USART3)
		{
			uint16_t Length =  DMA_BUF_SIZE/2 - RX3_Offset ; 
			//printf("HLength=%d\n",Length);
			
			if(Enqueue_Bytes(&QUART3,RX3_Buf+RX3_Offset,Length) <0){
					printf("FIFO Full\n");
			}
			RX3_Offset += Length; 
		}
}

// DMA传输完成中断   , 就是接收满了的时候 触发中断
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1)
    {
        uint16_t Length =  DMA_BUF_SIZE - RX1_Offset ; 
				for(uint16_t _k=0;_k<Length;_k++){ while(!(USART3->SR & USART_SR_TXE)); USART3->DR = RX1_Buf[RX1_Offset+_k]; } 
        //printf("CLength=%d\n",Length);
        RX1_Offset = 0 ; // 清空dma 位置基准值
    }
		else if(huart->Instance == USART3)
    {
        uint16_t Length =  DMA_BUF_SIZE - RX3_Offset ;  
				if(Enqueue_Bytes(&QUART3,RX3_Buf+RX3_Offset,Length) <0){
					printf("FIFO Full\n");
				}
        //printf("CLength=%d\n",Length);
        RX3_Offset = 0 ; // 清空dma 位置基准值
    }
}

// 用户自定义的函数 ， 处理串口空闲中断
void USER_UART_IRQHandler(UART_HandleTypeDef *huart)
{
	
	  if(huart->Instance == USART1)                                   //判断是否是串口1
    {
			
				if((__HAL_UART_GET_FLAG(huart, UART_FLAG_FE) ||  
            __HAL_UART_GET_FLAG(huart, UART_FLAG_NE) )== SET )   //判断是否是帧错误或者噪音中断
        {
            __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_FE);
            __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_NE);

            huart->ErrorCode &= HAL_UART_ERROR_FE; // 清除错误标志位
            huart->ErrorCode &= HAL_UART_ERROR_NE; // 清除错误标志位
            HAL_UART_DMAStop(huart); // 关闭DMA
            HAL_UART_Receive_DMA(huart,RX1_Buf,DMA_BUF_SIZE);// 重新启动DMA
        }
        if(__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE) != RESET )   //判断是否是空闲中断
        {
            __HAL_UART_CLEAR_IDLEFLAG(huart);    //清除空闲中断标志（否则会一直不断进入中断）

            //计算接收到的数据长度 : BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx)
            uint16_t Length =  DMA_BUF_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx) - RX1_Offset;
						for(uint16_t _k=0;_k<Length;_k++){ while(!(USART3->SR & USART_SR_TXE)); USART3->DR = RX1_Buf[RX1_Offset+_k]; } 
            RX1_Offset += Length; 
					  //printf("ILength=%d\n",Length);					   
        }
    }
		else if(huart->Instance == USART3)                                  //判断是否是串口3
    {
			  if((__HAL_UART_GET_FLAG(huart, UART_FLAG_FE) ||  
            __HAL_UART_GET_FLAG(huart, UART_FLAG_NE) )== SET )   //判断是否是帧错误或者噪音中断
        {
            __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_FE);
            __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_NE);

            huart->ErrorCode &= HAL_UART_ERROR_FE; // 清除错误标志位
            huart->ErrorCode &= HAL_UART_ERROR_NE; // 清除错误标志位
            HAL_UART_DMAStop(huart); // 关闭DMA
            HAL_UART_Receive_DMA(huart,RX3_Buf,DMA_BUF_SIZE);// 重新启动DMA
        }
        if(__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE) != RESET )   //判断是否是空闲中断
        {
            __HAL_UART_CLEAR_IDLEFLAG(huart);    //清除空闲中断标志（否则会一直不断进入中断）

            //计算接收到的数据长度 : BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx)
            uint16_t Length =  DMA_BUF_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart3_rx) - RX3_Offset;
						// 这里启动TIM4 开始计时， 用来检测接收到数据包后，多久要处理 ，设立使用10ms即可
            HAL_TIM_Base_Start_IT(&htim4); // 使能定时器4中断 
						if(Enqueue_Bytes(&QUART3,RX3_Buf+RX3_Offset,Length) <0){
							printf("FIFO Full\n");}
            RX3_Offset += Length; 
					  //printf("ILength=%d\n",Length);					   
        }
    }
		
}

/* USER CODE END 1 */
