// AgentC FFI Test — C Interop via @ffi("c")
// Tests external linking against libc symbols

#[ #>"C FFI: puts" ]
@ffi("c") ƒ puts(s: str) -> i32;

#[ #>"FFI test entry" ]
ƒ main() -> i64 {
    puts("Hello from C puts() via AgentC FFI!");
    0i
}
