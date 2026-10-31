<div align="center">

# RedBunny

**A small compiled language with a clean syntax and an LLVM backend, written in C.**

[![Website](https://img.shields.io/badge/Website-Live-ff4757?style=flat-square)](https://mohamedboukerche22.github.io/redbunny/)
[![LLVM](https://img.shields.io/badge/Backend-LLVM%2022-38bdf8?style=flat-square&logo=llvm&logoColor=white)](https://llvm.org)
[![C11](https://img.shields.io/badge/Implemented%20in-C11-555555?style=flat-square&logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))

[Website](https://mohamedboukerche22.github.io/redbunny/) ·
[Language manual](docs/DOCUMENTATION.md) ·
[Examples](examples/) ·
[Issues](https://github.com/Mohamedboukerche22/redbunny/issues)

</div>

---

RedBunny (`.redb`) is a compact, statically checked programming language. It
uses inferred variable types, lightweight function syntax, string templates and
native compilation through LLVM. The compiler itself is plain C with no runtime
dependencies beyond LLVM and Clang.

**Read the live manual:** <https://mohamedboukerche22.github.io/redbunny/>

## Why RedBunny?

- **Small language, real compiler.** Lexer, parser, type checker, LLVM codegen
  and native runtime — all in one readable C codebase.
- **Fast native output.** Programs are lowered to LLVM IR and linked with Clang.
- **Friendly syntax.** No type annotations for variables, no mandatory
  semicolons, and string templates built into the language.
- **Easy to embed.** The runtime is a single static archive, `libredb_runtime.a`.
- **Bundled math module.** `@include "mathlib"` provides powers, roots, trig,
  exponentials, logarithms, min/max and constants.
- **Hackable.** Roughly 7 source files and a test suite that runs in seconds.

## Quick Start

### Requirements

- LLVM development libraries (tested with LLVM 22)
- Clang
- CMake 3.18+
- A C11 compiler

On Debian/Ubuntu:

```bash
sudo apt install llvm-dev clang cmake build-essential
```

### Build from source

```bash
git clone https://github.com/Mohamedboukerche22/redbunny.git
cd redbunny

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Compile and run

```bash
./build/redb examples/hello.redb -o /tmp/hello
/tmp/hello
```

Output:

```text
Hello, World!
```

## Installation

### Linux / macOS

```bash
chmod +x scripts/install.sh
./scripts/install.sh
```

The installer builds with CMake and installs:

- `redb` to `/usr/local/bin/redb`
- `libredb_runtime.a` to `/usr/local/lib/`

Use a custom prefix without `sudo`:

```bash
PREFIX="$HOME/.local" ./scripts/install.sh
```

### Windows

```powershell
Set-ExecutionPolicy Bypass -Scope Process -Force
.\scripts\install.ps1
```

### Manual install

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build
```

## Language at a Glance

### Hello, world

```redb
@include "std"

main() {
    writeln("Hello, World!")
}
```

### Variables, templates and control flow

```redb
@include "std"

main() {
    name = "Mohamed"
    age = 20
    alive = true

    writeformat("Name: {name}, Age: {age}\n")

    if (age >= 18 && alive) {
        writeln("adult")
    } else {
        writeln("minor")
    }

    total = 0
    for (i in 0 to 9) {
        total = total + i
    }
    writeln(total)

    -- Count down inclusively: 3, 2, 1
    for (i in 3 downTo 1) {
        write(i)
    }
    writeln("")
}
```

### Functions

```redb
@include "std"

fact = do(int -> int) {
    if (arg1 <= 1) {
        return 1
    }
    return arg1 * fact(arg1 - 1)
}

main() {
    writeln(fact(10))
}
```

See [`examples/`](examples/) for complete programs and
[`docs/DOCUMENTATION.md`](docs/DOCUMENTATION.md) for the full language reference.

## Standard Library

`@include "mathlib"` loads the bundled math module. Includes resolve next to the
source file first, then the standard library directory (`lib/` in a checkout or
`share/redb/lib` when installed). Set `REDB_STDLIB` to override that directory.

| Function      | Signature         | Notes                                      |
|---------------|-------------------|--------------------------------------------|
| `square(x)`   | `int -> int`      | `x * x`                                    |
| `cube(x)`     | `int -> int`      | `x * x * x`                                |
| `power(x, n)` | `int, int -> int` | Non-negative `n`; returns `0` for `n < 0`  |
| `sqrt(x)`     | `double -> double`| Returns `0` for `x <= 0`                   |
| `sin(x)`      | `double -> double`| Taylor series, best for `-pi <= x <= pi`        |
| `cos(x)`      | `double -> double`| Taylor series, best for `-pi <= x <= pi`        |
| `exp(x)`      | `double -> double`| 20-term Taylor series, moderate inputs     |
| `ln(x)`       | `double -> double`| Range reduction + atanh series; returns `0` for `x <= 0` |
| `max(a, b)`   | `int, int -> int` | Larger of two integers                     |
| `min(a, b)`   | `int, int -> int` | Smaller of two integers                    |
| `PI()`        | `{} -> double`    | 3.141592653589793                          |
| `euler()`     | `{} -> double`    | 2.718281828459045                          |
| `tau()`       | `{} -> double`    | 6.283185307179586                          |
| `phi()`       | `{} -> double`    | 1.618033988749895                          |

```redb
@include "std"
@include "mathlib"

main() {
    writeln(square(9))
    writeln(power(2, 10))

    r = sqrt(2.0)
    writeformat("sqrt(2) = {r}\n")
}
```

## Command Line

```text
usage: redb <file.redb> [options]

  -o <path>       write the executable to <path>
  -S, --emit-ir   write LLVM IR only (no link step)
      --keep-ir   keep the generated .ll file next to the binary
      --run       compile and run the program
  -V, --version   print the compiler version
  -h, --help      show this help
```

| Variable       | Purpose                                  |
|----------------|------------------------------------------|
| `REDB_CC`      | C compiler used to link generated IR      |
| `REDB_RUNTIME` | explicit path to `libredb_runtime.a`      |
| `REDB_STDLIB`  | directory containing standard library modules |

Examples:

```bash
redb main.redb -o app && ./app
redb main.redb --run
redb main.redb -S -o main.ll
```

## Repository Layout

```text
.
├── CMakeLists.txt          build, install and test configuration
├── README.md
├── .nojekyll               serve the site as static files on GitHub Pages
├── index.html              live documentation website (GitHub Pages)
├── include/
│   └── redb.h              tokens, AST, shared types and prototypes
├── src/
│   ├── main.c              CLI driver, linking and execution
│   ├── util.c              diagnostics and shared helpers
│   ├── lexer.c             source text → token stream
│   ├── parser.c            recursive-descent parser → AST
│   ├── include.c           @include expansion and path resolution
│   ├── typecheck.c         scopes, inference and template lowering
│   └── codegen.c           AST → LLVM IR
├── runtime/
│   └── runtime.c           native runtime linked into every program
├── lib/
│   └── mathlib.redb        standard math module
├── docs/
│   └── DOCUMENTATION.md    complete language manual
├── examples/
│   ├── hello.redb
│   ├── tour.redb
│   ├── factorial.redb
│   ├── math_demo.redb
│   └── imports/
│       ├── app.redb
│       └── greeting.redb
├── scripts/
│   ├── install.sh          Linux / macOS installer
│   └── install.ps1         Windows installer
└── tests/
    ├── run.sh              test runner
    ├── cases/              programs with expected output
    └── errors/             programs that must be rejected
```

## How It Works

```text
source (.redb)
    │
    ▼
lexer.c → parser.c → include.c → typecheck.c → codegen.c
                                                    │
                                                    ▼
                                          LLVM IR → clang
                                                    │
                                                    ▼
                             native executable + libredb_runtime.a
```

1. **Lex** the source into tokens.
2. **Parse** declarations and expressions into an AST.
3. **Expand** `@include` files relative to the importing file.
4. **Check** scopes and types; lower string templates into runtime calls.
5. **Generate** LLVM IR and link it with Clang and the C runtime.

## Tests

```bash
ctest --test-dir build --output-on-failure
```

or run the suite directly:

```bash
bash tests/run.sh
```

The suite compiles working programs, checks their exact output, and verifies
that invalid programs are rejected with a compile error.

## Status

Version **0.1.0** — usable for small programs and language experiments.

Current limitations:

- No arrays, structs, classes or closures
- No garbage collector; strings are heap-allocated and never freed
- No module system beyond textual `@include`
- Windows support is provided but tested less than Linux/macOS

Implemented types: `int` (64-bit), `float` (32-bit), `double` (64-bit),
`bool`, and `str`.

## Contributing

Pull requests are welcome. Before opening one:

1. Build the project and run `ctest --test-dir build --output-on-failure`.
2. Add a test under `tests/cases/` or `tests/errors/` for behavior changes.
3. Keep the compiler dependency-free outside LLVM and Clang.

## Credits

Created by **Mohamed Boukerche** ([@Mohamedboukerche22](https://github.com/Mohamedboukerche22)).

Live documentation: <https://mohamedboukerche22.github.io/redbunny/>
