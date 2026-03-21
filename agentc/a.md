# AGENTC DOCS — USE THIS AS SOURCE OF TRUTH

A complete, accurate reference guide to write your first agent in 30 minutes.

---

## 1. Imports
Use `+>` to import stdlib modules. All external data from imported modules arrives as `Utr<T>` by default.

```agentc
+> io::rd;
+> net::get;
+> val::tru;
+> ctx::infer;
```

---

## 2. Bindings & Local Type Inference
### Bindings — `$` (immutable) and `~` (mutable)
`$` declares an immutable binding — it can never be re-assigned. `~` declares a mutable binding. The compiler enforces this — reassigning `$` produces E201.

```agentc
$ name: Str<64> = "AgentC";    // immutable — locked forever
~ count: i32    = 0i;          // mutable — can change
count = count + 1i;            // ok — count is ~
// name  = "other";            // ERROR E201 — name is $
```

### Local type inference — `:=`
Use `:=` for local bindings where type is obvious. Never at function signatures, struct fields, or module boundaries.

```agentc
$ x := 42i;          // inferred as i32 — ok in local scope
~ y := "hello";      // inferred as Str — ok in local scope
```

---

## 3. Core & Semantic Types
AgentC implements strict primitives and natively validates advanced semantic boundaries:

- **Boolean**: `B` (Capital `B`). Literals are `1b` (true) and `0b` (false). `true`/`false` keywords are invalid.
- **Results**: Represented as fused `T?E` without spaces between types (e.g., `Str?AgtE`).
- **Semantic Constructs**:
  - `Cnf<T,N>`: Confident type, guaranteeing an enforced confidence score of `N`.
  - `Bnd<T,L,H>`: Bounded type restricting values of `T` rigorously between low `L` and high `H`.
  - `Aim<T>`: Represents a directed objective intent natively targeting `T`.

---

## 4. Structs — Dat
`Dat` defines data structures. Mutability applies at the binding site, not the field level.

```agentc
Dat User {
    id:   u64;
    name: Str<128>;
    age:  u8;
}

// Mutability at binding site — not field level
$ user: User = User { id: 1u, name: "Deeven", age: 25u };
~ editable: User = User { id: 2u, name: "Agent", age: 1u };
editable.name = "AgentC";        // ok — editable is ~
```

---

## 5. Enums — Enm
`Enm` declares algebraic data types. Valid values are variants.

```agentc
Enm Status {
    Ok,
    Warn,
    Fail,
}

Enm NetResult {
    Ok(Str<4096>),     // parameterized variant
    Timeout(u32),      // carries ms value
    AuthErr,
}
```

---

## 6. Annotations & The Conflict Matrix
AgentC uses a highly distinct symbol system for metadata bounding blocks:
- Overarching symbols: `#[ ... ]` 
- Intent Definition: `#>"..."`
- Cost Definition: `#$(...)`

### Complete Annotation Conflict Matrix
To ensure deterministic predictability, AgentC enforces strict annotation overlap rules:
| Annotation A | Annotation B | Allowed? | Rule Enforcement |
| --- | --- | --- | --- |
| `#>"intent"` | `#$(cost)` | **Yes** | Standard LLM bound configuration |
| `@mem(ext)` | `#$(cost)` | **Yes** | External pulls cost bounded |
| `@correct{}` | `#$(cost)` | **No**  | Autonomous repair loops cannot bypass overarching bounds; must be bound implicitly |
| `@par` | `#>"intent"` | **No** | Intents are thread-unsafe when multiplexed; must bind intent per-route |

---

## 7. Execution, Self-Correction & Tracing
Functions are declared using `ƒ` with `^()` to lift returns. The `?` token trails to propagate errors.

### `@correct{}` Self-Correction with Repair
AgentC embeds an autonomous self-healing LLM retry block `(repair: closure)`.
```agentc
ƒ parse_payload() -> Dat?AgtE {
    @correct {
        $ res := ctx::infer("Extract data")?;
    }(repair: |err| {
        io::wr("log.txt", "Retrying chunk...");
    })
    ^()
}
```

### Parallel Execution 
Execution occurs concurrently across named fields via `@par`.
```agentc
@par {
    a: act1(),
    b: act2(),
}
```

---

## 8. Capability and Trust Rules (val)
Data crossing bounds is purely strictly un-aligned untrusted (`Utr<T>`). Agents may only exchange logic over verified aligned boundaries (`Tru<T>`). Capability bounds define exactly who handles what.
- A trusted agent handling pure deterministic logic can only accept `Ch<Tru<T>>`.
- Unsafe web/LLM streams emit `Ch<Utr<T>>`.

```agentc
$ input: Utr<Str> = net::get("https://...")?;
$ safe: Tru<Str>  = val::tru(input)?;
```

---

## 9. Context, Memory Tiers, and Budgeting
State boundaries are explicitly sandboxed via intrinsic directives.

### 4 Memory Tiers
Data caching dynamically allocates against context thresholds using exactly four levels:
- `@mem(work)`: Ephemeral fast-scratch workspace.
- `@mem(ep)`: Episodic contextual cache natively optimized.
- `@mem(sem)`: Semantic long-term vector-like knowledge constraints.
- `@mem(ext)`: External RAG bounding interfaces.

### Budget Tracking Primitives
`@budget()` provides inline tracking primitives measuring limits defined by `#$(cost)`.
```agentc
#>"Research target"
#$(cost: 500)
ƒ do_task() -> ()?AgtE {
    @budget(inspect);
    @mem(work).write("started");
    ^()
}
```

---

## 10. Multi-Agent System Primitives
Multi-agent concurrency runs on tightly bounded primitives communicating via `Ch<T>` topologies securely enforcing the `Utr`/`Tru` trust matrices.

- `Ch<T>`: Typed agent communication channel.
- `@spn`: Spawn agent instance.
- `@snd`: Send message payload natively.
- `@rcv`: Receive message conditionally.
- `@syn`: Synchronize logic block gracefully.
- `@syn!`: Hard synchronization (panic bound propagation constraint).

```agentc
$ ch: Ch<Str> = Ch::new();
@spn Worker;
@snd(ch, "job");
$ res := @rcv(ch);
@syn!;
```
