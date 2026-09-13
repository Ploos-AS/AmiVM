# AmiVM roadmap

## M0 — Architecture & feasibility — COMPLETE

- Define project scope and non-goals.
- Define the initial `Hyper/040` machine contract.
- Define the future `Compatibility` profile boundary.
- Compare ARAnyM, UAE/Amiberry, QEMU/m68k and related design approaches.
- Select CPU execution strategy and host architecture priorities.
- Define guest physical memory layout principles.
- Define paravirtual device model and discovery mechanism.
- Define boot contract for Linux/m68k first bring-up.
- Define AmigaOS/AROS compatibility strategy.
- Establish repository structure, build system and CI baseline.
- Produce an M0 feasibility decision for implementation.

## M1 — VM core skeleton — COMPLETE

- Host executable and configuration parser (`--ram-mib`, `--rom`).
- Guest physical address space.
- 1 MiB ROM/bootstrap region at `0x00f00000`.
- contiguous RAM mapping from `0x10000000`.
- MMIO window at `0xff000000`.
- explicit device registry.
- interrupt-controller baseline.
- monotonic timer baseline.
- `vmserial` output/status registers.
- deterministic machine-description dump.
- unit tests for RAM boundaries, ROM protection, configuration, device registration, interrupts, timer and serial status.
- Release-build self-test suitable for CI.

### M1 exit criteria

M1 is complete when the host can instantiate the Hyper/040 machine model without a CPU backend, expose its deterministic physical map and registered devices, load a bootstrap ROM, exercise RAM/MMIO safely, and pass the VM-core test suite in CI.

## M2 — 68k execution

- Integrate initial 68040-class execution backend.
- Define the internal CPU-backend API independently from the chosen implementation.
- Reset vector and first-instruction execution from the bootstrap region.
- Supervisor/user state transitions.
- Exceptions and interrupt injection.
- MMU support required for Linux/m68k.
- FPU baseline.
- Interpreter/reference mode for debugging where practical.
- Begin x86-64 and AArch64 JIT/dynarec qualification.

## M3 — Linux/m68k bring-up

- Define Linux machine/boot contract.
- Kernel command line and initrd handoff.
- `vmserial` Linux driver or early-console equivalent.
- Timer and interrupt support.
- First kernel boot to early userspace.
- Integrate AmiVM as an m68kDeb runtime target.

## M4 — High-performance virtual I/O

- `vmblock` storage.
- `vmnet` Ethernet.
- shared filesystem transport.
- framebuffer/RTG-style graphics.
- host input.
- optional audio baseline.
- Measure and optimize copy, interrupt and context-switch overhead.

## M5 — Amiga-compatible guest support

- Compatibility bootstrap path.
- AmigaOS/AROS virtual-device bindings.
- FastRAM/RTG-oriented machine profile.
- Amiga-style device drivers for selected paravirtual devices.
- Determine the minimum legacy chipset surface required by supported guests.
- Add workstation qualification workloads such as rendering/animation software where legally available.

## M6 — Compatibility profile

- Add selected A4000-class conventions only where needed.
- Preserve separation between performance-critical Hyper mode and compatibility hardware.
- Qualify representative AmigaOS and AROS workloads.

## M7 — Performance engineering

- JIT optimization on x86-64.
- JIT optimization on AArch64.
- direct memory fast paths.
- batched/paravirtual I/O.
- reduced interrupt overhead.
- zero-copy opportunities.
- benchmark against Amiberry, FS-UAE, ARAnyM and QEMU/m68k where comparisons are meaningful.
- add CPU/rendering workstation benchmarks representative of Vista/VistaPro, Scenery Animator and similar applications when suitable test material is available.

## M8 — Developer and automation platform

- snapshots and restore.
- deterministic launch manifests.
- headless and visible modes.
- machine-readable monitor/control API.
- CI runner integration.
- tracing/profiling hooks.

## M9 — Release engineering

- stable machine ABI/versioning.
- guest driver packages.
- Linux/m68k reference images.
- Amiga-compatible driver bundles.
- reproducible release builds.
- amd64 and arm64 release artifacts.
