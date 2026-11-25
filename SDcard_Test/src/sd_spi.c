#include "sd_spi.h"
#include "hal_data.h"
#include "diagnostics.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"
#include <string.h>

/* SPI Pin Mapping on BGK-RA6E2:
 * MOSI: P109 (Port 1, Pin 9)  - PMOD1 pin 2 & Arduino D11
 * MISO: P110 (Port 1, Pin 10) - PMOD1 pin 3 & Arduino D12
 * SCK:  P111 (Port 1, Pin 11) - PMOD1 pin 4 & Arduino D13
 *
 * Chip Select (CS) Candidates:
 * 1. P301 (Port 3, Pin 1) - Arduino D10
 * 2. P106 (Port 1, Pin 6) - PMOD1 Pin 1 / GPIO1
 * 3. P005 (Port 0, Pin 5) - PMOD1 RESET
 * 4. P402 (Port 4, Pin 2) - PMOD1 IRQ
 * (All candidate CS pins are driven simultaneously so wiring to any of them works!)
 */
#define SD_MOSI_PIN BSP_IO_PORT_01_PIN_09
#define SD_MISO_PIN BSP_IO_PORT_01_PIN_10
#define SD_SCK_PIN  BSP_IO_PORT_01_PIN_11

#define MOSI (1UL << 9)
#define MISO (1UL << 10)
#define SCK  (1UL << 11)
#define CLEAR_PIN(mask) ((mask) << 16)

#define SECTOR_SIZE 512U

static DSTATUS s_card_status = STA_NOINIT;
static bool s_block_addressed = false;
static bool s_slow_clock = true;

static void deselect(void);
static bool wait_ready(void);
static uint8_t command(uint8_t index, uint32_t argument);

static inline void cs_assert(void)
{
    R_PORT3->PCNTR3 = (1UL << (1 + 16)); /* P301 Low (Arduino D10) */
    R_PORT1->PCNTR3 = (1UL << (6 + 16)); /* P106 Low (PMOD1 Pin 1 / GPIO1) */
    R_PORT0->PCNTR3 = (1UL << (5 + 16)); /* P005 Low (PMOD1 Pin 8 / RESET) */
    R_PORT4->PCNTR3 = (1UL << (2 + 16)); /* P402 Low (PMOD1 Pin 7 / IRQ) */
}

static inline void cs_deassert(void)
{
    R_PORT3->PCNTR3 = (1UL << 1); /* P301 High */
    R_PORT1->PCNTR3 = (1UL << 6); /* P106 High */
    R_PORT0->PCNTR3 = (1UL << 5); /* P005 High */
    R_PORT4->PCNTR3 = (1UL << 2); /* P402 High */
}

static inline void sd_delay_cycles(uint32_t cycles)
{
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles)
    {
    }
}

static uint8_t spi_exchange(uint8_t tx)
{
    uint8_t rx = 0;
    for (unsigned bit = 0; bit < 8; ++bit)
    {
        /* 1. Set MOSI bit, SCK low */
        R_PORT1->PCNTR3 = ((tx & 0x80U) ? MOSI : CLEAR_PIN(MOSI)) | CLEAR_PIN(SCK);
        if (s_slow_clock)
        {
            R_BSP_SoftwareDelay(5U, BSP_DELAY_UNITS_MICROSECONDS); /* 100 kHz clock */
        }
        else
        {
            sd_delay_cycles(40U); /* ~2.5 MHz SPI clock */
        }

        /* 2. SCK high (SD samples MOSI on rising edge) */
        R_PORT1->PCNTR3 = SCK;
        if (s_slow_clock)
        {
            R_BSP_SoftwareDelay(5U, BSP_DELAY_UNITS_MICROSECONDS);
        }
        else
        {
            sd_delay_cycles(40U);
        }

        /* 3. Read MISO level while SCK is high */
        rx = (uint8_t) ((rx << 1) | (R_BSP_PinRead(SD_MISO_PIN) != 0U));
        tx = (uint8_t) (tx << 1);
    }
    R_PORT1->PCNTR3 = CLEAR_PIN(SCK);
    return rx;
}

static void deselect(void)
{
    cs_deassert();
    spi_exchange(0xFF);   /* Provide 8 dummy clocks */
}

static bool wait_ready(void)
{
    for (unsigned i = 0; i < 50000; ++i)
    {
        if (spi_exchange(0xFF) == 0xFF)
        {
            return true;
        }
        if (s_slow_clock)
        {
            R_BSP_SoftwareDelay(10U, BSP_DELAY_UNITS_MICROSECONDS);
        }
    }
    return false;
}

/* Sends an SD command packet. Leaves CS asserted (low) so caller can read data */
static uint8_t command(uint8_t index, uint32_t argument)
{
    deselect();
    cs_assert(); /* Assert all CS pins low */

    /* Wait ready for non-reset commands */
    if (index != 0)
    {
        if (!wait_ready())
        {
            return 0xFF;
        }
    }

    spi_exchange(0x40U | index);
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        spi_exchange((uint8_t) (argument >> shift));
    }
    /* Fixed precalculated CRC7: CMD0 (0x95), CMD8 (0x87); dummy 0x01 for others */
    spi_exchange(index == 0 ? 0x95 : (index == 8 ? 0x87 : 0x01));

    for (unsigned i = 0; i < 200; ++i)
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
    diag_text("[SD] Configuring SPI GPIO pins...\n");

    /* Configure SCK, MOSI as outputs, MISO as input with pullup */
    R_IOPORT_PinCfg(&g_ioport_ctrl, SD_SCK_PIN, IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW);
    R_IOPORT_PinCfg(&g_ioport_ctrl, SD_MOSI_PIN, IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH);
    R_IOPORT_PinCfg(&g_ioport_ctrl, SD_MISO_PIN, IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_PULLUP_ENABLE);

    /* Configure all candidate CS pins as outputs, initially HIGH */
    R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_03_PIN_01, IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH);
    R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_06, IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH);
    R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_05, IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH);
    R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_02, IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH);

    s_slow_clock = true;
    cs_deassert();
    R_BSP_SoftwareDelay(25U, BSP_DELAY_UNITS_MILLISECONDS);

    /* Diagnostic read of pin states before sending clocks */
    diag_text("[SD] Pin diagnostics (Idle):\n");
    diag_text("     CS  (P301/Arduino D10) = ");
    diag_u32(R_BSP_PinRead(BSP_IO_PORT_03_PIN_01));
    diag_text("\n     CS  (P106/PMOD1 Pin 1) = ");
    diag_u32(R_BSP_PinRead(BSP_IO_PORT_01_PIN_06));
    diag_text("\n     SCK (P111/PMOD1 Pin 4) = ");
    diag_u32(R_BSP_PinRead(SD_SCK_PIN));
    diag_text("\n     MOSI(P109/PMOD1 Pin 2) = ");
    diag_u32(R_BSP_PinRead(SD_MOSI_PIN));
    diag_text("\n     MISO(P110/PMOD1 Pin 3) = ");
    diag_u32(R_BSP_PinRead(SD_MISO_PIN));
    diag_text("\n");

    /* Send >= 74 dummy clock cycles (20 bytes = 160 clocks) with CS HIGH to power up card */
    cs_deassert();
    for (unsigned i = 0; i < 20; ++i)
    {
        spi_exchange(0xFF);
    }
    R_BSP_SoftwareDelay(5U, BSP_DELAY_UNITS_MILLISECONDS);

    /* Send CMD0: GO_IDLE_STATE -> expect 0x01 (in idle state) */
    uint8_t idle = 0xFF;
    for (unsigned attempt = 0; attempt < 50 && idle != 0x01; ++attempt)
    {
        idle = command(0, 0);
        deselect();
        if (idle != 0x01)
        {
            R_BSP_SoftwareDelay(10U, BSP_DELAY_UNITS_MILLISECONDS);
        }
    }

    diag_text("[SD] CMD0 response: ");
    diag_hex_byte(idle);
    diag_text("\n");

    if (idle != 0x01)
    {
        diag_text("[SD] ERROR: Card did not enter SPI idle mode (expected 0x01)!\n");
        diag_text("[SD] Verify power supply, GND, CS, SCK, MOSI, MISO connections.\n");
        return false;
    }

    /* Send CMD8: SEND_IF_COND (0x1AA: 2.7-3.6V, check pattern 0xAA) */
    uint8_t cmd8 = command(8, 0x1AA);
    uint8_t voltage = 0xFF;
    uint8_t echo = 0xFF;
    if (cmd8 == 0x01)
    {
        spi_exchange(0xFF);
        spi_exchange(0xFF);
        voltage = spi_exchange(0xFF);
        echo = spi_exchange(0xFF);
    }
    deselect();

    diag_text("[SD] CMD8 response: ");
    diag_hex_byte(cmd8);
    diag_text(", Echo: ");
    diag_hex_byte(echo);
    diag_text("\n");

    if (cmd8 != 0x01 || voltage != 0x01 || echo != 0xAA)
    {
        diag_text("[SD] ERROR: SD v2.0+ voltage/check pattern failed!\n");
        return false;
    }

    /* Send ACMD41 (CMD55 + CMD41 with HCS bit = 1: 0x40000000) until ready (response 0x00) */
    diag_text("[SD] Sending ACMD41 to initialize SD card...\n");
    bool initialized = false;
    for (unsigned attempt = 0; attempt < 2000; ++attempt)
    {
        uint8_t response55 = command(55, 0);
        deselect();
        if (response55 <= 0x01)
        {
            uint8_t response41 = command(41, 0x40000000UL);
            deselect();
            if (response41 == 0x00)
            {
                initialized = true;
                break;
            }
        }
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }

    if (!initialized)
    {
        diag_text("[SD] ERROR: ACMD41 initialization timed out!\n");
        return false;
    }

    /* Card is initialized! Read OCR via CMD58 to check CCS (Card Capacity Status) */
    uint8_t cmd58 = command(58, 0);
    if (cmd58 != 0x00)
    {
        deselect();
        diag_text("[SD] ERROR: CMD58 failed!\n");
        return false;
    }
    uint8_t ocr0 = spi_exchange(0xFF);
    spi_exchange(0xFF);
    spi_exchange(0xFF);
    spi_exchange(0xFF);
    deselect();

    s_block_addressed = ((ocr0 & 0x40U) != 0); /* CCS bit 30 */

    if (!(ocr0 & 0x80U))
    {
        diag_text("[SD] ERROR: Card power-up incomplete!\n");
        return false;
    }

    if (!s_block_addressed)
    {
        /* Standard capacity card: set sector size to 512 bytes via CMD16 */
        uint8_t cmd16 = command(16, SECTOR_SIZE);
        deselect();
        if (cmd16 != 0x00)
        {
            diag_text("[SD] ERROR: CMD16 set block size failed!\n");
            return false;
        }
    }

    return true;
}

DSTATUS disk_initialize(BYTE drive)
{
    if (drive != 0)
    {
        return STA_NOINIT;
    }

    if (s_card_status == STA_NOINIT)
    {
        if (start_card())
        {
            s_slow_clock = false; /* Switch to full-speed SPI clock */
            s_card_status = 0;
            diag_text(s_block_addressed ? "[SD] Card detected: SDHC/SDXC (Block Addressing)\n" :
                                          "[SD] Card detected: SDSC (Byte Addressing)\n");
        }
    }
    return s_card_status;
}

DSTATUS disk_status(BYTE drive)
{
    return (drive == 0) ? s_card_status : STA_NOINIT;
}

static bool read_sector(uint32_t address, BYTE *buffer)
{
    if (command(17, address) != 0)
    {
        return false;
    }
    uint8_t token = 0xFF;
    for (unsigned i = 0; i < 50000 && token == 0xFF; ++i)
    {
        token = spi_exchange(0xFF);
    }
    if (token != 0xFE)
    {
        return false;
    }
    for (unsigned i = 0; i < SECTOR_SIZE; ++i)
    {
        buffer[i] = spi_exchange(0xFF);
    }
    /* Discard 2-byte CRC (standard SPI mode FatFs implementation) */
    spi_exchange(0xFF);
    spi_exchange(0xFF);
    return true;
}

static bool write_sector(uint32_t address, const BYTE *buffer)
{
    if (command(24, address) != 0)
    {
        return false;
    }
    spi_exchange(0xFF);
    spi_exchange(0xFE); /* Start token */
    for (unsigned i = 0; i < SECTOR_SIZE; ++i)
    {
        spi_exchange(buffer[i]);
    }
    /* Dummy 2-byte CRC */
    spi_exchange(0xFF);
    spi_exchange(0xFF);

    uint8_t response = 0xFF;
    for (unsigned i = 0; i < 64 && response == 0xFF; ++i)
    {
        response = spi_exchange(0xFF);
    }
    /* Data accepted if lower 5 bits equal 0b00101 (0x05) */
    if ((response & 0x1FU) != 0x05)
    {
        return false;
    }
    if (!wait_ready())
    {
        return false;
    }
    return true;
}

DRESULT disk_read(BYTE drive, BYTE *buffer, LBA_t sector, UINT count)
{
    if (drive != 0 || count == 0 || buffer == NULL)
    {
        return RES_PARERR;
    }
    if (s_card_status != 0)
    {
        return RES_NOTRDY;
    }
    for (; count > 0; --count, ++sector, buffer += SECTOR_SIZE)
    {
        bool ok = read_sector(s_block_addressed ? sector : (sector * SECTOR_SIZE), buffer);
        deselect();
        if (!ok)
        {
            diag_text("[SD] ERROR reading sector ");
            diag_u32(sector);
            diag_text("\n");
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
    if (s_card_status != 0)
    {
        return RES_NOTRDY;
    }
    for (; count > 0; --count, ++sector, buffer += SECTOR_SIZE)
    {
        bool ok = write_sector(s_block_addressed ? sector : (sector * SECTOR_SIZE), buffer);
        deselect();
        if (!ok)
        {
            diag_text("[SD] ERROR writing sector ");
            diag_u32(sector);
            diag_text("\n");
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE drive, BYTE request, void *buffer)
{
    if (drive != 0)
    {
        return RES_PARERR;
    }
    if (s_card_status != 0)
    {
        return RES_NOTRDY;
    }

    switch (request)
    {
        case CTRL_SYNC:
        {
            cs_assert();
            bool ready = wait_ready();
            deselect();
            return ready ? RES_OK : RES_ERROR;
        }
        case GET_SECTOR_SIZE:
        {
            if (buffer != NULL)
            {
                *(WORD *) buffer = (WORD) SECTOR_SIZE;
                return RES_OK;
            }
            return RES_PARERR;
        }
        case GET_BLOCK_SIZE:
        {
            if (buffer != NULL)
            {
                *(DWORD *) buffer = 1; /* 1 sector */
                return RES_OK;
            }
            return RES_PARERR;
        }
        case GET_SECTOR_COUNT:
        {
            if (buffer != NULL)
            {
                sd_card_info_t info;
                if (sd_get_card_info(&info))
                {
                    *(LBA_t *) buffer = (LBA_t) (info.capacity_bytes / SECTOR_SIZE);
                    return RES_OK;
                }
            }
            return RES_ERROR;
        }
        default:
            return RES_PARERR;
    }
}

void sd_spi_reset(void)
{
    s_card_status = STA_NOINIT;
    s_block_addressed = false;
    s_slow_clock = true;
    deselect();
}

bool sd_get_card_info(sd_card_info_t *info)
{
    if (info == NULL || s_card_status != 0)
    {
        return false;
    }
    memset(info, 0, sizeof(sd_card_info_t));
    info->card_detected = true;
    info->is_sdhc = s_block_addressed;

    /* Read CID Register via CMD10 */
    if (command(10, 0) == 0x00)
    {
        uint8_t token = 0xFF;
        for (unsigned i = 0; i < 50000 && token == 0xFF; ++i)
        {
            token = spi_exchange(0xFF);
        }
        if (token == 0xFE)
        {
            uint8_t cid[16];
            for (unsigned i = 0; i < 16; ++i)
            {
                cid[i] = spi_exchange(0xFF);
            }
            spi_exchange(0xFF); /* CRC high */
            spi_exchange(0xFF); /* CRC low */

            info->mid = cid[0];
            info->oid[0] = (char) cid[1];
            info->oid[1] = (char) cid[2];
            info->oid[2] = '\0';
            for (int i = 0; i < 5; ++i)
            {
                info->pnm[i] = (char) cid[3 + i];
            }
            info->pnm[5] = '\0';
            info->prv_major = (cid[8] >> 4) & 0x0F;
            info->prv_minor = cid[8] & 0x0F;
            info->psn = ((uint32_t) cid[9] << 24) | ((uint32_t) cid[10] << 16) |
                        ((uint32_t) cid[11] << 8) | cid[12];
            info->mdt_year = 2000U + (uint16_t) (((cid[13] & 0x0F) << 4) | ((cid[14] >> 4) & 0x0F));
            info->mdt_month = cid[14] & 0x0F;
        }
    }
    deselect();

    /* Read CSD Register via CMD9 */
    if (command(9, 0) == 0x00)
    {
        uint8_t token = 0xFF;
        for (unsigned i = 0; i < 50000 && token == 0xFF; ++i)
        {
            token = spi_exchange(0xFF);
        }
        if (token == 0xFE)
        {
            uint8_t csd[16];
            for (unsigned i = 0; i < 16; ++i)
            {
                csd[i] = spi_exchange(0xFF);
            }
            spi_exchange(0xFF); /* CRC high */
            spi_exchange(0xFF); /* CRC low */

            uint8_t csd_ver = csd[0] >> 6;
            if (csd_ver == 1) /* CSD v2.0: SDHC / SDXC */
            {
                uint32_t c_size = ((uint32_t) (csd[7] & 0x3FU) << 16) |
                                  ((uint32_t) csd[8] << 8) |
                                  (uint32_t) csd[9];
                info->capacity_bytes = (uint64_t) (c_size + 1) * 512ULL * 1024ULL;
                info->capacity_mb = (uint32_t) (info->capacity_bytes / (1024ULL * 1024ULL));
            }
            else /* CSD v1.0: SDSC */
            {
                uint32_t c_size = ((uint32_t) (csd[6] & 3) << 10) |
                                  ((uint32_t) csd[7] << 2) |
                                  ((uint32_t) (csd[8] >> 6) & 3);
                uint8_t c_size_mult = (uint8_t) (((csd[9] & 3) << 1) | ((csd[10] >> 7) & 1));
                uint8_t read_bl_len = csd[5] & 0x0FU;
                uint64_t blocknr = (uint64_t) (c_size + 1) << (c_size_mult + 2);
                info->capacity_bytes = blocknr << read_bl_len;
                info->capacity_mb = (uint32_t) (info->capacity_bytes / (1024ULL * 1024ULL));
            }
            deselect();
            return true;
        }
    }
    deselect();
    return false;
}
