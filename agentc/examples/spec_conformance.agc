// AgentC Spec Conformance Test
// Exercises EVERY construct from a.md

+> io::wr;
+> net::get;
+> val::tru;
+> ctx::infer;

// Struct — Dat with ; field separators
Dat User {
    id:   u64;
    name: Str<128>;
    age:  u8;
}

// Enum — Enm with variants
Enm Status {
    Ok,
    Warn,
    Fail,
}

Enm NetResult {
    Ok(Str<4096>),
    Timeout(u32),
    AuthErr,
}

// Main agent function exercising all binding forms
#[
  #>"Spec conformance test agent"
  #!(net|io|llm)
  #&(net|fs|llm)
]
ƒ run_agent() -> ()?AgtE {
    // Typed immutable binding
    $ name: Str<64> = "AgentC";

    // Mutable typed binding
    ~ count: i32 = 0i;
    count = count + 1i;

    // Local type inference — :=
    $ x := 42i;
    ~ y := "hello";

    // Boolean type B with 1b/0b literals
    $ flag: B = 1b;
    $ off:  B = 0b;

    // Unsigned literals
    $ age: u64 = 25u;

    // Network — arrives as Utr<Str>
    $ raw: Utr<Str> = net::get("https://httpbin.org/json")?;

    // Trust elevation via val::tru
    $ safe: Tru<Str> = val::tru(raw)?;

    // LLM inference
    $ prompt: Tru<Str> = val::tru("Respond with: SPEC_PASS")?;
    $ answer: Utr<Str> = ctx::infer(prompt)?;
    $ trusted: Tru<Str> = val::tru(answer)?;

    // Write result
    io::wr("spec_output.txt", trusted)?;

    // Budget tracking
    @budget(inspect);

    // Parallel execution with named fields
    @par {
        a: io::wr("log_a.txt", safe),
        b: io::wr("log_b.txt", trusted),
    }

    ^(())
}
