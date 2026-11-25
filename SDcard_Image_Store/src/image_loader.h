#ifndef IMAGE_LOADER_H_
#define IMAGE_LOADER_H_

#include <stdint.h>
#include <stdbool.h>

#define MAX_IMAGE_FILE_SIZE   (12 * 1024) /* 12 KB buffer for compressed PNG file */
#define RAW_IMAGE_WIDTH       128
#define RAW_IMAGE_HEIGHT      128
#define RAW_IMAGE_PIXELS      (RAW_IMAGE_WIDTH * RAW_IMAGE_HEIGHT) /* 16,384 bytes */

/* 1. Raw Compressed PNG file buffer */
extern uint8_t g_image_buffer[MAX_IMAGE_FILE_SIZE];
extern uint32_t g_image_size;
extern uint32_t g_image_ram_address;

/* 2. Decoded Raw Grayscale Pixels (128x128 8bpp) for e2 studio Image Visualizer */
extern uint8_t g_decoded_pixels[16512];
extern uint32_t g_decoded_pixels_address;

/* Run image loader test */
bool image_loader_run(void);

#endif /* IMAGE_LOADER_H_ */
