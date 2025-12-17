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

Assembly output is in x86-64 (Intel) format.

## Examples

### Assignment

```rust
// Returns 17
fn test_func() => {
    let local_a = 5;
    let local_b = 12;
    return local_a + local_b;
}

// Returns 80
fn test_func_2() => {
    let local_c = 40;
    return local_c * 2;
}

// Returns 97
fn main() =>
{
    let a = test_func();
    let b = test_func_2();
    let c = a + b;
    return c;
}
```
