#!/bin/bash

# Enhanced compile script for WSL + MSVC linker
# Adjust paths below if your installation differs or set the environment variables:
#   NASM_DIR, MSVC_HOST_64, WIN_UM_64, WIN_UCRT_64, MSVC_64

set -e

# Accept an input ASM filename as the first argument (default: hello_world.asm)
PROGRAM_NAME="${1}"
if [[ ! -f "${PROGRAM_NAME}.asm" ]]; then
    echo "Error: input file '${PROGRAM_NAME}' not found."
    exit 1
fi


# Allow overrides via environment
NASM_DIR=${NASM_DIR:-"/mnt/c/Users/thoma/AppData/Local/bin/NASM"}
MSVC_HOST_64=${MSVC_HOST_64:-"/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64"}
WIN_UM_64=${WIN_UM_64:-"/mnt/c/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64"}
WIN_UCRT_64=${WIN_UCRT_64:-"/mnt/c/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/ucrt/x64"}
MSVC_64=${MSVC_64:-"/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/lib/x64"}

echo "Using NASM at: ${NASM_DIR}/nasm.exe"
echo "Using link.exe at: ${MSVC_HOST_64}/link.exe"

if [[ ! -x "${NASM_DIR}/nasm.exe" ]]; then
    echo "Error: nasm.exe not found or not executable at ${NASM_DIR}/nasm.exe"
    echo "Set NASM_DIR to where nasm.exe is installed or install NASM for Windows."
    exit 1
fi

if [[ ! -x "${MSVC_HOST_64}/link.exe" ]]; then
    echo "Error: link.exe not found at ${MSVC_HOST_64}/link.exe"
    echo "Run this script from a Visual Studio Developer prompt or set MSVC_HOST_64 to the correct path."
    exit 1
fi

if [[ ! -f "${WIN_UM_64}/kernel32.lib" ]]; then
    echo "Error: kernel32.lib not found in ${WIN_UM_64}"
    echo "Install the Windows SDK (Desktop development with C++) or set WIN_UM_64 to the correct Windows Kits lib path."
    exit 1
fi

if [[ ! -f "${WIN_UCRT_64}/ucrt.lib" ]]; then
    echo "Error: ucrt.lib not found in ${WIN_UCRT_64}"
    echo "Install the Windows SDK or set WIN_UCRT_64 correctly."
    exit 1
fi

if [[ ! -f "${MSVC_64}/legacy_stdio_definitions.lib" ]]; then
    echo "Warning: legacy_stdio_definitions.lib not found in ${MSVC_64}. Continuing without it."
    LEGACY_STDIO_DEF=""
else
    LEGACY_STDIO_DEF="${MSVC_64}/legacy_stdio_definitions.lib"
fi

echo "Assembling ${PROGRAM_NAME}.asm with NASM..."
"${NASM_DIR}/nasm.exe" -f win64 ${PROGRAM_NAME}.asm -o ${PROGRAM_NAME}.obj
echo "NASM compiled ${PROGRAM_NAME}.obj"

echo "Linking with MSVC link.exe..."
# Convert WSL/Posix paths to Windows paths to avoid link.exe treating them as options
if command -v wslpath >/dev/null 2>&1; then
    MSVC_LINK_WIN=$(wslpath -w "${MSVC_HOST_64}/link.exe")
    WIN_UM_WIN=$(wslpath -w "${WIN_UM_64}")
    WIN_UCRT_WIN=$(wslpath -w "${WIN_UCRT_64}")
    MSVC_LIB_WIN=$(wslpath -w "${MSVC_64}")
    KERNEL32_WIN=$(wslpath -w "${WIN_UM_64}/kernel32.lib")
    UCRT_WIN=$(wslpath -w "${WIN_UCRT_64}/ucrt.lib")
    if [[ -n "${LEGACY_STDIO_DEF}" ]]; then
        LEGACY_STDIO_DEF_WIN=$(wslpath -w "${LEGACY_STDIO_DEF}")
    else
        LEGACY_STDIO_DEF_WIN=
    fi
    OBJ_WIN=$(wslpath -w "$(pwd)/${PROGRAM_NAME}.obj")

    # Build a temporary Windows .cmd file containing the full linker invocation
    OUT_EXE_WIN=$(wslpath -w "$(pwd)/${PROGRAM_NAME}.exe")
    LINK_CMD_FILE=$(mktemp /tmp/linkcmdXXXX.cmd)

    # Try to find vcvarsall.bat in common Visual Studio locations
    VCVARS_CANDIDATES=(
        "/mnt/c/Program Files (x86)/Microsoft Visual Studio/2019/Professional/VC/Auxiliary/Build/vcvarsall.bat"
        "/mnt/c/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Auxiliary/Build/vcvarsall.bat"
        "/mnt/c/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/VC/Auxiliary/Build/vcvarsall.bat"
        "/mnt/c/Program Files (x86)/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvarsall.bat"
        "/mnt/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvarsall.bat"
    )

    VCVARS_PATH=""
    for p in "${VCVARS_CANDIDATES[@]}"; do
        if [[ -f "${p}" ]]; then
            VCVARS_PATH="${p}"
            break
        fi
    done

    if [[ -n "${VCVARS_PATH}" ]]; then
        VCVARS_WIN=$(wslpath -w "${VCVARS_PATH}")
        echo "Found vcvarsall: ${VCVARS_PATH} -> ${VCVARS_WIN}"
    else
        VCVARS_WIN=""
        echo "vcvarsall.bat not found in common locations. The linker may not find Windows SDK libraries."
    fi

    # Construct command line safely
    CMDLINE="\"${MSVC_LINK_WIN}\" /SUBSYSTEM:console /ENTRY:main /OUT:\"${OUT_EXE_WIN}\" /LIBPATH:\"${WIN_UM_WIN}\" /LIBPATH:\"${WIN_UCRT_WIN}\" /LIBPATH:\"${MSVC_LIB_WIN}\" \"${KERNEL32_WIN}\" \"${UCRT_WIN}\""
    if [[ -n "${LEGACY_STDIO_DEF_WIN}" ]]; then
        CMDLINE="${CMDLINE} \"${LEGACY_STDIO_DEF_WIN}\""
    fi
    CMDLINE="${CMDLINE} \"${OBJ_WIN}\""

    echo "@echo off" > "${LINK_CMD_FILE}"
    if [[ -n "${VCVARS_WIN}" ]]; then
        # call vcvarsall to set up MSVC env for x64
        echo "call \"${VCVARS_WIN}\" x64" >> "${LINK_CMD_FILE}"
    fi
    echo "${CMDLINE}" >> "${LINK_CMD_FILE}"

    # Run the generated .cmd file with cmd.exe (use Windows path to file)
    CMD_FILE_WIN=$(wslpath -w "${LINK_CMD_FILE}")
    cmd.exe /C "${CMD_FILE_WIN}"

    # Remove temporary command file
    rm -f "${LINK_CMD_FILE}"
else
    # Fallback: try invoking link.exe directly (may misinterpret POSIX paths)
    "${MSVC_HOST_64}/link.exe" \
        /SUBSYSTEM:console \
        /OUT:${PROGRAM_NAME}.exe \
        /LIBPATH:"${WIN_UM_64}" \
        /LIBPATH:"${WIN_UCRT_64}" \
        /LIBPATH:"${MSVC_64}" \
        "${WIN_UM_64}/kernel32.lib" \
        "${WIN_UCRT_64}/ucrt.lib" \
        ${LEGACY_STDIO_DEF} \
        ${PROGRAM_NAME}.obj
fi

echo "Running generated exe..."
./${PROGRAM_NAME}.exe