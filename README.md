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