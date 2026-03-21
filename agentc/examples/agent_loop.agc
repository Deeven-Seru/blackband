+> ctx::infer;
+> val::tru;

#[
  #>"asks LLM a question with self-correction on failure"
  #$(llm:2000)
  #!(none)
  #&(llm)
  #*(3)
  #?(0.8)
]
ƒ ask($ question: Tru<Str>) -> Tru<Str>?AgtE {
    $ raw: Utr<Str> = ctx::infer(question)?;
    $ answer: Tru<Str> = val::tru(raw)?;
    ^(answer)
}
