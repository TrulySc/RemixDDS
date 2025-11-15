---
title: Usage
layout: default
---

# Usage

## Single-File Conversion (`dds2png`)

Convert one DDS file to PNG:

```bash
./dds2png input.dds output.png
```

If the DDS uses a supported DXGI block format (BC1/2/3/4/5/7), it will be decoded and a PNG is written.

Return codes:

- `0` — success  
- `1` — failure (unsupported format, IO error, etc.)

---

## Batch Conversion (`batch_dds2png`)

Recursively convert every `.dds` file under a directory:

```bash
./batch_dds2png /path/to/capture_root
```

Specify number of worker threads:

```bash
./batch_dds2png /path/to/capture_root 12
```

The batch converter:

- Recursively scans for `.dds` files
- Skips files that already have a matching `.png` beside them
- Uses a job queue + worker threads
- Displays an H.E.V–style progress bar
- Prints errors for individual failures but continues processing

Example output (simplified):

```text
⏻ INITIALIZING H.E.V MARK IV SUIT SYSTEMS… OK
 BOOTING NEURAL INTERFACE… OK
...
[██████████░░░░░░░░░░░░░░] 42.0%  (1800 / 4280)    2480 remaining
```

---

## Tips for RTX Remix Captures

- Run `batch_dds2png` on the **root of the capture** directory.  
- The converter will mirror the folder structure, writing PNGs alongside the DDS files.
- You can safely re-run the tool: it will skip already-converted PNGs by default.
