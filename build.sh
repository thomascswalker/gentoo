#!/bin/bash
mkdir build
gcc ./src/main.c -o ./build/compiler.exe
./build/compiler.exe ./examples/file.c8