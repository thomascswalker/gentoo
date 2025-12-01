#!/usr/bin/env bash

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
gcc ${CFLAGS[@]} ./src/main.c -o "${COMPILER_BIN}"

# Run the the compiler.
"${COMPILER_BIN}" ./examples/file.c8