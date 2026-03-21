// proof_purity.agc
// PROOF: Pure functions cannot execute impure side-effects.
+> net::get;

#[ #>"Proof of Purity" #!(none) #&(net) ]
ƒ run_agent() -> ()?AgtE {
    // ATTEMPT TO VIOLATE: Call a network function inside a `#!(none)` pure function.
    // If successful, execution state is indeterministic.
    $ raw: Utr<Str> = net::get("https://httpbin.org/json")?;
    ^(())
}
