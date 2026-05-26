<div align="center">
  <img src="https://raw.githubusercontent.com/mlugg/setup-zig/main/logo.png" alt="ahhhhh logo" width="100" height="100" style="border-radius: 20px; box-shadow: 0 4px 20px rgba(0,0,0,0.15);" />

  <h1>⚡ The <code>ahhhhh</code> Programming Language</h1>
  <p><i>A high-performance systems-inspired language featuring a custom Bytecode VM, strict naming, and an automated Garbage Collector.</i></p>

  <p>
    <a href="https://github.com/CarlosFuessler/ahhhhh/actions"><img src="https://img.shields.io/github/actions/workflow/status/CarlosFuessler/ahhhhh/build.yml?branch=main&style=flat-square&logo=github-actions&logoColor=white" alt="Build Status"></a>
    <img src="https://img.shields.io/badge/compiler-Zig%200.13.0-orange?style=flat-square&logo=zig" alt="Built with Zig">
    <img src="https://img.shields.io/badge/vm-Stack%20Based-blue?style=flat-square" alt="Stack-Based VM">
    <img src="https://img.shields.io/badge/gc-Mark--and--Sweep-green?style=flat-square" alt="Mark-and-Sweep GC">
  </p>
</div>

---

## 📖 How the Language Works

The `ahhhhh` language runs on a modern stack-based pipeline. Rather than interpreting source code directly (which is slow), it compiles code down to a highly optimized binary **bytecode format** which is executed by an efficient, custom virtual machine written in C.

Below is an interactive breakdown of the core compilation and execution pipeline:

<details>
<summary><b>🔍 Phase 1: Lexical Analysis (The Lexer)</b></summary>
<br>
<blockquote>
  <b>Goal:</b> Convert raw source code characters into a stream of structured, identifiable tokens.
</blockquote>

The Lexer scans the source file character by character, discarding whitespace and comments while matching patterns to generate tokens (e.g., keywords like `fn`, symbols like `{`, and literals like `"hello"`). 
* It automatically handles the **no-semicolon** rule by identifying statement endings using newlines and curly braces, reducing visual clutter in your code.
</details>

<details>
<summary><b>🌳 Phase 2: Syntax Analysis (The Parser)</b></summary>
<br>
<blockquote>
  <b>Goal:</b> Group tokens into a hierarchical Abstract Syntax Tree (AST) representing the program's structure.
</blockquote>

The Parser utilizes recursive descent to enforce the grammatical rules of `ahhhhh`. It validates control flows, checks naming conventions at parsing time, and constructs the tree of operations.
* <b>Convention Enforcer:</b> It directly rejects variables or functions that do not use `snake_case`, and custom types that do not use `PascalCase`.
</details>

<details>
<summary><b>🛡️ Phase 3: Semantic Analysis & Type Checking</b></summary>
<br>
<blockquote>
  <b>Goal:</b> Validate types, resolve variables, and ensure complete safety before running code.
</blockquote>

Before generating code, the compiler performs static analysis to check that operations are type-safe. 
* It ensures you don't accidentally add a string to a number.
* It resolves variable scopes, verifying that all referenced variables and functions are declared and accessible in the current scope.
</details>

<details>
<summary><b>⚙️ Phase 4: Compilation to Stack Bytecode</b></summary>
<br>
<blockquote>
  <b>Goal:</b> Translate the validated AST into high-performance stack-based bytecode instructions.
</blockquote>

The compiler processes the AST nodes and emits sequential bytecode instructions (such as `OP_ADD`, `OP_PUSH_CONSTANT`, `OP_CALL`). This compilation step optimizes operations, compiles complex conditionals into jumps, and prepares the bytecode chunk for execution.
</details>

<details>
<summary><b>🚀 Phase 5: Execution (The VM & GC)</b></summary>
<br>
<blockquote>
  <b>Goal:</b> Execute bytecode at maximum speed while dynamically managing memory.
</blockquote>

The Virtual Machine (VM) executes the bytecode stream using a fast instruction loop. 
* <b>Garbage Collection:</b> Features a built-in, automated <b>Mark-and-Sweep Garbage Collector</b>. When memory threshold limits are met, the VM pauses briefly to mark all active heap objects (like strings, arrays, and structs) and sweeps away unreferenced ones to prevent memory leaks.
</details>

---

## 🛠️ Language Features & Syntax

<table width="100%">
  <thead>
    <tr>
      <th width="30%">Feature</th>
      <th width="40%">Description</th>
      <th width="30%">Syntax Example</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td><b>Naming Conventions</b></td>
      <td>Strict snake_case for variables/functions, PascalCase for custom types. No semicolons needed.</td>
      <td><pre lang="ahhhh">var my_number = 10.0</pre></td>
    </tr>
    <tr>
      <td><b>Functions</b></td>
      <td>Support parameters, return types, recursion, and nested declarations.</td>
      <td><pre lang="ahhhh">fn square(x: f64) f64 {
  return x * x
}</pre></td>
    </tr>
    <tr>
      <td><b>Control Flow</b></td>
      <td>Robust <code>if-else</code> blocks, <code>while</code> loops, <code>for</code> range loops, and <code>br</code> (break) statements.</td>
      <td><pre lang="ahhhh">for i in 0..5 {
  print(i)
}</pre></td>
    </tr>
    <tr>
      <td><b>Data Structures</b></td>
      <td>Flexible arrays with dynamic methods and custom user-defined <code>struct</code> types.</td>
      <td><pre lang="ahhhh">struct Point {
  x: f64,
  y: f64
}
var p = Point{ x: 1, y: 2 }</pre></td>
    </tr>
  </tbody>
</table>

---

## 🚀 Quick Start

### 1. Build from Source
Ensure you have **Zig 0.13.0** installed, then build the binary:
```bash
zig build
```

### 2. Execute Code
You can run a `.ahhhh` script directly or execute code inline using the CLI:
```bash
# Run a script file
./zig-out/bin/ahhhhh main.ahhhh

# Run inline code
./zig-out/bin/ahhhhh 'print("Hello from ahhhhh!")'
```

---

## 🎮 Graphic Example: Pong
Below is an example showing how `ahhhhh` integrates with graphics frameworks using built-in window and Raylib bindings:

```ahhhh
fn main() {
    var win = window_create(800, 600, "ahhhhh Pong")
    var ball_x = 400.0
    var ball_y = 300.0
    var ball_dx = 5.0
    var ball_dy = 5.0

    while !window_should_close(win) {
        // Ball Physics
        ball_x = ball_x + ball_dx
        ball_y = ball_y + ball_dy
        
        // Wall Bounce
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

## 🧰 Built-in Standard Library
The standard library is integrated directly into the runtime for maximum efficiency:

* 📥 **I/O Operations:** `print(...)`, `log(...)`, `input(prompt)`
* 📐 **Math Suite:** `sqrt(x)`, `abs(x)`, `pow(base, exp)`, `sin(x)`, `cos(x)`, `tan(x)`, `floor(x)`, `ceil(x)`
* ⚙️ **System & Runtime:** `clock()`, `exit(code)`
* 📦 **Array Utilities:** `append(arr, val)`, `pop(arr)`
