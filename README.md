# Kaleidoscope Language

A simple programming language implementation based on LLVM, following the [LLVM Kaleidoscope Tutorial](https://llvm.org/docs/tutorial/).

## Overview

Kaleidoscope is a simple, functional programming language that supports:
- Function definitions and calls
- Arithmetic operations (+, -, *, /)
- Comparison operations (<)
- Variable references
- External function declarations

## Project Structure

```
kaledioscope/
├── src/
│   ├── main.cpp          # Main driver and REPL
│   ├── lexer.h/.cpp      # Lexical analysis (tokenization)
│   ├── parser.h/.cpp     # Syntax analysis (parsing)
│   ├── ast.h/.cpp        # Abstract Syntax Tree classes
│   └── codegen.h/.cpp    # LLVM code generation
├── CMakeLists.txt        # Build configuration
├── build/                # Build artifacts (generated)
└── README.md            # This file
```

## Building

### Prerequisites

- CMake 3.20 or later
- LLVM 14+ with development headers
- C++17 compatible compiler (MSVC, GCC, Clang)

### Build Steps

1. Clone or download the project
2. Create a build directory:
   ```bash
   mkdir build
   cd build
   ```

3. Configure with CMake:
   ```bash
   cmake ..
   ```

4. Build the project:
   ```bash
   cmake --build . --config Release
   ```

## Usage

Run the interpreter:
```bash
./build/Release/kaledio_lang.exe
```

The interpreter provides a REPL (Read-Eval-Print Loop) where you can enter Kaleidoscope code:

```
ready> def foo(x) x + 1;
ready> foo(5);
ready> extern sin(x);
ready> sin(1.0);
```

### Language Examples

#### Function Definition
```
def fib(x)
  if x < 3 then
    1
  else
    fib(x-1) + fib(x-2);
```

#### Function Call
```
fib(10);
```

#### External Function Declaration
```
extern sin(x);
sin(3.14159);
```

## Architecture

The implementation is modular and follows compiler construction principles:

1. **Lexer** (`lexer.h/.cpp`): Tokenizes input source code into tokens
2. **Parser** (`parser.h/.cpp`): Parses tokens into an Abstract Syntax Tree (AST)
3. **AST** (`ast.h/.cpp`): Defines AST node classes and their code generation methods
4. **Code Generator** (`codegen.h/.cpp`): Translates AST to LLVM IR
5. **Main** (`main.cpp`): Provides the REPL interface and top-level parsing

## Dependencies

- **LLVM**: Core compiler infrastructure library
  - IR generation and optimization
  - Target-specific code generation
  - JIT compilation support

## Development

### Adding New Features

1. **New AST Nodes**: Add classes to `ast.h/.cpp` with `codegen()` methods
2. **New Syntax**: Extend parser in `parser.h/.cpp`
3. **New Tokens**: Add to token enum in `lexer.h/.cpp`
4. **New Operations**: Implement in AST codegen methods

### Testing

The REPL allows interactive testing. For automated testing, you can:
- Create test input files
- Pipe input to the executable
- Verify LLVM IR output

## License

This implementation is based on the LLVM Kaleidoscope tutorial and follows the same educational purpose.

## References

- [LLVM Kaleidoscope Tutorial](https://llvm.org/docs/tutorial/)
- [LLVM Documentation](https://llvm.org/docs/)
- [Crafting Interpreters](https://craftinginterpreters.com/) - for general compiler concepts