#include "lv_port_fs.h"

#include <string.h>

#include "ff.h"

static bool fs_ready(lv_fs_drv_t * drv);
static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode);
static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p);
static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br);
static lv_fs_res_t fs_write(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw);
static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence);
static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p);
static void * fs_dir_open(lv_fs_drv_t * drv, const char * path);
static lv_fs_res_t fs_dir_read(lv_fs_drv_t * drv, void * dir_p, char * fn);
static lv_fs_res_t fs_dir_close(lv_fs_drv_t * drv, void * dir_p);
static void fs_build_path(const char *path, char *full_path, uint32_t full_path_size);
static lv_fs_res_t fs_result_to_lv(FRESULT result);

void lv_port_fs_init(void)
{
    static lv_fs_drv_t fs_drv;

    lv_fs_drv_init(&fs_drv);

    // 把 FatFs 的 0: 盘映射成 LVGL 的 F: 盘，界面代码里后面就可以直接访问 F:/xxx。
    fs_drv.letter = 'F';
    fs_drv.ready_cb = fs_ready;
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.write_cb = fs_write;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;
    fs_drv.dir_open_cb = fs_dir_open;
    fs_drv.dir_read_cb = fs_dir_read;
    fs_drv.dir_close_cb = fs_dir_close;

    lv_fs_drv_register(&fs_drv);
}

static bool fs_ready(lv_fs_drv_t * drv)
{
    LV_UNUSED(drv);
    return true;
}

static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
{
    FIL *file;
    FRESULT result;
    BYTE flags = 0U;
    char full_path[LV_FS_MAX_PATH_LENGTH + 4];

    LV_UNUSED(drv);

    if(mode == LV_FS_MODE_WR) {
        flags = FA_WRITE | FA_OPEN_ALWAYS;
    }
    else if(mode == LV_FS_MODE_RD) {
        flags = FA_READ;
    }
    else if(mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) {
        flags = FA_READ | FA_WRITE | FA_OPEN_ALWAYS;
    }
    else {
        return NULL;
    }

    fs_build_path(path, full_path, sizeof(full_path));

    file = lv_mem_alloc(sizeof(FIL));
    if(file == NULL) {
        return NULL;
    }

    result = f_open(file, full_path, flags);
    if(result != FR_OK) {
        lv_mem_free(file);
        return NULL;
    }

    return file;
}

static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p)
{
    FRESULT result;

    LV_UNUSED(drv);

    result = f_close((FIL *)file_p);
    lv_mem_free(file_p);
    return fs_result_to_lv(result);
}

static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br)
{
    FRESULT result;
    UINT read_size = 0U;

    LV_UNUSED(drv);

    result = f_read((FIL *)file_p, buf, btr, &read_size);
    if(br != NULL) {
        *br = read_size;
    }

    return fs_result_to_lv(result);
}

static lv_fs_res_t fs_write(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw)
{
    FRESULT result;
    UINT write_size = 0U;

    LV_UNUSED(drv);

    result = f_write((FIL *)file_p, buf, btw, &write_size);
    if(bw != NULL) {
        *bw = write_size;
    }

    return fs_result_to_lv(result);
}

static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence)
{
    FIL *file = (FIL *)file_p;
    FSIZE_t target = 0U;

    LV_UNUSED(drv);

    switch(whence) {
        case LV_FS_SEEK_SET:
            target = pos;
            break;
        case LV_FS_SEEK_CUR:
            target = f_tell(file) + pos;
            break;
        case LV_FS_SEEK_END:
            target = f_size(file) + pos;
            break;
        default:
            return LV_FS_RES_INV_PARAM;
    }

    return fs_result_to_lv(f_lseek(file, target));
}

static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p)
{
    LV_UNUSED(drv);

    if(pos_p == NULL) {
        return LV_FS_RES_INV_PARAM;
    }

    *pos_p = f_tell((FIL *)file_p);
    return LV_FS_RES_OK;
}

static void * fs_dir_open(lv_fs_drv_t * drv, const char * path)
{
    DIR *dir;
    FRESULT result;
    char full_path[LV_FS_MAX_PATH_LENGTH + 4];

    LV_UNUSED(drv);

    fs_build_path(path, full_path, sizeof(full_path));

    dir = lv_mem_alloc(sizeof(DIR));
    if(dir == NULL) {
        return NULL;
    }

    result = f_opendir(dir, full_path);
    if(result != FR_OK) {
        lv_mem_free(dir);
        return NULL;
    }

    return dir;
}

static lv_fs_res_t fs_dir_read(lv_fs_drv_t * drv, void * dir_p, char * fn)
{
    FRESULT result;
    FILINFO info;

    LV_UNUSED(drv);

    if(fn == NULL) {
        return LV_FS_RES_INV_PARAM;
    }

    fn[0] = '\0';
    do {
        result = f_readdir((DIR *)dir_p, &info);
        if(result != FR_OK) {
            return fs_result_to_lv(result);
        }

        if(info.fname[0] == '\0') {
            return LV_FS_RES_OK;
        }

        if(info.fattrib & AM_DIR) {
            fn[0] = '/';
            strcpy(&fn[1], info.fname);
        }
        else {
            strcpy(fn, info.fname);
        }
    } while(strcmp(fn, "/.") == 0 || strcmp(fn, "/..") == 0);

    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_dir_close(lv_fs_drv_t * drv, void * dir_p)
{
    FRESULT result;

    LV_UNUSED(drv);

    result = f_closedir((DIR *)dir_p);
    lv_mem_free(dir_p);
    return fs_result_to_lv(result);
}

// LVGL 回调里拿到的 path 已经去掉了盘符 S:，这里只负责补回 FatFs 的逻辑盘号 0:。
static void fs_build_path(const char *path, char *full_path, uint32_t full_path_size)
{
    if((path == NULL) || (full_path == NULL) || (full_path_size < 4U)) {
        return;
    }

    if(path[0] == '/') {
        lv_snprintf(full_path, full_path_size, "0:%s", path);
    }
    else if(path[0] == '\0') {
        lv_snprintf(full_path, full_path_size, "0:/");
    }
    else {
        lv_snprintf(full_path, full_path_size, "0:/%s", path);
    }
}

static lv_fs_res_t fs_result_to_lv(FRESULT result)
{
    switch(result) {
        case FR_OK:
            return LV_FS_RES_OK;
        case FR_NO_FILE:
        case FR_NO_PATH:
        case FR_INVALID_NAME:
            return LV_FS_RES_NOT_EX;
        case FR_DENIED:
        case FR_EXIST:
        case FR_WRITE_PROTECTED:
            return LV_FS_RES_DENIED;
        case FR_NOT_READY:
            return LV_FS_RES_HW_ERR;
        case FR_NOT_ENOUGH_CORE:
            return LV_FS_RES_OUT_OF_MEM;
        case FR_INVALID_PARAMETER:
            return LV_FS_RES_INV_PARAM;
        case FR_TIMEOUT:
            return LV_FS_RES_TOUT;
        case FR_LOCKED:
            return LV_FS_RES_LOCKED;
        default:
            return LV_FS_RES_UNKNOWN;
    }
}
