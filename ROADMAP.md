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

## M3 — 68k guest OS bring-up

Linux/m68k is the first bring-up guest, but the machine and device contracts must remain OS-neutral so BSD and Amiga-family guests can follow without redesigning Hyper/040.

- Define an OS-neutral Hyper/040 machine and boot contract.
- Linux/m68k kernel command line and initrd handoff.
- `vmserial` Linux driver or early-console equivalent.
- Timer and interrupt support.
- First Linux kernel boot to early userspace.
- Integrate AmiVM as an m68kDeb runtime target.
- Qualify native m68k GCC/binutils/make builds inside Linux/m68k and record a reproducible build-performance baseline.
- Bring up NetBSD/m68k on Hyper/040 and add native AmiVM device support where required.
- Bring up OpenBSD/m68k where the maintained port and machine requirements permit a practical AmiVM target.
- Bring up AROS/m68k using the Hyper machine where possible and Amiga-compatible bindings where required.
- Establish the AmigaOS 3.x bootstrap/device contract for the Compatibility profile.

### M3 target matrix

- **Tier 1:** Linux/m68k, NetBSD/m68k, AROS/m68k, AmigaOS 3.x.
- **Supported/qualified where practical:** OpenBSD/m68k, subject to the maintained port and its machine requirements.
- Guest-specific boot mechanisms and drivers must not force unrelated historical hardware into the Hyper profile.

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
- make native m68k compilation a primary performance benchmark, including clean GCC/binutils/make project builds and representative m68kDeb package builds.
- compare native AmiVM build throughput with representative physical/emulated 68k environments where meaningful.
- qualify Vista, VistaPro and Scenery Animator as named Amiga workstation workloads where legally available.
- record CPU/FPU rendering timings using reproducible scenes/workloads suitable for redistribution or documented user-supplied test data.
- track benchmark regressions in CI where licensing and runtime requirements permit.

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
- NetBSD/m68k reference/test images where redistribution permits.
- OpenBSD/m68k qualification recipes where applicable.
- AROS/m68k reference/test images.
- Amiga-compatible driver bundles.
- reproducible release builds.
- amd64 and arm64 release artifacts.
