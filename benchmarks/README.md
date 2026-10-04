# AmiVM Powerload Suite

The Powerload Suite measures real AmiVM workloads rather than only synthetic CPU throughput.

## Workload IDs

- cpu.integer
- cpu.floating
- cpu.memory
- amiga.scene.copper
- amiga.scene.blitter
- render.raytrace
- scene.generation
- build.amiga
- build.m68k-linux
- build.m68k-netbsd

## Modes

- FAST — maximum throughput.
- DETERMINISTIC — reproducible CI and qualification.
- DEBUG — instrumentation and diagnosis.

Each benchmark should report workload ID, mode, wall-clock time, throughput, host, VM configuration, and reproducibility information.

The initial suite is intentionally small. New workloads should be added when they represent a meaningful real-world bottleneck or regression risk.
