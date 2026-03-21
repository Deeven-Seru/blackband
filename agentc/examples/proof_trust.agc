// proof_trust.agc
// PROOF: Untrusted data cannot breach trusted sinks.
+> net::get;
+> io::wr;

#[ #>"Proof of Trust" #!(net|io) #&(net|fs) ]
ƒ run_agent() -> ()?AgtE {
    $ raw: Utr<Str> = net::get("https://httpbin.org/json")?;
    // ATTEMPT TO VIOLATE: Pass untrusted data directly to disk
    // If successful, this is an arbitrary injection vulnerability.
    io::wr("hacked.txt", raw)?;
    ^(())
}
