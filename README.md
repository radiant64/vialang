# Via

Scheme-inspired language with an embeddable interpreter. Work in progress!

## Features

### Core Language Features

- Uses strict, dynamic typing.
- Supports native syntax meta-programming.
- Supports continuations.
  - Explicit support for exceptions and exception handling.
- Garbage collected.
- Guarantees proper tail call optimization/tail call elimination.

### Implementation Features

- Made for usage as an embeddable scripting engine.
    - ANSI/ISO C99 API without preprocessor macros, to simplify integration with
      arbitrary languages.
    - API allows C functions to be hooked in as regular procedures or special
      syntax forms, allowing the embedding application to extend the language
      arbitrarily.
    - Low binary footprint (in the order of kilobytes).
- Logic implemented as a virtual register machine.
  - Built-in two-pass assembler can create custom machine code programs.
  - Architecture supports JIT compilation (not currently implemented).
  - Heap is addressable, though no opcodes operating on the heap have been
    implemented yet.
- Uses a recursive descent parser, operating on VM-native data structures.
- Call stack is implemented as a linked list, and is separate from the data
  stack. Call stack frames can be inspected from a running program.
- Contains bundled library of procedures and syntax forms, implemented using a
  mixture of C functions, VM machine code programs, and in native Via code.

### Planned Features

(In no particular order:)

- Multi-threading support.
- JIT compiler.
- Proper documentation.
- Better packaging.
  - Support for generating `pkg-config` and CMake packages.
  - Improved public API.
  - Optional modern C++ API wrapper.
- Integrated REPL environment.

## Building

### Prerequisites

- A reasonably ANSI/ISO C99 conforming compiler supporting the `#pragma once`
  directive (for example GCC, Clang or Visual C++).
- CMake 3.12 or later.
- A build toolchain.

### CMake

A project can be generated using CMake. Support for in-source builds explicitly
is explicitly disabled. The recommended process is to create a directory named
`build` in the project root directory, and run CMake from there:

```sh
cd vialang
mkdir build
cd build
cmake ..
```

The generated project can then be built using your regular build toolchain
(or whichever toolchain CMake defaults to, such as
[Make](<https://en.wikipedia.org/wiki/Make_(software)>)).

#### Example

Building everything and running the included tests, using
[Make](<https://en.wikipedia.org/wiki/Make_(software)>):

```
make
make test
```

## Status

Working pre-alpha. API and implementation subject to breaking changes.

## Example Program

```
; Ye Olde Example Programme

; Sequences are explicit on all levels.
(begin
  (set-proc! factorial (number)
             (begin
               ; Iteration is implemented through a recursive tail call.
               (set-proc! factorial-impl (number acc)
                          (if (= number 1)
                              acc
                              (factorial-impl (- number 1) (* acc number)))))
               (factorial-impl number 1))
  (display (factorial 5)))
```

Output:
```
120
```

