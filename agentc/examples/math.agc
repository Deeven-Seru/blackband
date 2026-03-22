// math.agc
// Demonstrates simple arithmetic and mutable state tracking.

+> io::wr;
+> val::tru;

#[
    #>"Math and mutable state demo"
    #!(io)
    #&(fs)
]
ƒ run_agent() -> ()?AgtE {
    // Immutable constants
    $ a: i32 = 6i;
    $ b: i32 = 7i;

    // Mutable accumulator
    ~ result: i32 = 0i;

    result = a + b;         // 13
    result = result * 2i;   // 26
    result = result - 6i;   // 20

    // Write result to file
    $ out: Tru<Str> = val::tru("result=20")?;
    io::wr("math_result.txt", out)?;

    ^(())
}
