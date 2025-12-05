NASM_DIR=${NASM_DIR:-"/mnt/c/Users/thoma/AppData/Local/bin/NASM"}

"${NASM_DIR}/nasm.exe" -f win64 ./build/program.asm -o ./build/program.obj
ld -o ./build/program.exe ./build/program.obj
./build/program.exe