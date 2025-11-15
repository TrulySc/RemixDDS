cmake_minimum_required(VERSION 3.10)
project(dds_converter C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Sources
set(CONVERTER_SRC
    dds_bc_all_to_png.c
    bc7_decoder.cpp
    bc7decomp.cpp
)

# -----------------------------
# Standalone dds2png tool
# -----------------------------
add_executable(dds2png
    ${CONVERTER_SRC}
)

target_compile_definitions(dds2png PRIVATE STANDALONE)
target_link_libraries(dds2png m z)

# -----------------------------
# Multithreaded batch converter
# -----------------------------
add_executable(batch_dds2png
    batch_dds2png.cpp
    ${CONVERTER_SRC}
)

target_link_libraries(batch_dds2png pthread m z)

# -----------------------------
# Install Targets (optional)
# -----------------------------
install(TARGETS dds2png batch_dds2png DESTINATION bin)
