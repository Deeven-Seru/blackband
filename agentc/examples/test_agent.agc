+> ctx::infer;
+> val::san;
+> io::wr;
+> net::get;

#[
  #>"gemini api end-to-end validation"
  #$(net|io|llm:4000)
  #!(net|io)
  #&(net|fs|llm)
]
ƒ run_agent() -> ()?AgtE {
    $ raw: Utr<Str> = net::get("https://httpbin.org/json")?;
    $ clean: Tru<Str> = val::san(raw)?;
    $ prompt: Tru<Str> = val::san("Reply with exactly this text and nothing else: AGENTC_WORKS")?;
    $ answer: Utr<Str> = ctx::infer(prompt)?;
    $ trusted: Tru<Str> = val::san(answer)?;
    io::wr("test_output.txt", trusted)?;
    ^(())
}
