#!/bin/bash

INPUT_ARG="${1:-}"
ASM_FILE="./examples/${INPUT_ARG}.asm"
OBJ_FILE="./build/${INPUT_ARG}.o"
BIN_FILE="./build/${INPUT_ARG}"

nasm -f elf64 "${ASM_FILE}" -o "${OBJ_FILE}"
gcc "${OBJ_FILE}" -o "${BIN_FILE}" -z noexecstack -no-pie
"${BIN_FILE}"
PROGRAM_EXIT=$?
echo "Program exited: ${PROGRAM_EXIT}"
set -e