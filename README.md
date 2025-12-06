# Kaleidoscope Language

A functional programming language implementation based on LLVM, following the [LLVM Kaleidoscope Tutorial](https://llvm.org/docs/tutorial/) (Chapters 1-7).

## Overview

Kaleidoscope is a functional programming language that has been extended with imperative features. It supports:

- **Mutable Variables**: `var` declarations and `=` assignment
- **Control Flow**: `if/then/else` conditionals and `for` loops
- **User-Defined Operators**: Custom binary and unary operators with adjustable precedence
- **Functions**: Definitions, calls, and recursion
- **Arithmetic**: Standard operations (`+`, `-`, `*`, `/`) and comparisons (`<`, `>`)
- **JIT Compilation**: Immediate evaluation of top-level expressions
- **C Interoperability**: Call standard C library functions (e.g., `sin`, `cos`, `putchard`)

## Project Structure

```text
kaledioscope/
├── src/
│   ├── main.cpp          # Main driver and REPL
│   ├── lexer.h/.cpp      # Lexical analysis (tokenization)
│   ├── parser.h/.cpp     # Syntax analysis (parsing)
│   ├── ast.h/.cpp        # Abstract Syntax Tree classes
│   └── codegen.h/.cpp    # LLVM code generation
├── CMakeLists.txt        # Build configuration
├── build/                # Build artifacts (generated)
└── README.md             # This file
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

The interpreter provides a REPL (Read-Eval-Print Loop) where you can enter Kaleidoscope code.

### Language Examples

#### Basic Arithmetic

```kaledioscope
>>> 5 + 10;

>>> 100 - 50 / 2;
```

#### Function Definition

```kaledioscope
def foo(x) x + 1;
def multiply(a b) a * b;
def compare(x y) x < y;
```

#### External Function Declaration

```kaledioscope
extern sin(x);
extern cos(x);
```

#### Control Flow

If/Then/Else:

```kaledioscope
>>> def fib(x)
  if x < 3 then
    1
  else
    fib(x-1) + fib(x-2);
>>> fib(6);  # Returns 8
```

For Loop:

```kaledioscope
# Syntax: for var = start, condition, step in body
>>> extern putchard(x);  # declare external C function
>>> def printstar(n)
  for i = 1, i < n, 1.0 in
    putchard(42);  # ascii 42 = '*'
```

#### User-Defined Operators

You can define custom binary and unary operators.

```kaledioscope
>>> def unary!(v)
  if v then 0 else 1;

>>> def binary | 5 (L R)
  if LHS then 1 else if RHS then 1 else 0;
```

#### Variables and Assignment

Kaleidoscope supports mutable variables using the `var ... in` construct and the `=` operator.

```kaledioscope
>>> def binary : 1 (x y) y; # sequence operator
>>> def test(x)
  var a = 1 in
    (a = x : a);
>>> test(5)  # Returns 5
```

### Mandlebrot Set Example

Below is the complete Kaleidoscope script to generate a Mandelbrot set visualization using ASCII characters. This demonstrates the language's capability to handle complex arithmetic (simulated), control flow, mutable variables, and external C calls. (From the tutorial itself)

```kaledioscope
# 1. Define logical unary/binary operators
>>> def unary!(v)
  if v then 0 else 1;

def unary-(v)
  0-v;

def binary> 10 (LHS RHS)
  RHS < LHS;

def binary| 5 (LHS RHS)
  if LHS then 1 else if RHS then 1 else 0;

def binary& 6 (LHS RHS)
  if !LHS then 0 else !!RHS;

def binary = 9 (LHS RHS)
  !(LHS < RHS | LHS > RHS);

# 2. Define the sequencing operator (discard LHS, return RHS)
def binary : 1 (x y) y;

# 3. Import C putchar function
extern putchard(char);

# 4. Helper function to print a character based on iteration density
def printdensity(d)
  if d > 8 then
    putchard(32)  # ' '
  else if d > 4 then
    putchard(46)  # '.'
  else if d > 2 then
    putchard(43)  # '+'
  else
    putchard(42); # '*'

# 5. The generic Mandelbrot calculator
#    iterates z = z^2 + c
def mandelconverger(real imag iters creal cimag)
  if iters > 255 | (real*real + imag*imag > 4) then
    iters
  else
    mandelconverger(real*real - imag*imag + creal,
                    2*real*imag + cimag,
                    iters+1, creal, cimag);

# 6. Function to iterate over the complex plane coordinates
def mandelhelp(xmin xmax xstep   ymin ymax ystep)
  for y = ymin, y < ymax, ystep in (
    (for x = xmin, x < xmax, xstep in
       printdensity(mandelconverger(0,0,0, x, y)))
    : putchard(10)
  );

# 7. Main entry point with coordinates
def mandel(realstart imagstart realmag imagmag)
  mandelhelp(realstart, realstart+realmag*78, realmag,
             imagstart, imagstart+imagmag*40, imagmag);

# RUN IT:
>>> mandel(-2.3, -1.3, 0.05, 0.07);
```

#### Output

This is the output (screenshot) of the above Mandelbrot set program from my machine:

![ASCII Mandelbrot Set Image](mandelbrot.png)

## Formal Grammar

The language grammar is defined as follows (in EBNF notation):

```
program         ::= (definition | external | expression | ';')*

definition      ::= 'def' prototype expression
external        ::= 'extern' prototype
prototype       ::= identifier '(' identifier* ')'
                  | 'binary' LETTER number? '(' identifier identifier ')'
                  | 'unary' LETTER '(' identifier ')'

expression      ::= unary | 'var' identifier ('=' expression)? (',' identifier ('=' expression)?)* 'in' expression
unary           ::= primary
                  | '!' unary | '-' unary 
primary         ::= identifier
                  | number
                  | '(' expression ')'
                  | identifier '(' expression* ')'
                  | 'if' expression 'then' expression 'else' expression
                  | 'for' identifier '=' expression ',' expression (',' expression)? 'in' expression
                  | identifier '=' expression 

binop           ::= '+' | '-' | '*' | '/' | '<' | '>' | '=' | '&' | '|' | ':'
```

### Operator Precedence

Operators are evaluated with the following precedence (highest to lowest):

| Precedence | Operators |
|-----------|-----------|
| 40        | `*`, `/` |
| 20        | `+`, `-` |
| 10        | `<`, `>` |
| 20        | `=`      |

The implementation is modular and follows compiler construction principles:

1. **Lexer** (`lexer.h/.cpp`): Tokenizes input source code into tokens
2. **Parser** (`parser.h/.cpp`): Parses tokens into an Abstract Syntax Tree (AST)
3. **AST** (`ast.h/.cpp`): Defines AST node classes and their code generation methods
4. **Code Generator** (`codegen.h/.cpp`): Translates AST to LLVM IR
5. **Main** (`main.cpp`): Provides the REPL interface and top-level parsing

## License

This implementation is based on the LLVM Kaleidoscope tutorial and follows the same educational purpose.

## References

- [LLVM Kaleidoscope Tutorial](https://llvm.org/docs/tutorial/)
- [LLVM Documentation](https://llvm.org/docs/)
