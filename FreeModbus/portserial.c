/*
 * FreeModbus Libary: BARE Port
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id$
 */

#include "port.h"
#include "usart.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"

/* ----------------------- static functions ---------------------------------*/
static void prvvUARTTxReadyISR( void );
static void prvvUARTRxISR( void );

/* ----------------------- Start implementation -----------------------------*/
void vMBPortSerialEnable( BOOL xRxEnable, BOOL xTxEnable )
{
    if(xRxEnable)
    {
			//
			// 如果使用了485控制芯片,那么在此处将485设置为接收模式
			//
			// 低电平  默认为接收模式	
			// 查阅资料得知，进入发送完成中断以后，还要延时2-4ms等待串口数据完全发送出去才使能485接收，不然会导致最后一个字节发送不完
			// 延时多久需要调整 9600波特率 5000 - 8000 , 9000后通信失效, 取中间值6500
			// 延时多久需要调整 115200波特率 600 - 900, 1000后通信失效 , 取中间值750
			//for( __IO uint32_t i=0;i<600;i++);
			HAL_GPIO_WritePin(RS485_WR_GPIO_Port, RS485_WR_Pin, GPIO_PIN_RESET);
			
			__HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);		// 使能接收非空中断
        
    }
    else
    {
        __HAL_UART_DISABLE_IT(&huart2, UART_IT_RXNE);		// 禁能接收非空中断
    }

    if(xTxEnable)
    {
			
			
			// 如果使用了485控制芯片,那么在此处将485设置为发送模式
			//  RS485_WR ENABLE 高电平  发送模式
			HAL_GPIO_WritePin(RS485_WR_GPIO_Port, RS485_WR_Pin, GPIO_PIN_SET);
			
			__HAL_UART_ENABLE_IT(&huart2, UART_IT_TXE);			// 使能发送为空中断
        
    }
    else
    {
        __HAL_UART_DISABLE_IT(&huart2, UART_IT_TXE);		// 禁能发送为空中断
    }
}

BOOL xMBPortSerialInit( UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity )
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = ulBaudRate;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    switch(eParity)
    {
    // 奇校验
    case MB_PAR_ODD:
        huart2.Init.Parity = UART_PARITY_ODD;
        huart2.Init.WordLength = UART_WORDLENGTH_9B;			// 带奇偶校验数据位为9bits
        break;

    // 偶校验
    case MB_PAR_EVEN:
        huart2.Init.Parity = UART_PARITY_EVEN;
        huart2.Init.WordLength = UART_WORDLENGTH_9B;			// 带奇偶校验数据位为9bits
        break;

    // 无校验
    default:
        huart2.Init.Parity = UART_PARITY_NONE;
        huart2.Init.WordLength = UART_WORDLENGTH_8B;			// 无奇偶校验数据位为8bits
        break;
    }
    return HAL_UART_Init(&huart2) == HAL_OK ? TRUE : FALSE;
}

BOOL xMBPortSerialPutByte( CHAR ucByte )
{
    USART2->DR = ucByte;
    return TRUE;
}

BOOL xMBPortSerialGetByte( CHAR * pucByte )
{
    *pucByte = (USART2->DR & (uint16_t)0x00FF);
    return TRUE;
}

/* Create an interrupt handler for the transmit buffer empty interrupt
 * (or an equivalent) for your target processor. This function should then
 * call pxMBFrameCBTransmitterEmpty( ) which tells the protocol stack that
 * a new character can be sent. The protocol stack will then call
 * xMBPortSerialPutByte( ) to send the character.
 */
static void prvvUARTTxReadyISR( void )
{
    pxMBFrameCBTransmitterEmpty(  );
}

/* Create an interrupt handler for the receive interrupt for your target
 * processor. This function should then call pxMBFrameCBByteReceived( ). The
 * protocol stack will then call xMBPortSerialGetByte( ) to retrieve the
 * character.
 */
static void prvvUARTRxISR( void )
{
    pxMBFrameCBByteReceived(  );
}

void USART2_IRQHandler(void)
{
    if(__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE))			// 接收非空中断标记被置位
    {
        __HAL_UART_CLEAR_FLAG(&huart2, UART_FLAG_RXNE);		// 清除中断标记
        prvvUARTRxISR();										// 通知modbus有数据到达
    }

    if(__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE) && 
       __HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC))	// 发送为空中断标记被置位
    {
        __HAL_UART_CLEAR_FLAG(&huart2, UART_FLAG_TXE);		// 清除中断标记
        prvvUARTTxReadyISR();									// 通知modbus数据可以发送
    }
}
