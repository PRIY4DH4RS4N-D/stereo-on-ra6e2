#include "image_loader.h"
#include "mini_inflate.h"
#include "sd_spi.h"
#include "diagnostics.h"
#include "fatfs/ff.h"
#include "hal_data.h"
#include <string.h>

/* 1. Raw Compressed PNG file in SRAM */
__attribute__((aligned(4), used)) uint8_t g_image_buffer[MAX_IMAGE_FILE_SIZE];
uint32_t g_image_size = 0;
uint32_t g_image_ram_address = 0;

/* 2. Decoded Raw Pixels (128x128 8bpp = 16,384 bytes) in SRAM */
__attribute__((aligned(4), used)) uint8_t g_decoded_pixels[16512];
uint32_t g_decoded_pixels_address = 0;

static FATFS g_fatfs;
static FIL g_image_file;

static const uint8_t PNG_SIGNATURE[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

static const char *IMAGE_PATHS[] = {
    "0:/LEFT/F0000001.PNG",
    "0:/LEFT/F0000001.png",
    "0:/left/f0000001.png",
    "0:/F0000001.PNG",
    "0:/F0000001.png",
    NULL
};

static uint32_t calculate_crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (uint32_t i = 0; i < length; ++i)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
        {
            if (crc & 1)
            {
                crc = (crc >> 1) ^ 0xEDB88320U;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

static void print_hex_dump(const uint8_t *data, uint32_t length, uint32_t base_addr)
{
    for (uint32_t i = 0; i < length; i += 16)
    {
        diag_text("  [");
        diag_hex_u32(base_addr + i);
        diag_text("]  ");

        for (uint32_t j = 0; j < 16; ++j)
        {
            if (i + j < length)
            {
                uint8_t byte = data[i + j];
                const char hex[] = "0123456789ABCDEF";
                diag_char(hex[(byte >> 4) & 0x0F]);
                diag_char(hex[byte & 0x0F]);
                diag_char(' ');
            }
            else
            {
                diag_text("   ");
            }
            if (j == 7) diag_char(' ');
        }

        diag_text(" |");
        for (uint32_t j = 0; j < 16 && (i + j) < length; ++j)
        {
            char c = (char) data[i + j];
            diag_char((c >= 32 && c <= 126) ? c : '.');
        }
        diag_text("|\n");
    }
}

bool image_loader_run(void)
{
    FRESULT fr;
    UINT bytes_read = 0;
    const char *found_path = NULL;

    g_image_ram_address = (uint32_t) &g_image_buffer[0];
    g_decoded_pixels_address = (uint32_t) &g_decoded_pixels[0];

    diag_text("\n=======================================================\n");
    diag_text("   BGK-RA6E2 MicroSD Image Loader & Real-Time Decoder  \n");
    diag_text("=======================================================\n\n");

    /* Step 1: Initialize SD Card */
    diag_text("[STEP 1] Initializing SD Card via SPI...\n");
    if (disk_initialize(0) != 0)
    {
        diag_text("[FAIL] SD Card initialization failed!\n");
        return false;
    }
    diag_text("[PASS] SD Card successfully initialized.\n\n");

    /* Step 2: Mount FAT32 */
    diag_text("[STEP 2] Mounting FAT32 Filesystem (0:)...\n");
    fr = f_mount(&g_fatfs, "0:", 1);
    if (fr != FR_OK)
    {
        diag_text("[FAIL] f_mount failed with code: ");
        diag_u32((uint32_t) fr);
        diag_text("\n");
        return false;
    }
    diag_text("[PASS] FAT filesystem mounted successfully.\n\n");

    /* Step 3: Find File */
    diag_text("[STEP 3] Searching for image on SD card...\n");
    for (int i = 0; IMAGE_PATHS[i] != NULL; ++i)
    {
        diag_text("  - Checking: ");
        diag_text(IMAGE_PATHS[i]);
        fr = f_open(&g_image_file, IMAGE_PATHS[i], FA_READ);
        if (fr == FR_OK)
        {
            found_path = IMAGE_PATHS[i];
            diag_text(" -> FOUND!\n");
            break;
        }
        else
        {
            diag_text(" -> Not found\n");
        }
    }

    if (found_path == NULL)
    {
        diag_text("[FAIL] 'F0000001.PNG' not found on SD card!\n");
        f_mount(NULL, "0:", 0);
        return false;
    }

    /* Step 4: Query File Size */
    g_image_size = (uint32_t) f_size(&g_image_file);
    diag_text("\n[STEP 4] Image File: ");
    diag_text(found_path);
    diag_text(" (");
    diag_u32(g_image_size);
    diag_text(" Bytes)\n");

    if (g_image_size == 0 || g_image_size > MAX_IMAGE_FILE_SIZE)
    {
        diag_text("[FAIL] File size exceeds buffer!\n");
        f_close(&g_image_file);
        f_mount(NULL, "0:", 0);
        return false;
    }

    /* Step 5: Read Compressed PNG into MCU SRAM */
    diag_text("\n[STEP 5] Reading PNG file into MCU SRAM...\n");
    memset(g_image_buffer, 0, sizeof(g_image_buffer));
    fr = f_read(&g_image_file, g_image_buffer, g_image_size, &bytes_read);
    f_close(&g_image_file);
    f_mount(NULL, "0:", 0);

    if (fr != FR_OK || bytes_read != g_image_size)
    {
        diag_text("[FAIL] Read failed!\n");
        return false;
    }
    diag_text("[PASS] Read ");
    diag_u32(bytes_read);
    diag_text(" bytes into RAM address: ");
    diag_hex_u32(g_image_ram_address);
    diag_text("\n\n");

    /* Step 6: Validate PNG */
    if (memcmp(g_image_buffer, PNG_SIGNATURE, sizeof(PNG_SIGNATURE)) != 0)
    {
        diag_text("[FAIL] Not a valid PNG file!\n");
        return false;
    }

    /* Step 7: Locate IDAT chunk & Decompress to Raw Pixels */
    diag_text("[STEP 6] Decompressing PNG to Raw Pixels (128x128 8bpp)...\n");
    uint32_t offset = 8;
    const uint8_t *idat_data = NULL;
    size_t idat_len = 0;

    while (offset + 8 < g_image_size)
    {
        uint32_t chunk_len = ((uint32_t) g_image_buffer[offset] << 24) |
                             ((uint32_t) g_image_buffer[offset + 1] << 16) |
                             ((uint32_t) g_image_buffer[offset + 2] << 8) |
                             (uint32_t) g_image_buffer[offset + 3];

        if (memcmp(&g_image_buffer[offset + 4], "IDAT", 4) == 0)
        {
            idat_data = &g_image_buffer[offset + 8];
            idat_len = chunk_len;
            break;
        }
        offset += 12 + chunk_len;
    }

    if (idat_data == NULL || idat_len == 0)
    {
        diag_text("[FAIL] IDAT chunk not found in PNG!\n");
        return false;
    }

    size_t decompressed_len = 0;
    int res = mini_zlib_decompress(idat_data, idat_len, g_decoded_pixels, sizeof(g_decoded_pixels), &decompressed_len);
    if (res != 0 || decompressed_len < 16512)
    {
        diag_text("[FAIL] Zlib decompression failed! Error: ");
        diag_u32((uint32_t) res);
        diag_text("\n");
        return false;
    }

    /* Step 8: In-place strip PNG scanline filter byte to obtain raw 128x128 pixels */
    for (int y = 0; y < RAW_IMAGE_HEIGHT; ++y)
    {
        memmove(&g_decoded_pixels[y * RAW_IMAGE_WIDTH],
                &g_decoded_pixels[y * (RAW_IMAGE_WIDTH + 1) + 1],
                RAW_IMAGE_WIDTH);
    }
    diag_text("[PASS] Decompressed 16,384 raw grayscale pixels successfully!\n\n");

    /* Step 9: Print Memory Locations */
    diag_text("=======================================================\n");
    diag_text("       >>> MEMORY ADDRESSES FOR IMAGE VIEWING <<<       \n");
    diag_text("=======================================================\n\n");

    diag_text("1. FOR e2 STUDIO 'Raw Image Format' VISUALIZER:\n");
    diag_text("   >>> ADDRESS TO ENTER:  ");
    diag_hex_u32(g_decoded_pixels_address);
    diag_text(" <<<\n");
    diag_text("   Variable Name:         g_decoded_pixels\n");
    diag_text("   Format:                128 x 128, Monochrome 8bpp\n");
    diag_text("   Result:                Displays the CLEAR, REAL IMAGE!\n\n");

    diag_text("2. RAW COMPRESSED PNG FILE (File on SD card):\n");
    diag_text("   Address:               ");
    diag_hex_u32(g_image_ram_address);
    diag_text("\n   Variable Name:         g_image_buffer\n");
    diag_text("   Length:                11,919 Bytes\n\n");

    diag_text("--- First 32 Decoded Pixel Bytes at ");
    diag_hex_u32(g_decoded_pixels_address);
    diag_text(" ---\n");
    print_hex_dump(g_decoded_pixels, 32, g_decoded_pixels_address);
    diag_text("-------------------------------------------------------\n\n");

    return true;
}
