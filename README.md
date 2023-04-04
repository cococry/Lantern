# Lantern
![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)

<img src="https://github.com/cococry/Lantern/blob/main/branding/lantern_logo.png"
     alt="Lantern Logo"
     style="float: left; margin-right: 10px;" 
     width=230px
     />
     
**Lantern** is a [stack based](https://en.wikipedia.org/wiki/Stack-oriented_programming), [interpreted](https://en.wikipedia.org/wiki/Interpreter_(computing)) programming language written in **C**.
The goal of the project is creating a fast, bloatless and powerful programming tool 
that can be used for multiple purposes. 

The language has **access to the underlying memory structure** and in general
is desinged to be easily used as a [low level programming language](https://en.wikipedia.org/wiki/Low-level_programming_language) too.

Lantern is, due to the stackbased paradigm of the language it is written in 
[reverse polish notation](https://en.wikipedia.org/wiki/Reverse_Polish_notation).
wich sets it apart from a lot languages that work with AST or other structures.

## Feature list

- [x] Conditions & loops
- [x] Heap Memory System
- [x] Stackframe System
- [x] Variables & Type checking
- [x] Stack Mechanics
- [ ] Raw Memory Access
- [ ] Functions
- [ ] Else-If Functionality
- [ ] Macros
- [ ] Access to Syscalls
- [ ] Defining Structs (C-Style)
- [ ] Adding Fundamental Variable Types (float, double, char...)
- [ ] String Concatenation & Equality Operators

## Building

### Linux

#### Requirements
- GCC Compiler
- Make command

```console
mkdir bin
make
```

### Windows

#### Requirements
- GCC Compiler

```console
mkdir bin
build.bat
```

## Example Usage

### A implentation of the [FizzBuzz problem](https://de.wikipedia.org/wiki/Fizz_buzz)
```bash
0 = counter

while counter 100 < run
    counter 3 % 0 == counter 5 % 0 == and if
        "fizzbuzz" println
    fi counter 3 % 0 == elif
        "fizz" println
    fi counter 5 % 0 == elif
        "buzz" println
    endi
    counter 1 + = counter
    counter println
endw
```

## Inspiration
- [Forth](https://de.wikipedia.org/wiki/Forth_(Programmiersprache)), a stack based, imperative programming language
- [Python](https://de.wikipedia.org/wiki/Python_(Programmiersprache)), a easy to used, interpreted programming language
- [C](https://de.wikipedia.org/wiki/C_(Programmiersprache)), a statically typed, low level programming language


## Contributing

You can contribue to Lantern by:
  - [Fixing bugs or contributing to features](https://github.com/cococry/Lantern/issues)
  - [Changing features or adding new functionality](https://github.com/cococry/Lantern/pulls)
