#include "modbus.h"
#include <stdint.h>
#include <stdio.h>
#include "gpio.h"
#include "aht20.h"
#include "ina226.h"
#include "adc.h"
#include "i2c.h"
#include "mb.h"
#include "canfestival.h"
#include "testslave.h"

// 定义1个 500ms 定时时间到标志位, 刷新各种硬件的状态   
__IO uint8_t TIM1_Timeout_Flag = 0 ; 
uint8_t SlaveAddress = 1  ; // 设备的默认地址为1
__IO uint8_t Modify_SlaveAddress_Flag = 0 ; // 修改从机地址标志位




uint8_t Write_SlaveAddress(uint8_t addr,uint8_t data)
{
		// i  是eeprom的内存地址 
		// AT24C02_ADDRESS_WRITE 是24c02的地址
		// 1000 是超时时间 
		HAL_I2C_Mem_Write(&hi2c1, AT24C02_ADDRESS_WRITE,addr,I2C_MEMADD_SIZE_8BIT,&data,1, 1000); 
		return SET;
}

uint8_t Read_SlaveAddress(uint8_t addr,uint8_t *pdata)
{
		// i  是eeprom的内存地址 
		// AT24C02_ADDRESS_READ 是24c02的地址
		// 1000 是超时时间 
		HAL_I2C_Mem_Read(&hi2c1, AT24C02_ADDRESS_READ,addr,I2C_MEMADD_SIZE_8BIT,pdata,1,1000); 
		// eeprom 刚开始没有被写入过， 初始值是0 
		if( (*pdata == 0 )||(*pdata == 0xff) )
		{
				*pdata = 1; 
		}
		return SET;
}


void Modbus_Init(void)
{
		// 程序执行后， 会自动从EEPROM读取设备的地址
		Read_SlaveAddress(0,&SlaveAddress);
		printf("SlaveAddress=%d\n",SlaveAddress);
		eMBInit(MB_RTU, SlaveAddress, 0, 115200, MB_PAR_NONE);		// 初始化modbus为RTU方式，波特率115200，无校验
		eMBEnable();
}



void Modbus_Parse(void)
{
		
		
		if(Modify_SlaveAddress_Flag) 
		{
				Modify_SlaveAddress_Flag = 0 ;
				SlaveAddress = REG_HOLD_BUF[9] ;
				//printf("SlaveAddress=%d\n",SlaveAddress);
				Write_SlaveAddress(0,SlaveAddress); 
				HAL_Delay(10);
				eMBDisable();
				eMBClose(); 
				Modbus_Init();	
			
				setNodeId(&TestSlave_Data, SlaveAddress); // 设置CAN总线的从机地址
		}
		

	if(REG_HOLD_BUF[0] & LED1_CMD)   
		{
				LED_Control(LED1,ON);
		}
		else 
		{
				LED_Control(LED1,OFF);
		}
		/***************************************/
		if(REG_HOLD_BUF[0] & LED2_CMD)   
		{
				LED_Control(LED2,ON);
		}
		else 
		{
				LED_Control(LED2,OFF);
		}
		/***************************************/
		if(REG_HOLD_BUF[0] & BEEP_CMD)   
		{
				BEEP_Control(ON);
		}
		else 
		{
				BEEP_Control(OFF);
		}
		/***************************************/
		if(REG_HOLD_BUF[0] & RELAY_CMD)   
		{
				RELAY_Control(ON);
		}
		else 
		{
				RELAY_Control(OFF);
		}

if(TIM1_Timeout_Flag)  // 500ms 周期性的执行一次
		{
				TIM1_Timeout_Flag = 0 ;	
				//  刷新各种硬件传感器的状态  
				AHT20_Read(); // 读温湿度				
			
				// 刷新ina226传感器
				INA226_Read();
			
				// 刷新电位器和CPU温度
				ADC_VR_CPU_Read();
		}
}

