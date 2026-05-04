#include <stdio.h>
#include <string.h>

#include "freertos.h"
#include "fatfs.h"
#include "usb_device.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_port_fs.h"
#include "lvgl.h"

#include "initial.h"
#include "bsp_sdram.h"
#include "w25q256.h"

#define debug 0

#if debug
static void FatFs_BasicTest(void)
{
  volatile FRESULT mount_fr = FR_OK;            // 挂载结果，断点时先看它是不是 FR_OK。
  volatile FRESULT mkfs_fr = FR_OK;             // 格式化结果，只在空盘自动格式化模式下有意义。
  volatile FRESULT open_write_fr = FR_OK;       // 打开写文件结果。
  volatile FRESULT write_fr = FR_OK;            // 实际写文件结果。
  volatile FRESULT open_read_fr = FR_OK;        // 打开读文件结果。
  volatile FRESULT read_fr = FR_OK;             // 实际读文件结果。
  UINT bytes_written = 0U;             // 实际写入字节数。
  UINT bytes_read = 0U;                // 实际读回字节数。
  volatile uint32_t test_mode = 0U;             // 0=空盘自动格式化后验证，1=只验证已有文件系统。
  volatile uint32_t format_ran = 0U;            // 本轮是否真的执行过格式化。
  BYTE work[4096];                     // FatFs 格式化工作缓冲区。
  char tx[] = "fatfs test on w25q256\r\n"; // 写入 Flash 文件系统的测试字符串。
  char rx[64] = {0};                   // 从文件里读回的内容。
  volatile int cmp_result = -1;                 // strcmp 比较结果，0 说明读写一致。

  memset(rx, 0, sizeof(rx));

  mount_fr = f_mount(&USERFatFS, USERPath, 1);

  if ((mount_fr == FR_NO_FILESYSTEM) && (test_mode == 0U)) {  // 模式 0 下，空白 Flash 会先自动格式化，再继续后面的写读验证。
    format_ran = 1U;
    mkfs_fr = f_mkfs(USERPath, FM_ANY | FM_SFD, 0, work, sizeof(work));
    if (mkfs_fr == FR_OK) {
      mount_fr = f_mount(&USERFatFS, USERPath, 1);
    }
  }

  if (mount_fr == FR_OK) {  // 两种模式只要挂载成功，都会继续做覆盖写和读回比较。
    open_write_fr = f_open(&USERFile, "0:/test.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (open_write_fr == FR_OK) {
      write_fr = f_write(&USERFile, tx, (UINT)(sizeof(tx) - 1U), &bytes_written);
      f_close(&USERFile);
    }
  }

  if ((mount_fr == FR_OK) && (write_fr == FR_OK)
    && (bytes_written == (sizeof(tx) - 1U))) {
    open_read_fr = f_open(&USERFile, "0:/test.txt", FA_READ);
    if (open_read_fr == FR_OK) {
      read_fr = f_read(&USERFile, rx, sizeof(rx) - 1U, &bytes_read);
      f_close(&USERFile);
    }
  }

  if (read_fr == FR_OK) {
    cmp_result = strcmp(tx, rx);
  }
	
  osDelay(1);  // 在这里打断点，直接观察本函数里的局部变量即可。
}

void test(void)
{
  FatFs_BasicTest();
}

#endif

static void FileSystem_Init(void)
{
    FRESULT fr;
    static uint8_t work[4096];

    MX_FATFS_Init();

    fr = f_mount(&USERFatFS, USERPath, 1);
    if (fr == FR_NO_FILESYSTEM) {
        fr = f_mkfs(USERPath, FM_ANY | FM_SFD, 0, work, sizeof(work));
        if (fr != FR_OK) {
            return;
        }

        fr = f_mount(&USERFatFS, USERPath, 1);
        if (fr != FR_OK) {
            return;
        }
    } else if (fr != FR_OK) {
        return;
    }

    f_mkdir("0:/image");
    f_mkdir("0:/note");
    f_mkdir("0:/font");
    f_mkdir("0:/ota");
}

void initial(void)
{
  #if debug
  test();
  #endif

  BSP_SDRAM_Init();
  W25Q256_Init();
  FileSystem_Init();
  MX_USB_DEVICE_Init();
  lv_init();
  lv_port_disp_init();
  lv_port_indev_init();
  lv_port_fs_init();
}