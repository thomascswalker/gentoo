source ./build.sh

valgrind  --leak-check=full ./build/compiler.exe ./examples/file.c8