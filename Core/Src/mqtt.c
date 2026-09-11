#include "mqtt.h"
#include "wifi4g.h"
#include "usart.h"
#include "oled.h"
#include "modbus.h"
#include "gpio.h"
#include "cjson.h"
#include "crc32.h"
#include "w25q32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__IO uint8_t MQTT_UPLoad_Flag  = 0 ; // 数据上传服务器标志位， 每隔5秒上传1次
__IO uint8_t MQTT_Download_Flag = 0 ;  // 开始接收服务器下发的命令，开始解析的标志位
__IO uint8_t MQTT_OTA_FLAG = 0; //  请求服务器发送分包固件标识位

OTA_FW_Info_T OTA_Info ; // 描述OTA固件信息的结构体


uint8_t *Get_CPUID(void)
{
	static uint8_t cpuid[32] = {0};
	// 每个芯片都有唯一的 96_bit unique ID
	uint32_t CPU_ID[3];
	CPU_ID[0] = *(__IO uint32_t *)(0X1FFFF7F0); // 高字节
	CPU_ID[1] = *(__IO uint32_t *)(0X1FFFF7EC); //
	CPU_ID[2] = *(__IO uint32_t *)(0X1FFFF7E8); // 低字节

	/* 芯片的唯一ID */
	sprintf((char *)cpuid, "%08X%08X%08X", CPU_ID[0], CPU_ID[1], CPU_ID[2]);
	return cpuid;
}

uint8_t ESP8266_Connect_MQTTServer(void)
{
	uint8_t buf[64] = {0};
	OLED_ShowStr(0,4,(unsigned char *)"MQTTServer... --",2);	//测试8*16字符
	printf("cpuid:%s\n",Get_CPUID());

	strcpy((char *)Parse_Substr,"OK\r\n");
	WIFI4G_CMD_Status = WIFI4G_NOT;			// 初始化标志位
	sprintf((char *)buf, "AT+MQTTUSERCFG=0,1,\"%s1\",\"\",\"\",0,0,\"\"\r\n",Get_CPUID()); // 单链接
	printf("buf=%s\n",buf);
	HAL_UART_Transmit(&huart3, buf, strlen((char *)buf), 1000);
	if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR)
	{
		return RESET;												// 等待OK返回
	}

	// 连接mqtt服务器
	WIFI4G_CMD_Status = WIFI4G_NOT ;	 // 初始化标志位
	strcpy((char *)buf,"AT+MQTTCONN=0,\"broker.emqx.io\",1883,1\r\n");
	HAL_UART_Transmit(&huart3,buf,strlen((char *)buf),1000);
	if(Test_WIFI4G_CMD_Status(5*1000) == WIFI4G_ERROR)  {
					return RESET ;
	}

	//  订阅主题消息 , 订阅一个下行数据的主题
	WIFI4G_CMD_Status = WIFI4G_NOT ;	 // 初始化标志位
	OLED_ShowStr(0,4,(unsigned char *)"AT+MQTTSUB",2);				//测试8*16字符
	sprintf((char *)buf,"AT+MQTTSUB=0,\"STM32V9/DownLoad/%s\",0\r\n",Get_CPUID());
	HAL_UART_Transmit(&huart3,buf,strlen((char *)buf),1000);

	uint8_t ret = Test_WIFI4G_CMD_Status(5*1000) ;
	if (ret == WIFI4G_OK)
	{
			OLED_ShowStr(0,4,(unsigned char *)"MQTTServer... OK",2);	//测试8*16字符
			return SET;
	}
	else
	{
			OLED_ShowStr(0,4,(unsigned char *)"MQTTServer... --",2);	//测试8*16字符
			return  RESET;
	}
}

/* ThingsBoard 版 ML307 连接流程：
   服务器地址/token 已保存在 4G 模块内部（AT+REST 后自动连接），
   这里只做：多主题使能 + 不过滤 + 订阅三个下行主题 + 复位重连 */
uint8_t ML307_Connect_MQTTServer(void)
{
	uint8_t buf[128] = {0};
	uint8_t ret;
	OLED_ShowStr(0,4,(unsigned char *)"MQTTServer... --",2);
	/* Configure ThingsBoard server and device token (auto-config module on boot) */
	WIFI4G_CMD_Status = WIFI4G_NOT;
	strcpy((char *)Parse_Substr,"OK\r\n");
	sprintf((char *)buf,"AT+MQTT=\"%s\",\"%s\",\"\"\r\n",THINGSBOARD_TOKEN,THINGSBOARD_TOKEN);
	HAL_UART_Transmit(&huart3,buf,strlen((char *)buf),1000);
	if(Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR)  {
			return RESET; }

	WIFI4G_CMD_Status = WIFI4G_NOT;
	sprintf((char *)buf,"AT+MQTTIP=\"%s\",\"%s\"\r\n",THINGSBOARD_HOST,THINGSBOARD_PORT);
	HAL_UART_Transmit(&huart3,buf,strlen((char *)buf),1000);
	if(Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR)  {
			return RESET; }

		/* 配置 DTU 数据任务（透传数据自动发布到已配置的主题） */
	WIFI4G_CMD_Status = WIFI4G_NOT;
	sprintf((char *)buf,"AT+DTUTASK=\"1\",\"20\"\r\n");
	HAL_UART_Transmit(&huart3,buf,strlen((char *)buf),1000);
	if(Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR)  {
			return RESET; }

/* 使能多通道订阅与发布主题 */
	WIFI4G_CMD_Status = WIFI4G_NOT;
	strcpy((char *)Parse_Substr,"OK\r\n");
	sprintf((char *)buf,"AT+MQMULTEN=1\r\n");
	HAL_UART_Transmit(&huart3,buf,strlen((char *)buf),1000);
	if(Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR)  {
			return RESET; }

	/* 不过滤任何主题 */
	WIFI4G_CMD_Status = WIFI4G_NOT;
	sprintf((char *)buf,"AT+MQTTFILTER=0\r\n");
	HAL_UART_Transmit(&huart3,buf,strlen((char *)buf),1000);
	if(Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR)  {
			return RESET; }

	/* 订阅下行主题1：RPC 命令 */
	WIFI4G_CMD_Status = WIFI4G_NOT;
	sprintf((char *)buf,"AT+MQSUBM=0,1,0,4,\"%s\"\r\n",SUB1);
	HAL_UART_Transmit(&huart3,buf,strlen((char *)buf),1000);
	if(Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR)  {
			return RESET; }

	/* 订阅下行主题2：OTA 信息数据 */
	WIFI4G_CMD_Status = WIFI4G_NOT;
	sprintf((char *)buf,"AT+MQSUBM=1,1,0,4,\"%s\"\r\n",SUB2);
	HAL_UART_Transmit(&huart3,buf,strlen((char *)buf),1000);
	if(Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR)  {
			return RESET; }

	/* 订阅下行主题3：OTA 固件块 */
	WIFI4G_CMD_Status = WIFI4G_NOT;
	sprintf((char *)buf,"AT+MQSUBM=2,1,0,4,\"%s\"\r\n",SUB3);
	HAL_UART_Transmit(&huart3,buf,strlen((char *)buf),1000);
	if(Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR)  {
			return RESET; }

		/* 配置上行发布主题（透明发布模式：串口直发数据自动发布到该主题） */
	WIFI4G_CMD_Status = WIFI4G_NOT;
	sprintf((char *)buf,"AT+MQPUB=\"1\",\"1\",\"4\",\"%s\"\r\n",PUB1);
	HAL_UART_Transmit(&huart3,buf,strlen((char *)buf),1000);
	if(Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR)  {
			return RESET; }

/* 复位4G模组, 复位后自动连接服务器 */
	WIFI4G_CMD_Status = WIFI4G_NOT;
	strcpy((char *)Parse_Substr,"MQTT_CONNECT:");	 // 服务器连接成功的返回值
	sprintf((char *)buf,"AT+REST\r\n");
	HAL_UART_Transmit(&huart3,buf,strlen((char *)buf),1000);
	ret = Test_WIFI4G_CMD_Status(180*1000) ;  // 延时180秒
	if (ret == WIFI4G_OK)
	{
			OLED_ShowStr(0,4,(unsigned char *)"MQTTServer... OK",2);
			return SET;
	}
	else
	{
			OLED_ShowStr(0,4,(unsigned char *)"MQTTServer... --",2);
			return  RESET;
	}
}

uint8_t MQTT_Connect_Server(void)
{
		#if MQTT_WIFI_4G_ENABLE
		// 连接wifi
		ESP8266_Connect_WIFI();
		// 连接mqtt服务器
		if( ESP8266_Connect_MQTTServer() == SET)
		{
				MQTT_Download_Flag = 1 ; //  开启解析 服务器数据的标志位
				printf("MQTT 服务器连接成功  \n");
		}

		#else
		// 连接mqtt服务器
		if( ML307_Connect_MQTTServer() == SET)
		{
				MQTT_Download_Flag = 1 ; //  开启解析 服务器数据的标志位
				printf("MQTT 服务器连接成功  \n");
		}

		#endif
		return SET;
}

/* 按主题发布消息：
   topic=PUB1 遥测/dataNum 1；topic=PUB2 RPC应答/dataNum=请求id；topic=PUB3 请求固件块 */
uint8_t MQTT_SendData(uint8_t *topic,uint32_t dataNum,char *status)
{
	uint8_t sendbuf[128] = {0};
	uint8_t buf[256] = {0};
	Uart3_TxLock();  // 防止多任务并发发 huart3

	if(strncmp((char *)topic,PUB1,strlen(PUB1)-1) == 0 )
	{
		  if(dataNum == 1) // 上传传感器数据（逐键分条发布，避开逗号切分）
			{
				uint8_t one[64] = {0};
				const char *keys[7] = {"TP","RH","VO","CU","PW","VR","CPU"};
				uint16_t vals[7]  = {REG_HOLD_BUF[1],REG_HOLD_BUF[2],REG_HOLD_BUF[3],
									 REG_HOLD_BUF[4],REG_HOLD_BUF[5],REG_HOLD_BUF[6],
									 REG_HOLD_BUF[7]};
				for(uint8_t i=0;i<7;i++)
				{
					sprintf((char *)one,"MQPUB,1,%s,{%s:%d}\r\n",PUB1,keys[i],vals[i]);
					HAL_UART_Transmit(&huart3,one,strlen((char *)one),1000);
					HAL_Delay(300);  /* 间隔慢发，避免连发导致 MQTT 断连 */
				}
			}
			else if(dataNum == 2)  // ota 状态更新
			{
				sprintf((char *)sendbuf,"{\"fw_state\":\"UPDATED\"}");
				sprintf((char *)buf,"MQPUB,1,%s,%s",PUB1,sendbuf);
			}
			else if(dataNum == 3)  // ota 失败上报
			{
				sprintf((char *)sendbuf,"{\"fw_state\":\"FAILED\"}");
				sprintf((char *)buf,"MQPUB,1,%s,%s",PUB1,sendbuf);
			}
	}
	else if(strncmp((char *)topic,PUB2,strlen(PUB2)-1) == 0 )
	{
			// 上行主题2   回复服务器下发的命令 PUB2  "v1/devices/me/rpc/response/"
			sprintf((char *)buf,"MQPUB,1,%s%d,%s",PUB2,dataNum,status);
	}
	else if(strncmp((char *)topic,PUB3,strlen(PUB3)-1) == 0 )
	{
			// 上行主题3   请求下发固件块 "v2/fw/request/%d/chunk/%d"
			sprintf((char *)buf,"MQPUB,1,v2/fw/request/%d/chunk/%d,%s",
				OTA_Info.request_id,OTA_Info.chunk_id,status);
	}

		HAL_UART_Transmit(&huart3,(uint8_t*)buf,strlen((char *)buf),1000);
	Uart3_TxUnlock();
	return SET;
}

uint8_t MQTT_Parse_JsonData(uint8_t *json)
{
		cJSON *cjson_device = NULL;
		cJSON *cjson_led1 = NULL;
		cJSON *cjson_led2 = NULL;
		cJSON *cjson_beep = NULL;
		cJSON *cjson_relay = NULL;
		    /* 解析整段JSO数据 */
    cjson_device = cJSON_Parse((char *)json);
    if(cjson_device == NULL)
    {
        printf("parse fail.\n");
        return RESET;
    }

    /* 依次根据名称提取JSON数据（键值对） */
    cjson_led1 = cJSON_GetObjectItem(cjson_device, "LED1");
		cjson_led2 = cJSON_GetObjectItem(cjson_device, "LED2");
		cjson_beep = cJSON_GetObjectItem(cjson_device, "BEEP");
		cjson_relay = cJSON_GetObjectItem(cjson_device, "RELAY");

		if(cjson_led1 != NULL)
		{
				if(cjson_led1->valueint == 0 ) // led1 off
				{
						REG_HOLD_BUF[0] = REG_HOLD_BUF[0] & (~LED1_CMD) ;  // 第0位 写0
				}
				else
				{
						REG_HOLD_BUF[0] = REG_HOLD_BUF[0] | LED1_CMD    ; // 第0位 写1
				}

		}

		if(cjson_led2 != NULL)
		{
				if(cjson_led2->valueint == 0 ) // led2 off
				{
						REG_HOLD_BUF[0] = REG_HOLD_BUF[0] & (~LED2_CMD) ;  // 第1位 写0
				}
				else
				{
						REG_HOLD_BUF[0] = REG_HOLD_BUF[0] | LED2_CMD    ; // 第1位 写1
				}

		}

		if(cjson_beep != NULL)
		{
				if(cjson_beep->valueint == 0 ) // beep off
				{
						REG_HOLD_BUF[0] = REG_HOLD_BUF[0] & (~BEEP_CMD) ;  // 第2位 写0
				}
				else
				{
						REG_HOLD_BUF[0] = REG_HOLD_BUF[0] | BEEP_CMD    ; // 第2位 写1
				}

		}

		if(cjson_relay != NULL)
		{
				if(cjson_relay->valueint == 0 ) // relay off
				{
						REG_HOLD_BUF[0] = REG_HOLD_BUF[0] & (~RELAY_CMD) ;  // 第3位 写0
				}
				else
				{
						REG_HOLD_BUF[0] = REG_HOLD_BUF[0] | RELAY_CMD    ; // 第3位 写1
				}

		}

		cJSON_Delete(cjson_device); // 一定要释放内存
		return SET;
}

/* 按 ThingsBoard 主题解析模块下发的数据：
   1. RPC 请求（v1/devices/me/rpc/request/）
   2. OTA 信息（v1/devices/me/attributes）
   3. OTA 固件块（v2/fw/response/{request_id}/chunk/{chunk_id},） */
uint8_t CAT1_Parse_ATComander(uint8_t *recvbuf)
{
		char *scan = (char *)recvbuf;
		uint8_t got = 0;

		for(;;) /* 一个批次可能含多条消息，逐条扫描 */
		{
				char substr[64];
				char *prpc = strstr(scan, "v1/devices/me/rpc/request/");
				char *patt = strstr(scan, "v1/devices/me/attributes");
				sprintf(substr, "v2/fw/response/%d/chunk/%d,", (int)OTA_Info.request_id, (int)OTA_Info.chunk_id);
				char *pfw = strstr(scan, substr);

				/* 选三条链中最靠前的一条 */
				char *p = NULL; int kind = 0;
				if(prpc && (!patt || prpc < patt) && (!pfw || prpc < pfw)) { p = prpc; kind = 1; }
				else if(patt && (!pfw || patt < pfw)) { p = patt; kind = 2; }
				else if(pfw) { p = pfw; kind = 3; }
				if(p == NULL) break;

				if(kind == 1) /* RPC 命令 */
				{
						uint32_t dataNum = strtol(p + strlen("v1/devices/me/rpc/request/"), NULL, 10);
						char *leftp  = strstr(p, "{");
						char *rightp = strstr(p, "}");
						if(leftp && rightp)
						{
								strncpy((char *)OTA_Info.recv_buf, leftp, (uint32_t)(rightp-leftp+1));
								OTA_Info.recv_buf[rightp-leftp+1] = 0;
								MQTT_Parse_DeviceData((uint8_t*)OTA_Info.recv_buf, dataNum);
								got = 1;
						}
						scan = p + strlen("v1/devices/me/rpc/request/");
				}
				else if(kind == 2) /* OTA 信息 */
				{
						char *leftp  = strstr(p, "{");
						char *rightp = strstr(p, "}");
						if(leftp && rightp)
						{
								strncpy((char *)OTA_Info.recv_buf, leftp, (uint32_t)(rightp-leftp+1));
								OTA_Info.recv_buf[rightp-leftp+1] = 0;
								printf("[JSON] %s\r\n", OTA_Info.recv_buf);
								printf("[RES] %d\r\n", (int)MQTT_Parse_OTAData(OTA_Info.recv_buf));
								got = 1;
						}
						scan = p + strlen("v1/devices/me/attributes");
				}
				else /* OTA 固件块 */
				{
						{
							char *dstart = p + strlen(substr);
							char *dq = strchr(dstart, ',');   /* 跳过长度字段 "256," */
							if(dq) dstart = dq + 1;
							memcpy(OTA_Info.recv_buf, dstart, OTA_Info.request_bytes);
						}
						OTA_Info.recv_flag = 1;
						got = 1;
						break;  /* 二进制固件块：处理后直接结束本次扫描 */
				}
		}
		return got ? SET : RESET;
}

/* 解析 RPC 命令（method/params）并执行 */
uint8_t MQTT_Parse_DeviceData(uint8_t *json,uint32_t dataNum)
{
		cJSON *cjson_device = NULL;
		cJSON *cjson_method = NULL;
		cJSON *cjson_params = NULL;
		/* 解析整段JSON数据 */
		cjson_device = cJSON_Parse((char *)json);
		if (cjson_device == NULL)
		{
			return RESET;
		}
		/* 依次根据名称提取JSON数据（键值对） */
		cjson_method  = cJSON_GetObjectItem(cjson_device, "method");
		cjson_params  = cJSON_GetObjectItem(cjson_device, "params");

		// 获取LED1的状态
		if(cjson_method != NULL && strcmp(cjson_method->valuestring,"led1Status") == 0 ) {
				if (HAL_GPIO_ReadPin(LED1_GPIO_Port, LED1_Pin) == GPIO_PIN_RESET)
				{
						MQTT_SendData((uint8_t *)PUB2,dataNum,"true");
				}else {
						MQTT_SendData((uint8_t *)PUB2,dataNum,"false");
				}
		}
		// 获取beep的状态
		else if(cjson_method != NULL && strcmp(cjson_method->valuestring,"beepStatus") == 0 ) {
				if (HAL_GPIO_ReadPin(BEEP_GPIO_Port, BEEP_Pin) == GPIO_PIN_SET)
				{
						MQTT_SendData((uint8_t *)PUB2,dataNum,"true");
				}else {
						MQTT_SendData((uint8_t *)PUB2,dataNum,"false");
				}
		}
		// 获取继电器的状态
		else if(cjson_method != NULL && strcmp(cjson_method->valuestring,"relayStatus") == 0 ) {
				if (HAL_GPIO_ReadPin(RELAY_GPIO_Port, RELAY_Pin) == GPIO_PIN_SET)
				{
						MQTT_SendData((uint8_t *)PUB2,dataNum,"true");
				}else {
						MQTT_SendData((uint8_t *)PUB2,dataNum,"false");
				}
		}

		// 控制LED1的状态
		if(cjson_method != NULL && strcmp(cjson_method->valuestring,"led1Set") == 0 )
		{
				if(cjson_params->valueint == 1 )
				{
						REG_HOLD_BUF[0]  |= LED1_CMD  ;   // 第0位 写 1
						MQTT_SendData((uint8_t *)PUB2,dataNum,"true");
				}else if(cjson_params->valueint == 0 )
				{
						REG_HOLD_BUF[0]  = REG_HOLD_BUF[0] & (~LED1_CMD)  ; // 第0位 写 0
						MQTT_SendData((uint8_t *)PUB2,dataNum,"false");
				}
		}

		// 控制BEEP的状态
		else if(cjson_method != NULL && strcmp(cjson_method->valuestring,"beepSet") == 0 )
		{
				if(cjson_params->valueint == 1 )
				{
						REG_HOLD_BUF[0]  |= BEEP_CMD  ;   // 第2位 写 1
						MQTT_SendData((uint8_t *)PUB2,dataNum,"true");
				}else if(cjson_params->valueint == 0 )
				{
						REG_HOLD_BUF[0]  = REG_HOLD_BUF[0] & (~BEEP_CMD)  ; // 第2位 写 0
						MQTT_SendData((uint8_t *)PUB2,dataNum,"false");
				}
		}

		// OTA 触发（RPC 通道）
		if(cjson_method != NULL && strcmp(cjson_method->valuestring,"otaStart") == 0 )
		{
				cJSON *cj_size = cJSON_GetObjectItem(cjson_params, "fw_size");
				cJSON *cj_crc  = cJSON_GetObjectItem(cjson_params, "fw_checksum");
				if(cj_size != NULL && cj_crc != NULL)
				{
						strcpy(OTA_Info.fw_title, "rpc");
						strcpy(OTA_Info.fw_version, "1.0");
						strcpy(OTA_Info.fw_checksum, cj_crc->valuestring);
						OTA_Info.fw_size = cj_size->valueint;
						MQTT_OTA_FLAG = 1;
						MQTT_SendData((uint8_t *)PUB2,dataNum,"true");
						printf("[OTA] RPC start: size=%d crc=%s\r\n", (int)OTA_Info.fw_size, OTA_Info.fw_checksum);
				}
		}

		// 控制relay的状态
		if(cjson_method != NULL && strcmp(cjson_method->valuestring,"relaySet") == 0 )
		{
				if(cjson_params->valueint == 1 )
				{
						REG_HOLD_BUF[0]  |= RELAY_CMD  ;   // 第3位 写 1
						MQTT_SendData((uint8_t *)PUB2,dataNum,"true");
				}else if(cjson_params->valueint == 0 )
				{
						REG_HOLD_BUF[0]  = REG_HOLD_BUF[0] & (~RELAY_CMD)  ; // 第3位 写 0
						MQTT_SendData((uint8_t *)PUB2,dataNum,"false");
				}
		}

		cJSON_Delete(cjson_device); // 一定要释放内存
		return SET;
}

/* 解析 OTA 信息（fw_title/fw_version/fw_size/fw_checksum） */
uint8_t MQTT_Parse_OTAData(uint8_t *json)
{
		cJSON *cjson_device = NULL;
		cJSON *cjson_title = NULL;
		cJSON *cjson_version = NULL;
		cJSON *cjson_size = NULL;
		cJSON *cjson_checksum = NULL;

		/* 解析整段JSON数据 */
		cjson_device = cJSON_Parse((char *)json);
		if (cjson_device == NULL)
		{
			return RESET;
		}
		/* 依次根据名称提取JSON数据（键值对） */
		cjson_title  = cJSON_GetObjectItem(cjson_device, "fw_title");
		if(cjson_title == NULL )
		{
				cJSON_Delete(cjson_device);
				return RESET;
		}
		cjson_version  = cJSON_GetObjectItem(cjson_device, "fw_version");
		cjson_size  = cJSON_GetObjectItem(cjson_device, "fw_size");
		cjson_checksum  = cJSON_GetObjectItem(cjson_device, "fw_checksum");
		strcpy(OTA_Info.fw_title,cjson_title->valuestring);
		strcpy(OTA_Info.fw_version,cjson_version->valuestring);
		strcpy(OTA_Info.fw_checksum,cjson_checksum->valuestring);
		OTA_Info.fw_size = cjson_size->valueint;

		MQTT_OTA_FLAG = 1; // 可以处理OTA数据标志位 ， 可以从服务器下载数据包
		printf("[OTA] attrs parsed: size=%d crc=%s\r\n", (int)OTA_Info.fw_size, OTA_Info.fw_checksum);
		cJSON_Delete(cjson_device); // 一定要释放内存
		return SET;
}

/* 每收到数据包时调用（len=256或最后一包） */
void Update_Progress(uint32_t len) {
    OTA_Info.received_bytes += len;
    uint8_t percent = (OTA_Info.received_bytes * 100) / OTA_Info.fw_size;
    char progress[20] = { 0 };
    if(percent > 100 ) percent = 100;
    if(percent == 100)
    {
        sprintf(progress,"APP%s        OK",OTA_Info.fw_version);
    }
    else
    {
        sprintf(progress,"APP%s      %3d%%",OTA_Info.fw_version,percent);
    }
    OLED_ShowStr(0, 6, (uint8_t*)progress, 2);
}

/* OTA 下载状态机：分块请求固件 -> 写 W25Q 仓库 -> CRC 校验 -> 写升级标志 */
uint8_t MQTT_OTA_GetFW(void)
{
    uint32_t size = OTA_Info.fw_size/256;      // 一共要传输多少次
    uint8_t  msglen = OTA_Info.fw_size%256/4;
    if(OTA_Info.fw_size%256 != 0)              // 有余数，多传一次
    {
        size++;
    }

    CRC32_Init(&crc_ctx);

    /* 擦除 W25Q 固件仓库（64K = 16 个 4K 扇区，约 0.7 秒） */
    printf("OTA: erasing W25Q warehouse...\r\n");
    for(uint32_t a = W25Q_FW_SLOT_ADDR; a < W25Q_FW_SLOT_ADDR + W25Q_FW_SLOT_SIZE; a += 4096)
    {
        W25Q_SectorErase(a);
    }

    OTA_Info.received_bytes = 0;
    OTA_Info.request_id  = HAL_GetTick() + (rand() & 0xFFFF);
    OTA_Info.chunk_id    = 0;
    OTA_Info.request_bytes = 256;

    for(uint32_t i = 0; i < size; i++, OTA_Info.chunk_id++)
    {
        char strNumber[10] = {0};
        if((i == size-1) && (OTA_Info.fw_size%256 != 0)) // 最后1包
        {
            OTA_Info.request_bytes = OTA_Info.fw_size%256;
        }
        sprintf(strNumber,"%d",256);
        MQTT_SendData((uint8_t *)PUB3, OTA_Info.request_id, strNumber);

        /* 等待服务器返回固件块（MQTT 任务上下文，忙等可接受；其他任务不受影响） */
        uint32_t Timeout = 10*1000;
        while(OTA_Info.recv_flag == 0)
        {
            if((Timeout--) == 0)
            {
                printf("OTA: chunk %d timeout\r\n", (int)OTA_Info.chunk_id);
                MQTT_SendData((uint8_t *)PUB1, 3, NULL); // FAILED
                OTA_Busy = 0;
                return RESET;
            }
            HAL_Delay(1);
        }
        OTA_Info.recv_flag = 0;
        if(i == 0) printf("chunk0a = %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                OTA_Info.recv_buf[0],OTA_Info.recv_buf[1],OTA_Info.recv_buf[2],OTA_Info.recv_buf[3],
                OTA_Info.recv_buf[4],OTA_Info.recv_buf[5],OTA_Info.recv_buf[6],OTA_Info.recv_buf[7]);
        if(i == 0) printf("chunk0b = %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                OTA_Info.recv_buf[8],OTA_Info.recv_buf[9],OTA_Info.recv_buf[10],OTA_Info.recv_buf[11],
                OTA_Info.recv_buf[12],OTA_Info.recv_buf[13],OTA_Info.recv_buf[14],OTA_Info.recv_buf[15]);
        Update_Progress(OTA_Info.request_bytes);

        /* 写 W25Q 仓库（256 字节对齐，不跨页） */
        if((i == size-1) && (OTA_Info.fw_size%256 != 0)) // 最后1包
        {
            if(OTA_Info.fw_size%256%4 != 0) msglen++;
            CRC32_Update(&crc_ctx, OTA_Info.recv_buf, OTA_Info.fw_size%256);
            W25Q_PageProgram(W25Q_FW_SLOT_ADDR + i*256, OTA_Info.recv_buf, OTA_Info.fw_size%256);
        }
        else
        {
            CRC32_Update(&crc_ctx, OTA_Info.recv_buf, 256);
            W25Q_PageProgram(W25Q_FW_SLOT_ADDR + i*256, OTA_Info.recv_buf, 256);
        }
    }

    /* CRC32 校验 */
    uint32_t crc32 = CRC32_Final(&crc_ctx, 1);
    char crcstr[10] = {0};
    sprintf(crcstr,"%x",crc32);
    printf("OTA: local crc=%s server crc=%s\r\n", crcstr, OTA_Info.fw_checksum);
    if(strncmp(OTA_Info.fw_checksum, crcstr, strlen(OTA_Info.fw_checksum)) == 0)
    {
        /* 读回校验：打印仓库前 16 字节 */
        uint8_t verify[16] = {0};
        W25Q_Read(W25Q_FW_SLOT_ADDR, verify, 16);
        printf("OTA: W25Q head = %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
               verify[0],verify[1],verify[2],verify[3],verify[4],verify[5],verify[6],verify[7]);

        /* 写升级标志到 W25Q 状态扇区（M5 bootloader 会读取） */
        W25Q_SectorErase(W25Q_STATE_SECTOR_ADDR);
        uint8_t flag_buf[8] = {0xAA,0xAA,0xAA,0xAA,
                               (uint8_t)(OTA_Info.fw_size), (uint8_t)(OTA_Info.fw_size>>8),
                               (uint8_t)(OTA_Info.fw_size>>16), (uint8_t)(OTA_Info.fw_size>>24)};
        W25Q_PageProgram(W25Q_STATE_SECTOR_ADDR, flag_buf, 8);
        printf("OTA: CRC OK, flag+size written, reboot to flash...\r\n");

        MQTT_SendData((uint8_t *)PUB1, 2, NULL); // UPDATED
        HAL_Delay(1000);
        HAL_NVIC_SystemReset();
    }
    else
    {
        printf("OTA: CRC FAIL\r\n");
        {
            uint8_t dbg[16];
            W25Q_Read(W25Q_FW_SLOT_ADDR, dbg, 16);
            printf("W25Q head = %02X %02X %02X %02X %02X %02X %02X %02X\r\n", dbg[0],dbg[1],dbg[2],dbg[3],dbg[4],dbg[5],dbg[6],dbg[7]);
            W25Q_Read(W25Q_FW_SLOT_ADDR + OTA_Info.fw_size - 16, dbg, 16);
            printf("W25Q tail = %02X %02X %02X %02X %02X %02X %02X %02X\r\n", dbg[0],dbg[1],dbg[2],dbg[3],dbg[4],dbg[5],dbg[6],dbg[7]);
        }
        OLED_ShowStr(64+16, 6, (uint8_t*)"  Fail", 2);
        MQTT_SendData((uint8_t *)PUB1, 3, NULL); // FAILED
        OTA_Busy = 0;
        return RESET;
    }
    return SET;
}

