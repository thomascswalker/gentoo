nasm -f elf64 ./examples/printf.asm -o ./build/printf.o
gcc ./build/printf.o -o ./build/printf -z noexecstack  -no-pie
./build/printf
printf "\n%d\n" "$?"