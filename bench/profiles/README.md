# Powerload profiles

AmiVM has one primary performance target: MAX.

Workloads do not get separate performance profiles. CPU, Amiga scene, raytracing, scene generation, Amiga builds, m68k Linux builds, and m68k NetBSD builds all compete against the same goal: maximum correct throughput.

The reference configuration exists for correctness qualification and semantic comparison. It is not the performance target.

The MAX target is represented by max.toml. Backend/configuration experiments may be added later, but they are implementation candidates, not separate workload performance targets.
