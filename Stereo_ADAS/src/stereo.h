#ifndef STEREO_H
#define STEREO_H

#include <stdbool.h>
#include <stdint.h>

#define STEREO_WIDTH 128U
#define STEREO_HEIGHT 128U
#define STEREO_DISPARITIES 16U

typedef bool (*stereo_read_row_t)(uint8_t *left, uint8_t *right);
typedef bool (*stereo_write_row_t)(const uint8_t *disparity);

bool stereo_process_pair(stereo_read_row_t read_row, stereo_write_row_t write_row);
bool stereo_run_dataset(void);

#endif
