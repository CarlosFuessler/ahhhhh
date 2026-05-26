# ahhhhh Syntax Reference

## Naming Conventions
- **snake_case** for variables and functions.
- **PascalCase** for custom types (Structs, Enums).
- **No semicolons**.

## Basic Syntax

### Variables
```ahhhh
var x = 1.0           // Inferred as f64
const y: string = "hi" // Explicit type
var z: bool = true
```

### Functions
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
    if x == 50 { br } // break
}
```

### Data Structures
```ahhhh
struct Point {
    x: f64,
    y: f64
}

var p = Point{ x: 10, y: 20 }
print(p.x)

var arr = [1, 2, 3]
print(arr[0])
print(arr.len)
```

## Built-in Functions

- **I/O**: `print(...)`, `log(...)`, `input(prompt)`
- **Math**: `sqrt(x)`, `abs(x)`, `pow(base, exp)`, `ln(x)`, `exp(x)`, `log10(x)`, `atan2(y, x)`, `sin(x)`, `cos(x)`, `tan(x)`, `floor(x)`, `ceil(x)`
- **System**: `clock()`, `exit(code)`
- **Arrays**: `append(arr, val)`, `pop(arr)`

## File Includes
Use `@(path)` to include other `.ahhhh` files.
```ahhhh
@(math_utils.ahhhh)
```
