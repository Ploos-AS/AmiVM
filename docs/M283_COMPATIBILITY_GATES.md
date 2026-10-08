# M283 CPU and JIT compatibility gates

Goal: preserve high performance and high functional compatibility. Cycle-accurate Amiga hardware timing belongs to Amilea.

## Verification plan

1. Inventory existing CPU, FPU, IR, JIT and execution tests through M282.
2. Run matching deterministic instruction sequences through the reference backend and optimized execution engine.
3. Compare registers, PC, SR, stack pointers, exception state, MMU translations, FPU status, modified memory and visible device state at guest instruction boundaries.
4. Add regression fixtures for branches, flags, addressing modes, faults, interrupts, self-modifying code and JIT cache invalidation.
5. Report unsupported instructions and fallback paths explicitly.
6. Gate changes on correctness tests and independently track performance benchmarks.

This is a proposed test plan, not evidence that these tests have passed. v1.0 excludes accelerators, Apollo/Vampire and third-party compatible machines.
