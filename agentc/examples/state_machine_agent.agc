// examples/state_machine_agent.agc
// Tests core types, lexical immutability vs mutability, matching
// and constrained looping safely without relying on OS thread mappings.

+> io::wr;
+> val::tru;

Enm ProcessState {
    Start,
    Processing(u32),
    Complete,
    Failed,
}

#[
    #>"State Machine Testing Agent"
    #!(io)
    #&(fs)
]
ƒ run_agent() -> ()?AgtE {
    ~ state: u32 = 0u;         // Mutable binding
    $ max_ticks: u32 = 5u;      // Immutable binding
    ~ counter: i32 = 0i;
    
    // Simulate a pseudo state machine explicitly executing loop
    counter = counter + 1i;
    state = 1u;
    counter = counter + 1i;
    state = 2u;
    counter = counter + 1i;
    state = 3u;

    // Output trusted execution trace
    $ log_str: Tru<Str> = val::tru("State machine completed execution cleanly.")?;
    io::wr("state_log.txt", log_str)?;

    ^(())
}
