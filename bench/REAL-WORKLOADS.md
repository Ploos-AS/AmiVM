# AmiVM real-software Powerloads

The AmiVM product is the VM/emulator. The MAX target is defined by real guest software, not by synthetic microbenchmarks.

## Priority order

1. AmigaOS software
2. m68k Linux software
3. m68k NetBSD software

## Required workload classes

### AmigaOS

- compiler/linker builds
- raytracing
- scenery/demo generation
- software rendering
- image/audio processing
- compression

### m68k Linux

- native compilation
- userspace builds
- compute
- rendering/raytracing
- compression

### m68k NetBSD

- native compilation
- ports/build workloads
- compute
- compression
- representative userspace programs

Every real workload should have a guest artifact, an execution command, a correctness artifact, and a measurement definition. The same workload should be runnable in FAST mode and, where useful, DETERMINISTIC mode.

Microbenchmarks remain valuable for finding regressions in individual execution-engine components, but they do not define the AmiVM MAX score.
