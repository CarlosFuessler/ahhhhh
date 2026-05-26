# The ahhhhh Programming Language

A systems-inspired language featuring a custom stack-based bytecode virtual machine, strict naming conventions, and an automated garbage collector.

---

## How the Language Works

The ahhhhh compiler and runtime pipeline is designed for high efficiency. Source code is not interpreted directly; instead, it is compiled to stack-based bytecode and executed on a custom virtual machine.

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

## Key Features

| Feature | Description | Syntax Example |
| :--- | :--- | :--- |
| **Naming Conventions** | Strict snake_case for variables/functions, PascalCase for Structs. | `var item_count = 5.0` |
| **Functions** | Supports recursion, typed parameters, and explicit return types. | `fn double(x: f64) f64 { return x * 2 }` |
| **Control Flow** | Supports if-else branches, while loops, and for range iterations. | `for i in 0..5 { print(i) }` |
| **Data Structures** | Dynamic arrays and user-defined struct types. | `struct Point { x: f64 }` |

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
./zig-out/bin/ahhhhh main.ahhhh

# Execute inline code
./zig-out/bin/ahhhhh 'print("Hello, World!")'
```

---

## Example: Pong Game
The language integrates with graphics libraries through window and Raylib bindings:

```ahhhh
fn main() {
    var win = window_create(800, 600, "ahhhhh Pong")
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

---

## Standard Library Reference

* **Input & Output:** `print(...)`, `log(...)`, `input(prompt)`
* **Mathematics:** `sqrt(x)`, `abs(x)`, `pow(base, exp)`, `sin(x)`, `cos(x)`, `tan(x)`, `floor(x)`, `ceil(x)`
* **System & Time:** `clock()`, `exit(code)`
* **Array Manipulation:** `append(arr, val)`, `pop(arr)`
