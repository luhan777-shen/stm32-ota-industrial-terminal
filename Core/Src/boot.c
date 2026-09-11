/**
  ******************************************************************************
  * @file    boot.c
  * @brief   OTA Bootloader（8K 区，寄存器级实现，不依赖 HAL/RTOS）
  *          上电流程：读 W25Q 状态扇区 -> 有升级标志则搬运固件到内部 flash
  *          -> 清标志 -> 跳转 APP（0x08002000）
  *          运行环境：HSI 8MHz，中断关闭，仅用局部变量（RW 未初始化！）
  ******************************************************************************
  */
#include "stm32f1xx.h"

/* ---- 内部常量 ---- */
#define W25Q_STATE_SECTOR   0x3FF000UL   /* 状态扇区地址 */
#define W25Q_FW_SLOT        0x000000UL   /* 固件仓库地址 */
#define APP_ADDR            0x08002000UL /* APP 起始地址 */
#define APP_MAX_SIZE        0xE000UL     /* APP 最大 56K */
#define UPGRADE_FLAG        0xAAAAAAAALU

#define CS_LOW()  (GPIOA->BSRR = (uint32_t)GPIO_PIN_4 << 16U)
#define CS_HIGH() (GPIOA->BSRR = GPIO_PIN_4)

/* ---- UART 诊断（8MHz 下 115200，寄存器级） ---- */
static void uart_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;
    GPIOA->CRH = (GPIOA->CRH & ~(0xFUL << 4)) | (0xBUL << 4);  /* PA9 TX AF */
    USART1->BRR = 8000000UL / 115200UL;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE;
}
static void uart_putc(char c)
{
    while (!(USART1->SR & USART_SR_TXE));
    USART1->DR = c;
}
static void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}
static void uart_hex8(uint8_t v)
{
    static const char h[] = "0123456789ABCDEF";
    uart_putc(h[v >> 4]); uart_putc(h[v & 0xF]);
}

/* ---- SPI1 最小驱动（PA5=SCK PA6=MISO PA7=MOSI PA4=CS，9MHz） ---- */
static void spi_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN | RCC_APB2ENR_IOPAEN;

    uint32_t crl = GPIOA->CRL;
    crl &= ~(0xFUL << 16) & ~(0xFUL << 20) & ~(0xFUL << 24) & ~(0xFUL << 28);
    crl |= (0x3UL << 16) | (0xBUL << 20) | (0x8UL << 24) | (0xBUL << 28);
    GPIOA->CRL = crl;                 /* PA4 输出 / PA5 AF / PA6 输入上拉 / PA7 AF */
    CS_HIGH();

    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_SPE | (2UL << 3);
}

static uint8_t spi_byte(uint8_t b)
{
    while (!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = b;
    while (!(SPI1->SR & SPI_SR_RXNE));
    return (uint8_t)SPI1->DR;
}

/* ---- W25Q 最小驱动 ---- */
static void w25q_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint32_t i;
    CS_LOW();
    spi_byte(0x03);
    spi_byte((uint8_t)(addr >> 16));
    spi_byte((uint8_t)(addr >> 8));
    spi_byte((uint8_t)(addr));
    for (i = 0; i < len; i++) buf[i] = spi_byte(0xFF);
    CS_HIGH();
}

static void w25q_wait_busy(void)
{
    CS_LOW();
    spi_byte(0x05);
    while (spi_byte(0xFF) & 0x01);
    CS_HIGH();
}

static void w25q_sector_erase(uint32_t addr)
{
    CS_LOW();
    spi_byte(0x06);                   /* 写使能 */
    CS_HIGH();
    CS_LOW();
    spi_byte(0x20);
    spi_byte((uint8_t)(addr >> 16));
    spi_byte((uint8_t)(addr >> 8));
    spi_byte((uint8_t)(addr));
    CS_HIGH();
    w25q_wait_busy();
}

/* ---- 内部 flash 最小驱动 ---- */
static void flash_unlock(void)
{
    FLASH->KEYR = 0x45670123UL;
    FLASH->KEYR = 0xCDEF89ABUL;
}

static void flash_erase_page(uint32_t addr)
{
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR  = addr;
    FLASH->CR |= FLASH_CR_STRT;
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->CR &= ~FLASH_CR_PER;
}

static void flash_program_half(uint32_t addr, uint16_t h)
{
    FLASH->CR |= FLASH_CR_PG;
    *(volatile uint16_t *)addr = h;
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->CR &= ~FLASH_CR_PG;
}

/* ---- bootloader 主流程 ---- */
void boot_main(void)
{
    uint8_t st[8];
    uint32_t flag, size;

    /* LED 阶段指示：入口 LED1 亮 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    GPIOB->CRH = (GPIOB->CRH & ~0xFFU) | 0x33U;  /* PB8/PB9 输出 */
    GPIOB->BSRR = ((uint32_t)9U << 16U) | 8U;      /* LED1 on, LED2 off */

    uart_init();
    uart_puts("BOOT\r\n");
    spi_init();
    w25q_read(W25Q_STATE_SECTOR, st, 8);
    uart_puts("flag="); uart_hex8(st[0]); uart_hex8(st[1]); uart_hex8(st[2]); uart_hex8(st[3]);
    uart_puts(" size="); uart_hex8(st[4]); uart_hex8(st[5]); uart_hex8(st[6]); uart_hex8(st[7]);
    uart_puts("\r\n");
    flag = (uint32_t)st[0] | ((uint32_t)st[1] << 8) |
           ((uint32_t)st[2] << 16) | ((uint32_t)st[3] << 24);
    size = (uint32_t)st[4] | ((uint32_t)st[5] << 8) |
           ((uint32_t)st[6] << 16) | ((uint32_t)st[7] << 24);

    if ((flag == UPGRADE_FLAG) && (size > 0) && (size <= APP_MAX_SIZE))
    {
        uint8_t buf[256];
        uint32_t off;

        flash_unlock();
        uart_puts("ERASE\r\n");
        /* 擦除 APP 区 */
        for (off = 0; off < size; off += 1024UL)
        {
            flash_erase_page(APP_ADDR + off);
        }
        /* 从 W25Q 搬运到内部 flash（半字编程） */
        for (off = 0; off < size; off += 256UL)
        {
            uint32_t n = ((size - off) >= 256UL) ? 256UL : (size - off);
            uint32_t i;
            w25q_read(W25Q_FW_SLOT + off, buf, n);
            for (i = 0; i < n; i += 2UL)
            {
                uint16_t h = (uint16_t)buf[i] | ((uint16_t)buf[i + 1] << 8);
                flash_program_half(APP_ADDR + off + i, h);
            }
        }
        /* 清除升级标志（擦除状态扇区） */
        w25q_sector_erase(W25Q_STATE_SECTOR);
        GPIOB->BSRR = 9U;   /* 搬运完成：LED1 灭 */
        uart_puts("COPY OK\r\n");
        /* 读回校验：flash 前 16 字节 */
        {
            uint32_t k;
            uart_puts("VFY:");
            for (k = 0; k < 16; k++)
            {
                uart_hex8(*(volatile uint8_t *)(APP_ADDR + k));
                uart_putc(' ');
            }
            uart_puts("\r\n");
        }
    }

    /* 跳转 APP */
    GPIOB->BSRR = (8U << 16U);   /* 跳转前：LED2 亮 */
    uart_puts("JUMP\r\n");
    {
        uint32_t sp = *(volatile uint32_t *)APP_ADDR;
        uint32_t pc = *(volatile uint32_t *)(APP_ADDR + 4UL);
        __set_MSP(sp);
        SCB->VTOR = APP_ADDR;
        ((void (*)(void))pc)();
    }
}
