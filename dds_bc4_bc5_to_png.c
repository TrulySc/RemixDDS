// dds_bc4_bc5_to_png.c
// Native BC4/BC5 → PNG converter
// Writes real PNG files without libpng.
// Only dependency: zlib.
//
// Build:
//   gcc -std=c11 -O2 dds_bc4_bc5_to_png.c -o dds_bc4_bc5_to_png -lz -lm
//
// Usage:
//   ./dds_bc4_bc5_to_png input.dds output.png

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>
#include <math.h>

#define DDS_MAGIC 0x20534444u
#define DDS_FOURCC(a,b,c,d) ((uint32_t)(a) | ((uint32_t)(b)<<8) | ((uint32_t)(c)<<16) | ((uint32_t)(d)<<24))

#define DXGI_FORMAT_BC4_UNORM 80u
#define DXGI_FORMAT_BC5_UNORM 83u

#pragma pack(push,1)
typedef struct {
    uint32_t dwSize, dwFlags, dwFourCC, dwRGBBitCount;
    uint32_t dwRBitMask, dwGBitMask, dwBBitMask, dwABitMask;
} DDS_PIXELFORMAT;

typedef struct {
    uint32_t dwSize, dwFlags, dwHeight, dwWidth, dwPitchOrLinearSize;
    uint32_t dwDepth, dwMipMapCount, dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t dwCaps, dwCaps2, dwCaps3, dwCaps4, dwReserved2;
} DDS_HEADER;

typedef struct {
    uint32_t dxgiFormat, resourceDimension, miscFlag, arraySize, miscFlags2;
} DDS_HEADER_DX10;
#pragma pack(pop)

// ---------------- PNG Writer ---------------------
static void png_write_u32(FILE *f, uint32_t v) {
    uint8_t b[4] = {v>>24, v>>16, v>>8, v};
    fwrite(b,1,4,f);
}

static uint32_t crc32_chunk(const uint8_t *buf, size_t len) {
    return crc32(0L, buf, len);
}

static int write_png_gray8(const char *path, uint32_t w, uint32_t h, const uint8_t *img)
{
    FILE *f = fopen(path,"wb");
    if(!f) return -1;

    // PNG signature
    static const uint8_t sig[8] = {137,80,78,71,13,10,26,10};
    fwrite(sig,1,8,f);

    // IHDR
    uint8_t ihdr[13];
    memset(ihdr,0,13);
    ihdr[0]=w>>24; ihdr[1]=w>>16; ihdr[2]=w>>8; ihdr[3]=w;
    ihdr[4]=h>>24; ihdr[5]=h>>16; ihdr[6]=h>>8; ihdr[7]=h;
    ihdr[8]=8;   // bit depth
    ihdr[9]=0;   // color type = grayscale
    ihdr[10]=0;  // compression
    ihdr[11]=0;  // filter
    ihdr[12]=0;  // interlace

    png_write_u32(f,13);
    fwrite("IHDR",1,4,f);
    fwrite(ihdr,1,13,f);
    uint32_t c = crc32(0L, Z_NULL, 0);
    c = crc32(c,(uint8_t*)"IHDR",4);
    c = crc32(c,ihdr,13);
    png_write_u32(f,c);

    // IDAT
    size_t raw_scan = (size_t)w + 1; // filter byte + pixels
    size_t raw_size = raw_scan * h;
    uint8_t *raw = malloc(raw_size);

    for (uint32_t y=0; y<h; y++) {
        raw[y*raw_scan] = 0; // no filter
        memcpy(&raw[y*raw_scan + 1], &img[y*w], w);
    }

    uLongf comp_bound = compressBound(raw_size);
    uint8_t *comp = malloc(comp_bound);

    if (compress2(comp, &comp_bound, raw, raw_size, 9) != Z_OK) {
        fclose(f);
        free(raw);
        free(comp);
        return -1;
    }

    png_write_u32(f, comp_bound);
    fwrite("IDAT",1,4,f);
    c = crc32(0L,Z_NULL,0);
    c = crc32(c,(uint8_t*)"IDAT",4);
    c = crc32(c, comp, comp_bound);
    fwrite(comp,1,comp_bound,f);
    png_write_u32(f, c);

    free(raw);
    free(comp);

    // IEND
    png_write_u32(f,0);
    fwrite("IEND",1,4,f);
    c = crc32(0L,Z_NULL,0);
    c = crc32(c,(uint8_t*)"IEND",4);
    png_write_u32(f,c);

    fclose(f);
    return 0;
}

static int write_png_rgb8(const char *path, uint32_t w, uint32_t h, const uint8_t *img)
{
    FILE *f = fopen(path,"wb");
    if(!f) return -1;

    static const uint8_t sig[8]={137,80,78,71,13,10,26,10};
    fwrite(sig,1,8,f);

    uint8_t ihdr[13]={0};
    ihdr[0]=w>>24; ihdr[1]=w>>16; ihdr[2]=w>>8; ihdr[3]=w;
    ihdr[4]=h>>24; ihdr[5]=h>>16; ihdr[6]=h>>8; ihdr[7]=h;
    ihdr[8]=8; ihdr[9]=2;  // color type = RGB
    png_write_u32(f,13);
    fwrite("IHDR",1,4,f);
    fwrite(ihdr,1,13,f);

    uint32_t c=crc32(0L,Z_NULL,0);
    c=crc32(c,(uint8_t*)"IHDR",4);
    c=crc32(c,ihdr,13);
    png_write_u32(f,c);

    size_t raw_scan = (size_t)w*3 + 1;
    size_t raw_size = raw_scan * h;
    uint8_t *raw = malloc(raw_size);

    for (uint32_t y=0; y<h; y++) {
        raw[y*raw_scan] = 0;
        memcpy(&raw[y*raw_scan + 1], &img[y*w*3], w*3);
    }

    uLongf comp_bound = compressBound(raw_size);
    uint8_t *comp = malloc(comp_bound);

    compress2(comp,&comp_bound,raw,raw_size,9);

    png_write_u32(f, comp_bound);
    fwrite("IDAT",1,4,f);

    c=crc32(0L,Z_NULL,0);
    c=crc32(c,(uint8_t*)"IDAT",4);
    c=crc32(c,comp,comp_bound);
    fwrite(comp,1,comp_bound,f);
    png_write_u32(f,c);

    free(raw);
    free(comp);

    png_write_u32(f,0);
    fwrite("IEND",1,4,f);
    c=crc32(0L,Z_NULL,0);
    c=crc32(c,(uint8_t*)"IEND",4);
    png_write_u32(f,c);

    fclose(f);
    return 0;
}


// ---------------- BC4 decoder ------------------
static void decode_bc4(const uint8_t blk[8], uint8_t out[16])
{
    uint8_t r0=blk[0], r1=blk[1];
    uint8_t pal[8];

    pal[0]=r0; pal[1]=r1;

    if (r0>r1) {
        pal[2]=(6*r0+  r1+3)/7;
        pal[3]=(5*r0+2*r1+3)/7;
        pal[4]=(4*r0+3*r1+3)/7;
        pal[5]=(3*r0+4*r1+3)/7;
        pal[6]=(2*r0+5*r1+3)/7;
        pal[7]=(  r0+6*r1+3)/7;
    } else {
        pal[2]=(4*r0+  r1+2)/5;
        pal[3]=(3*r0+2*r1+2)/5;
        pal[4]=(2*r0+3*r1+2)/5;
        pal[5]=(  r0+4*r1+2)/5;
        pal[6]=0;
        pal[7]=255;
    }

    uint64_t bits=0;
    for(int i=0;i<6;i++) bits |= ((uint64_t)blk[2+i]) << (8*i);

    for (int i=0;i<16;i++) out[i] = pal[(bits>>(3*i))&7];
}


// ---------------- Main -------------------
int main(int argc,char **argv)
{
    if(argc != 3){
        fprintf(stderr,"Usage: %s input.dds output.png\n",argv[0]);
        return 1;
    }

    const char *in=argv[1], *out=argv[2];
    FILE *f=fopen(in,"rb");
    if(!f){ perror("open"); return 1; }

    uint32_t magic; fread(&magic,4,1,f);
    if(magic!=DDS_MAGIC){ fprintf(stderr,"Not DDS\n"); return 1;}

    DDS_HEADER hdr; fread(&hdr,sizeof(hdr),1,f);
    if(hdr.ddspf.dwFourCC != DDS_FOURCC('D','X','1','0')){
        fprintf(stderr,"Only DX10 DDS supported\n");
        return 1;
    }

    DDS_HEADER_DX10 dx10;
    fread(&dx10,sizeof(dx10),1,f);

    uint32_t w=hdr.dwWidth, h=hdr.dwHeight;
    uint32_t bx=(w+3)/4, by=(h+3)/4;
    uint64_t blocks=(uint64_t)bx*by;

    uint32_t fmt=dx10.dxgiFormat;

    if(fmt==DXGI_FORMAT_BC4_UNORM)
    {
        size_t bytes = blocks*8;
        uint8_t *bc = malloc(bytes);
        fread(bc,1,bytes,f);
        fclose(f);

        uint8_t *img = malloc((size_t)w*h);

        for(uint32_t yb=0; yb<by; yb++)
            for(uint32_t xb=0; xb<bx; xb++)
            {
                uint8_t pix[16];
                decode_bc4(bc + (yb*bx+xb)*8, pix);

                for(int py=0; py<4; py++)
                    for(int px=0; px<4; px++)
                    {
                        uint32_t x = xb*4+px;
                        uint32_t y = yb*4+py;
                        if(x<w && y<h)
                            img[y*w+x] = pix[py*4+px];
                    }
            }

            int ok = write_png_gray8(out, w, h, img);

            free(img);
            free(bc);
            return ok;
    }

    if(fmt==DXGI_FORMAT_BC5_UNORM)
    {
        size_t bytes = blocks*16;
        uint8_t *bc = malloc(bytes);
        fread(bc,1,bytes,f);
        fclose(f);

        uint8_t *img = malloc((size_t)w*h*3);

        for(uint32_t yb=0; yb<by; yb++)
            for(uint32_t xb=0; xb<bx; xb++)
            {
                uint8_t rx[16], gy[16];
                const uint8_t *blk = bc + (yb*bx+xb)*16;

                decode_bc4(blk,   rx);
                decode_bc4(blk+8, gy);

                for(int py=0; py<4; py++)
                    for(int px=0; px<4; px++)
                    {
                        uint32_t x = xb*4+px;
                        uint32_t y = yb*4+py;
                        if(x>=w||y>=h) continue;

                        double nx = rx[py*4+px]/255.0 *2 -1;
                        double ny = gy[py*4+px]/255.0 *2 -1;
                        double nz2 = 1 - nx*nx - ny*ny;
                        double nz = nz2>0 ? sqrt(nz2) : 0;

                        size_t p = ((size_t)y*w + x)*3;
                        img[p+0] = (uint8_t)((nx*0.5+0.5)*255);
                        img[p+1] = (uint8_t)((ny*0.5+0.5)*255);
                        img[p+2] = (uint8_t)((nz*0.5+0.5)*255);
                    }
            }

            int ok = write_png_rgb8(out, w, h, img);

            free(img);
            free(bc);

            return ok;
    }

    fprintf(stderr,"Unsupported DXGI format %u\n",fmt);
    return 1;
}
