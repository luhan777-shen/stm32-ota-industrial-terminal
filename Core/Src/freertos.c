/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "modbus.h"
#include "mb.h"
#include "mqtt.h"
#include "canfestival.h"
#include "testslave.h"
#include "f1can.h"
#include "tim.h"
#include "usart.h"
#include "wifi4g.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
osThreadId ModbusHandle;
osThreadId MQTTHandle;
osThreadId CANopenHandle;
osThreadId DownlinkHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void ModbusTask(void const * argument);
void MQTTTask(void const * argument);
void CANopenTask(void const * argument);
void DownlinkTask(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  printf("[DL] start\r\n");
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of Modbus */
  osThreadDef(Modbus, ModbusTask, osPriorityNormal, 0, 256);
  ModbusHandle = osThreadCreate(osThread(Modbus), NULL);

  /* definition and creation of MQTT */
  osThreadDef(MQTT, MQTTTask, osPriorityNormal, 0, 256);
  MQTTHandle = osThreadCreate(osThread(MQTT), NULL);

  /* definition and creation of CANopen */
  osThreadDef(CANopen, CANopenTask, osPriorityNormal, 0, 256);
  CANopenHandle = osThreadCreate(osThread(CANopen), NULL);
  /* definition and creation of Downlink (ThingsBoard 下行解析，任务上下文) */
  osThreadDef(Downlink, DownlinkTask, osPriorityNormal, 0, 768);
  DownlinkHandle = osThreadCreate(osThread(Downlink), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_ModbusTask */
/**
  * @brief  Function implementing the Modbus thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_ModbusTask */
void ModbusTask(void const * argument)
{
  /* USER CODE BEGIN ModbusTask */
	Modbus_Init();
  /* Infinite loop */
  for(;;)
  {

		eMBPoll();// 轮训查询
			
		Modbus_Parse(); 
		
    osDelay(10);
  }
  /* USER CODE END ModbusTask */
}

/* USER CODE BEGIN Header_MQTTTask */
/**
* @brief Function implementing the MQTT thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_MQTTTask */
void MQTTTask(void const * argument)
{
  /* USER CODE BEGIN MQTTTask */
	MQTT_Connect_Server();

	


  /* Infinite loop */
  for(;;)
  {

				if(MQTT_OTA_FLAG)  // ThingsBoard 下发 OTA 信息，开始下载
		{
			MQTT_OTA_FLAG = 0;
			printf("[OTA] flag seen, start GetFW\r\n");
			MQTT_OTA_GetFW();
		}
		MQTT_SendData((uint8_t *)PUB1, 1, NULL);
    osDelay(5000);
  }
  /* USER CODE END MQTTTask */
}

/* USER CODE BEGIN Header_CANopenTask */
/**
* @brief Function implementing the CANopen thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_CANopenTask */
void CANopenTask(void const * argument)
{
  /* USER CODE BEGIN CANopenTask */
	HAL_TIM_Base_Start_IT(&htim2); // 使能定时器2中断 
	CanFestival_Can_Init();	//初始化can过滤器
	setNodeId(&TestSlave_Data, SlaveAddress);
	setState(&TestSlave_Data, Initialisation);
	setState(&TestSlave_Data, Operational);
  /* Infinite loop */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END CANopenTask */
}

/* DownlinkTask: ThingsBoard 下行解析（RPC/attributes/OTA 块），任务上下文 */
void DownlinkTask(void const * argument)
{
  for(;;)
  {
    if(Downlink_Ready_Flag)
    {
      Downlink_Ready_Flag = 0;
      CAT1_Parse_ATComander(Downlink_Buf);
    }
    osDelay(10);
  }
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

