// dds_bc_all_to_png.c
//
// Native BC1/BC2/BC3/BC4/BC5/BC7 DDS → PNG converter.
// - BC1_UNORM (71) -> RGBA PNG (DXT1)
// - BC2_UNORM (74) -> RGBA PNG (DXT3)
// - BC3_UNORM (77) -> RGBA PNG (DXT5)
// - BC4_UNORM (80) -> grayscale PNG
// - BC5_UNORM (83) -> RGB PNG (normal map)
// - BC7_UNORM (98) -> RGBA PNG
//
// No libpng, no external tools. Only dependency: zlib.
//
// Public entry point (used by batch_dds2png.cpp):
//     extern "C" int dds2png_convert(const char* input, const char* output);
//
// Standalone build usage (if STANDALONE is defined):
//     dds2png in.dds out.png
//
// Example builds:
//   g++ -std=c++17 -O2 dds_bc_all_to_png.c bc7_decoder.cpp bc7decomp.cpp -o dds2png -lz -lm
//   g++ -std=c++17 -O2 batch_dds2png.cpp dds_bc_all_to_png.c bc7_decoder.cpp bc7decomp.cpp -o batch_dds2png -lz -lm -lpthread

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <zlib.h>

// ----------------------- DDS Structures & Constants -----------------------

#define DDS_MAGIC 0x20534444u
#define DDS_FOURCC(a,b,c,d) ((uint32_t)(a) | ((uint32_t)(b)<<8) | ((uint32_t)(c)<<16) | ((uint32_t)(d)<<24))

// DXGI formats we support
#define DXGI_FORMAT_BC1_UNORM 71u
#define DXGI_FORMAT_BC2_UNORM 74u
#define DXGI_FORMAT_BC3_UNORM 77u
#define DXGI_FORMAT_BC4_UNORM 80u
#define DXGI_FORMAT_BC5_UNORM 83u
#define DXGI_FORMAT_BC7_UNORM 98u

#pragma pack(push,1)

typedef struct {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwFourCC;
    uint32_t dwRGBBitCount;
    uint32_t dwRBitMask;
    uint32_t dwGBitMask;
    uint32_t dwBBitMask;
    uint32_t dwABitMask;
} DDS_PIXELFORMAT;

typedef struct {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwHeight;
    uint32_t dwWidth;
    uint32_t dwPitchOrLinearSize;
    uint32_t dwDepth;
    uint32_t dwMipMapCount;
    uint32_t dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t dwCaps;
    uint32_t dwCaps2;
    uint32_t dwCaps3;
    uint32_t dwCaps4;
    uint32_t dwReserved2;
} DDS_HEADER;

typedef struct {
    uint32_t dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
} DDS_HEADER_DX10;

#pragma pack(pop)

// ----------------------- External BC7 Decoder -----------------------
//
// Implemented in bc7_decoder.cpp and backed by bc7decomp.cpp

#ifdef __cplusplus
extern "C" {
    #endif
    void bc7_decode_block(const uint8_t block[16], uint8_t out_rgba[16 * 4]);
    #ifdef __cplusplus
}
#endif

// ----------------------- PNG Helper Functions -----------------------

static void png_write_u32(FILE* f, uint32_t v)
{
    uint8_t b[4];
    b[0] = (uint8_t)((v >> 24) & 0xFF);
    b[1] = (uint8_t)((v >> 16) & 0xFF);
    b[2] = (uint8_t)((v >>  8) & 0xFF);
    b[3] = (uint8_t)( v        & 0xFF);
    fwrite(b, 1, 4, f);
}

static int write_png_core(
    const char* path,
    uint32_t width,
    uint32_t height,
    const uint8_t* image,
    uint8_t color_type,     // 0 = gray, 2 = RGB, 6 = RGBA
    uint8_t bytes_per_pixel // 1, 3, or 4
)
{
    FILE* f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "ERROR: Failed to open '%s' for writing\n", path);
        return 1;
    }

    // PNG signature
    static const uint8_t sig[8] = {137,80,78,71,13,10,26,10};
    fwrite(sig, 1, 8, f);

    // IHDR chunk
    uint8_t ihdr[13];
    ihdr[0] = (uint8_t)((width  >> 24) & 0xFF);
    ihdr[1] = (uint8_t)((width  >> 16) & 0xFF);
    ihdr[2] = (uint8_t)((width  >>  8) & 0xFF);
    ihdr[3] = (uint8_t)( width         & 0xFF);

    ihdr[4] = (uint8_t)((height >> 24) & 0xFF);
    ihdr[5] = (uint8_t)((height >> 16) & 0xFF);
    ihdr[6] = (uint8_t)((height >>  8) & 0xFF);
    ihdr[7] = (uint8_t)( height        & 0xFF);

    ihdr[8]  = 8;            // bit depth
    ihdr[9]  = color_type;   // 0 = gray, 2 = RGB, 6 = RGBA
    ihdr[10] = 0;            // compression method
    ihdr[11] = 0;            // filter method
    ihdr[12] = 0;            // interlace (none)

    png_write_u32(f, 13);
    fwrite("IHDR", 1, 4, f);
    fwrite(ihdr, 1, 13, f);

    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef*)"IHDR", 4);
    crc = crc32(crc, ihdr, 13);
    png_write_u32(f, crc);

    // Build uncompressed scanline buffer: [filter byte][pixel bytes...]
    const size_t stride_raw = (size_t)width * bytes_per_pixel + 1;
    const size_t raw_size   = stride_raw * height;

    uint8_t* raw = (uint8_t*)malloc(raw_size);
    if (!raw) {
        fprintf(stderr, "ERROR: Out of memory in write_png_core raw\n");
        fclose(f);
        return 1;
    }

    for (uint32_t y = 0; y < height; ++y) {
        size_t row_off = (size_t)y * stride_raw;
        raw[row_off] = 0; // filter type 0 (None)
        memcpy(&raw[row_off + 1], &image[(size_t)y * width * bytes_per_pixel], width * bytes_per_pixel);
    }

    // Compress with zlib
    uLongf comp_bound = compressBound(raw_size);
    uint8_t* comp = (uint8_t*)malloc(comp_bound);
    if (!comp) {
        fprintf(stderr, "ERROR: Out of memory in write_png_core comp\n");
        free(raw);
        fclose(f);
        return 1;
    }

    if (compress2(comp, &comp_bound, raw, raw_size, 9) != Z_OK) {
        fprintf(stderr, "ERROR: zlib compress2() failed\n");
        free(raw);
        free(comp);
        fclose(f);
        return 1;
    }

    // IDAT chunk
    png_write_u32(f, (uint32_t)comp_bound);
    fwrite("IDAT", 1, 4, f);

    crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef*)"IDAT", 4);
    crc = crc32(crc, comp, comp_bound);
    fwrite(comp, 1, comp_bound, f);
    png_write_u32(f, crc);

    free(raw);
    free(comp);

    // IEND chunk
    png_write_u32(f, 0);
    fwrite("IEND", 1, 4, f);
    crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef*)"IEND", 4);
    png_write_u32(f, crc);

    fclose(f);
    return 0;
}

static int write_png_gray8(const char* path, uint32_t w, uint32_t h, const uint8_t* img)
{
    return write_png_core(path, w, h, img, 0, 1);
}

static int write_png_rgb8(const char* path, uint32_t w, uint32_t h, const uint8_t* img)
{
    return write_png_core(path, w, h, img, 2, 3);
}

static int write_png_rgba8(const char* path, uint32_t w, uint32_t h, const uint8_t* img)
{
    return write_png_core(path, w, h, img, 6, 4);
}

// ----------------------- BC4 Block Decode (also used for BC3 alpha) -----------------------

static void decode_bc4_block(const uint8_t block[8], uint8_t out[16])
{
    uint8_t r0 = block[0];
    uint8_t r1 = block[1];

    uint8_t pal[8];
    pal[0] = r0;
    pal[1] = r1;

    if (r0 > r1) {
        pal[2] = (uint8_t)((6*r0 +   r1 + 3) / 7);
        pal[3] = (uint8_t)((5*r0 + 2*r1 + 3) / 7);
        pal[4] = (uint8_t)((4*r0 + 3*r1 + 3) / 7);
        pal[5] = (uint8_t)((3*r0 + 4*r1 + 3) / 7);
        pal[6] = (uint8_t)((2*r0 + 5*r1 + 3) / 7);
        pal[7] = (uint8_t)((  r0 + 6*r1 + 3) / 7);
    } else {
        pal[2] = (uint8_t)((4*r0 +   r1 + 2) / 5);
        pal[3] = (uint8_t)((3*r0 + 2*r1 + 2) / 5);
        pal[4] = (uint8_t)((2*r0 + 3*r1 + 2) / 5);
        pal[5] = (uint8_t)((  r0 + 4*r1 + 2) / 5);
        pal[6] = 0;
        pal[7] = 255;
    }

    uint64_t bits = 0;
    for (int i = 0; i < 6; ++i) {
        bits |= ((uint64_t)block[2 + i]) << (8 * i);
    }

    for (int i = 0; i < 16; ++i) {
        out[i] = pal[(bits >> (3 * i)) & 7u];
    }
}

// ----------------------- BC1 / BC2 / BC3 Decoding -----------------------

// Convert 16-bit 5:6:5 color to 8-bit per channel.
static void rgb565_to_rgb888(uint16_t c, uint8_t* r, uint8_t* g, uint8_t* b)
{
    uint8_t r5 = (uint8_t)((c >> 11) & 0x1F);
    uint8_t g6 = (uint8_t)((c >> 5)  & 0x3F);
    uint8_t b5 = (uint8_t)( c        & 0x1F);

    *r = (uint8_t)((r5 * 255 + 15) / 31);
    *g = (uint8_t)((g6 * 255 + 31) / 63);
    *b = (uint8_t)((b5 * 255 + 15) / 31);
}

// Decode a BC1 (DXT1) block into 16 RGBA pixels.
static void decode_bc1_block(const uint8_t block[8], uint8_t out_rgba[16 * 4])
{
    uint16_t c0 = (uint16_t)(block[0] | (block[1] << 8));
    uint16_t c1 = (uint16_t)(block[2] | (block[3] << 8));

    uint8_t r0, g0, b0;
    uint8_t r1, g1, b1;
    rgb565_to_rgb888(c0, &r0, &g0, &b0);
    rgb565_to_rgb888(c1, &r1, &g1, &b1);

    uint8_t c[4][4]; // [color_index][rgba]
    c[0][0] = r0; c[0][1] = g0; c[0][2] = b0; c[0][3] = 255;
    c[1][0] = r1; c[1][1] = g1; c[1][2] = b1; c[1][3] = 255;

    if (c0 > c1) {
        // 4-color block
        c[2][0] = (uint8_t)((2*r0 + r1) / 3);
        c[2][1] = (uint8_t)((2*g0 + g1) / 3);
        c[2][2] = (uint8_t)((2*b0 + b1) / 3);
        c[2][3] = 255;

        c[3][0] = (uint8_t)((r0 + 2*r1) / 3);
        c[3][1] = (uint8_t)((g0 + 2*g1) / 3);
        c[3][2] = (uint8_t)((b0 + 2*b1) / 3);
        c[3][3] = 255;
    } else {
        // 3-color + 1 transparency
        c[2][0] = (uint8_t)((r0 + r1) / 2);
        c[2][1] = (uint8_t)((g0 + g1) / 2);
        c[2][2] = (uint8_t)((b0 + b1) / 2);
        c[2][3] = 255;

        c[3][0] = 0;
        c[3][1] = 0;
        c[3][2] = 0;
        c[3][3] = 0; // transparent
    }

    uint32_t indices = (uint32_t)(block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24));

    for (int i = 0; i < 16; ++i) {
        uint32_t idx = (indices >> (2 * i)) & 0x3u;
        out_rgba[i*4 + 0] = c[idx][0];
        out_rgba[i*4 + 1] = c[idx][1];
        out_rgba[i*4 + 2] = c[idx][2];
        out_rgba[i*4 + 3] = c[idx][3];
    }
}

// Decode BC2 (DXT3) alpha (explicit 4-bit alpha).
static void decode_bc2_alpha(const uint8_t alphaBlock[8], uint8_t out_alpha[16])
{
    // 64 bits total, little-endian, 16 4-bit values
    uint64_t bits = 0;
    for (int i = 0; i < 8; ++i) {
        bits |= ((uint64_t)alphaBlock[i]) << (8 * i);
    }
    for (int i = 0; i < 16; ++i) {
        uint8_t a4 = (uint8_t)((bits >> (4 * i)) & 0xF);
        out_alpha[i] = (uint8_t)(a4 * 17); // 0..15 -> 0..255
    }
}

// ----------------------- Main Conversion Function -----------------------

#ifdef __cplusplus
extern "C" {
    #endif

    int dds2png_convert(const char* input, const char* output)
    {
        FILE* f = fopen(input, "rb");
        if (!f) {
            fprintf(stderr, "ERROR: cannot open '%s'\n", input);
            return 1;
        }

        // Check magic
        uint32_t magic = 0;
        if (fread(&magic, 1, 4, f) != 4 || magic != DDS_MAGIC) {
            fclose(f);
            return 1;
        }

        // Read DDS header
        DDS_HEADER hdr;
        if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
            fclose(f);
            return 1;
        }

        // We only support DX10 extended header
        if (hdr.ddspf.dwFourCC != DDS_FOURCC('D','X','1','0')) {
            fprintf(stderr, "ERROR: non-DX10 DDS unsupported: %s\n", input);
            fclose(f);
            return 1;
        }

        DDS_HEADER_DX10 dx10;
        if (fread(&dx10, sizeof(dx10), 1, f) != 1) {
            fclose(f);
            return 1;
        }

        uint32_t w = hdr.dwWidth;
        uint32_t h = hdr.dwHeight;

        if (w == 0 || h == 0) {
            fclose(f);
            return 1;
        }

        uint32_t fmt = dx10.dxgiFormat;
        uint32_t blocks_x = (w + 3) / 4;
        uint32_t blocks_y = (h + 3) / 4;
        uint64_t block_count = (uint64_t)blocks_x * blocks_y;

        // ---------------- BC1 (71) ----------------
        if (fmt == DXGI_FORMAT_BC1_UNORM)
        {
            size_t bc_bytes = (size_t)block_count * 8u;
            uint8_t* bc = (uint8_t*)malloc(bc_bytes);
            if (!bc) {
                fclose(f);
                return 1;
            }

            if (fread(bc, 1, bc_bytes, f) != bc_bytes) {
                free(bc);
                fclose(f);
                return 1;
            }
            fclose(f);

            uint8_t* img = (uint8_t*)malloc((size_t)w * h * 4u);
            if (!img) {
                free(bc);
                return 1;
            }

            for (uint32_t by = 0; by < blocks_y; ++by) {
                for (uint32_t bx = 0; bx < blocks_x; ++bx) {
                    const uint8_t* blk = bc + ((uint64_t)by * blocks_x + bx) * 8u;
                    uint8_t rgba_block[16 * 4];
                    decode_bc1_block(blk, rgba_block);

                    for (uint32_t py = 0; py < 4; ++py) {
                        for (uint32_t px = 0; px < 4; ++px) {
                            uint32_t x = bx * 4 + px;
                            uint32_t y = by * 4 + py;
                            if (x >= w || y >= h) continue;

                            size_t dst = ((size_t)y * w + x) * 4u;
                            size_t src = (py * 4 + px) * 4u;

                            img[dst + 0] = rgba_block[src + 0];
                            img[dst + 1] = rgba_block[src + 1];
                            img[dst + 2] = rgba_block[src + 2];
                            img[dst + 3] = rgba_block[src + 3];
                        }
                    }
                }
            }

            int ret = write_png_rgba8(output, w, h, img);
            free(img);
            free(bc);
            return ret;
        }

        // ---------------- BC2 (74) ----------------
        if (fmt == DXGI_FORMAT_BC2_UNORM)
        {
            size_t bc_bytes = (size_t)block_count * 16u;
            uint8_t* bc = (uint8_t*)malloc(bc_bytes);
            if (!bc) {
                fclose(f);
                return 1;
            }

            if (fread(bc, 1, bc_bytes, f) != bc_bytes) {
                free(bc);
                fclose(f);
                return 1;
            }
            fclose(f);

            uint8_t* img = (uint8_t*)malloc((size_t)w * h * 4u);
            if (!img) {
                free(bc);
                return 1;
            }

            for (uint32_t by = 0; by < blocks_y; ++by) {
                for (uint32_t bx = 0; bx < blocks_x; ++bx) {
                    const uint8_t* blk = bc + ((uint64_t)by * blocks_x + bx) * 16u;

                    // First 8 bytes: alpha; next 8 bytes: BC1 color
                    uint8_t alpha_block[8];
                    memcpy(alpha_block, blk, 8);
                    uint8_t alpha[16];
                    decode_bc2_alpha(alpha_block, alpha);

                    uint8_t rgba_color[16 * 4];
                    decode_bc1_block(blk + 8, rgba_color);

                    for (uint32_t py = 0; py < 4; ++py) {
                        for (uint32_t px = 0; px < 4; ++px) {
                            uint32_t x = bx * 4 + px;
                            uint32_t y = by * 4 + py;
                            if (x >= w || y >= h) continue;

                            size_t dst = ((size_t)y * w + x) * 4u;
                            size_t idx = (size_t)py * 4 + px;
                            size_t src = idx * 4u;

                            img[dst + 0] = rgba_color[src + 0];
                            img[dst + 1] = rgba_color[src + 1];
                            img[dst + 2] = rgba_color[src + 2];
                            img[dst + 3] = alpha[idx];
                        }
                    }
                }
            }

            int ret = write_png_rgba8(output, w, h, img);
            free(img);
            free(bc);
            return ret;
        }

        // ---------------- BC3 (77) ----------------
        if (fmt == DXGI_FORMAT_BC3_UNORM)
        {
            size_t bc_bytes = (size_t)block_count * 16u;
            uint8_t* bc = (uint8_t*)malloc(bc_bytes);
            if (!bc) {
                fclose(f);
                return 1;
            }

            if (fread(bc, 1, bc_bytes, f) != bc_bytes) {
                free(bc);
                fclose(f);
                return 1;
            }
            fclose(f);

            uint8_t* img = (uint8_t*)malloc((size_t)w * h * 4u);
            if (!img) {
                free(bc);
                return 1;
            }

            for (uint32_t by = 0; by < blocks_y; ++by) {
                for (uint32_t bx = 0; bx < blocks_x; ++bx) {
                    const uint8_t* blk = bc + ((uint64_t)by * blocks_x + bx) * 16u;

                    // First 8 bytes: BC4-style alpha; next 8 bytes: BC1 color
                    uint8_t alpha[16];
                    decode_bc4_block(blk, alpha);

                    uint8_t rgba_color[16 * 4];
                    decode_bc1_block(blk + 8, rgba_color);

                    for (uint32_t py = 0; py < 4; ++py) {
                        for (uint32_t px = 0; px < 4; ++px) {
                            uint32_t x = bx * 4 + px;
                            uint32_t y = by * 4 + py;
                            if (x >= w || y >= h) continue;

                            size_t dst = ((size_t)y * w + x) * 4u;
                            size_t idx = (size_t)py * 4 + px;
                            size_t src = idx * 4u;

                            img[dst + 0] = rgba_color[src + 0];
                            img[dst + 1] = rgba_color[src + 1];
                            img[dst + 2] = rgba_color[src + 2];
                            img[dst + 3] = alpha[idx];
                        }
                    }
                }
            }

            int ret = write_png_rgba8(output, w, h, img);
            free(img);
            free(bc);
            return ret;
        }

        // ---------------- BC4 (80) ----------------
        if (fmt == DXGI_FORMAT_BC4_UNORM)
        {
            size_t bc_bytes = (size_t)block_count * 8u;
            uint8_t* bc = (uint8_t*)malloc(bc_bytes);
            if (!bc) {
                fclose(f);
                return 1;
            }

            if (fread(bc, 1, bc_bytes, f) != bc_bytes) {
                free(bc);
                fclose(f);
                return 1;
            }
            fclose(f);

            uint8_t* img = (uint8_t*)malloc((size_t)w * h);
            if (!img) {
                free(bc);
                return 1;
            }

            for (uint32_t by = 0; by < blocks_y; ++by) {
                for (uint32_t bx = 0; bx < blocks_x; ++bx) {
                    const uint8_t* blk = bc + ((uint64_t)by * blocks_x + bx) * 8u;
                    uint8_t block_pixels[16];
                    decode_bc4_block(blk, block_pixels);

                    for (uint32_t py = 0; py < 4; ++py) {
                        for (uint32_t px = 0; px < 4; ++px) {
                            uint32_t x = bx * 4 + px;
                            uint32_t y = by * 4 + py;
                            if (x < w && y < h) {
                                img[(size_t)y * w + x] = block_pixels[py * 4 + px];
                            }
                        }
                    }
                }
            }

            int ret = write_png_gray8(output, w, h, img);
            free(img);
            free(bc);
            return ret;
        }

        // ---------------- BC5 (83) ----------------
        if (fmt == DXGI_FORMAT_BC5_UNORM)
        {
            size_t bc_bytes = (size_t)block_count * 16u;
            uint8_t* bc = (uint8_t*)malloc(bc_bytes);
            if (!bc) {
                fclose(f);
                return 1;
            }

            if (fread(bc, 1, bc_bytes, f) != bc_bytes) {
                free(bc);
                fclose(f);
                return 1;
            }
            fclose(f);

            uint8_t* img = (uint8_t*)malloc((size_t)w * h * 3u);
            if (!img) {
                free(bc);
                return 1;
            }

            for (uint32_t by = 0; by < blocks_y; ++by) {
                for (uint32_t bx = 0; bx < blocks_x; ++bx) {
                    const uint8_t* blk = bc + ((uint64_t)by * blocks_x + bx) * 16u;

                    uint8_t rx[16];
                    uint8_t gy[16];
                    decode_bc4_block(blk,     rx);
                    decode_bc4_block(blk + 8, gy);

                    for (uint32_t py = 0; py < 4; ++py) {
                        for (uint32_t px = 0; px < 4; ++px) {
                            uint32_t x = bx * 4 + px;
                            uint32_t y = by * 4 + py;
                            if (x >= w || y >= h) continue;

                            double nx = (double)rx[py*4 + px] / 255.0 * 2.0 - 1.0;
                            double ny = (double)gy[py*4 + px] / 255.0 * 2.0 - 1.0;
                            double nz2 = 1.0 - nx*nx - ny*ny;
                            double nz  = (nz2 > 0.0) ? sqrt(nz2) : 0.0;

                            size_t idx = ((size_t)y * w + x) * 3u;
                            img[idx + 0] = (uint8_t)((nx * 0.5 + 0.5) * 255.0 + 0.5);
                            img[idx + 1] = (uint8_t)((ny * 0.5 + 0.5) * 255.0 + 0.5);
                            img[idx + 2] = (uint8_t)((nz * 0.5 + 0.5) * 255.0 + 0.5);
                        }
                    }
                }
            }

            int ret = write_png_rgb8(output, w, h, img);
            free(img);
            free(bc);
            return ret;
        }

        // ---------------- BC7 (98) ----------------
        if (fmt == DXGI_FORMAT_BC7_UNORM)
        {
            size_t bc_bytes = (size_t)block_count * 16u;
            uint8_t* bc = (uint8_t*)malloc(bc_bytes);
            if (!bc) {
                fclose(f);
                return 1;
            }

            if (fread(bc, 1, bc_bytes, f) != bc_bytes) {
                free(bc);
                fclose(f);
                return 1;
            }

            fclose(f);

            uint8_t* img = (uint8_t*)malloc((size_t)w * h * 4u);
            if (!img) {
                free(bc);
                return 1;
            }

            for (uint32_t by = 0; by < blocks_y; ++by) {
                for (uint32_t bx = 0; bx < blocks_x; ++bx) {
                    const uint8_t* blk = bc + ((uint64_t)by * blocks_x + bx) * 16u;
                    uint8_t rgba_block[16 * 4];

                    bc7_decode_block(blk, rgba_block);

                    for (uint32_t py = 0; py < 4; ++py) {
                        for (uint32_t px = 0; px < 4; ++px) {
                            uint32_t x = bx * 4 + px;
                            uint32_t y = by * 4 + py;
                            if (x >= w || y >= h) continue;

                            size_t dst = ((size_t)y * w + x) * 4u;
                            size_t src = (py * 4 + px) * 4u;

                            img[dst + 0] = rgba_block[src + 0];
                            img[dst + 1] = rgba_block[src + 1];
                            img[dst + 2] = rgba_block[src + 2];
                            img[dst + 3] = rgba_block[src + 3];
                        }
                    }
                }
            }

            int ret = write_png_rgba8(output, w, h, img);
            free(img);
            free(bc);
            return ret;
        }

        // ---------------- Unsupported Format ----------------
        fprintf(stderr,
                "ERROR: Unsupported DXGI format %u in '%s' (BC1=71, BC2=74, BC3=77, BC4=80, BC5=83, BC7=98)\n",
                fmt, input);
        fclose(f);
        return 1;
    }

    #ifdef __cplusplus
}
#endif

// ----------------------- Optional Standalone Main -----------------------

#ifdef STANDALONE
int main(int argc, char** argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s input.dds output.png\n", argv[0]);
        return 1;
    }
    return dds2png_convert(argv[1], argv[2]);
}
#endif
