source ./run.sh -d

valgrind  --leak-check=full -s ./build/compiler.exe ./examples/file.c8