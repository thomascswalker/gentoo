#!/bin/bash

# Enhanced compile script for WSL + MSVC linker
# Adjust paths below if your installation differs or set the environment variables:
#   NASM_DIR, MSVC_HOST_64, WIN_UM_64, WIN_UCRT_64, MSVC_64

set -e

BUILD_DIR="./build"

# Accept an input ASM filename as the first argument
PROGRAM_NAME="${BUILD_DIR}/${1}"
if [[ ! -f "${PROGRAM_NAME}.asm" ]]; then
    echo "Error: input file '${PROGRAM_NAME}' not found."
    exit 1
fi

echo "Assembling ${PROGRAM_NAME}.asm with NASM..."
nasm.exe -f win64 ${PROGRAM_NAME}.asm -o ${PROGRAM_NAME}.obj
echo "NASM compiled ${PROGRAM_NAME}.obj"

echo "Linking with ld..."

ld -o ${PROGRAM_NAME}.exe ${PROGRAM_NAME}.obj

echo "Running generated exe..."
./${PROGRAM_NAME}.exe