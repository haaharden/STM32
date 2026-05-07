#ifndef SYSTEM_STORGE_H
#define SYSTEM_STORGE_H

#include <stdint.h>

typedef struct {
    const char *name;
    uint32_t addr;
    uint32_t size;
    uint32_t crc;
} res_entry_t;

static const res_entry_t g_res_table[] = {
    { "lvgl.image",  0x00000000, 1152004, 0 },
    { "ota_1.bin",   0x00119404, 1152004, 0 },
    { "ota_2.bin",   0x00232808, 1152004, 0 },
};



#endif