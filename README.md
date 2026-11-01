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
  exponentials, logarithms, abs/gcd/lcm, integer/double min/max and constants.
- **Bundled algorithm module.** `@include "algorithm"` provides binary search,
  in-place reverse and heap sort, min/max element, and 64-bit integer limits.
- **Dynamic arrays.** `array<T>` values support indexing, `size()`, `resize()`,
  literal initializers and nested arrays.
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

### One-line install

Linux / macOS:

```bash
curl -fsSL https://raw.githubusercontent.com/Mohamedboukerche22/redbunny/main/scripts/get.sh | bash
```

Windows PowerShell:

```powershell
irm https://raw.githubusercontent.com/Mohamedboukerche22/redbunny/main/scripts/get.ps1 | iex
```

The bootstrap downloads the current `main` branch, installs the compiler, then compiles and runs `examples/hello.redb`. Linux/macOS installs to `~/.local`; Windows installs to `%LOCALAPPDATA%\RedBunny`. Override with `PREFIX=/some/path` on Linux/macOS or `$env:PREFIX = "D:\RedBunny"` on Windows.

### From a source checkout

#### Linux / macOS

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

#### Windows

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

### Editor support

Vim and Neovim get syntax highlighting, `.redb` filetype detection, comment
strings and indentation from [`editors/vim`](editors/vim). Link it into your
package directory:

```bash
mkdir -p ~/.vim/pack/redb/start
ln -s "$PWD/editors/vim" ~/.vim/pack/redb/start/redb
```

For Neovim use `~/.local/share/nvim/site/pack/redb/start` instead. See
[`editors/vim/README.md`](editors/vim/README.md) for `runtimepath` and
`lazy.nvim` setups.

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

### Arrays

```redb
@include "std"

main() {
    array<int> nums(3)
    nums[0] = 10
    nums[2] = 30
    writeln(nums[0] + nums[2])   -- 40
    writeln(nums.size())         -- 3

    nums.resize(5)
    writeln(nums.size())         -- 5

    array<int> literal = {2, 4, 6}
    writeln(literal[1])          -- 4

    array<auto> inferred = {1, 2, 3}
    array<auto> sized(2) = {2, 5}
    writeln(inferred.size() + sized.size()) -- 5

    array<array<int>> grid = {{1, 2}, {3, 4}}
    writeln(grid[0][1] + grid[1][0]) -- 5
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

`@include "mathlib"` loads the bundled math module and `@include "algorithm"`
loads array algorithms. Includes resolve next to the source file first, then the
standard library directory (`lib/` in a checkout or `share/redb/lib` when
installed). Set `REDB_STDLIB` to override that directory.

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
| `maxf(a, b)`  | `double, double -> double` | Larger of two doubles              |
| `minf(a, b)`  | `double, double -> double` | Smaller of two doubles             |
| `abs(x)`      | `int -> int`      | Absolute value                             |
| `gcd(a, b)`   | `int, int -> int` | Greatest common divisor                    |
| `lcm(a, b)`   | `int, int -> int` | Least common multiple                      |
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

### Algorithm library

`@include "algorithm"` loads array algorithms for `array<int>`. `sort` and
`reverse` mutate their array in place.

| Function                   | Signature                | Notes                                        |
|----------------------------|--------------------------|----------------------------------------------|
| `MAX_INT64()`              | `{} -> int`              | `9223372036854775807`                        |
| `MIN_INT64()`              | `{} -> int`              | `-9223372036854775808`                       |
| `binary_search(a, target)` | `array<int>, int -> int` | Index in an ascending sorted array, or `-1`  |
| `reverse(a)`               | `array<int> -> array<int>` | Reverses in place; returns the same array  |
| `max_element(a)`           | `array<int> -> int`      | Largest element; empty array gives `MIN_INT64()` |
| `min_element(a)`           | `array<int> -> int`      | Smallest element; empty array gives `MAX_INT64()` |
| `sort(a)`                  | `array<int> -> {}`       | In-place heap sort, `O(n log n)`             |

`MAX_INT64` and `MIN_INT64` are zero-argument constant helpers because RedBunny
has no `const` keyword.

```redb
@include "std"
@include "algorithm"

main() {
    array<int> nums = {5, 1, 4, 2, 3}

    sort(nums)
    writeln(nums[0])                 -- 1
    writeln(binary_search(nums, 4))  -- 3

    reverse(nums)
    writeln(nums[0])                 -- 5
    writeln(max_element(nums))       -- 5
    writeln(min_element(nums))       -- 1
    writeln(MAX_INT64())
    writeln(MIN_INT64())
}
```

## AI Training Data

`TRAIN_IA_MODEL/` converts the manual, examples, standard library and tests into
instruction-tuning data for local models. The generated dataset is hosted on
Hugging Face: [mohamedboukerche55/redbunny-training-data](https://huggingface.co/datasets/mohamedboukerche55/redbunny-training-data).

Generate it with:

```bash
python3 TRAIN_IA_MODEL/build.py
```

It writes OpenAI-style chat JSONL, a concise syntax-focused set, Alpaca JSONL,
a `moh-gpt`-compatible `messages` file, train/eval splits, a plain-text corpus
and a manifest under `TRAIN_IA_MODEL/data/`. Keep it updated while editing
with `python3 TRAIN_IA_MODEL/watch.py`, or let the GitHub Actions workflow
regenerate it on push. See `TRAIN_IA_MODEL/README.md` for training instructions.

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
│   ├── mathlib.redb        standard math module
│   └── algorithm.redb      standard array algorithms
├── docs/
│   └── DOCUMENTATION.md    complete language manual
├── editors/
│   └── vim/
│       ├── README.md       Vim/Neovim install instructions
│       ├── ftdetect/redb.vim
│       ├── ftplugin/redb.vim
│       ├── indent/redb.vim
│       └── syntax/redb.vim
├── examples/
│   ├── hello.redb
│   ├── tour.redb
│   ├── factorial.redb
│   ├── math_demo.redb
│   └── imports/
│       ├── app.redb
│       └── greeting.redb
├── scripts/
│   ├── get.sh              Linux / macOS one-line installer
│   ├── get.ps1             Windows one-line installer
│   ├── install.sh          Linux / macOS source installer
│   └── install.ps1         Windows source installer
├── tests/
│   ├── run.sh              test runner
│   ├── cases/              programs with expected output
│   └── errors/             programs that must be rejected
└── TRAIN_IA_MODEL/
    ├── build.py            generate training data from the repo
    ├── watch.py            rebuild on source changes
    ├── seed.jsonl          curated syntax Q&A
    ├── train_lora.py       optional local LoRA training script
    └── data/               generated chat, Alpaca and plain-text datasets
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

- No structs, classes or closures
- No garbage collector; strings and arrays are heap-allocated and never freed
- No module system beyond textual `@include`
- Windows support is provided but tested less than Linux/macOS

Implemented types: `int` (64-bit), `float` (32-bit), `double` (64-bit),
`bool`, `str`, and `array<T>`.

## Contributing

Pull requests are welcome. Before opening one:

1. Build the project and run `ctest --test-dir build --output-on-failure`.
2. Add a test under `tests/cases/` or `tests/errors/` for behavior changes.
3. Keep the compiler dependency-free outside LLVM and Clang.

## Credits

Created by **Mohamed Boukerche** ([@Mohamedboukerche22](https://github.com/Mohamedboukerche22)).

Live documentation: <https://mohamedboukerche22.github.io/redbunny/>
