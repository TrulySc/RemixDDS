#ifndef BC7_DECODER_H
#define BC7_DECODER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Decode one BC7 16-byte block into 16 RGBA8 pixels.
void bc7_decode_block(const uint8_t block[16], uint8_t out_rgba[16 * 4]);

#ifdef __cplusplus
}
#endif

#endif
