#ifndef SD_SPI_H
#define SD_SPI_H

#include <stdint.h>
#include <stdbool.h>
#include "fatfs/ff.h"
#include "fatfs/diskio.h"

typedef struct
{
    bool     card_detected;
    bool     is_sdhc;          /* true: SDHC / SDXC (block-addressed), false: SDSC (byte-addressed) */
    uint64_t capacity_bytes;   /* Card raw capacity in bytes */
    uint32_t capacity_mb;      /* Card capacity in Megabytes (MB) */
    uint8_t  mid;              /* Manufacturer ID (e.g., 0x03 for SanDisk) */
    char     oid[3];           /* OEM / Application ID (e.g., "SD") */
    char     pnm[6];           /* Product Name (5 ASCII chars, null-terminated, e.g. "SC32G") */
    uint8_t  prv_major;        /* Product revision major */
    uint8_t  prv_minor;        /* Product revision minor */
    uint32_t psn;              /* Product Serial Number */
    uint16_t mdt_year;         /* Manufacturing Date Year */
    uint8_t  mdt_month;        /* Manufacturing Date Month */
} sd_card_info_t;

/* Function to query card identification and capacity info */
bool sd_get_card_info(sd_card_info_t *info);

/* Reset SPI driver state for re-initialization */
void sd_spi_reset(void);

#endif /* SD_SPI_H */
