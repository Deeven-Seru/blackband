+> net::get;
+> ctx::infer;
+> io::wr;
+> val::tru;

#[ #>"Summarize JSON payload" #!(net|io|llm) #&(net|fs|llm) ]
ƒ run_agent() -> ()?AgtE {
    // Step 1: Fetch data from a network endpoint
    $ raw: Utr<Str> = net::get("https://httpbin.org/json")?;

    // Step 2: Elevate trust and call Gemini LLM
    $ prompt: Tru<Str> = val::tru(raw)?;
    $ raw_sum: Utr<Str> = ctx::infer(prompt)?;

    // Step 3: Write trusted result to a file
    $ summary: Tru<Str> = val::tru(raw_sum)?;
    io::wr("summary_agentc.txt", summary)?;

    ^(())
}
