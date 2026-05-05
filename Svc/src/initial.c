#include <stdio.h>
#include <string.h>

#include "freertos.h"
#include "fatfs.h"
#include "usb_device.h"
#include "usb_host.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_port_fs.h"
#include "lvgl.h"

#include "initial.h"
#include "bsp_sdram.h"
#include "w25q256.h"
#include "log.h"

#define debug 0

usb_mode_t usb_mode = USB_MODE_IDLE;

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

    //f_mkdir("0:/image");
    //f_mkdir("0:/font");
    f_mkdir("0:/note");
    f_mkdir("0:/ota");
}

void initial(void)
{
  #if debug
  test();
  #endif

  lv_fs_file_t file;
  lv_img_header_t header;
  volatile lv_fs_res_t fs_res = LV_FS_RES_UNKNOWN; // LVGL 的文件系统 API 返回值，断点时先看它是不是 LV_FS_RES_OK。
  volatile lv_res_t res = LV_RES_INV; // LVGL 的返回值，断点时先看它是不是 LV_RES_OK。
  BSP_SDRAM_Init();
  W25Q256_Init();
  FileSystem_Init();
  /*
  usb_mode = USB_MODE_DEVICE;
  MX_USB_DEVICE_Init();
  */
  usb_mode = USB_MODE_HOST;
	MX_USB_HOST_Init();

  lv_init();
  lv_port_disp_init();
  lv_port_indev_init();
  lv_port_fs_init();
	
  LOGI("test start");
  fs_res = lv_fs_open(&file, "F:/chaiqvan.bin", LV_FS_MODE_RD);
  if(fs_res == LV_FS_RES_OK) {
    lv_fs_close(&file);
    LOGI("File opened successfully.");
  }
  res = lv_img_decoder_get_info("F:/chaiqvan.bin", &header);
  if(res == LV_RES_OK) {
    LOGI("Image info retrieved successfully: width=%d, height=%d, color_format=%d", header.w, header.h, header.cf);
  }
  osDelay(1); // 在这里打断点，直接观察本函数里的局部变量即可。
}

/*
void test_host(void)
{
    FIL file;
    static volatile FRESULT frd;
    static volatile UINT br;
    static volatile UINT bw;

    char buf[32] = {0};
    char wbuf[] = "123459876\r\n";

    // 1. 以读写方式打开，不存在就创建，存在就清空
    frd = f_open(&file, "1:/test.txt", FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
    if (frd != FR_OK) {
        return;
    }

    // 2. 写入数据
    frd = f_write(&file, wbuf, strlen(wbuf), &bw);
    if (frd != FR_OK || bw != strlen(wbuf)) {
        f_close(&file);
        return;
    }

    // 3. 同步到U盘，测试时建议加
    frd = f_sync(&file);
    if (frd != FR_OK) {
        f_close(&file);
        return;
    }

    // 4. 文件指针回到开头
    frd = f_lseek(&file, 0);
    if (frd != FR_OK) {
        f_close(&file);
        return;
    }

    // 5. 读出来
    memset(buf, 0, sizeof(buf));

    frd = f_read(&file, buf, sizeof(buf) - 1, &br);
    if (frd == FR_OK) {
        buf[br] = '\0';

    // 打断点看：bw 应该是 11,br 应该是 11,buf 应该是 "123456789\r\n"
    }

    f_close(&file);
}*/
void test_list_usb_root_debug(void)
{
    static FRESULT fr;
    static DIR dir;
    static FILINFO fno;
    static unsigned int file_count;
    static unsigned int dir_count;

    file_count = 0;
    dir_count = 0;

    fr = f_opendir(&dir, "1:/");
    if (fr != FR_OK) {
        return;
    }

    while (1) {
        fr = f_readdir(&dir, &fno);

        if (fr != FR_OK) {
            break;
        }

        if (fno.fname[0] == '\0') {
            break;
        }

        if (fno.fattrib & AM_DIR) {
            dir_count++;
        } else {
            file_count++;
        }

        /*
           在这里打断点看：
           fno.fname
           fno.fsize
           fno.fattrib
           file_count
           dir_count
        */
    }

    f_closedir(&dir);
}
void mount_usb_host(void)
{
  static uint8_t userh_mounted = 0;
	static volatile FRESULT fr;
  if (Appli_state == APPLICATION_READY && userh_mounted == 0) {
      fr = f_mount(&USERHFatFS, USERHPath, 1);
      if (fr == FR_OK) {
          userh_mounted = 1;
          test_list_usb_root_debug();
      }
      
  }

  if (Appli_state == APPLICATION_DISCONNECT && userh_mounted) {
      f_mount(NULL, USERHPath, 0);
      userh_mounted = 0;
  }
}