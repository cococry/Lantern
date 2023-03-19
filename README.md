# Lantern

## Overview
Lantern is a stack based programming language
written in C++ 17. 

Due to the stack based paradigm of the language
its written in the reverse polish notation.

The language is tokeniesd and iterpreted 
interpreted. It Supports conditions and 
non linear interpretation

## Usage

## Linux

Building Lantern
```console
make build
```
To interprete a Lantern program run:
```console
./lantern <filepath>
```

## Windows

Building Lantern
```console
build.bat
```
To interprete a Lantern program run:
```console
.\lantern.exe <filepath>
```

## Features

* String & Int Operations (+, -, *, /, ==, !=...)
* Conditional Statements (if, else)
* While loops
* Printing to console output 
* String literals

## Basic Lantern Program

```bash
"Hello, World" print

10 10 * 100 == if
  "10 times 10 is equal to 100!" print
endi

"He" "llo" + print
30 30 + print

0
while 0 prev 10 < run
  0 prev print
  1 +
endw
```
