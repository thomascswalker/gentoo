#!/bin/bash

EXT="g2"

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
    echo "Building build dir"
  mkdir "${BUILD_DIRECTORY}"
fi

# Compile with GCC
COMPILER_BIN="./build/compiler.exe"
echo "Compiling with gcc..."
gcc ${CFLAGS[@]} ./src/*.c -o "${COMPILER_BIN}"

# Determine input file (positional argument). If none given, use the example.
INPUT_NAME="${1:-}"
INPUT_FILE=""
if [ -z "${INPUT_NAME}" ]; then
    INPUT_NAME="program"
    INPUT_FILE="./examples/${INPUT_NAME}.${EXT}"
else
    # If the provided path exists use it, otherwise try under ./examples
    if [ -f "${INPUT_NAME}" ]; then
        : # use as-is
    elif [ -f "./examples/${INPUT_NAME}" ]; then
        INPUT_FILE="./examples/${INPUT_NAME}"
    else
        echo "Input file '${INPUT_NAME}' not found." >&2
        exit 2
    fi
fi

# Run the compiler with the chosen input file
echo "Running Gentoo compiler..."
"${COMPILER_BIN}" "${INPUT_FILE}"
if [ ! $? -eq 0 ] ; then
    exit
fi

echo "Assembling Gentoo output..."
nasm -f elf64 ./build/${INPUT_NAME}.asm -o ./build/${INPUT_NAME}.o
gcc ./build/${INPUT_NAME}.o -o ./build/${INPUT_NAME} -z noexecstack

echo "Executing program '${INPUT_NAME}':"
./build/program
echo $?