#ifndef SD_CHECK_H
#define SD_CHECK_H

typedef enum
{
    SD_CHECK_RUNNING = 0,
    SD_CHECK_PASS_VOLUME = 1,
    SD_CHECK_PASS_FILE = 2,
    SD_CHECK_NO_CARD = -1,
    SD_CHECK_SECTOR_FAILED = -2,
    SD_CHECK_MOUNT_FAILED = -3,
    SD_CHECK_FILE_FAILED = -4,
} sd_check_result_t;

extern volatile sd_check_result_t g_sd_check_result;
void sd_check_run(void);
void sd_check_report(void);

#endif
