#ifndef __WIFI4G_H
#define __WIFI4G_H
#include <stdint.h>
#include "main.h"
#include "fifo.h"

enum
{
  WIFI4G_NOT = 0, // Not found
  WIFI4G_OK,
  WIFI4G_ERROR
};

extern __IO uint8_t WIFI4G_CMD_Status; // OK状态标志位

extern uint8_t Parse_Substr[];

uint8_t Test_WIFI4G_CMD_Status(uint32_t Timeout);
extern uint8_t Downlink_Buf[];
extern volatile uint8_t Downlink_Ready_Flag;
extern volatile uint16_t Downlink_Len;
extern volatile uint8_t OTA_Busy;
void Uart3_TxLock(void);
void Uart3_TxUnlock(void);
uint8_t WIFI4G_Parse_Queue(sequeue_t *sq);
uint8_t ESP8266_Connect_WIFI(void);
void ESP8266_Init(void);
void ESP8266_Test(void);

#endif
