
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "stm32f1xx_it.h"
#include "wifi4g.h"
#include "main.h"
#include "usart.h"
#include "oled.h"
#include "gpio.h"
#include "fifo.h"
#include "mqtt.h"

#pragma diag_suppress 870

__IO uint8_t WIFI4G_CMD_Status = 0;	   // 命令执行结果的状态标志位
__IO uint8_t WIFI4G_TouChuan_Flag = 0; // 透传标志

uint8_t Parse_Substr[32] = {0}; // 用来判断指令成功的状态

uint8_t Test_WIFI4G_CMD_Status(uint32_t Timeout)
{
	while (WIFI4G_CMD_Status == WIFI4G_NOT) // 没找到继续找
	{
		if ((Timeout--) == 0)
			return WIFI4G_NOT; // 超时时间到 ， 函数返回
		HAL_Delay(1);
	}
	return WIFI4G_CMD_Status;
}

// 这里要使用一个全局变量 Parse_Substr, 表示要解析的数组
uint8_t RecvBuf[NSize] = {0};
uint8_t Downlink_Buf[NSize] = {0};       // 下行数据投递缓冲（中断->任务）
volatile uint8_t  Downlink_Ready_Flag = 0; // 下行数据就绪标志
volatile uint16_t Downlink_Len = 0;
volatile uint8_t  OTA_Busy = 0;         // 下行数据长度（二进制安全）
volatile uint8_t huart3_tx_lock = 0;      // huart3 发送自旋锁（任务上下文用）

void Uart3_TxLock(void){ while(huart3_tx_lock){ HAL_Delay(1);} huart3_tx_lock = 1; }
void Uart3_TxUnlock(void){ huart3_tx_lock = 0; }

uint8_t WIFI4G_Parse_Queue(sequeue_t *sq)
{
	// memset(RecvBuf,0,NSize);
	// 把队列中的数据导出到数组内， 便于进行搜索
	int32_t i = 0;
	if (sq != NULL)
	{
		while (!Queue_IsEmpty(sq))
		{
			Dequeue(sq, RecvBuf + i);
			while(!(USART1->SR & USART_SR_TXE)); // 寄存器级直发，避免与 printf 的 HAL 发送冲突
			if(!OTA_Busy)  /* download: skip forward */
			{
				USART1->DR = RecvBuf[i];
			}                    // 把收到的数据进行转发
			i++;
		}
		RecvBuf[i] = 0;
	}

	// ThingsBoard 主题解析移交 DownlinkTask（中断只投递，任务里解析）
	if(MQTT_Download_Flag) // 连接到服务器后开始解析数据
	{
			if(i < sizeof(Downlink_Buf))
			{
					memcpy(Downlink_Buf, RecvBuf, i);   /* 二进制安全：不能用 strcpy */
					Downlink_Len = (uint16_t)i;
					Downlink_Ready_Flag = 1;
			}
			return SET;
	}


	// printf("RecvBuf:%s\n",RecvBuf);
	if (strstr((const char *)RecvBuf, (const char *)Parse_Substr) != NULL) // 找到单词 返回单词的位置
	{
		// printf("found Parse_Substr:%s\n",Parse_Substr);
		return WIFI4G_OK; // 找到 指令返回OK
	}
	else if (strstr((const char *)RecvBuf, (const char *)"ERROR\r\n") != NULL)
	{
		// printf("found Parse_Substr:%s\n","ERROR");
		return WIFI4G_ERROR; // 找到指令返回ERROR
	}
	else
	{
		// printf("not found Parse_Substr:%s\n",Parse_Substr);
		return WIFI4G_NOT; // 没找到返回假
	}
}

void ESP8266_Init(void)
{
	if (ESP8266_Connect_WIFI() == SET)
	{
		printf("WIFI已链接 \n");
	}
}

uint8_t ESP8266_Connect_WIFI(void)
{
	uint8_t buf[64] = {0};
	uint8_t ret;

	printf("******************************************\n");
	strcpy((char *)Parse_Substr, "OK\r\n");
	WIFI4G_CMD_Status = WIFI4G_NOT; // 初始化标志位

	strcpy((char *)buf, "AT+CWMODE=1\r\n"); // 设置无线网卡模式
	HAL_UART_Transmit(&huart3, buf, strlen((char *)buf), 1000);

	if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR)
	{
		return RESET;									// 指令执行失败
	}

	// 链接WIFI，成功后立即返回， 失败后使用SmartConfig 配置新的WIFI
	printf("******************************************\n");
	strcpy((char *)buf, "AT+CWJAP\r\n"); // 接至上次 Wi-Fi 配置中的 AP
	strcpy((char *)Parse_Substr, "OK\r\n");
	WIFI4G_CMD_Status = WIFI4G_NOT; // 初始化标志位
	HAL_UART_Transmit(&huart3, buf, strlen((char *)buf), 1000);

	// 找到指定的字符， wifi链接成功, 超时时间20秒
	ret = Test_WIFI4G_CMD_Status(20 * 1000);
	if (ret == WIFI4G_OK)
	{
		OLED_ShowStr(0, 2, (unsigned char *)"WIFI          OK", 2); // 测试8*16字符
		return SET;													// wifi 链接成功，函数返回
	}
	else if (ret == WIFI4G_NOT) // 指令执行无结果
	{
		return RESET;
	}
	else // ERROR
	{

		// 执行到这里， WIFI链接失败，需要重新配置新的WIFI
		printf("******************************************\n");
		OLED_ShowStr(112, 2, (unsigned char *)"--", 2); // 测试8*16字符
		printf("没有链接wifi，需要链接wifi，请打开airkiss小程序 \r\n");

		strcpy((char *)Parse_Substr, "smartconfig connected wifi\r\n");
		WIFI4G_CMD_Status = WIFI4G_NOT; // 初始化标志位
		// 手机热点设置为WPA2 ，AT命令使用 AT+CWSTARTSMART=3,3
		strcpy((char *)buf, "AT+CWSTARTSMART=3,3\r\n");
		OLED_ShowStr(0, 2, (unsigned char *)"AT+CWSTARTSMART", 2); // 测试8*16字符
		HAL_UART_Transmit(&huart3, buf, strlen((char *)buf), 1000);
		// 找到指定的字符，超时时间200秒 ，
		ret = Test_WIFI4G_CMD_Status(200 * 1000);
		printf("ret=%d\n", ret);
		if (ret == WIFI4G_OK) // 链接上wifi
		{
			OLED_ShowStr(0, 2, (unsigned char *)"WIFI          OK", 2); // 测试8*16字符
			printf("关闭smartconfig\r\n");

			// 关闭 smartconfig
			strcpy((char *)Parse_Substr, "OK\r\n");
			strcpy((char *)buf, "AT+CWSTOPSMART\r\n");
			OLED_ShowStr(0, 2, (unsigned char *)"AT+CWSTOPSMART", 2); // 测试8*16字符
			HAL_UART_Transmit(&huart3, buf, strlen((char *)buf), 1000);

			if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR)
			{
				OLED_ShowStr(112, 2, (unsigned char *)"--", 2); // 测试8*16字符
				return RESET;									// 指令执行失败
			}

			OLED_ShowStr(0, 2, (unsigned char *)"WIFI          OK", 2); // 测试8*16字符
			return SET;													// wifi 链接成功，函数返回
		}
	}
	return SET;
}
uint8_t ESP8266_Connect_TCPServer(void)
{
	uint8_t buf[64] = {0};
	WIFI4G_CMD_Status = WIFI4G_NOT;			// 初始化标志位
	strcpy((char *)buf, "AT+CIPMUX=0\r\n"); // 单链接
	HAL_UART_Transmit(&huart3, buf, strlen((char *)buf), 1000);

	OLED_ShowStr(0, 4, (unsigned char *)"AT+CIPMUX", 2); // 测试8*16字符
	if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR)
	{
		OLED_ShowStr(0, 4, (unsigned char *)"TouChuan...   --", 2); // 测试8*16字符
		return RESET;												// 等待OK返回
	}

	WIFI4G_CMD_Status = WIFI4G_NOT;			 // 初始化标志位
	strcpy((char *)buf, "AT+CIPMODE=1\r\n"); // 设置透传模式
	HAL_UART_Transmit(&huart3, buf, strlen((char *)buf), 1000);

	OLED_ShowStr(0, 4, (unsigned char *)"AT+CIPMODE=1", 2); // 测试8*16字符
	if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR)
	{
		OLED_ShowStr(0, 4, (unsigned char *)"TouChuan...   --", 2); // 测试8*16字符
		return RESET;												// 等待OK返回
	}

	WIFI4G_CMD_Status = WIFI4G_NOT; // 初始化标志位
	// strcpy((char *)buf,"AT+CIPSTART=\"TCP\",\"172.20.10.11\",8000\r\n");
	strcpy((char *)buf, "AT+CIPSTART=\"TCP\",\"192.168.1.186\",8000\r\n");
	HAL_UART_Transmit(&huart3, buf, strlen((char *)buf), 1000);

	OLED_ShowStr(0, 4, (unsigned char *)"AT+CIPSTART", 2); // 测试8*16字符

	// 找到指定的字符， wifi链接成功, 超时时间20秒
	uint8_t ret = Test_WIFI4G_CMD_Status(20 * 1000);
	if (ret == WIFI4G_OK) // 连接到服务器
	{
		strcpy((char *)buf, "AT+CIPSEND\r\n"); // 发送数据 , 开启透传
		HAL_UART_Transmit(&huart3, buf, strlen((char *)buf), 1000);

		OLED_ShowStr(0, 4, (unsigned char *)"TouChuan...   OK", 2); // 测试8*16字符
		OLED_ShowStr(0, 6, (unsigned char *)"Exit TouChuan --", 2); // 测试8*16字符

		WIFI4G_TouChuan_Flag = 1; //  设置透传标志
		return SET;
	}
	else // ERROR  没有连接到服务器，需要重新连接
	{
		OLED_ShowStr(0, 6, (unsigned char *)"TouChuan --", 2); // 测试8*16字符
		return RESET;
	}
}

void ESP8266_Disconnect(void)
{

	uint8_t buf[64] = {0};
	WIFI4G_CMD_Status = WIFI4G_NOT; // 初始化标志位
	strcpy((char *)buf, "+++");
	HAL_UART_Transmit(&huart3, buf, strlen((const char *)buf), 1000);
	OLED_ShowStr(0, 6, (unsigned char *)"+++         ", 2); // 测试8*16字符

	HAL_Delay(100);
	OLED_ShowStr(0, 6, (unsigned char *)"Exit TouChuan", 2); // 测试8*16字符
	printf("Exit TouChuan\n");

	strcpy((char *)buf, "AT+CIPCLOSE\r\n"); // 关闭连接
	HAL_UART_Transmit(&huart3, buf, strlen((char *)buf), 1000);

	WIFI4G_CMD_Status = WIFI4G_NOT; // 初始化标志位
	if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR)
	{
		OLED_ShowStr(0, 6, (unsigned char *)"Exit TouChuan --", 2); // 测试8*16字符
		return;														// 等待OK返回
	}
	OLED_ShowStr(0, 6, (unsigned char *)"Exit TouChuan OK", 2); // 测试8*16字符
	OLED_ShowStr(0, 4, (unsigned char *)"TouChuan...   --", 2); // 测试8*16字符

	// 防止在透传模式下再次按下OK 按键, 做一个标志
	WIFI4G_TouChuan_Flag = 0; // 退出透传模式
}

void ESP8266_Test(void)
{

	if (Key_Value == KEY_OK) // OK 按键 连接服务器
	{
		Key_Value = 0;
		if (WIFI4G_TouChuan_Flag == 0) // 在非透传模式下连接设置透传
		{
			ESP8266_Connect_TCPServer();
		}
	}
	else if (Key_Value == KEY_ESC) // ESC按键 退出透传并断开连接
	{
		Key_Value = 0;
		ESP8266_Disconnect();
	}
	HAL_Delay(200);
}
