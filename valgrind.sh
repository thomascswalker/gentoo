source ./run.sh -d

valgrind  --leak-check=full --show-leak-kinds=all -s ./build/compiler.exe ./examples/file.c8