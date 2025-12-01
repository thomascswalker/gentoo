#!/bin/bash

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
BUILD_DIRECTORY="./build"
if [ ! -d "${BUILD_DIRECTORY}" ]; then
  mkdir "${BUILD_DIRECTORY}"
fi

# Compile with GCC
COMPILER_BIN="./build/compiler.exe"
gcc ${CFLAGS[@]} ./src/*.c -o "${COMPILER_BIN}"

# Determine input file (positional argument). If none given, use the example.
INPUT_FILE="${1:-}"
if [ -z "${INPUT_FILE}" ]; then
    INPUT_FILE="./examples/program.t"
else
    # If the provided path exists use it, otherwise try under ./examples
    if [ -f "${INPUT_FILE}" ]; then
        : # use as-is
    elif [ -f "./examples/${INPUT_FILE}" ]; then
        INPUT_FILE="./examples/${INPUT_FILE}"
    else
        echo "Input file '${INPUT_FILE}' not found." >&2
        exit 2
    fi
fi

# Run the compiler with the chosen input file
"${COMPILER_BIN}" "${INPUT_FILE}"