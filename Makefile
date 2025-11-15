CC      = gcc
CXX     = g++
CFLAGS  = -O2 -std=c11
CXXFLAGS= -O2 -std=c++17
LDFLAGS = -lm -lz
THREADS = -lpthread

SRC_COMMON = dds_bc_all_to_png.c bc7_decoder.cpp bc7decomp.cpp

# -----------------------------
# Standalone dds2png
# -----------------------------
dds2png: $(SRC_COMMON)
	$(CXX) $(CXXFLAGS) -DSTANDALONE $(SRC_COMMON) -o dds2png $(LDFLAGS)

# -----------------------------
# Multithreaded HEV batch tool
# -----------------------------
batch_dds2png: batch_dds2png.cpp $(SRC_COMMON)
	$(CXX) $(CXXFLAGS) batch_dds2png.cpp $(SRC_COMMON) -o batch_dds2png $(LDFLAGS) $(THREADS)

# -----------------------------
# Convenience targets
# -----------------------------
all: dds2png batch_dds2png

clean:
	rm -f dds2png batch_dds2png *.o

.PHONY: all clean
