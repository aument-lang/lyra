# Lyra

**Lyra** is the Lowered Intermediate Representation for Aument a high-level intermediate representation specialized for the Aument programming language.

Aument translates source code directly into an in-memory bytecode format suitable for interpretation. The compiler compiles this bytecode format into C. Lyra is intended to be an intermediate layer after bytecode translation: Aument bytecode will be compiled into Lyra, which will be optimized and ultimately be converted into C code.

## Optimizations

*unticked boxes mean unimplemented*

  * [x] Type inference
  * [x] Constant propagation
  * [x] Remove dead instructions
  * [ ] Generating reference count operations
  * [ ] Specialize dynamic functions
