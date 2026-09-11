/**
  ******************************************************************************
  * @file    w25q32.c
  * @brief   W25Q32JVSSIQ SPI NOR Flash driver (polling mode)
  *          Wiring: PA5=SCK  PA6=MISO  PA7=MOSI  PA4=CS
  ******************************************************************************
  */
#include "main.h"
#include "spi.h"
#include "w25q32.h"
#include <stdio.h>

/* Software CS control */
#define W25Q_CS_LOW()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)
#define W25Q_CS_HIGH()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)

/* Send 1 byte and receive 1 byte simultaneously (SPI full duplex) */
static uint8_t W25Q_SpiByte(uint8_t byte)
{
    uint8_t rx = 0xFF;
    HAL_SPI_TransmitReceive(&hspi1, &byte, &rx, 1, 100);
    return rx;
}

/* Read JEDEC ID: expect EF 40 16 for W25Q32JV */
void W25Q_ReadID(uint8_t *id)
{
    W25Q_CS_LOW();
    W25Q_SpiByte(W25Q_JEDEC_ID);
    id[0] = W25Q_SpiByte(0xFF);
    id[1] = W25Q_SpiByte(0xFF);
    id[2] = W25Q_SpiByte(0xFF);
    W25Q_CS_HIGH();
}

/* Write enable (must be called before every erase/program) */
void W25Q_WriteEnable(void)
{
    W25Q_CS_LOW();
    W25Q_SpiByte(W25Q_WRITE_ENABLE);
    W25Q_CS_HIGH();
}

/* Wait until busy bit clears (status register bit0) */
void W25Q_WaitBusy(void)
{
    W25Q_CS_LOW();
    W25Q_SpiByte(W25Q_READ_STATUS_REG1);
    while (W25Q_SpiByte(0xFF) & 0x01);
    W25Q_CS_HIGH();
}

/* Read data (any length) */
void W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    W25Q_CS_LOW();
    W25Q_SpiByte(W25Q_READ_DATA);
    W25Q_SpiByte((addr >> 16) & 0xFF);
    W25Q_SpiByte((addr >> 8) & 0xFF);
    W25Q_SpiByte(addr & 0xFF);
    for (uint32_t i = 0; i < len; i++)
    {
        buf[i] = W25Q_SpiByte(0xFF);
    }
    W25Q_CS_HIGH();
}

/* Page program (len<=256, caller must not cross 256B page boundary) */
void W25Q_PageProgram(uint32_t addr, uint8_t *buf, uint16_t len)
{
    W25Q_WriteEnable();
    W25Q_CS_LOW();
    W25Q_SpiByte(W25Q_PAGE_PROGRAM);
    W25Q_SpiByte((addr >> 16) & 0xFF);
    W25Q_SpiByte((addr >> 8) & 0xFF);
    W25Q_SpiByte(addr & 0xFF);
    for (uint16_t i = 0; i < len; i++)
    {
        W25Q_SpiByte(buf[i]);
    }
    W25Q_CS_HIGH();
    W25Q_WaitBusy();
}

/* 4K sector erase */
void W25Q_SectorErase(uint32_t addr)
{
    W25Q_WriteEnable();
    W25Q_CS_LOW();
    W25Q_SpiByte(W25Q_SECTOR_ERASE);
    W25Q_SpiByte((addr >> 16) & 0xFF);
    W25Q_SpiByte((addr >> 8) & 0xFF);
    W25Q_SpiByte(addr & 0xFF);
    W25Q_CS_HIGH();
    W25Q_WaitBusy();  /* sector erase typically 45ms */
}

/* Chip erase (test only, takes 20-40s) */
void W25Q_ChipErase(void)
{
    W25Q_WriteEnable();
    W25Q_CS_LOW();
    W25Q_SpiByte(W25Q_CHIP_ERASE);
    W25Q_CS_HIGH();
    W25Q_WaitBusy();
}

/* M1 verification: read ID and print */
void W25Q_Test(void)
{
    uint8_t id[3] = {0};
    W25Q_ReadID(id);
    printf("W25Q ID = %02X %02X %02X\r\n", id[0], id[1], id[2]);
    if (id[0] == 0xEF && id[1] == 0x40 && id[2] == 0x16)
    {
        printf("W25Q32 OK\r\n");
    }
    else
    {
        printf("W25Q FAIL - check wiring / CS pin\r\n");
    }
}
