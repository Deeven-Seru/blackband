<p align="center">
  <strong>AgentC</strong><br>
  <em>A compiled, type-safe language for building verifiable AI agents.</em>
</p>

<p align="center">
  <code>Lexer → Parser → TypeChecker → LLVM → Native Binary</code>
</p>

---

AgentC is a domain-specific language that compiles to native code via LLVM. It is designed for one purpose: building AI agents whose safety properties are enforced at compile time, not at runtime.

The compiler treats intent declarations, trust boundaries, token budgets, and side-effect tracking as first-class citizens in the type system. Code that violates these constraints does not compile.

```agentc
+> net::get;
+> val::tru;
+> ctx::infer;

#[
  #>"Fetch and summarize a webpage"
  #$(cost: 500)
  #!(net|llm)
]
ƒ summarize(url: Str<256>) -> Tru<Str>?AgtE {
    $ raw: Utr<Str> = net::get(url)?;
    $ safe: Tru<Str> = val::tru(raw)?;
    $ result: Utr<Str> = ctx::infer(safe)?;
    $ verified: Tru<Str> = val::tru(result)?;
    ^(verified)
}
```

---

## Building from Source

### Prerequisites

- macOS (ARM64) or Linux
- LLVM 17+ (`brew install llvm`)
- libcurl (`brew install curl`)
- C++20 compiler (clang++ from LLVM)

### Compile the Compiler

```sh
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"

clang++ -O2 -std=c++20 -fexceptions \
  $(llvm-config --cxxflags | sed 's/-fno-exceptions//g') \
  src/main.cpp src/lexer/lexer.cpp src/lexer/token.cpp \
  src/parser/ast.cpp src/parser/parser.cpp \
  src/typechecker/typechecker.cpp src/typechecker/typeenv.cpp \
  src/typechecker/rules.cpp src/codegen/codegen.cpp \
  src/toolchain/repl.cpp src/toolchain/debugger.cpp \
  src/toolchain/pkg.cpp runtime/agentc_runtime.cpp \
  runtime/modules/io.cpp runtime/modules/net.cpp \
  runtime/modules/ctx.cpp runtime/modules/val.cpp \
  runtime/modules/mem.cpp runtime/modules/trc.cpp \
  $(llvm-config --ldflags) $(llvm-config --libs core) \
  -lcurl -lstdc++ -o agentc
```

### Run a Program

```sh
./agentc examples/hello.agc        # Compiles to a.out
./a.out                             # Runs the binary
```

### Other Modes

```sh
./agentc repl                       # Interactive REPL
./agentc debug examples/hello.agc   # Step-through debugger
./agentc get <git-url>              # Fetch remote package
```

---

## Language Reference

### Functions

Functions are declared with `ƒ` (U+0192). Return values are lifted with `^()`.

```agentc
ƒ add(a: i32, b: i32) -> i32 {
    ^(a + b)
}
```

### Bindings

| Sigil | Meaning | Example |
|-------|---------|---------|
| `$` | Immutable binding | `$ x: i32 = 42i;` |
| `~` | Mutable binding | `~ count: i32 = 0i;` |
| `:=` | Type inference | `$ x := 42i;` |

### Primitive Types

| Type | Description |
|------|-------------|
| `i8` `i16` `i32` `i64` `i128` | Signed integers |
| `u8` `u16` `u32` `u64` `u128` | Unsigned integers |
| `f32` `f64` | Floating point |
| `B` | Boolean (`1b` / `0b`) |
| `str` / `Str<N>` | String / bounded string |
| `()` | Unit |
| `!` | Never |

Integer literals require a suffix: `42i`, `25u`, `3.14f`.

### Semantic Types

| Type | Purpose |
|------|---------|
| `Tru<T>` | Validated, trusted data |
| `Utr<T>` | Untrusted data (all external input) |
| `Aim<T>` | Intent-bound value |
| `Cst<T>` | Cost-tracked value |
| `Cnf<T>` | Confidence-scored value |
| `Bnd<T>` | Bounded value |
| `Own<T>` `Ref<T>` `Shr<T>` | Ownership types |
| `Opt<T>` | Optional |
| `T?E` | Result type |
| `Lst<T>` `Map<K,V>` | Collections |
| `Ch<T>` | Typed channel |

### Structs and Enums

```agentc
Dat User {
    id:   u64;
    name: Str<128>;
    age:  u8;
}

Enm Status {
    Ok,
    Warn,
    Fail(Str<256>),
}
```

### Control Flow

| Syntax | Meaning |
|--------|---------|
| `?(cond) { ... } : { ... }` | If / else |
| `>>(expr) { arms }` | Pattern match |
| `@iter var <- expr` | For-in loop |
| `^(expr)` | Return |
| `expr?` | Error propagation |

### Annotations

Annotations are compiler-enforced metadata. Every function requires at minimum `#>"intent"`.

```agentc
#[
  #>"Describe what this function does"
  #$(cost: tokens:1000|latency:500)
  #!(net|io|llm)
  #&(net|fs)
  @verify(post: result > 0)
  @conf(0.95)
  @retry(3:100)
]
ƒ my_function() -> ()?AgtE { ... }
```

| Annotation | Purpose |
|------------|---------|
| `#>"..."` | Intent declaration (mandatory) |
| `#$(...)` | Resource budget |
| `#!(...)` | Side-effect declaration |
| `#&(...)` | Required capabilities |
| `@verify(...)` | Runtime contract |
| `@conf(N)` | Confidence threshold |
| `@retry(N:ms)` | Retry with backoff |
| `@goal("...")` | High-level objective |
| `@ffi("c")` | C foreign function interface |

### C FFI

Call any C function by declaring it with `@ffi("c")`:

```agentc
@ffi("c") ƒ puts(s: str) -> i32;
@ffi("c") ƒ sqrt(x: f64) -> f64;
```

The compiler automatically handles type coercion (AgentC fat strings → C `char*`).

### Standard Library

| Module | Functions | Description |
|--------|-----------|-------------|
| `io` | `rd` `wr` `ex` `del` `ls` | Filesystem operations |
| `net` | `get` `post` | HTTP via libcurl |
| `val` | `tru` `san` `bnd` `vch` | Trust validation |
| `ctx` | `infer` `embed` | LLM inference and embeddings |
| `trc` | trace logging | Execution tracing |
| `mem` | memory ops | Memory tier management |

### Concurrency

```agentc
$ ch: Ch<Str> = Ch::new();
@spn Worker;
@snd(ch, "task");
$ result := @rcv(ch);
@syn!;

@par {
    a: fetch_data(),
    b: process_queue(),
}
```

---

## Project Structure

```
agentc/
├── src/
│   ├── lexer/          # Tokenizer (lexer.cpp, token.hpp)
│   ├── parser/         # AST builder (parser.cpp, ast.hpp)
│   ├── typechecker/    # Semantic analysis (typechecker.cpp, rules.cpp)
│   ├── codegen/        # LLVM IR generator (codegen.cpp)
│   ├── toolchain/      # REPL, debugger, package manager
│   ├── lsp/            # Language Server Protocol implementation
│   └── main.cpp        # CLI entry point
├── runtime/
│   ├── agentc_runtime.cpp
│   └── modules/        # io, net, ctx, val, mem, trc
├── examples/           # 16 example programs
├── tests/              # Test suite
├── vscode-agentc/      # VS Code extension
└── CMakeLists.txt
```

### Compiler Pipeline

```
Source (.agc) → Lexer → Token Stream → Parser → AST → TypeChecker → LLVM IR → Native Binary
```

All errors are emitted as structured JSON:

```json
{
  "err": "E201",
  "cat": "Semantic",
  "loc": [3, 5],
  "why": { "exp": "Immutable binding", "got": "Reassignment" },
  "fix": { "act": "Use ~ for mutable binding" }
}
```

---

## For AI Agents

This section provides context for AI coding agents operating on this codebase.

### Key Facts

- **Language**: C++20, compiled with clang++ against LLVM 22
- **Build**: Single-command compilation (no build system required for dev)
- **Test**: `./agentc examples/<file>.agc` compiles to `a.out`, then `./a.out` runs it
- **Spec**: `a.md` in the project root is the canonical language specification

### Code Conventions

- Function keyword is `ƒ` (U+0192), not `fn`
- All token types are in `src/lexer/token.hpp` as `Token::Kind` enum
- AST nodes are in `src/parser/ast.hpp`
- Parser is recursive descent in `src/parser/parser.cpp`
- LLVM codegen is in `src/codegen/codegen.cpp`
- Runtime C++ implementations back the stdlib in `runtime/modules/`
- Errors use JSON format with codes (E1xx = parse, E2xx = type, E8xx = semantic)

### Adding a New Feature

1. Add token(s) to `src/lexer/token.hpp` enum
2. Add lexer rules to `src/lexer/lexer.cpp`
3. Add MATCH entries to `src/lexer/token.cpp`
4. Add AST node fields to `src/parser/ast.hpp`
5. Add parsing logic to `src/parser/parser.cpp`
6. Add type rules to `src/typechecker/rules.cpp`
7. Add codegen to `src/codegen/codegen.cpp`
8. Add runtime backing to `runtime/modules/` if needed

### Common Pitfalls

- The `AnnotationNode::value` is a `std::variant`, not `std::any` — the typechecker uses `std::get<>` on it
- `FnNode::body` is a `StmtNode` (value type), not a pointer — use `StmtNode::Kind::Block` for empty bodies
- External FFI functions set `is_external = true` and skip `gen_fn_body` in codegen
- String type in LLVM is `{ptr, i64}` — C FFI auto-coerces to raw `ptr` via `ExtractValue`

---

## License

Proprietary. All rights reserved.
