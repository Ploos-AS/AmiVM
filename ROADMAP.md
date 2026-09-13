# AmiVM roadmap

## M0 — Architecture & feasibility

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

### M0 exit criteria

M0 is complete when:

- the machine model is documented well enough to implement without redesigning fundamentals;
- the first CPU backend approach is selected;
- the first boot path is selected;
- virtual device discovery and interrupt delivery have a documented contract;
- Linux/m68k and Amiga-compatible guest paths are explicitly separated where required;
- the project has a minimal buildable host skeleton and CI smoke test.

## M1 — VM core skeleton

- Host executable and configuration parser.
- Guest physical address space.
- ROM/bootstrap region.
- RAM mapping.
- Interrupt controller/timer baseline.
- Serial console.
- Deterministic machine description dump.
- Unit tests for memory map and device registration.

## M2 — 68k execution

- Integrate initial 68040-class execution backend.
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
