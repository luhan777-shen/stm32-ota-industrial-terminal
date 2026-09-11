#ifndef __MQTT_H_
#define __MQTT_H_

#include "main.h"
#include "fifo.h"

// 宏的值为1  , 使能WIFI
// 宏的值为0  , 使能4G
#define  MQTT_WIFI_4G_ENABLE    0

/* ThingsBoard 服务器与设备令牌（开机自动配置 4G 模块） */
#define THINGSBOARD_HOST   "xthings.cloud"
#define THINGSBOARD_PORT   "1883"
#define THINGSBOARD_TOKEN  "WPeGVvXa06Dg1wR2h56J"


/* ThingsBoard 下行主题（订阅） */
#define SUB1 "v1/devices/me/rpc/request/+"     // 下行主题1：接收服务器的 RPC 命令
#define SUB2 "v1/devices/me/attributes"        // 下行主题2：接收服务器的 OTA 信息数据
#define SUB3 "v2/fw/response/+/chunk/#"        // 下行主题3：传输 OTA 固件块内容

/* ThingsBoard 上行主题（发布） */
#define PUB1 "v1/devices/me/telemetry"         // 上行主题1：上传遥测数据
#define PUB2 "v1/devices/me/rpc/response/"     // 上行主题2：回复服务器下发的命令
#define PUB3 "v2/fw/request/+/chunk/#"         // 上行主题3：请求服务器下发固件块

/* OTA 固件信息结构体 */
typedef struct
{
		char fw_title[32];
		char fw_version[32];
		uint32_t fw_size;
		char fw_checksum[32];
		uint8_t recv_buf[NSize]; // 保存接收数据的数组
		uint8_t recv_flag  ;     // OTA 接收到固件内容标识位
		uint32_t request_id  ;   // 请求的数据包id号
		uint32_t chunk_id  ;     // 请求的数据块id号
		uint32_t request_bytes ; // 请求下载的字节数
		uint32_t received_bytes; // 接收到的字节数，用于显示进度
}OTA_FW_Info_T ;

extern OTA_FW_Info_T OTA_Info;            // 描述OTA固件信息的结构体
extern  __IO uint8_t MQTT_OTA_FLAG ;      // 请求服务器发送分包固件标识位
extern  __IO uint8_t MQTT_UPLoad_Flag  ;  // 数据上传服务器标志位， 每隔5秒上传1次
extern  __IO uint8_t MQTT_Download_Flag ; // 开始接收服务器下发的命令，开始解析的标志位

uint8_t ESP8266_Connect_MQTTServer(void);
uint8_t MQTT_SendData(uint8_t *topic,uint32_t dataNum,char *status);
uint8_t MQTT_Parse_JsonData(uint8_t *json);
uint8_t MQTT_Connect_Server(void);
uint8_t CAT1_Parse_ATComander(uint8_t *recvbuf);
uint8_t MQTT_Parse_DeviceData(uint8_t *json,uint32_t dataNum);
uint8_t MQTT_Parse_OTAData(uint8_t *json);
void Update_Progress(uint32_t len);
uint8_t MQTT_OTA_GetFW(void);

#endif //  __MQTT_H_
