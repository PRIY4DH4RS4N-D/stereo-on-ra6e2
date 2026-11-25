#include "stereo.h"
#include "diagnostics.h"
#include "fatfs/ff.h"
#include <string.h>

#define SECTOR_SIZE 512U
#define PAIR_SIZE (2U * STEREO_WIDTH * STEREO_HEIGHT)
#define INPUT_NAME "0:/STEREO.BIN"
#define BMP_IMAGE_SIZE (STEREO_WIDTH * STEREO_HEIGHT)

_Static_assert(STEREO_WIDTH % 4U == 0, "BMP scanlines must be 4-byte aligned");
_Static_assert(BMP_IMAGE_SIZE % SECTOR_SIZE == 0, "Each BMP must end on a sector boundary");
_Static_assert(54U + 4U * STEREO_DISPARITIES <= SECTOR_SIZE, "BMP palette must fit in header");

enum
{
    STEREO_RUNNING = 1,
    STEREO_DONE = 2,
    STEREO_MOUNT_FAILED = -1,
    STEREO_INPUT_FAILED = -2,
    STEREO_INVALID_DATASET = -3,
    STEREO_OUTPUT_FAILED = -4,
    STEREO_READ_FAILED = -5,
    STEREO_WRITE_FAILED = -6,
};

/* Inspect these symbols in the debugger. */
volatile int g_stereo_status;
volatile FRESULT g_stereo_fatfs_result;
volatile uint32_t g_stereo_frames_total;
volatile uint32_t g_stereo_frames_done;
char g_stereo_output_name[] = "0:/RUN00000/F0000000.BMP";

static FATFS fs;
static FIL input;
static FIL output;
static uint8_t read_sector[SECTOR_SIZE];
static uint8_t write_sector[SECTOR_SIZE];
static unsigned read_offset;
static unsigned write_offset;

static void log_fs_error(const char *operation)
{
    FRESULT result = g_stereo_fatfs_result;
    diag_text("[FS] ERROR ");
    diag_text(operation);
    diag_text(" (FatFs ");
    diag_u32((uint32_t) result);
    diag_text(")\n");
    switch (result)
    {
        case FR_NOT_READY:
            diag_text("[FS] Card is not ready. See the [SD] messages.\n");
            break;
        case FR_NO_FILESYSTEM:
            diag_text("[FS] No supported filesystem. Check that the card is FAT32.\n");
            break;
        case FR_NO_FILE:
            diag_text("[FS] File not found. Check STEREO.BIN is at the card root.\n");
            break;
        case FR_WRITE_PROTECTED:
            diag_text("[FS] Card is write protected.\n");
            break;
        case FR_DENIED:
            diag_text("[FS] Access denied. Check free space and file attributes.\n");
            break;
        default:
            break;
    }
}

static bool read_exact(void *buffer, UINT length)
{
    UINT count = 0;
    g_stereo_fatfs_result = f_read(&input, buffer, length, &count);
    return g_stereo_fatfs_result == FR_OK && count == length;
}

static bool write_exact(const void *buffer, UINT length)
{
    UINT count = 0;
    g_stereo_fatfs_result = f_write(&output, buffer, length, &count);
    return g_stereo_fatfs_result == FR_OK && count == length;
}

static bool read_row(uint8_t *left, uint8_t *right)
{
    if (read_offset == SECTOR_SIZE)
    {
        if (!read_exact(read_sector, SECTOR_SIZE))
        {
            g_stereo_status = STEREO_READ_FAILED;
            log_fs_error("reading frame data from STEREO.BIN");
            return false;
        }
        read_offset = 0;
    }
    memcpy(left, read_sector + read_offset, STEREO_WIDTH);
    memcpy(right, read_sector + read_offset + STEREO_WIDTH, STEREO_WIDTH);
    read_offset += 2U * STEREO_WIDTH;
    return true;
}

static bool write_row(const uint8_t *row)
{
    memcpy(write_sector + write_offset, row, STEREO_WIDTH);
    write_offset += STEREO_WIDTH;
    if (write_offset == SECTOR_SIZE)
    {
        write_offset = 0;
        if (!write_exact(write_sector, SECTOR_SIZE))
        {
            g_stereo_status = STEREO_WRITE_FAILED;
            log_fs_error("writing BMP pixels. If FatFs is 0, the card is likely full");
            return false;
        }
    }
    return true;
}

static void write_digits(char *digits, unsigned length, uint32_t number)
{
    while (length > 0)
    {
        digits[--length] = (char) ('0' + number % 10U);
        number /= 10U;
    }
}

static bool create_run_directory(void)
{
    char directory[] = "0:/RUN00000";
    for (uint32_t run = 1; run <= 99999; ++run)
    {
        write_digits(directory + 6, 5, run);
        g_stereo_fatfs_result = f_mkdir(directory);
        if (g_stereo_fatfs_result == FR_OK)
        {
            memcpy(g_stereo_output_name, directory, sizeof(directory) - 1);
            diag_text("[FS] Created output directory ");
            diag_text(directory);
            diag_text("\n");
            return true;
        }
        if (g_stereo_fatfs_result != FR_EXIST)
        {
            return false;
        }
    }
    return false;
}

static void put_le32(unsigned offset, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
    {
        write_sector[offset + i] = (uint8_t) (value >> (8U * i));
    }
}

static bool write_bmp_header(void)
{
    memset(write_sector, 0, sizeof(write_sector));
    write_sector[0] = 'B';
    write_sector[1] = 'M';
    put_le32(2, SECTOR_SIZE + BMP_IMAGE_SIZE);
    put_le32(10, SECTOR_SIZE);
    put_le32(14, 40);
    put_le32(18, STEREO_WIDTH);
    put_le32(22, (uint32_t) -(int32_t) STEREO_HEIGHT);  /* Top-down row order. */
    write_sector[26] = 1;  /* Color planes. */
    write_sector[28] = 8;  /* Bits per pixel. */
    put_le32(34, BMP_IMAGE_SIZE);
    put_le32(46, STEREO_DISPARITIES);
    for (unsigned d = 0; d < STEREO_DISPARITIES; ++d)
    {
        uint8_t gray = (uint8_t) (d * 255U / (STEREO_DISPARITIES - 1U));
        memset(write_sector + 54U + 4U * d, gray, 3);
    }
    return write_exact(write_sector, SECTOR_SIZE);
}

static bool process_frame(uint32_t number)
{
    write_digits(g_stereo_output_name + 13, 7, number);
    char temporary_name[sizeof(g_stereo_output_name)];
    memcpy(temporary_name, g_stereo_output_name, sizeof(temporary_name));
    memcpy(temporary_name + sizeof(temporary_name) - 4U, "TMP", 3);
    g_stereo_fatfs_result = f_open(&output, temporary_name, FA_WRITE | FA_CREATE_NEW);
    if (g_stereo_fatfs_result != FR_OK)
    {
        g_stereo_status = STEREO_OUTPUT_FAILED;
        log_fs_error("opening temporary BMP for writing");
        return false;
    }

    write_offset = 0;
    if (!write_bmp_header())
    {
        g_stereo_status = STEREO_WRITE_FAILED;
        log_fs_error("writing BMP header. If FatFs is 0, the card is likely full");
    }
    else if (!stereo_process_pair(read_row, write_row) && g_stereo_status == STEREO_RUNNING)
    {
        g_stereo_status = STEREO_WRITE_FAILED;
    }
    FRESULT close_result = f_close(&output);
    if (g_stereo_status == STEREO_RUNNING)
    {
        g_stereo_fatfs_result = close_result;
        if (close_result == FR_OK)
        {
            g_stereo_fatfs_result = f_rename(temporary_name, g_stereo_output_name);
        }
        if (g_stereo_fatfs_result == FR_OK)
        {
            diag_text("[BMP] Saved ");
            diag_text(g_stereo_output_name);
            diag_text(" (");
            diag_u32(number);
            diag_text("/");
            diag_u32(g_stereo_frames_total);
            diag_text(")\n");
            return true;
        }
        g_stereo_status = STEREO_WRITE_FAILED;
        log_fs_error(close_result == FR_OK ? "renaming saved BMP" : "closing and syncing BMP");
    }
    if (close_result == FR_OK)
    {
        (void) f_unlink(temporary_name);
    }
    return false;
}

bool stereo_run_dataset(void)
{
    g_stereo_status = STEREO_RUNNING;
    diag_text("[FS] Mounting SD card\n");
    g_stereo_fatfs_result = f_mount(&fs, "0:", 1);
    if (g_stereo_fatfs_result != FR_OK)
    {
        g_stereo_status = STEREO_MOUNT_FAILED;
        log_fs_error("mounting the card");
        return false;
    }
    diag_text("[FS] Mounted. Opening /STEREO.BIN\n");
    g_stereo_fatfs_result = f_open(&input, INPUT_NAME, FA_READ);
    if (g_stereo_fatfs_result != FR_OK)
    {
        g_stereo_status = STEREO_INPUT_FAILED;
        log_fs_error("opening /STEREO.BIN");
        return false;
    }
    if (!read_exact(read_sector, SECTOR_SIZE))
    {
        g_stereo_status = STEREO_READ_FAILED;
        log_fs_error("reading the 512-byte dataset header");
        (void) f_close(&input);
        return false;
    }

    /* Header layout: 8-byte magic, 16-bit width/height, 32-bit pair count. */
    uint32_t count = (uint32_t) read_sector[12] | ((uint32_t) read_sector[13] << 8) |
                     ((uint32_t) read_sector[14] << 16) | ((uint32_t) read_sector[15] << 24);
    const uint8_t dimensions[] = {STEREO_WIDTH, 0, STEREO_HEIGHT, 0};
    if (memcmp(read_sector, "R6ST0001", 8) != 0 ||
        memcmp(read_sector + 8, dimensions, sizeof(dimensions)) != 0 ||
        count == 0 || count > (UINT32_MAX - SECTOR_SIZE) / PAIR_SIZE ||
        f_size(&input) != SECTOR_SIZE + count * PAIR_SIZE)
    {
        g_stereo_status = STEREO_INVALID_DATASET;
        diag_text("[DATA] ERROR invalid STEREO.BIN header or size. File bytes ");
        diag_u32(f_size(&input));
        diag_text(", declared pairs ");
        diag_u32(count);
        diag_text("\n");
        (void) f_close(&input);
        return false;
    }
    g_stereo_frames_total = count;
    diag_text("[DATA] Loaded ");
    diag_u32(count);
    diag_text(" stereo pairs. Starting on-board matching.\n");

    if (!create_run_directory())
    {
        g_stereo_status = STEREO_OUTPUT_FAILED;
        log_fs_error("creating output directory");
        (void) f_close(&input);
        return false;
    }
    read_offset = SECTOR_SIZE;
    for (uint32_t frame = 1; frame <= count; ++frame)
    {
        if (!process_frame(frame))
        {
            diag_text("[RUN] Failed while processing ");
            diag_text(g_stereo_output_name);
            diag_text("\n");
            (void) f_close(&input);
            return false;
        }
        g_stereo_frames_done = frame;
    }
    (void) f_close(&input);
    g_stereo_status = STEREO_DONE;
    return true;
}
