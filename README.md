# The ahhhh Programming Language

A systems-inspired language featuring a custom stack-based bytecode virtual machine, strict naming conventions, and an automated garbage collector.

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│  main.ahhhh ────────────────────────────────────────────────────────────── [ahhhh VM]  │
├───────────────────────────────────────────────────────┬────────────────────────────────┤
│  fn factorial(n: f64) f64 {                           │  VM EXECUTION STATE            │
│      if n <= 1 { return 1 }                           ├────────────────────────────────┤
│      return n * factorial(n - 1)                      │  IP: 0x00A8  [OP_CALL]         │
│  }                                                    │  SP: 0x0004  [STACK SIZE: 3]   │
│                                                       ├────────────────────────────────┤
│  fn main() {                                          │  VM STACK                      │
│      var res = factorial(5)                           │  ┌──────────────────────────┐  │
│      print("factorial(5) = ", res, .cyan)             │  │  0x0003: [VALUE_NUMBER]5 │  │
│  }                                                    │  │  0x0002: [VALUE_NUMBER]4 │  │
│                                                       │  │  0x0001: [VALUE_NUMBER]3 │  │
│                                                       │  └──────────────────────────┘  │
├───────────────────────────────────────────────────────┴────────────────────────────────┤
│                                                                                        │
│                  ____ _        __      __      __      __      __                      │
│                 / __ `/       / /_    / /_    / /_    / /_    / /_                     │
│                / /_/ /       / __ \  / __ \  / __ \  / __ \  / __ \                    │
│                \__,_/       /_/ /_/ /_/ /_/ /_/ /_/ /_/ /_/ /_/ /_/                    │
│                                                                                        │
│                  -- THE SYSTEMS-INSPIRED VM PROGRAMMING LANGUAGE --                    │
│                                                                                        │
├────────────────────────────────────────────────────────────────────────────────────────┤
│  Compiler Terminal                                                                     │
├────────────────────────────────────────────────────────────────────────────────────────┤
│  $ zig build && ./zig-out/bin/ahhhh main.ahhhh                                         │
│  [lexer]   Successfully tokenized 'main.ahhhh' into 28 tokens.                         │
│  [parser]  Generated AST with 2 functions and 1 include.                               │
│  [codegen] Generated 14 VM opcodes (constant table size: 4).                           │
│  [vm]      Starting stack-based bytecode interpreter...                                │
│  factorial(5) = 120                                                                    │
│  [vm]      Execution finished in 0.18ms. GC reclaimed 0 bytes (0 objects allocated).   │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## How the Language Works

The ahhhh compiler and runtime pipeline is designed for high efficiency. Source code is not interpreted directly; instead, it is compiled to stack-based bytecode and executed on a custom virtual machine.

### 1. Lexical Analysis (Lexer)
The lexer converts raw source code characters into a stream of tokens. It identifies keywords, identifier names, operators, and literals. Semicolons are optional, as the lexer dynamically resolves statement endings using newlines and braces.

### 2. Syntax Analysis (Parser)
The parser groups the token stream into an Abstract Syntax Tree (AST) representing the program's structural logic. It enforces syntax grammar rules and naming conventions (snake_case for variables and functions, PascalCase for custom types).

### 3. Semantic Analysis & Type Checking
Before bytecode generation, the compiler performs static analysis to ensure type safety. It checks that types match in assignments, mathematical operations, and function signatures, while verifying variable scopes.

### 4. Code Generation (Bytecode Compiler)
The compiler translates the AST into a linear stream of compact bytecode instructions (e.g., OP_ADD, OP_CALL). Conditional logic and loops are resolved into efficient byte jumps.

### 5. Execution (Stack-Based VM)
The stack-based Virtual Machine (VM) runs the bytecode instructions. It includes an integrated Mark-and-Sweep Garbage Collector that automatically reclaims memory allocated for dynamic objects like strings, arrays, and structs.

---

## Language Specifications and Syntax

### Naming Conventions
* **snake_case** for variables and functions.
* **PascalCase** for custom types (Structs, Enums).
* **No semicolons** are required to end statements.

### Variables and Constants
Variables are declared using the `var` keyword and their types can be inferred or explicitly specified. Constants are defined using the `const` keyword.
```ahhhh
var x = 1.0           // Inferred as f64
const y: string = "hi" // Explicit type
var z: bool = true
```

### Functions
Functions are declared using `fn` and can specify typed parameters and return values.
```ahhhh
fn add(a: f64, b: f64) f64 {
    return a + b
}

fn greet(name: string) {
    print("Hello, ", name)
}
```

### Control Flow
```ahhhh
if x > 0 {
    print("positive")
} else {
    print("non-positive")
}

for i in 0..10 {
    print(i)
}

while x < 100 {
    x = x * 2
    if x == 50 { br } // break out of loop
}
```

### Data Structures

#### Structs
```ahhhh
struct Point {
    x: f64,
    y: f64
}

var p = Point{ x: 10, y: 20 }
print(p.x)
```

#### Arrays
```ahhhh
var arr = [1, 2, 3]
print(arr[0])
print(arr.len) // Get array length
```

### File Includes
Use `@(path)` to import and run other `.ahhhh` source files directly inside your script.
```ahhhh
@(math_utils.ahhhh)
```

---

## Standard Library Reference

* **Input & Output:** `print(...)`, `log(...)`, `input(prompt)`
* **Mathematics:** `sqrt(x)`, `abs(x)`, `pow(base, exp)`, `ln(x)`, `exp(x)`, `log10(x)`, `atan2(y, x)`, `sin(x)`, `cos(x)`, `tan(x)`, `floor(x)`, `ceil(x)`
* **System & Time:** `clock()`, `exit(code)`
* **Array Manipulation:** `append(arr, val)`, `pop(arr)`

---

## Quick Start

### Build the Runtime
To compile the compiler and VM, ensure you have Zig 0.13.0 installed, then run:
```bash
zig build
```

### Run Programs
You can execute `.ahhhh` scripts or run code inline directly via the CLI:
```bash
# Execute a script file
./zig-out/bin/ahhhh main.ahhhh

# Execute inline code
./zig-out/bin/ahhhh 'print("Hello, World!")'
```

---

## Example: Pong Game
The language integrates with graphics libraries through window and Raylib bindings:

```ahhhh
fn main() {
    var win = window_create(800, 600, "ahhhh Pong")
    var ball_x = 400.0
    var ball_y = 300.0
    var ball_dx = 5.0
    var ball_dy = 5.0

    while !window_should_close(win) {
        ball_x = ball_x + ball_dx
        ball_y = ball_y + ball_dy
        
        if ball_y < 10 { ball_dy = abs(ball_dy) }
        if ball_y > 590 { ball_dy = -abs(ball_dy) }
        
        begin_draw(win)
        clear_background(win, 20, 20, 20, 255)
        draw_circle(win, ball_x, ball_y, 10, 255, 255, 255, 255)
        end_draw(win)
    }
}

main()
```
