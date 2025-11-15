---
title: RTX Remix DDS → PNG Converter
layout: default
---

# 🟧 RTX Remix DDS → PNG Converter  
### H.E.V MARK IV TEXTURE CONVERSION SUBSYSTEM

```
======================================================
   H.E.V MARK IV SUIT — TEXTURE CONVERSION SUBSYSTEM
======================================================
⏻ INITIALIZING BIOS…............................. OK
 BOOTING NEURAL INTERFACE…...................... OK
󱜮 CALIBRATING SENSOR ARRAY…...................... OK
 LOADING DECOMPRESSION MODULES…................. OK
 VITAL SIGNS…………………………………… STABLE
 ENVIRONMENTAL CONTROLS…………………… ONLINE
 SYSTEM READY.
======================================================
```

## Overview

This project is a complete GPU-block software decoder designed for use with:

- RTX Remix captures  
- Source Engine / Source 2 textures  
- General DX10-style DDS texture workflows  

No texconv, nvcompress, ImageMagick, or GPU hardware is required.

---

## Supported DXGI Formats

| DXGI | Format | Notes | Output |
|------|--------|-------|--------|
| 71   | BC1    | DXT1, RGB / 1-bit alpha | RGBA PNG |
| 74   | BC2    | DXT3, explicit alpha    | RGBA PNG |
| 77   | BC3    | DXT5, interpolated alpha| RGBA PNG |
| 80   | BC4    | Grayscale height/mask   | Gray PNG |
| 83   | BC5    | Normal maps             | RGB PNG  |
| 98   | BC7    | High-quality color/alpha| RGBA PNG |

---

## Features

- Native C/C++ software decoders (no GPU required)
- Standalone converter: `dds2png`
- Multithreaded batch processor: `batch_dds2png`
- H.E.V–themed terminal UI for the batch tool
- Zero external image libraries (only zlib + pthread + libm)
- Optimized for very large RTX Remix capture sets
