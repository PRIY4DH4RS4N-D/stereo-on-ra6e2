/*
 * Simple, robust Deflate/zlib decompressor for embedded systems
 * Decompresses RFC 1950 / 1951 Deflate stream without dynamic memory allocation
 */
#ifndef MINI_INFLATE_H_
#define MINI_INFLATE_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decompresses zlib-wrapped deflate stream into output buffer */
int mini_zlib_decompress(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* MINI_INFLATE_H_ */
