---
title: Installation
layout: default
---

# Installation

## Requirements

- C compiler: **gcc** or **clang**
- C++ compiler: **g++** (C++17)
- Libraries:
  - `zlib`
  - `pthread` (POSIX threads)
  - `libm`

On Debian/Ubuntu:

```bash
sudo apt update
sudo apt install build-essential zlib1g-dev cmake
```

On Arch:

```bash
sudo pacman -S base-devel zlib cmake
```

---

## Build with CMake (Recommended)

```bash
mkdir build
cd build
cmake ..
make -j
```

This produces:

- `dds2png` — single-file converter  
- `batch_dds2png` — multithreaded batch converter

---

## Build with Makefile

If you prefer the provided **Makefile**:

```bash
make -j
```

To build only one tool:

```bash
make dds2png
make batch_dds2png
```

You can then place the binaries somewhere in your `PATH`:

```bash
sudo cp dds2png batch_dds2png /usr/local/bin/
```
