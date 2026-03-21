// proof_cap.agc
// PROOF: Missing capability bounds block standard library execution.
+> net::get;

#[ #>"Proof of Capabilities" #!(net) #&(fs) ]  // Has fs capability, missing net capability
ƒ run_agent() -> ()?AgtE {
    // ATTEMPT TO VIOLATE: Call a network construct without `#&(net)` cap.
    // If successful, this bypassing the agent sandboxing matrix.
    $ raw: Utr<Str> = net::get("https://httpbin.org/json")?;
    ^(())
}
