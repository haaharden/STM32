#include "flash_platform.h"

#include <string.h>

// 用一个静态 4KB 缓冲做扇区读改写，避免把 512B 逻辑块写入直接落到底层擦写粒度不匹配的问题。
static DSTATUS g_flash_platform_status = STA_NOINIT;
static uint8_t g_flash_platform_erase_buffer[FLASH_PLATFORM_ERASE_SIZE];

static uint32_t FlashPlatform_SectorToAddress(DWORD sector);
static uint32_t FlashPlatform_AlignEraseAddress(uint32_t address);
static DRESULT FlashPlatform_CheckRange(DWORD sector, UINT count);
static DRESULT FlashPlatform_ResultFromW25Q(uint8_t result);
static DRESULT FlashPlatform_EnsureReady(void);
static DRESULT FlashPlatform_WriteEraseUnit(uint32_t erase_address, uint32_t offset, const uint8_t *buffer, uint32_t length);

DSTATUS FlashPlatform_Initialize(void)
{
  if (W25Q256_Init() == W25Q256_OK) {  // 初始化成功后就把磁盘状态标记成可用。
    g_flash_platform_status = 0U;
  } else {
    g_flash_platform_status = STA_NOINIT;
  }

  return g_flash_platform_status;
}

DSTATUS FlashPlatform_Status(void)
{
  uint8_t status = 0U;

  if ((g_flash_platform_status & STA_NOINIT) != 0U) {  // 还没初始化时不主动触发底层访问，直接回当前状态。
    return g_flash_platform_status;
  }

  status = W25Q256_GetStatus();  // 已初始化后只做轻量状态探测，避免把错误吞掉。
  if ((status != W25Q256_OK) && (status != W25Q256_BUSY)) {
    g_flash_platform_status = STA_NOINIT;
  }

  return g_flash_platform_status;
}

DRESULT FlashPlatform_Read(BYTE *buff, DWORD sector, UINT count)
{
  DRESULT result = FlashPlatform_CheckRange(sector, count);

  if (buff == 0) {  // 读缓冲区为空时不能继续往下转到底层接口。
    return RES_PARERR;
  }

  if (result != RES_OK) {
    return result;
  }

  result = FlashPlatform_EnsureReady();
  if (result != RES_OK) {
    return result;
  }

  return FlashPlatform_ResultFromW25Q(
    W25Q256_Read(FlashPlatform_SectorToAddress(sector), buff, ((uint32_t)count) * FLASH_PLATFORM_SECTOR_SIZE)
  );
}

DRESULT FlashPlatform_Write(const BYTE *buff, DWORD sector, UINT count)
{
  DRESULT result = FlashPlatform_CheckRange(sector, count);
  DWORD current_sector = sector;
  UINT remaining = count;
  const BYTE *current_buffer = buff;

  if (buff == 0) {  // 写缓冲区为空时，部分块读改写路径里的 memcpy 会直接出问题，所以在入口处拦住。
    return RES_PARERR;
  }

  if (result != RES_OK) {
    return result;
  }

  result = FlashPlatform_EnsureReady();
  if (result != RES_OK) {
    return result;
  }

  while (remaining > 0U) {  // 按 4KB 擦除单元分批处理，避免跨擦除单元时把相邻数据写坏。
    uint32_t address = FlashPlatform_SectorToAddress(current_sector);
    uint32_t erase_address = FlashPlatform_AlignEraseAddress(address);
    uint32_t offset = address - erase_address;
    uint32_t max_length = FLASH_PLATFORM_ERASE_SIZE - offset;
    UINT chunk_count = (UINT)(max_length / FLASH_PLATFORM_SECTOR_SIZE);
    uint32_t chunk_length = 0U;

    if (chunk_count > remaining) {
      chunk_count = remaining;
    }

    chunk_length = ((uint32_t)chunk_count) * FLASH_PLATFORM_SECTOR_SIZE;

    if ((offset == 0U) && (chunk_length == FLASH_PLATFORM_ERASE_SIZE)) {  // 恰好整块覆盖时，直接擦整块再回写 4KB，少一次读操作。
      result = FlashPlatform_ResultFromW25Q(W25Q256_EraseSector(erase_address));
      if (result != RES_OK) {
        return result;
      }

      result = FlashPlatform_ResultFromW25Q(W25Q256_Write(erase_address, current_buffer, chunk_length));
      if (result != RES_OK) {
        return result;
      }
    } else {
      result = FlashPlatform_WriteEraseUnit(erase_address, offset, current_buffer, chunk_length);
      if (result != RES_OK) {
        return result;
      }
    }

    current_sector += chunk_count;
    current_buffer += chunk_length;
    remaining -= chunk_count;
  }

  return RES_OK;
}

DRESULT FlashPlatform_Ioctl(BYTE cmd, void *buff)
{
  if ((cmd != CTRL_SYNC) && (buff == 0)) {  // 除了同步命令以外，其它 ioctl 都需要回填参数缓冲区。
    return RES_PARERR;
  }

  switch (cmd) {
    case CTRL_SYNC:
    {
      uint8_t status = 0U;

      if (FlashPlatform_EnsureReady() != RES_OK) {
        return RES_NOTRDY;
      }

      status = W25Q256_GetStatus();
      if (status == W25Q256_BUSY) {
        return RES_NOTRDY;
      }

      return (status == W25Q256_OK) ? RES_OK : RES_ERROR;
    }

    case GET_SECTOR_COUNT:
      *((DWORD *)buff) = (DWORD)FLASH_PLATFORM_SECTOR_COUNT;
      return RES_OK;

    case GET_SECTOR_SIZE:
      *((WORD *)buff) = (WORD)FLASH_PLATFORM_SECTOR_SIZE;
      return RES_OK;

    case GET_BLOCK_SIZE:
      *((DWORD *)buff) = (DWORD)FLASH_PLATFORM_BLOCK_SIZE;
      return RES_OK;

    case CTRL_TRIM:
      return RES_OK;  // 当前不主动擦除 trim 区间，避免非对齐 trim 误伤相邻 512B 逻辑块。

    default:
      return RES_PARERR;
  }
}

static uint32_t FlashPlatform_SectorToAddress(DWORD sector)
{
  return ((uint32_t)sector) * FLASH_PLATFORM_SECTOR_SIZE;  // FatFs 传进来的 sector 是 512B LBA，这里转换成物理地址。
}

static uint32_t FlashPlatform_AlignEraseAddress(uint32_t address)
{
  return address - (address % FLASH_PLATFORM_ERASE_SIZE);  // 任何写入都要先回到所属的 4KB 擦除单元起点。
}

static DRESULT FlashPlatform_CheckRange(DWORD sector, UINT count)
{
  uint32_t sector_count = (uint32_t)sector;
  uint32_t request_count = (uint32_t)count;

  if ((count == 0U) || (sector_count >= FLASH_PLATFORM_SECTOR_COUNT)) {
    return RES_PARERR;
  }

  if (request_count > (FLASH_PLATFORM_SECTOR_COUNT - sector_count)) {  // 用减法判断上界，避免 sector + count 溢出。
    return RES_PARERR;
  }

  return RES_OK;
}

static DRESULT FlashPlatform_ResultFromW25Q(uint8_t result)
{
  switch (result) {
    case W25Q256_OK:
      return RES_OK;

    case W25Q256_INVALID_PARAM:
      return RES_PARERR;

    case W25Q256_BUSY:
      return RES_NOTRDY;

    default:
      return RES_ERROR;
  }
}

static DRESULT FlashPlatform_EnsureReady(void)
{
  if ((g_flash_platform_status & STA_NOINIT) != 0U) {  // 首次进入读写/同步时再懒初始化，方便上层在任意时机调用。
    if (FlashPlatform_Initialize() != 0U) {
      return RES_NOTRDY;
    }
  }

  return RES_OK;
}

static DRESULT FlashPlatform_WriteEraseUnit(uint32_t erase_address, uint32_t offset, const uint8_t *buffer, uint32_t length)
{
  DRESULT result = RES_OK;

  result = FlashPlatform_ResultFromW25Q(W25Q256_Read(erase_address, g_flash_platform_erase_buffer, FLASH_PLATFORM_ERASE_SIZE));
  if (result != RES_OK) {
    return result;
  }

  memcpy(&g_flash_platform_erase_buffer[offset], buffer, length);  // 只覆盖本次更新的 512B 逻辑块区间，其它内容原样保留。

  result = FlashPlatform_ResultFromW25Q(W25Q256_EraseSector(erase_address));
  if (result != RES_OK) {
    return result;
  }

  return FlashPlatform_ResultFromW25Q(W25Q256_Write(erase_address, g_flash_platform_erase_buffer, FLASH_PLATFORM_ERASE_SIZE));
}