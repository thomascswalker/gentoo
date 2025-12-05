# Gentoo Language
## Building from source

### Requirements

- `Win64`
- `nasm`
- `gcc`

Optional:

- `WSL`

### Architecture & Design

#### Stage 0

This is the bootstrap stage. A very simple compiler is written in C and compiles the 
minimal viable compiler into an executable so that a compiler can be written in the 
language itself.

## Examples

### Assignment

```rust
let my_var = 12;
const my_var2 = 16;
const my_var3 = 32;
```
