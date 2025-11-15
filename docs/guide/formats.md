---
title: Supported Formats
layout: default
---

# Supported Formats

The converter handles the most common block-compressed formats found in:

- RTX Remix captures  
- Source 1 / Source 2 pipelines  
- General DX10+ DDS textures  

---

## BC1 / DXT1 (DXGI 71)

- Color: 4-bit interpolated RGB (5:6:5 per endpoint)
- Alpha: implicit (1-bit) when color0 <= color1
- Output: **RGBA PNG** (transparent pixels preserved)

---

## BC2 / DXT3 (DXGI 74)

- Color: BC1-style (DXT1)
- Alpha: explicit 4-bit per pixel
- Output: **RGBA PNG**
- Good for UI, decals, and hard-cut alpha.

---

## BC3 / DXT5 (DXGI 77)

- Color: BC1-style
- Alpha: BC4-style interpolation
- Output: **RGBA PNG**
- Common for diffuse + smooth alpha textures.

---

## BC4 (DXGI 80)

- Single-channel block compression
- Used for:
  - Height maps
  - Masks
  - Metallic/roughness channels in some pipelines
- Output: **8-bit grayscale PNG**

---

## BC5 (DXGI 83)

- Two channels (typically X/Y of a normal map)
- Z is reconstructed from X/Y:
  `z = sqrt(max(0, 1 - x² - y²))`
- Output: **RGB PNG** (encoded normal vector)

---

## BC7 (DXGI 98)

- High-quality color + alpha
- Several block modes, variable precision
- Used heavily in modern pipelines for:
  - Albedo / base color
  - Roughness/metallic
  - Normal maps

Output: **RGBA PNG**.

---

## Unsupported Formats

If a DDS uses a DXGI format not in the list above, the converter prints:

```text
ERROR: Unsupported DXGI format <N> in '<path>'
```

and returns `1` for that file, but the batch tool continues with the remaining jobs.
