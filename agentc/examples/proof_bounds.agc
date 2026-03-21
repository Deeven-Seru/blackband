// proof_bounds.agc
// PROOF: Range bounds must be structurally enforced via val module.
#[ #>"Proof of Bounds" #!(none) #&(none) ]
ƒ run_agent() -> ()?AgtE {
    $ raw: i32 = 200i;
    
    // ATTEMPT TO VIOLATE: Assing raw unbound int to bounded capacity cleanly.
    $ safe_age: Bnd<i32, 0, 120> = raw;
    ^(())
}
