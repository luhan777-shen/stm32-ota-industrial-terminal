#ifndef __W25Q32_H__
#define __W25Q32_H__

#include <stdint.h>

/* W25Q32JVSSIQ 指令集 */
#define W25Q_WRITE_ENABLE       0x06  /* 写使能 */
#define W25Q_WRITE_DISABLE      0x04  /* 写禁止 */
#define W25Q_READ_STATUS_REG1   0x05  /* 读状态寄存器1（bit0=BUSY） */
#define W25Q_READ_DATA          0x03  /* 读数据 */
#define W25Q_PAGE_PROGRAM       0x02  /* 页写（<=256B，不能跨页） */
#define W25Q_SECTOR_ERASE       0x20  /* 4K 扇区擦除 */
#define W25Q_BLOCK_ERASE_64K    0xD8  /* 64K 块擦除 */
#define W25Q_CHIP_ERASE         0xC7  /* 全片擦除 */
#define W25Q_JEDEC_ID           0x9F  /* 读 ID（应回 EF 40 19） */

/* OTA 分区（W25Q 内部地址） */
#define W25Q_FW_SLOT_ADDR       0x000000U  /* 固件仓库 64K */
#define W25Q_FW_SLOT_SIZE       0x010000U
#define W25Q_STATE_SECTOR_ADDR  0x3FF000U  /* 状态扇区（最后 4K） */

void    W25Q_ReadID(uint8_t *id);
void    W25Q_WriteEnable(void);
void    W25Q_WaitBusy(void);
void    W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len);
void    W25Q_PageProgram(uint32_t addr, uint8_t *buf, uint16_t len);
void    W25Q_SectorErase(uint32_t addr);
void    W25Q_ChipErase(void);
void    W25Q_Test(void);   /* M1 验证函数：读 ID 并打印 */

#endif /* __W25Q32_H__ */
