#!/bin/bash

# Immediately fail if any common problems occur
set -euo pipefail

CALLING_DIR="$(pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"
shopt -s nullglob

EXT="g2"
DEFAULT_EXAMPLE="program"

SRC_STAGE0_DIR="./src/stage0"
ARCH_DIR="${SRC_STAGE0_DIR}/arch"
EXAMPLES_DIR="./examples"
BUILD_DIRECTORY="./build"
COMPILER_BIN="${BUILD_DIRECTORY}/compiler.exe"
ASSEMBLY_BASENAME="program"
ASM_OUTPUT="${BUILD_DIRECTORY}/${ASSEMBLY_BASENAME}.asm"
OBJ_OUTPUT="${BUILD_DIRECTORY}/${ASSEMBLY_BASENAME}.o"
BINARY_OUTPUT="${BUILD_DIRECTORY}/${ASSEMBLY_BASENAME}"

# Arguments:
# -d | --debug: Enables GCC debug mode and defines the '_DEBUG' macro.
CFLAGS=()
while (( "$#" )); do
    case "$1" in
        -d|--debug)
            CFLAGS+=(-g -D_DEBUG)
            shift
            ;;
        --) shift; break ;;
        *) break ;;
    esac
done

# If the build directory does not exist, make it.
if [ ! -d "${BUILD_DIRECTORY}" ]; then
    echo "Creating build directory at ${BUILD_DIRECTORY}"
    mkdir -p "${BUILD_DIRECTORY}"
fi

# Compile Stage 0 sources with GCC
SOURCE_FILES=("${SRC_STAGE0_DIR}/"*.c "${ARCH_DIR}/"*.c)
if [ ${#SOURCE_FILES[@]} -eq 0 ]; then
    echo "No stage0 source files found under ${SRC_STAGE0_DIR}." >&2
    exit 1
fi

INCLUDE_PATHS=("-I${SRC_STAGE0_DIR}" "-I${ARCH_DIR}")

echo "Compiling Stage 0 compiler with gcc..."
GCC_COMMAND=(gcc)
if [ ${#CFLAGS[@]} -gt 0 ]; then
    GCC_COMMAND+=("${CFLAGS[@]}")
fi
GCC_COMMAND+=("${INCLUDE_PATHS[@]}")
GCC_COMMAND+=("${SOURCE_FILES[@]}" -o "${COMPILER_BIN}")
"${GCC_COMMAND[@]}"

# Determine input file (positional argument). If none given, use the example.
INPUT_ARG="${1:-}"
if [ -z "${INPUT_ARG}" ]; then
    INPUT_FILE="${EXAMPLES_DIR}/${DEFAULT_EXAMPLE}.${EXT}"
else
    if [ -f "${INPUT_ARG}" ]; then
        INPUT_FILE="${INPUT_ARG}"
    elif [[ "${INPUT_ARG}" != /* && -f "${CALLING_DIR}/${INPUT_ARG}" ]]; then
        INPUT_FILE="${CALLING_DIR}/${INPUT_ARG}"
    elif [ -f "${EXAMPLES_DIR}/${INPUT_ARG}" ]; then
        INPUT_FILE="${EXAMPLES_DIR}/${INPUT_ARG}"
    elif [ -f "${EXAMPLES_DIR}/${INPUT_ARG}.${EXT}" ]; then
        INPUT_FILE="${EXAMPLES_DIR}/${INPUT_ARG}.${EXT}"
    else
        echo "Input file '${INPUT_ARG}' not found." >&2
        exit 2
    fi
fi
INPUT_LABEL="$(basename "${INPUT_FILE}")"

# Run the compiler with the chosen input file
echo "Running Gentoo compiler..."
"${COMPILER_BIN}" "${INPUT_FILE}" # x86_64

if [ ! -f "${ASM_OUTPUT}" ]; then
    echo "Expected assembly output '${ASM_OUTPUT}' not found." >&2
    exit 3
fi

echo "Assembling Gentoo output..."
nasm -f elf64 "${ASM_OUTPUT}" -o "${OBJ_OUTPUT}"
gcc "${OBJ_OUTPUT}" -o "${BINARY_OUTPUT}" -z noexecstack

echo "Executing program generated from '${INPUT_LABEL}':"
set +e
"${BINARY_OUTPUT}"
PROGRAM_EXIT=$?
set -e
echo "" # New line
echo "Program exited with code: ${PROGRAM_EXIT}"
exit "${PROGRAM_EXIT}"