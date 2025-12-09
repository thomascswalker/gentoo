# Gentoo Language
## Building from source

### Requirements

- `gcc`

If on Windows:

- `WSL`

### Architecture & Design

#### Supported Architectures

- [ ] x86-32 (Linux)
- [ ] x86-32 (Windows)
- [ ] x86-32 (Mac)
- [x] x86-64 (Linux)
- [ ] x86-64 (Windows)
- [ ] x86-64 (Mac)

#### Stage 0

This is the bootstrap stage. A very simple compiler is written in C and constructs the 
minimal viable compiler into an executable so that a compiler can be written in the 
language itself.

Assembly output is in x86-64 (AT&T) format.

## Examples

### Assignment

```rust
let a = 3 * (7 - 5) * 12;
let b = a - 16;
return b;
```
