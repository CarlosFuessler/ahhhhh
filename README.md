# The ahhhh Programming Language

A systems-inspired language featuring a custom stack-based bytecode virtual machine, static type checking, and an automated garbage collector.

---

## Quick Start

### 1. Build the Runtime
To compile the compiler and VM, ensure you have Zig 0.13.0 installed, then run:
```bash
zig build
```

### 2. Run Programs
Execute `.ahhhh` scripts or run inline code directly:
```bash
# Execute a script file
./zig-out/bin/ahhhh main.ahhhh

# Execute inline code directly
./zig-out/bin/ahhhh 'print("Hello, World!")'
```

---

## Syntax Overview

### Variables & Constants
```go
var x = 1.0            // inferred as f64
const y: string = "hi" // explicit type
var active: bool = true
```

### Functions
```go
fn add(a: f64, b: f64) f64 {
    return a + b
}

fn greet(name: string) {
    print("Hello, ", name)
}
```

### Control Flow
```go
if x > 0 {
    print("positive")
} else {
    print("non-positive")
}

// Range loops
for i in 0..10 {
    print(i)
}

// While loops with break
while x < 100 {
    x = x * 2
    if x == 50 { br }
}
```

### Structs & Arrays
```go
// custom structures
struct Point {
    x: f64,
    y: f64
}
var p = Point{ x: 10, y: 20 }
print(p.x)

// arrays
var arr = [1, 2, 3]
print(arr[0])
print(arr.len) // array length
```

### File Includes
```go
@(math_utils.ahhhh) // imports another script
```

---

## Under the Hood

The `ahhhh` compilation and execution pipeline consists of:
1. Lexer & Parser:** Converts source text to a stream of tokens, and constructs an Abstract Syntax Tree (AST).
2. Type Checker: Enforces static semantic and type analysis (snake_case for variables/functions, PascalCase for structs).
3. Bytecode Compiler: Generates linear bytecode instructions (e.g., `OP_ADD`, `OP_CALL`) and patches jump offsets.
4. Stack VM: Executes instructions on a custom runtime stack, backed by an integrated Mark-and-Sweep Garbage Collector that automatically reclaims heap-allocated resources (strings, arrays, structs).
