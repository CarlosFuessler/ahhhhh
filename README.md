# ahhhhh Programming Language

A custom systems programming language with a bytecode VM, enforced naming conventions, and no semicolons.

## Features

- **Core Pipeline**: Lexer → Parser → Compiler → Bytecode VM.
- **Types**: Numbers (`f64`), Strings, Booleans, Arrays, and Structs.
- **Control Flow**: `if`/`else`, `while`, `for` range loops, and `switch` statements.
- **Functions**: Support for recursion, parameters, and return types.
- **Memory Management**: Built-in Mark-and-Sweep Garbage Collector.
- **Standard Library**: Math (`sqrt`, `pow`, `sin`, etc.), I/O (`print`, `input`), and Array utilities (`push`, `pop`).
- **Type Checking**: Basic compile-time type validation for expressions and assignments.

## Quick Start

### Build

```bash
zig build
```

### Run

```bash
# Run a source file:
./zig-out/bin/ahhhhh main.ahhhh

# Run inline code:
./zig-out/bin/ahhhhh 'print("hello world")'
```

## Syntax at a Glance

```ahhhh
fn factorial(n: f64) f64 {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}

fn main() {
    var x = factorial(5)
    print("Factorial of 5 is: ", x)
}

main()
```

See [syntax.md](syntax.md) for full syntax details.

## Recent Updates

- **Type Checking**: Implemented basic type validation in the compiler.
- **Standard Library**: Added more math functions and `exit()`.
- **Modulo Operator**: Added `%` support.
- **Cleanup**: Removed the old unused tree-walk interpreter.
