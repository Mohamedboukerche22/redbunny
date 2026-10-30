# RedBunny (.redb)

A small compiled language with dynamic-feeling variables, a compact function
syntax and an LLVM backend. The compiler is written in C.

```redb
@include "std"

main() {
    name = "Mohamed"
    writeformat("Hello, {name}!\n")
}
```

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/redb examples/hello.redb -o /tmp/hello
/tmp/hello
```

Or install system-wide:

```bash
./install.sh          # Linux / macOS
.\install.ps1         # Windows
```

## Layout

| File           | Purpose                                      |
|----------------|----------------------------------------------|
| `redb.h`       | tokens, AST, shared types and prototypes     |
| `lexer.c`      | source to token stream                       |
| `parser.c`     | recursive-descent parser and AST             |
| `include.c`    | source include expansion                     |
| `typecheck.c`  | scope resolution, inference, format lowering |
| `codegen.c`    | AST to LLVM IR                               |
| `runtime.c`    | native runtime called by generated IR        |
| `main.c`       | CLI, linking and execution                   |

See [DOCUMENTATION.md](DOCUMENTATION.md) for the language manual.

## Status

Version 0.1.0. No arrays, structs, closures or garbage collector yet.
