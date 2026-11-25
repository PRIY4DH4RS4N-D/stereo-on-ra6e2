#include "sd_check.h"
#include "diagnostics.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"

volatile sd_check_result_t g_sd_check_result;
volatile DSTATUS g_sd_card_status;
volatile DRESULT g_sd_sector_result;
volatile FRESULT g_sd_fatfs_result;
volatile UINT g_sd_sample_bytes;

static FATFS volume;
static BYTE sector[512];

void sd_check_run(void)
{
    g_sd_check_result = SD_CHECK_RUNNING;
    g_sd_card_status = disk_initialize(0);
    if (g_sd_card_status != 0)
    {
        g_sd_check_result = SD_CHECK_NO_CARD;
        return;
    }

    diag_text("[CHECK] Reading raw sector 0 with CRC validation\n");
    g_sd_sector_result = disk_read(0, sector, 0, 1);
    if (g_sd_sector_result != RES_OK)
    {
        g_sd_check_result = SD_CHECK_SECTOR_FAILED;
        return;
    }
    diag_text("[CHECK] Raw sector 0 read OK. Mounting FAT volume\n");
    g_sd_fatfs_result = f_mount(&volume, "0:", 1);
    if (g_sd_fatfs_result != FR_OK)
    {
        g_sd_check_result = SD_CHECK_MOUNT_FAILED;
        return;
    }

    diag_text("[CHECK] FAT volume mounted. Looking for optional /STEREO.BIN\n");
    FIL sample;
    g_sd_fatfs_result = f_open(&sample, "0:/STEREO.BIN", FA_READ);
    if (g_sd_fatfs_result == FR_NO_FILE)
    {
        g_sd_check_result = SD_CHECK_PASS_VOLUME;
        return;
    }
    if (g_sd_fatfs_result != FR_OK)
    {
        g_sd_check_result = SD_CHECK_FILE_FAILED;
        return;
    }

    UINT bytes = 0;
    g_sd_fatfs_result = f_read(&sample, sector, sizeof(sector), &bytes);
    g_sd_sample_bytes = bytes;
    FRESULT closed = f_close(&sample);
    if (g_sd_fatfs_result == FR_OK)
    {
        g_sd_fatfs_result = closed;
    }
    if (g_sd_fatfs_result != FR_OK)
    {
        g_sd_check_result = SD_CHECK_FILE_FAILED;
        return;
    }
    g_sd_check_result = bytes > 0 ? SD_CHECK_PASS_FILE : SD_CHECK_PASS_VOLUME;
}

void sd_check_report(void)
{
    switch (g_sd_check_result)
    {
        case SD_CHECK_PASS_VOLUME:
            diag_text("[PASS] SD detected, sector read OK, FAT mounted. /STEREO.BIN absent or empty.\n");
            break;
        case SD_CHECK_PASS_FILE:
            diag_text("[PASS] SD detected, sector read OK, FAT mounted. Read ");
            diag_u32(g_sd_sample_bytes);
            diag_text(" bytes from /STEREO.BIN.\n");
            break;
        case SD_CHECK_NO_CARD:
            diag_text("[FAIL] Card detection failed. Check SD power and PMOD1 SPI wiring.\n");
            break;
        case SD_CHECK_SECTOR_FAILED:
            diag_text("[FAIL] Card detected, but raw sector 0 could not be read.\n");
            break;
        case SD_CHECK_MOUNT_FAILED:
        case SD_CHECK_FILE_FAILED:
            diag_text(g_sd_check_result == SD_CHECK_MOUNT_FAILED ?
                      "[FAIL] Raw sector read OK, but FAT mount failed. FatFs " :
                      "[FAIL] FAT mounted, but /STEREO.BIN could not be read. FatFs ");
            diag_u32((uint32_t) g_sd_fatfs_result);
            diag_text("\n");
            break;
        default:
            break;
    }
}
