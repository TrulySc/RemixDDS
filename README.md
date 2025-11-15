# 🟧 H.E.V MARK IV SYSTEMS — DDS → PNG CONVERTER  
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

## 🧩 FULL GPU BLOCK FORMAT SUPPORT  
**BC1 / BC2 / BC3 / BC4 / BC5 / BC7 → PNG**

| DXGI | Format | Description | Output |
|------|---------|-------------|---------|
| 71 | BC1 | DXT1 (RGB/1-bit A) | RGBA PNG |
| 74 | BC2 | DXT3 (explicit A) | RGBA PNG |
| 77 | BC3 | DXT5 (interpolated A) | RGBA PNG |
| 80 | BC4 | Greyscale | Gray PNG |
| 83 | BC5 | Normal map | RGB PNG |
| 98 | BC7 | High-quality | RGBA PNG |

---

# 🛠 SYSTEM MODULES

### 🔸 **dds2png**  
Single-file converter.

### 🔸 **batch_dds2png**  
Multithreaded Black Mesa / H.E.V–themed recursive DDS→PNG processor.

---

# ⚙ BUILD REQUIREMENTS

```
zlib
pthread
libm
gcc/clang
cmake OR make
```

---

# 🧱 BUILD (CMAKE)
```
mkdir build
cd build
cmake ..
make -j
```

# 🧱 BUILD (MAKEFILE)
```
make -j
```

---

# 🚀 USAGE

### Convert one:
```
./dds2png in.dds out.png
```

### Mass convert:
```
./batch_dds2png /path/to/folder
```

### Specify threads:
```
./batch_dds2png /path/to/folder 8
```

---

# 📜 LICENSE
MIT for all components.

---

# 🔊 H.E.V SYSTEM READY  
“**POWER LEVEL IS: 100%**”  
