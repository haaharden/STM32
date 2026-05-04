#ifndef __FLASH_PLATFORM_H
#define __FLASH_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "diskio.h"
#include "w25q256.h"

// 这里把 W25Q 的物理特性转换成 FatFs 能直接消费的逻辑块信息。
#define FLASH_PLATFORM_SECTOR_SIZE        512UL
#define FLASH_PLATFORM_ERASE_SIZE         W25Q256_ERASE_SIZE
#define FLASH_PLATFORM_SECTOR_COUNT       (W25Q256_TOTAL_SIZE / FLASH_PLATFORM_SECTOR_SIZE)
#define FLASH_PLATFORM_BLOCK_SIZE         (FLASH_PLATFORM_ERASE_SIZE / FLASH_PLATFORM_SECTOR_SIZE)

// 这些函数的语义直接对应 FatFs 的 diskio 五个入口，用户后续只需要在 user_diskio.c 里转调。
DSTATUS FlashPlatform_Initialize(void);
DSTATUS FlashPlatform_Status(void);
DRESULT FlashPlatform_Read(BYTE *buff, DWORD sector, UINT count);
DRESULT FlashPlatform_Write(const BYTE *buff, DWORD sector, UINT count);
DRESULT FlashPlatform_Ioctl(BYTE cmd, void *buff);

#ifdef __cplusplus
}
#endif

#endif