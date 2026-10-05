# AmiVM Powerloads

AmiVM is an Amiga VM/emulator. **MAX performance is the primary objective.**

Powerloads are measurements used to optimize and qualify the VM; they are not the product themselves.

## Priority

1. Real AmigaOS workloads.
2. Real m68k Linux workloads.
3. Real m68k NetBSD workloads.
4. Deterministic microbenchmarks for CPU/execution-engine regression testing.

## AmigaOS powerloads

- compiler and linker builds
- raytracing
- scenery/demo generation
- image/audio processing
- compression
- filesystem-heavy workloads
- CPU-intensive applications

## m68k Linux powerloads

- native compilation
- userspace builds
- compute workloads
- rendering/raytracing
- compression

## m68k NetBSD powerloads

- native compilation
- ports/build workloads
- compute
- compression
- representative userspace workloads

Microbenchmarks such as cpu.integer and the current deterministic raytrace test remain useful as fast regression tests, but must not be mistaken for the primary definition of AmiVM performance.

The goal is to make real software run as fast as possible on AmiVM.
