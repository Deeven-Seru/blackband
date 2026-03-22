// snapshot_example.agc
// Demonstrates snapshot/restore for speculative execution and budget rollback.
//
// Scenario: checkpoint agent state before a speculative LLM call; if the call
// succeeds, commit by writing the result to disk; the @restore keyword would
// roll everything back (heap allocations, budget counters, trace ops) had the
// agent chosen to discard the work instead.

+> ctx::infer;
+> val::tru;
+> io::wr;

#[
    #>"Speculative LLM execution with snapshot/restore rollback"
    #$(cost: tokens:2000|latency:5000)
    #!(io|llm)
    #&(fs|llm)
]
ƒ run_agent() -> ()?AgtE {

    // Step 1 — take a checkpoint before any speculative work
    @snapshot;

    // Step 2 — attempt speculative LLM inference
    $ prompt: Tru<Str> = val::tru(
        "Reply with exactly: SPECULATIVE_OK"
    )?;

    $ result: Utr<Str> = ctx::infer(prompt)?;

    // Step 3 — validate the model output (would @restore on error)
    $ trusted: Tru<Str> = val::tru(result)?;

    // Step 4 — commit: write result to disk (post-snapshot work is kept)
    io::wr("speculative_result.txt", trusted)?;

    // Step 5 — inspect budget after speculative work
    @budget(inspect);

    ^(())
}
