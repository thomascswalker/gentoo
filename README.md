# T Language

## Grammar

```ebnf
 program = { assignment, ";", white space };
 identifier = alphabetic character, { alphabetic character | digit } ;
 number = digit, { digit } ;
 assignment = identifier , "=" , ( number | identifier ) ;
 alphabetic character = "A" | "B" | "C" | "D" | "E" | "F" | "G"
                      | "H" | "I" | "J" | "K" | "L" | "M" | "N"
                      | "O" | "P" | "Q" | "R" | "S" | "T" | "U"
                      | "V" | "W" | "X" | "Y" | "Z" | "a" | "b" 
                      | "c" | "d" | "e" | "f" | "g" | "h" | "i" 
                      | "j" | "k" | "l" | "m" | "n" | "o" | "p" 
                      | "q" | "r" | "s" | "t" | "u" | "v" | "w" 
                      | "x" | "y" | "z" ;
 digit = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
 white space = ? white space characters ? ;
 all characters = ? all visible characters ? ;
```

## Examples

### Assignment

```rust
let my_var = 12;
const my_var2 = 16;
const my_var3 = 32;
```

## Building with LLVM

This project includes a minimal LLVM backend under `src/codegen_llvm.*`. To build with LLVM support you need LLVM development libraries and headers available on your system.

Recommended ways to obtain LLVM:
- On Linux/macOS: install `llvm`/`clang` via your package manager, and use `llvm-config` to provide compiler/linker flags.
- On Windows: install the official LLVM distribution (https://llvm.org) or use `vcpkg` to install `llvm` and point CMake at `LLVM_DIR`.

Build with CMake (Unix-like systems with llvm-config):

```
mkdir build && cd build
cmake ..
cmake --build .
```

On Windows with a provided `LLVM_DIR` (e.g. from an LLVM install or vcpkg):

```
build_windows.bat C:\\path\\to\\llvm\\lib\\cmake\\llvm
```

If `llvm-config` is present the CMake script will use it to set compile and link flags automatically. Otherwise CMake will try to find the LLVM CMake config under `LLVM_DIR`.

After building you can run the compiler on an example file:

```
./tlang examples/program.t
```
