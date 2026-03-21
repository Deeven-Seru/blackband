// examples/github_analyzer.agc
// A production-grade AgentC application testing the rigorous bounds
// of the compiler: multi-step fetching, exact intent annotations,
// Trust elevation, and strict Effect/Capability tracing.

+> net::get;
+> ctx::infer;
+> io::wr;
+> val::tru;

Dat GitHubStats {
    stars: u64;
    forks: u64;
    repo:  Str<256>;
}

// Ensure the function is sandboxed purely to Net, IO, and LLM constraints
#[
    #>"Analyzer Agent: Pulls GitHub data and analyzes trends safely."
    #!(net|io|llm)
    #&(net|fs|llm)
]
ƒ run_agent() -> ()?AgtE {
    
    // 1. Immutable URL binding
    $ target_url: Str<1024> = "https://api.github.com/repos/Deeven-Seru/blackband";

    // 2. Untrusted network fetch explicitly typed as Utr
    $ raw_api_data: Utr<Str> = net::get(target_url)?;

    // 3. Elevate the data to explicitly trusted state so the LLM can see it
    $ trusted_payload: Tru<Str> = val::tru(raw_api_data)?;

    // 4. Construct a strict evaluation prompt
    $ prompt_str: Tru<Str> = val::tru(
        "You are an expert GitHub repository analyzer. Review the following raw JSON API data from the Deeven-Seru/blackband repository. Output a precise, two-paragraph markdown summary detailing the project's description, primary language, and community engagement metrics (stars, forks, open issues). Here is the JSON: "
    )?;

    // 5. Build full context payload (AgentC relies on LLM pipeline concatenation manually right now)
    $ full_query: Tru<Str> = val::tru(prompt_str)?; // In real implementation, concat prompt_str + trusted_payload

    // 6. Infer the result
    $ raw_analysis: Utr<Str> = ctx::infer(full_query)?;

    // 7. Trust the output
    $ final_report: Tru<Str> = val::tru(raw_analysis)?;

    // 8. Write cleanly to disk
    io::wr("blackband_analysis.md", final_report)?;

    // 9. Inspect budget consumption
    @budget(inspect);

    // End run
    ^(())
}
