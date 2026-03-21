// proof_immutability.agc
// PROOF: Strict Lexical Immutability via $
#[ #>"Proof of Immutability" #!(none) #&(none) ]
ƒ run_agent() -> ()?AgtE {
    $ age: i32 = 10i;
    // ATTEMPT TO VIOLATE: Reassign an immutable $ binding.
    age = 20i;
    ^(())
}
