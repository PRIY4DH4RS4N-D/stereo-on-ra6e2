#include "hal_data.h"
#include "diagnostics.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"

/* PMOD1 in its factory SPI/UART position. P301 is on port 3. */
#define SD_CS_PIN   BSP_IO_PORT_03_PIN_01
#define SD_MOSI_PIN BSP_IO_PORT_01_PIN_09
#define SD_MISO_PIN BSP_IO_PORT_01_PIN_10
#define SD_SCK_PIN  BSP_IO_PORT_01_PIN_11
#define MOSI (1UL << 9)
#define MISO (1UL << 10)
#define SCK  (1UL << 11)
#define CS   (1UL << 1)
#define CLEAR_PIN(mask) ((mask) << 16)
#define SECTOR_SIZE 512U
#define SD_TIMEOUT_BYTES 500000U

static DSTATUS status = STA_NOINIT;
static bool block_addressed;
static bool slow_clock = true;

static void log_response(const char *operation, uint8_t response)
{
    diag_text(operation);
    diag_hex_byte(response);
    diag_text("\n");
}

static void log_sector_error(const char *operation, LBA_t sector)
{
    diag_text(operation);
    diag_u32(sector);
    diag_text("\n");
}

static uint8_t spi_exchange(uint8_t tx)
{
    uint8_t rx = 0;
    for (unsigned bit = 0; bit < 8; ++bit)
    {
        R_PORT1->PCNTR3 = ((tx & 0x80U) ? MOSI : CLEAR_PIN(MOSI)) | CLEAR_PIN(SCK);
        if (slow_clock)
        {
            R_BSP_SoftwareDelay(2U, BSP_DELAY_UNITS_MICROSECONDS);
        }
        R_PORT1->PCNTR3 = SCK;
        if (slow_clock)
        {
            R_BSP_SoftwareDelay(2U, BSP_DELAY_UNITS_MICROSECONDS);
        }
        rx = (uint8_t) ((rx << 1) | ((R_PORT1->PIDR & MISO) != 0U));
        tx = (uint8_t) (tx << 1);
    }
    R_PORT1->PCNTR3 = CLEAR_PIN(SCK);
    return rx;
}

static void deselect(void)
{
    R_PORT3->PCNTR3 = CS;
    spi_exchange(0xFF);
}

static bool wait_ready(void)
{
    unsigned limit = slow_clock ? 32U : SD_TIMEOUT_BYTES;
    for (unsigned i = 0; i < limit; ++i)
    {
        if (spi_exchange(0xFF) == 0xFF)
        {
            return true;
        }
    }
    return false;
}

/* A command leaves CS asserted so the caller can read any trailing data. */
static uint8_t command(uint8_t index, uint32_t argument)
{
    deselect();
    R_PORT3->PCNTR3 = CLEAR_PIN(CS);
    if (!wait_ready())
    {
        return 0xFF;
    }
    spi_exchange(0x40U | index);
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        spi_exchange((uint8_t) (argument >> shift));
    }
    spi_exchange(index == 0 ? 0x95 : (index == 8 ? 0x87 : 0x01));
    for (unsigned i = 0; i < 10; ++i)
    {
        uint8_t response = spi_exchange(0xFF);
        if ((response & 0x80U) == 0)
        {
            return response;
        }
    }
    return 0xFF;
}

static bool start_card(void)
{
    diag_text("[SD] Initializing SPI card on PMOD1\n");
    if (R_IOPORT_PinCfg(&g_ioport_ctrl, SD_CS_PIN, IOPORT_CFG_PORT_DIRECTION_OUTPUT |
                        IOPORT_CFG_PORT_OUTPUT_HIGH) != FSP_SUCCESS ||
        R_IOPORT_PinCfg(&g_ioport_ctrl, SD_SCK_PIN, IOPORT_CFG_PORT_DIRECTION_OUTPUT |
                        IOPORT_CFG_PORT_OUTPUT_LOW) != FSP_SUCCESS ||
        R_IOPORT_PinCfg(&g_ioport_ctrl, SD_MOSI_PIN, IOPORT_CFG_PORT_DIRECTION_OUTPUT |
                        IOPORT_CFG_PORT_OUTPUT_HIGH) != FSP_SUCCESS ||
        R_IOPORT_PinCfg(&g_ioport_ctrl, SD_MISO_PIN, IOPORT_CFG_PORT_DIRECTION_INPUT |
                        IOPORT_CFG_PULLUP_ENABLE) != FSP_SUCCESS)
    {
        diag_text("[SD] ERROR configuring SPI GPIO pins\n");
        return false;
    }
    slow_clock = true;
    R_BSP_SoftwareDelay(10U, BSP_DELAY_UNITS_MILLISECONDS);
    for (unsigned i = 0; i < 10; ++i)
    {
        spi_exchange(0xFF);
    }
    uint8_t idle = 0xFF;
    for (unsigned attempt = 0; attempt < 10 && idle != 1; ++attempt)
    {
        idle = command(0, 0);
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }
    deselect();
    log_response("[SD] CMD0 response ", idle);
    if (idle != 1)
    {
        diag_text("[SD] ERROR card did not enter SPI mode. Check power, CS, SCK, MOSI and MISO.\n");
        return false;
    }
    uint8_t cmd8 = command(8, 0x1AA);
    uint8_t voltage = 0xFF;
    uint8_t echo = 0xFF;
    if (cmd8 == 1)
    {
        spi_exchange(0xFF);
        spi_exchange(0xFF);
        voltage = spi_exchange(0xFF);
        echo = spi_exchange(0xFF);
    }
    deselect();
    log_response("[SD] CMD8 response ", cmd8);
    if (cmd8 != 1 || voltage != 0x01 || echo != 0xAA)
    {
        log_response("[SD] ERROR SD v2 voltage/check pattern failed. Echo ", echo);
        return false;
    }

    for (unsigned attempt = 0; attempt < 1000; ++attempt)
    {
        uint8_t response = command(55, 0);
        if (response == 1 || response == 0)
        {
            response = command(41, 0x40000000UL);
            if (response == 0)
            {
                uint8_t cmd58 = command(58, 0);
                if (cmd58 != 0)
                {
                    deselect();
                    log_response("[SD] ERROR CMD58 response ", cmd58);
                    return false;
                }
                uint8_t ocr = spi_exchange(0xFF);
                spi_exchange(0xFF);
                spi_exchange(0xFF);
                spi_exchange(0xFF);
                block_addressed = (ocr & 0x40U) != 0;
                if (!(ocr & 0x80U))
                {
                    deselect();
                    log_response("[SD] ERROR card reports power-up incomplete. OCR high byte ", ocr);
                    return false;
                }
                if (!block_addressed)
                {
                    uint8_t cmd16 = command(16, SECTOR_SIZE);
                    if (cmd16 != 0)
                    {
                        deselect();
                        log_response("[SD] ERROR CMD16 response ", cmd16);
                        return false;
                    }
                }
                return true;
            }
        }
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }
    deselect();
    diag_text("[SD] ERROR ACMD41 timed out before card became ready\n");
    return false;
}

DSTATUS disk_initialize(BYTE drive)
{
    if (drive != 0)
    {
        return STA_NOINIT;
    }
    if (status == STA_NOINIT)
    {
        bool ready = start_card();
        deselect();
        if (ready)
        {
            slow_clock = false;
            status = 0;
            diag_text(block_addressed ? "[SD] Card detected (SDHC/SDXC)\n" :
                                        "[SD] Card detected (SDSC)\n");
        }
    }
    return status;
}

DSTATUS disk_status(BYTE drive)
{
    return drive == 0 ? status : STA_NOINIT;
}

/* CRC-16/CCITT with initial value zero, as used for SD data blocks. */
static uint16_t crc_byte(uint16_t crc, uint8_t data)
{
    uint32_t mix = (crc >> 8) ^ data;
    mix ^= mix >> 4;
    return (uint16_t) ((crc << 8) ^ (mix << 12) ^ (mix << 5) ^ mix);
}

static bool read_sector(uint32_t address, BYTE *buffer)
{
    if (command(17, address) != 0)
    {
        return false;
    }
    uint8_t token = 0xFF;
    for (unsigned i = 0; i < SD_TIMEOUT_BYTES && token == 0xFF; ++i)
    {
        token = spi_exchange(0xFF);
    }
    if (token != 0xFE)
    {
        return false;
    }
    uint16_t crc = 0;
    for (unsigned i = 0; i < SECTOR_SIZE; ++i)
    {
        buffer[i] = spi_exchange(0xFF);
        crc = crc_byte(crc, buffer[i]);
    }
    uint16_t received = (uint16_t) ((uint16_t) spi_exchange(0xFF) << 8);
    received |= spi_exchange(0xFF);
    return crc == received;
}

static bool write_sector(uint32_t address, const BYTE *buffer)
{
    if (command(24, address) != 0)
    {
        return false;
    }
    spi_exchange(0xFF);
    spi_exchange(0xFE);
    uint16_t crc = 0;
    for (unsigned i = 0; i < SECTOR_SIZE; ++i)
    {
        spi_exchange(buffer[i]);
        crc = crc_byte(crc, buffer[i]);
    }
    spi_exchange((uint8_t) (crc >> 8));
    spi_exchange((uint8_t) crc);
    uint8_t response = 0xFF;
    for (unsigned i = 0; i < 10 && response == 0xFF; ++i)
    {
        response = spi_exchange(0xFF);
    }
    if ((response & 0x1FU) != 0x05 || !wait_ready())
    {
        return false;
    }
    uint8_t r1 = command(13, 0);
    uint8_t r2 = spi_exchange(0xFF);
    return r1 == 0 && r2 == 0;
}

DRESULT disk_read(BYTE drive, BYTE *buffer, LBA_t sector, UINT count)
{
    if (drive != 0 || count == 0 || buffer == NULL)
    {
        return RES_PARERR;
    }
    if (status != 0)
    {
        return RES_NOTRDY;
    }
    for (; count > 0; --count, ++sector, buffer += SECTOR_SIZE)
    {
        bool ok = read_sector(block_addressed ? sector : sector * SECTOR_SIZE, buffer);
        deselect();
        if (!ok)
        {
            log_sector_error("[SD] ERROR reading sector ", sector);
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_write(BYTE drive, const BYTE *buffer, LBA_t sector, UINT count)
{
    if (drive != 0 || count == 0 || buffer == NULL)
    {
        return RES_PARERR;
    }
    if (status != 0)
    {
        return RES_NOTRDY;
    }
    for (; count > 0; --count, ++sector, buffer += SECTOR_SIZE)
    {
        bool ok = write_sector(block_addressed ? sector : sector * SECTOR_SIZE, buffer);
        deselect();
        if (!ok)
        {
            log_sector_error("[SD] ERROR writing sector ", sector);
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE drive, BYTE request, void *buffer)
{
    (void) buffer;
    if (drive != 0 || request != CTRL_SYNC)
    {
        return RES_PARERR;
    }
    if (status != 0)
    {
        return RES_NOTRDY;
    }
    R_PORT3->PCNTR3 = CLEAR_PIN(CS);
    bool ready = wait_ready();
    deselect();
    if (!ready)
    {
        diag_text("[SD] ERROR card remained busy during sync\n");
    }
    return ready ? RES_OK : RES_ERROR;
}
