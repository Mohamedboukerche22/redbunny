# RedBunny (.redb) Programming Language Manual

Made with ❤️ by mohamedboukerche22 (GitHub: [github.com/mohamedboukerche22](https://github.com/mohamedboukerche22))

Live manual: **https://mohamedboukerche22.github.io/redbunny/**

RedBunny is a small, fast, compiled language. Source files use the `.redb`
extension, the compiler is written in C, and programs are lowered to LLVM IR
before being linked into native executables.

---

## 1. Installation

### One-line install

Linux / macOS:

```bash
curl -fsSL https://raw.githubusercontent.com/Mohamedboukerche22/redbunny/main/scripts/get.sh | bash
```

Windows PowerShell:

```powershell
irm https://raw.githubusercontent.com/Mohamedboukerche22/redbunny/main/scripts/get.ps1 | iex
```

The bootstrap downloads the current `main` branch, installs the compiler, then
compiles and runs `examples/hello.redb`. Linux/macOS installs to `~/.local`;
Windows installs to `%LOCALAPPDATA%\RedBunny`. Override with
`PREFIX=/some/path` on Linux/macOS or `$env:PREFIX = "D:\RedBunny"` on Windows.

### From a source checkout

#### Linux / macOS

```bash
chmod +x scripts/install.sh
./scripts/install.sh
```

The installer builds with CMake and installs:

- `redb` to `/usr/local/bin/redb`
- `libredb_runtime.a` to `/usr/local/lib/`

Override the prefix with `PREFIX=$HOME/.local ./scripts/install.sh`.

#### Windows

```powershell
Set-ExecutionPolicy Bypass -Scope Process -Force
.\scripts\install.ps1
```

Requirements: LLVM development libraries, Clang, CMake and Ninja. The compiler
links generated IR with `clang` at build time, so `clang` must be on `PATH`.

### Manual build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build
```

---

## 2. Hello World

```redb
@include "std"

main() {
    writeln("Hello, World!");
}
```

Compile and run:

```bash
redb hello.redb -o hello
./hello
```

---

## 3. Comments

```redb
-- single-line comment

-*
   multi-line
   comment block
*-
```

Comments are removed by the lexer and never appear in the generated program.

Semicolons are optional. Statements are separated by the grammar, not by
newlines, so either style works:

```redb
main() {
    a = 1;
    b = 2
    writeln(a + b)
}
```

---

## 4. Program Structure

Every executable program must define exactly one `main()` block:

```redb
main() {
    writeln("start");
}
```

`main()` returns an integer exit status to the operating system. A bare
`return` exits with `0`; `return 42` exits with `42`.

Top-level declarations are limited to:

- `@include "..."`
- `main() { ... }`
- function definitions such as `name = do(...) { ... }`

All other statements live inside `main()` or another function.

---

## 5. Variables and Types

Variables are introduced by assignment. The first assignment fixes the static
type used by the LLVM backend:

```redb
main() {
    name = "Mohamed"     -- str
    age = 20             -- int (64-bit signed)
    pi = 3.14159         -- double
    alive = true         -- bool

    writeformat("Name: {name}, Age: {age}, pi: {pi}, alive: {alive}\n")
}
```

Supported types:

| Type     | Meaning                              |
|----------|--------------------------------------|
| `int`    | signed 64-bit integer                |
| `float`  | 32-bit IEEE floating point           |
| `double` | 64-bit IEEE floating point           |
| `str`    | null-terminated UTF-8 byte string    |
| `bool`   | `true` or `false`                    |
| `void`   | no value, written `{}` in signatures |
| `array<T>` | heap array of `T`                  |
| class name | heap object with typed fields      |

A decimal literal such as `3.14` is a `double`. Integer literals are `int`.
Numeric values convert automatically between `int`, `float` and `double`
according to the destination type.

Reassigning an existing variable keeps its original type:

```redb
main() {
    count = 0
    count = count + 1   -- ok
    count = "one"       -- compile error
}
```

New names are block scoped. Assigning an existing name writes the nearest
visible binding:

```redb
main() {
    if (true) {
        local = 1
    }
    writeln(local)      -- error: unknown identifier
}
```

### Arrays

An array holds many values of one element type. Declare one with `array<T>`,
where `T` is `int`, `float`, `double`, `bool`, `str`, `auto`, another array
type, or a class name.

```redb
main() {
    array<int> nums(5)          -- 5 elements, all 0
    nums[0] = 10
    nums[4] = 40
    writeln(nums[0] + nums[4])  -- 50
    writeln(nums.size())        -- 5

    nums.resize(7)              -- grow to 7; new elements are 0
    writeln(nums.size())        -- 7

    array<auto> alias = nums    -- element type copied from nums (int)
    alias[1] = 3
    writeln(nums[1])            -- 3: alias and nums are the same array

    array<array<int>> matrix(2) -- two inner arrays, each starts empty
    array<int> row(3)
    row[2] = 9
    matrix[1] = row
    writeln(matrix[1][2])       -- 9

    array<int> literal = {2, 4, 6}
    writeln(literal[0] + literal[2]) -- 8
    writeln(literal.size())          -- 3

    array<auto> inferred = {1, 2, 3}
    writeln(inferred.size())         -- 3

    array<auto> sized(2) = {2, 5}
    writeln(sized[0] + sized[1])     -- 7

    array<array<int>> grid = {{1, 2}, {3, 4}}
    writeln(grid[0][1] + grid[1][0]) -- 5
}
```

| Form                        | Meaning                                                    |
|-----------------------------|------------------------------------------------------------|
| `array<int> a(5)`           | 5 elements, all `0` (`0.0` for double, `""` for str)       |
| `array<int> a`              | empty array (size 0)                                       |
| `array<int> a = b`          | `a` refers to the same array as `b`                        |
| `array<auto> a = b`         | `a` takes the element type of `b`                          |
| `array<auto> a(5)`          | no initializer, so the element type defaults to `int`      |
| `array<int> a = {1, 2, 3}`  | literal initializer; length is inferred                    |
| `array<auto> a = {1, 2}`    | element type inferred from the literal                     |
| `array<auto> a(2) = {1, 2}` | explicit size must match the literal length                |
| `a[i]`, `a[i] = x`          | read or write element `i` (indexes start at 0)             |
| `a.size()`                  | number of elements                                         |
| `a.resize(n)`               | change the size; new elements are zero or empty            |
| `array<array<int>> m(3)`    | nested arrays; `m[0][1] = 7` works                         |

Arrays can be passed to and returned from functions:

```redb
total = do(array<int> -> int) {
    return arg1[0] + arg1[1]
}
```

Notes:

- Array literals use braces: `{1, 2, 3}`. Elements must share one type; numeric
  literals are promoted (`{1, 2.5}` becomes `array<double>`).
- An index outside `0` to `size() - 1` stops the program with a runtime error.
- Assigning one array to another does not copy it; both names share the elements.
- Inner arrays created by `array<array<T>> m(n)` start empty. Call `resize()` on
  them before indexing.
- A whole array cannot be printed with `write` or `writeln`. Print one element at
  a time.
- `array` and `auto` are reserved words.

---

## 6. Operators

Arithmetic:

```redb
a = 1 + 2
b = 10 - 3
c = 4 * 5
d = 20 / 4
e = 21 % 4
```

Comparison and logic:

```redb
writeln(3 < 5)
writeln(3 <= 3)
writeln(3 == 3)
writeln(3 != 4)
writeln(true && false)
writeln(true || false)
writeln(!false)
```

Bit shifts work on `int` operands and compile to a single LLVM shift:

```redb
writeln(1 << 4)     -- 16
writeln(256 >> 4)   -- 16
writeln(-8 >> 1)    -- -4, right shift keeps the sign
```

Shift counts must be in the range `0` to `63`. Shifts do not check the count
at runtime; out-of-range counts follow LLVM behavior.

String concatenation uses `+`:

```redb
main() {
    name = "Red" + "Bunny"
    writeln(name)       -- RedBunny
}
```

String equality uses `==` and `!=`. Ordering comparisons are numeric only.

Precedence, highest to lowest:

1. unary `-`, `!`
2. `*`, `/`, `%`
3. `+`, `-`
4. `<<`, `>>`
5. `<`, `<=`, `>`, `>=`
6. `==`, `!=`
7. `&&`
8. `||`

---

## 7. Output

### `write`

Prints without a trailing newline. Accepts `str`, `int`, `float`, `double` or
`bool`.

```redb
write("loading")
write("...")
writeln("")
```

### `writeln`

Prints a value followed by a newline.

```redb
writeln("done")
writeln(42)
writeln(3.5)
writeln(true)
```

### `writeformat`

Interpolates variables into a template:

```redb
main() {
    name = "Mohamed"
    age = 20

    writeformat("my name is {name} and my age is {age}\n")
}
```

Use doubled braces for literal braces:

```redb
writeformat("{{name}} prints a literal brace block\n")
```

Only variable names may appear inside `{...}`. Unknown names and function names
are rejected at compile time.

---

## 8. Control Flow

### `if` / `else`

```redb
main() {
    age = 20

    if (age >= 18) {
        writeln("adult")
    } else {
        writeln("minor")
    }
}
```

`else if` works as expected:

```redb
if (score >= 90) {
    writeln("A")
} else if (score >= 80) {
    writeln("B")
} else {
    writeln("C")
}
```

Conditions accept `bool` values and numeric values (`0` is false, everything
else is true).

### `for`

```redb
main() {
    for(i in 0 to 9) {
        writeln(i)
    }
}
```

The range is inclusive: the loop above runs ten times, from `0` through `9`.

Use `downTo` to count downward:

```redb
main() {
    for(i in 5 downTo 1) {
        writeln(i)    -- 5, 4, 3, 2, 1
    }
}
```

`to` counts upward and `downTo` counts downward. If the bounds point the wrong
way, the body does not run. `downTo` is a single keyword with a capital `T`.

### `while`

```redb
main() {
    n = 0
    while (n < 5) {
        write(n)
        n = n + 1
    }
    writeln("")
}
```

### `break` and `continue`

```redb
for(i in 0 to 9) {
    if (i == 2) { continue }
    if (i == 5) { break }
    write(i)
}
```

`break` and `continue` may only appear inside a loop.

---

## 9. Functions

Function values use a compact signature syntax:

```redb
add_one = do(int -> int) {
    return arg1 + 1;
}
```

- `do(...)` declares a function.
- The parameters appear before `->`.
- The return type appears after `->`.
- `{}` means `void`.
- `do()` is shorthand for `do({} -> {})`.
- `doto({} => int)` declares a function that takes no arguments.
- `=>` is accepted as an alias for `->`.

Parameters are available as `arg1`, `arg2`, and so on.

```redb
add = do(int, int -> int) {
    return arg1 + arg2
}

greet = do(str -> {}) {
    writeln(arg1)
}

answer = doto({} => int) {
    return 42
}

main() {
    writeln(add(2, 3))
    greet("hello")
    writeln(answer())
}
```

Functions are hoisted before `main`, so they can call each other and recurse:

```redb
fact = do(int -> int) {
    if (arg1 <= 1) {
        return 1
    }
    return arg1 * fact(arg1 - 1)
}

main() {
    writeln(fact(5))    -- 120
}
```

The type checker verifies that every `return` matches the declared return type.
It does not prove that all control-flow paths return. If execution reaches the
end of a non-void function, the generated code returns zero, so write explicit
returns on every path.

---

## 10. Classes

Classes group named fields with methods. Declare a class with `class`; fields
use explicit types; methods use the same `do(...)` syntax as functions.

```redb
@include "std"

class Counter {
    int value

    init = do(int -> {}) {
        value = arg1
    }

    add = do(int -> {}) {
        value = value + arg1
    }

    get = do(-> int) {
        return value
    }
}

main() {
    c = Counter(5)
    writeln(c.get())      -- 5
    c.add(7)
    writeln(c.value)      -- 12
    c.value = 20
    writeln(c.get())      -- 20
}
```

- Instantiate with `Name(args)`. `init` is the constructor and must return
  `{}` (void). If a class has no `init`, only `Name()` is valid.
- Fields are zero-initialized: numeric fields are `0`, `bool` is `false`, and
  `str`, array and class fields start as null pointers. Initialize arrays and
  strings in `init`.
- Each field occupies an 8-byte slot. Read with `obj.field`; assign with
  `obj.field = value`.
- Call methods with `obj.method(...)`. Inside a method, fields are in scope
  without `obj.`, and `self` refers to the receiver. Use `self.method(...)` to
  call another method.
- Method parameters are `arg1`, `arg2`, ... just like functions.
- Classes have no inheritance, access control, static members, overloads,
  destructors or garbage collection.
- Class-typed fields start as null. Calling a method or reading a field through
  a null object will crash.

---

## 11. Includes and Standard Library

`@include "std"` is accepted and the runtime is linked automatically.

Includes are resolved in this order:

1. Relative to the including file.
2. The standard library directory: `REDB_STDLIB` if set, otherwise `lib/` in a
   source checkout or `share/redb/lib` when installed.

The `.redb` suffix is optional. Local files shadow standard library modules.
Include cycles are rejected after 32 levels. The bundled modules are `mathlib`
and `algorithm`.

```redb
-- greeting.redb (local module)
greet = do(str -> {}) {
    writeln("Hello, " + arg1 + "!")
}
```

```redb
-- app.redb
@include "std"
@include "greeting"
@include "mathlib"

main() {
    greet("RedBunny")
    writeln(square(9))    -- 81
}
```

### Math library

`@include "mathlib"` loads the standard math module:

| Function            | Signature              | Notes                                      |
|---------------------|------------------------|--------------------------------------------|
| `square(x)`         | `int -> int`           | `x * x`                                    |
| `cube(x)`           | `int -> int`           | `x * x * x`                                |
| `power(x, n)`       | `int, int -> int`      | Non-negative `n`; returns `0` for `n < 0`  |
| `sqrt(x)`           | `double -> double`     | Returns `0` for `x <= 0`                   |
| `sin(x)`            | `double -> double`     | Taylor series, best for `-pi <= x <= pi`        |
| `cos(x)`            | `double -> double`     | Taylor series, best for `-pi <= x <= pi`        |
| `exp(x)`            | `double -> double`     | 20-term Taylor series, moderate inputs     |
| `ln(x)`             | `double -> double`     | Range reduction + atanh series; returns `0` for `x <= 0` |
| `max(a, b)`         | `int, int -> int`      | Larger of two integers                     |
| `min(a, b)`         | `int, int -> int`      | Smaller of two integers                    |
| `maxf(a, b)`        | `double, double -> double` | Larger of two doubles                  |
| `minf(a, b)`        | `double, double -> double` | Smaller of two doubles                 |
| `abs(x)`            | `int -> int`           | Absolute value                             |
| `gcd(a, b)`         | `int, int -> int`      | Greatest common divisor                    |
| `lcm(a, b)`         | `int, int -> int`      | Least common multiple                      |
| `PI()`              | `{} -> double`         | 3.141592653589793                          |
| `euler()`           | `{} -> double`         | 2.718281828459045                          |
| `tau()`             | `{} -> double`         | 6.283185307179586                          |
| `phi()`             | `{} -> double`         | 1.618033988749895                          |

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

| Function                  | Signature              | Notes                                      |
|---------------------------|------------------------|--------------------------------------------|
| `MAX_INT64()`             | `{} -> int`            | `9223372036854775807`                      |
| `MIN_INT64()`             | `{} -> int`            | `-9223372036854775808`                     |
| `binary_search(a, target)`| `array<int>, int -> int` | Index in an ascending sorted array, or `-1` |
| `reverse(a)`              | `array<int> -> array<int>` | Reverses in place; returns the same array |
| `max_element(a)`          | `array<int> -> int`    | Largest element; empty array gives `MIN_INT64()` |
| `min_element(a)`          | `array<int> -> int`    | Smallest element; empty array gives `MAX_INT64()` |
| `sort(a)`                 | `array<int> -> {}`     | In-place heap sort, `O(n log n)`           |

`MAX_INT64` and `MIN_INT64` are zero-argument constant helpers because RedBunny
has no `const` keyword.

The module also defines a fixed-capacity FIFO queue:

| Member                | Signature        | Notes                                      |
|-----------------------|------------------|--------------------------------------------|
| `Queue(capacity)`     | `int -> Queue`   | Constructor; capacity must be greater than zero |
| `q.enqueue(value)`    | `int -> {}`      | Prints `queue full` and drops the value when full |
| `q.dequeue()`         | `{} -> int`      | Removes the front value; returns `-1` when empty |
| `q.front()`           | `{} -> int`      | Peeks at the front value; returns `-1` when empty |
| `q.size()`            | `{} -> int`      | Number of stored values                    |
| `q.is_empty()`        | `{} -> bool`     | True when `size() == 0`                    |
| `q.is_full()`         | `{} -> bool`     | True when `size() == capacity`             |

```redb
@include "std"
@include "algorithm"

main() {
    q = Queue(3)
    q.enqueue(10)
    q.enqueue(20)
    writeln(q.front())      -- 10
    writeln(q.dequeue())    -- 10
    writeln(q.size())       -- 1
}
```

See `examples/queue_demo.redb` for a longer ring-buffer example.

The module also defines `vector(capacity)`, a growable `int` vector. The
constructor clamps capacity to at least 1. `push_back` is amortized O(1);
`push_front` is O(n).

| Member                | Signature        | Notes                                      |
|-----------------------|------------------|--------------------------------------------|
| `vector(capacity)`    | `int -> vector`  | Constructor; capacity is at least 1        |
| `v.push_back(value)`  | `int -> {}`      | Appends a value, growing if needed         |
| `v.push_front(value)` | `int -> {}`      | Inserts at index 0, shifting existing values |
| `v.get(index)`        | `int -> int`     | Returns the value, or `-1` when out of range |
| `v.get_size()`        | `{} -> int`      | Number of stored values                    |
| `v.get_capacity()`    | `{} -> int`      | Allocated slot count                       |
| `v.reserve(n)`        | `int -> {}`      | Grows capacity to at least `n`             |

```redb
@include "std"
@include "algorithm"

main() {
    v = vector(2)
    v.push_back(10)
    v.push_back(20)
    v.push_front(5)

    writeln(v.get_size())      -- 3
    writeln(v.get(0))          -- 5
    writeln(v.get(1))          -- 10
    writeln(v.get_capacity())  -- 4
}
```

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

---

## 12. Command Line

```text
usage: redb <file.redb> [options]

  -o <path>       write the executable to <path>
  -S, --emit-ir   write LLVM IR only (no link step)
      --keep-ir   keep the generated .ll file next to the binary
      --run       compile and run the program
  -V, --version   print the compiler version
  -h, --help      show this help
```

Examples:

```bash
redb main.redb -o output_bin
./output_bin

redb main.redb -S -o main.ll
redb main.redb --run

REDB_CC=clang-22 redb main.redb -o app
```

Environment variables:

| Variable       | Purpose                                      |
|----------------|----------------------------------------------|
| `REDB_CC`      | C compiler used to link the generated IR      |
| `REDB_RUNTIME` | explicit path to `libredb_runtime.a`          |
| `REDB_STDLIB`  | directory containing standard library modules |

---

## 13. Runtime ABI

The generated IR calls these C functions:

```c
void  redb_write(const char *str);
void  redb_writeln(const char *str);
void  redb_write_int(long long value);
void  redb_write_double(double value);
void  redb_write_bool(int value);
void  redb_newline(void);
char *redb_str_concat(const char *left, const char *right);
int   redb_str_eq(const char *left, const char *right);
void *redb_alloc(size_t size);
void *redb_alloc_zero(size_t size);
void  redb_writeformat(const char *fmt, ...);

void *redb_array_new(long long len, long long nest, long long leaf_size);
void *redb_array_at(void *array, long long index);
long long redb_array_size(void *array);
void  redb_array_resize(void *array, long long size);
```

Class objects are allocated with `redb_alloc_zero`; each field is stored in an
8-byte slot. String concatenation allocates a new buffer with `malloc`.
RedBunny currently has no garbage collector, so long-running string-building
loops will grow memory.

---

## 14. Current Limitations

- No maps, structs or pointers.
- No generics beyond `array<T>`.
- No class inheritance, interfaces, access control or static members.
- Class fields are fixed 8-byte slots and there are no field initializers.
- No closures; functions cannot capture enclosing locals.
- No exceptions or panic handling.
- No package registry; includes resolve relative to the source file, then the
  standard library directory.
- Floating point `%` uses `frem` and is not the same as C `fmod` for all inputs.

These are deliberate v0.1 boundaries, not parser accidents.

---

## 15. Examples

The `examples/` directory contains:

- `hello.redb` — minimal program
- `tour.redb` — variables, formatting, loops, conditionals, functions
- `factorial.redb` — recursion
- `arrays.redb` — arrays, literals, resizing and nested arrays
- `algorithm_demo.redb` — sorting, searching and array algorithms
- `queue_demo.redb` — a class-based FIFO queue
- `math_demo.redb` — standard math library
- `imports/app.redb` — local and standard library includes
- `imports/greeting.redb` — local module used by `app.redb`

Build them all:

```bash
redb examples/hello.redb -o /tmp/hello && /tmp/hello
redb examples/tour.redb -o /tmp/tour && /tmp/tour
redb examples/factorial.redb -o /tmp/factorial && /tmp/factorial
redb examples/arrays.redb -o /tmp/arrays && /tmp/arrays
redb examples/algorithm_demo.redb -o /tmp/algorithm_demo && /tmp/algorithm_demo
redb examples/queue_demo.redb -o /tmp/queue_demo && /tmp/queue_demo
redb examples/math_demo.redb -o /tmp/math_demo && /tmp/math_demo
redb examples/imports/app.redb -o /tmp/app && /tmp/app
```

---

## 16. Editor Support

Vim and Neovim support lives in `editors/vim`:

- `ftdetect/redb.vim` — maps `*.redb` to `filetype=redb`
- `syntax/redb.vim` — keywords, types, functions, numbers, strings, comments
  and operators
- `ftplugin/redb.vim` — `--` comment strings and `.redb` suffix handling
- `indent/redb.vim` — brace-based indentation

Install for Vim 8+:

```bash
mkdir -p ~/.vim/pack/redb/start
ln -s /path/to/redb/editors/vim ~/.vim/pack/redb/start/redb
```

Install for Neovim:

```bash
mkdir -p ~/.local/share/nvim/site/pack/redb/start
ln -s /path/to/redb/editors/vim ~/.local/share/nvim/site/pack/redb/start/redb
```

Or add it directly to `runtimepath`:

```vim
set runtimepath+=/path/to/redb/editors/vim
filetype plugin indent on
syntax on
```

Open any `.redb` file and check `:set filetype?`; it should report
`filetype=redb`.
