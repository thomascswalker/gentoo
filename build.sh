#!/bin/bash

BUILD_DIRECTORY="./build"

if [ ! -d "$BUILD_DIRECTORY" ]; then
  mkdir "$BUILD_DIRECTORY"
fi

gcc ./src/main.c -o ./build/compiler.exe

