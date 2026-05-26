# Copilot Instructions

## Build, test, and lint commands

### First-time setup
```bash
# Configure CMake (required once)
cmake -S . -B build
```

### Building and running
```bash
# Build the interpreter
cmake --build build -- -j2

# Run a program file
./build/ahhhhh path/to/file.ahhhh

# Run inline source
./build/ahhhhh 'export fn main() { return }'

# Suppress verbose compiler/VM output
./build/ahhhhh file.ahhhh 2>/dev/null
```

### Testing

Tests are `.ahhhh` files in the repository root. Run them directly:

```bash
# Run a single test
./build/ahhhhh test.ahhhh

# Available test files
./build/ahhhhh test_while.ahhhh
./build/ahhhhh test_final.ahhhh
./build/ahhhhh test2.ahhhh
./build/ahhhhh test3.ahhhh
```

No automated test framework or CTest registration exists. Tests are manual.

### Debugging / Verbose output

The compiler and VM emit extensive debug output to stderr by default:
- `EMIT_CONSTANT`, `chunk_write_byte` - bytecode emission during compilation
- `dump_chunk` - full bytecode dump
- `VM iter` - VM execution trace (limited to iterations 100-120 by default)

Redirect stderr to suppress this: `./build/ahhhhh file.ahhhh 2>/dev/null`

There is no lint target/configuration in this repository today.

## High-level architecture

The execution pipeline is: **Lexer → Parser → Compiler → VM**.

- **Entry and orchestration (`src/main.c`)**: accepts one argument (`<code-or-path>`), reads it as a file if possible (otherwise treats it as inline source), lexes/parses into `AstProgram`, recursively executes included files, then compiles and runs the program via the VM.
- **Include mechanism**: annotation statements `@(path)` are parsed as `AST_STMT_ANNOTATION`. Before running a program, `main.c` recursively processes included files. Include resolution checks the given path, then `<input_dir>/<path>`, and retries with `.ahhhh` appended when needed.
- **Frontend pipeline (`src/lexer/*`, `src/parser/*`)**: lexer emits tokens (including `TOKEN_NEWLINE`), token stream provides bounded lookahead, and parser builds a full AST with precedence-based expression parsing (unary → multiplicative → additive → comparison → equality → assignment), plus postfix call/index/dot handling.
- **Bytecode compilation (`src/compiler/compiler.c`)**: compiles AST to bytecode (stored in `Chunk`), emitting constants and instructions. Outputs debug info to stderr during compilation (`EMIT_CONSTANT`, `chunk_write_byte`, `dump_chunk`).
- **VM execution (`src/vm/vm.c`)**: stack-based VM interprets bytecode with dynamic values (`null/number/bool/string/object/function`), statement control (`return`, `br`, `fw`), and native function calls. Outputs VM execution trace to stderr (iterations 100-120 only, by default).
- **Stdlib wiring (`src/stdlib/stdlib.c`, `stdlib/stdlib.ahhhh`)**: C runtime registers native functions (`__native_log`, `__native_print`, `__native_window_*` for Raylib graphics), and script stdlib maps those to language-level helpers (`log`, `print`, `window_create`, `draw_text`, etc.).
- **Program entry behavior**: after top-level execution, VM auto-invokes `export fn main()` if it exists. Any global code runs before `main()` is called.
- **Graphics support**: Raylib is linked as a dependency; stdlib exposes window creation, drawing, and rendering via native functions wired in `src/stdlib/stdlib.c`.
- **Interpreter module (`src/interpreter/interpreter.c`)**: currently a placeholder; the active execution path flows through lexer → parser → compiler → VM, not through the interpreter module.

## Key conventions specific to this codebase

- **Newline-oriented syntax**: parser explicitly skips/consumes `TOKEN_NEWLINE`; semicolon-separated style is not the norm (`syntax.md` also states `kein ;`).
- **Include-by-annotation convention**: shared modules are imported with `@(relative/or/absolute/path)` and commonly rely on extension auto-resolution to `.ahhhh`.
- **Symbol/color literals**: dot-prefixed values (for example `.red`) are parsed as identifier-like lvalues and become strings at runtime; `log` interprets supported symbols as ANSI colors.
- **Constructor naming convention**: identifiers starting with uppercase letters (or `[...]` forms) are treated as constructors and instantiate runtime objects; constructor brace calls support named fields (`Type{ field: value }`).
- **Types are mostly syntactic today**: `struct`, `enum`, and explicit type annotations are parsed into AST but are currently runtime no-ops.
- **Binary name drift in docs**: `README.md` still references `prog_language`, but the active CMake target/executable is `ahhhhh`.

## Standard library API

The `stdlib/stdlib.ahhhh` module exposes the following functions:

**I/O**:
- `print(value)` - Native print function
- `log(value, [color])` - Print with optional ANSI color (symbol literals like `.red`, `.blue`)
- `println(value)` - Convenience wrapper around `log`
- `log_pair(label, value)` - Print label-value pair
- `color(value, shade)` - Print colored value

**Graphics** (require `@(stdlib/stdlib.ahhhh)` include):
- `window_create(width, height, title)` - Create a Raylib window
- `window_should_close(window)` - Check if window close button was pressed
- `window_close(window)` - Close the window
- `begin_draw(window)` - Start drawing frame
- `end_draw(window)` - Finish drawing and render
- `clear_background(window, r, g, b, a)` - Clear with RGBA color
- `draw_text(window, text, x, y, size, r, g, b, a)` - Draw text at position with RGBA color

**Math utilities**:
- `add(a, b)`, `sub(a, b)`, `mul(a, b)`, `div(a, b)` - Arithmetic helpers
- `abs(value)` - Absolute value
- `min(a, b)`, `max(a, b)` - Min/max of two values
- `clamp(value, min_value, max_value)` - Clamp value to range
- `between(value, min_value, max_value)` - Check if value is in range

**Iteration helpers**:
- `sum_range(start, end)` - Sum integers from start to end (inclusive)
- `repeat(text, times)` - Repeat string N times
