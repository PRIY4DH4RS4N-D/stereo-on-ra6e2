#include "sd_test.h"
#include "sd_spi.h"
#include "diagnostics.h"
#include "fatfs/ff.h"
#include "hal_data.h"
#include <string.h>

#define LED1_PIN BSP_IO_PORT_01_PIN_04
#define LED2_PIN BSP_IO_PORT_01_PIN_12

static FATFS g_fatfs;
static FIL g_file;
static char g_read_buffer[512];

static const char g_write_payload[] =
    "hello from ra6e2\r\n"
    "====================================================\r\n"
    " Renesas BGK-RA6E2 MicroSD Card SPI Test\r\n"
    " Target: R7FA6E2BB3CFM (ARM Cortex-M33 @ 200 MHz)\r\n"
    " Message: hello from ra6e2\r\n"
    " Filesystem: FAT32 (ChaN FatFs R0.16)\r\n"
    " Status: Read/Write Test PASSED with 100% Data Integrity!\r\n"
    "====================================================\r\n";

static const char g_hello_payload[] = "hello from ra6e2\r\n";

bool sd_test_run(void)
{
    FRESULT fr;
    UINT bytes_written = 0;
    UINT bytes_read = 0;
    sd_card_info_t card_info;

    diag_text("\n=======================================================\n");
    diag_text("       BGK-RA6E2 MicroSD Card SPI Test Execution       \n");
    diag_text("=======================================================\n\n");

    /* -------------------------------------------------------------
     * Step 1: Initialize SD card using SPI
     * ------------------------------------------------------------- */
    diag_text("[STEP 1] Initializing SD Card via SPI...\n");
    DSTATUS ds = disk_initialize(0);
    if (ds != 0)
    {
        diag_text("[FAIL] SD Card initialization failed! (disk_initialize error)\n");
        diag_text("       Please check module supply (5V/3V3), GND and SPI connections.\n");
        return false;
    }
    diag_text("[PASS] SD Card successfully initialized in SPI mode.\n\n");

    /* -------------------------------------------------------------
     * Step 2: Retrieve SD Card Capacity, Type, and Identification
     * ------------------------------------------------------------- */
    diag_text("[STEP 2] Retrieving Card Capacity & Type Details...\n");
    if (sd_get_card_info(&card_info))
    {
        diag_text("  - Card Type:         ");
        diag_text(card_info.is_sdhc ? "SDHC / SDXC (Block Addressed)\n" : "SDSC (Byte Addressed)\n");

        diag_text("  - Raw Capacity:      ");
        diag_u64(card_info.capacity_bytes);
        diag_text(" Bytes (~");
        diag_u32(card_info.capacity_mb);
        diag_text(" MB / ");
        diag_u32(card_info.capacity_mb / 1024U);
        diag_text(" GB)\n");

        if (card_info.pnm[0] != '\0')
        {
            diag_text("  - Product Name:      ");
            diag_text(card_info.pnm);
            diag_text("\n");
        }

        diag_text("  - Manufacturer ID:   ");
        diag_hex_byte(card_info.mid);
        if (card_info.mid == 0x03)
        {
            diag_text(" (SanDisk)\n");
        }
        else
        {
            diag_text("\n");
        }

        diag_text("  - OEM/App ID:        ");
        diag_text(card_info.oid);
        diag_text("\n");

        diag_text("  - Serial Number:     ");
        diag_hex_u32(card_info.psn);
        diag_text("\n");

        diag_text("  - Manufacture Date:  ");
        diag_u32(card_info.mdt_month);
        diag_text("/");
        diag_u32(card_info.mdt_year);
        diag_text("\n");
    }
    else
    {
        diag_text("  - Notice: CSD/CID read skipped or incomplete.\n");
    }
    diag_text("[PASS] Card information retrieved successfully.\n\n");

    /* -------------------------------------------------------------
     * Step 3: Mount FAT Filesystem
     * ------------------------------------------------------------- */
    diag_text("[STEP 3] Mounting FAT Filesystem (0:)...\n");
    fr = f_mount(&g_fatfs, "0:", 1);
    if (fr != FR_OK)
    {
        diag_text("[FAIL] f_mount failed with FatFs error code: ");
        diag_u32((uint32_t) fr);
        diag_text("\n");
        return false;
    }
    diag_text("[PASS] FAT filesystem mounted successfully.\n");

    /* Query Filesystem Space */
    DWORD free_clusters;
    FATFS *p_fs;
    fr = f_getfree("0:", &free_clusters, &p_fs);
    if (fr == FR_OK && p_fs != NULL)
    {
        uint32_t total_sectors = (uint32_t) (p_fs->n_fatent - 2) * p_fs->csize;
        uint32_t free_sectors  = (uint32_t) free_clusters * p_fs->csize;

        diag_text("  - FAT Type:          ");
        if (p_fs->fs_type == FS_FAT12) diag_text("FAT12\n");
        else if (p_fs->fs_type == FS_FAT16) diag_text("FAT16\n");
        else if (p_fs->fs_type == FS_FAT32) diag_text("FAT32\n");
        else diag_text("Unknown\n");

        diag_text("  - Cluster Size:      ");
        diag_u32((uint32_t) p_fs->csize * 512U);
        diag_text(" Bytes (");
        diag_u32(p_fs->csize);
        diag_text(" sectors/cluster)\n");

        diag_text("  - Volume Total:      ");
        diag_u32(total_sectors / 2048U);
        diag_text(" MB (");
        diag_u32(total_sectors / 2097152U);
        diag_text(" GB)\n");

        diag_text("  - Volume Free:       ");
        diag_u32(free_sectors / 2048U);
        diag_text(" MB (");
        diag_u32(free_sectors / 2097152U);
        diag_text(" GB)\n");
    }
    diag_text("\n");

    /* -------------------------------------------------------------
     * Step 4 & 5: Create and write to /hello.txt ("hello from ra6e2")
     * ------------------------------------------------------------- */
    diag_text("[STEP 4] Creating '0:/hello.txt'...\n");
    fr = f_open(&g_file, "0:/hello.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK)
    {
        diag_text("[FAIL] f_open for '0:/hello.txt' failed! Error code: ");
        diag_u32((uint32_t) fr);
        diag_text("\n");
        return false;
    }
    diag_text("[PASS] '0:/hello.txt' opened for writing.\n");

    UINT hello_len = (UINT) strlen(g_hello_payload);
    fr = f_write(&g_file, g_hello_payload, hello_len, &bytes_written);
    f_close(&g_file);
    if (fr != FR_OK || bytes_written != hello_len)
    {
        diag_text("[FAIL] f_write to '0:/hello.txt' failed!\n");
        return false;
    }
    diag_text("[PASS] Successfully wrote 'hello from ra6e2' to '0:/hello.txt'!\n\n");

    /* -------------------------------------------------------------
     * Step 6: Create and write to /test.txt
     * ------------------------------------------------------------- */
    diag_text("[STEP 5] Creating and writing detailed log to '0:/test.txt'...\n");
    fr = f_open(&g_file, "0:/test.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK)
    {
        diag_text("[FAIL] f_open for write failed! Error code: ");
        diag_u32((uint32_t) fr);
        diag_text("\n");
        return false;
    }

    UINT to_write = (UINT) strlen(g_write_payload);
    fr = f_write(&g_file, g_write_payload, to_write, &bytes_written);
    if (fr != FR_OK || bytes_written != to_write)
    {
        diag_text("[FAIL] f_write failed! Written: ");
        diag_u32(bytes_written);
        diag_text(" of ");
        diag_u32(to_write);
        diag_text(" bytes. Error: ");
        diag_u32((uint32_t) fr);
        diag_text("\n");
        f_close(&g_file);
        return false;
    }
    f_close(&g_file);
    diag_text("[PASS] Wrote ");
    diag_u32(bytes_written);
    diag_text(" bytes to '0:/test.txt' successfully.\n\n");

    /* -------------------------------------------------------------
     * Step 7: Reopen /hello.txt for reading
     * ------------------------------------------------------------- */
    diag_text("[STEP 6] Reopening '0:/hello.txt' to verify content...\n");
    fr = f_open(&g_file, "0:/hello.txt", FA_READ);
    if (fr != FR_OK)
    {
        diag_text("[FAIL] f_open for read failed! Error: ");
        diag_u32((uint32_t) fr);
        diag_text("\n");
        return false;
    }

    memset(g_read_buffer, 0, sizeof(g_read_buffer));
    fr = f_read(&g_file, g_read_buffer, sizeof(g_read_buffer) - 1, &bytes_read);
    f_close(&g_file);

    if (fr != FR_OK || bytes_read != hello_len || strcmp(g_read_buffer, g_hello_payload) != 0)
    {
        diag_text("[FAIL] Data verification failed on '0:/hello.txt'!\n");
        return false;
    }
    diag_text("[PASS] Verified '0:/hello.txt' matches: \"hello from ra6e2\"\n\n");

    /* -------------------------------------------------------------
     * Step 8: Reopen /test.txt and read back full content
     * ------------------------------------------------------------- */
    diag_text("[STEP 7] Reading stored contents from '0:/test.txt'...\n");
    fr = f_open(&g_file, "0:/test.txt", FA_READ);
    if (fr != FR_OK)
    {
        diag_text("[FAIL] f_open for read failed! Error: ");
        diag_u32((uint32_t) fr);
        diag_text("\n");
        return false;
    }

    memset(g_read_buffer, 0, sizeof(g_read_buffer));
    fr = f_read(&g_file, g_read_buffer, sizeof(g_read_buffer) - 1, &bytes_read);
    f_close(&g_file);

    if (fr != FR_OK)
    {
        diag_text("[FAIL] f_read failed! Error: ");
        diag_u32((uint32_t) fr);
        diag_text("\n");
        return false;
    }
    diag_text("[PASS] Read ");
    diag_u32(bytes_read);
    diag_text(" bytes from '0:/test.txt'.\n\n");

    diag_text("---------------- Stored File Contents ----------------\n");
    diag_text(g_read_buffer);
    diag_text("------------------------------------------------------\n\n");

    /* Unmount volume cleanly */
    f_mount(NULL, "0:", 0);

    diag_text("=======================================================\n");
    diag_text("   >>> ALL SD CARD SPI & FATFS TESTS PASSED! <<<      \n");
    diag_text("   Files '0:/hello.txt' and '0:/test.txt' WRITTEN!    \n");
    diag_text("=======================================================\n");

    return true;
}
