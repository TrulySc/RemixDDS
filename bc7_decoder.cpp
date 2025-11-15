#include <stdint.h>
#include "bc7_decoder.h"
#include "bc7decomp.h"   // from the bc7decomp/bc7enc_rdo repo

extern "C" void bc7_decode_block(const uint8_t block[16], uint8_t out_rgba[16 * 4])
{
    bc7decomp::color_rgba pixels[16];

    bool ok = bc7decomp::unpack_bc7(block, pixels);
    if (!ok) {
        // Fallback: bright magenta if decode fails
        for (int i = 0; i < 16; ++i) {
            out_rgba[i*4+0] = 255;
            out_rgba[i*4+1] = 0;
            out_rgba[i*4+2] = 255;
            out_rgba[i*4+3] = 255;
        }
        return;
    }

    for (int i = 0; i < 16; ++i) {
        out_rgba[i*4+0] = pixels[i].r;
        out_rgba[i*4+1] = pixels[i].g;
        out_rgba[i*4+2] = pixels[i].b;
        out_rgba[i*4+3] = pixels[i].a;
    }
}
