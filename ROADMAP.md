# AmiVM roadmap

## M0 — Architecture & feasibility — COMPLETE

- Define project scope and non-goals.
- Define the initial `Hyper/040` machine contract.
- Define 68020/68030/68040/68060 guest CPU profiles over one shared execution architecture, with Hyper/040 and Hyper/060 performance profiles.
- Define the future `Compatibility` profile boundary.
- Compare ARAnyM, UAE/Amiberry, QEMU/m68k and related design approaches.
- Select CPU execution strategy and host architecture priorities.
- Define guest physical memory layout principles.
- Define paravirtual device model and discovery mechanism.
- Define boot contract for Linux/m68k first bring-up.
- Define AmigaOS/AROS compatibility strategy.
- Establish repository structure, build system and CI baseline.
- Define FS-UAE configuration compatibility as a machine-profile import contract.
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

## M2 — 68k execution — IN PROGRESS

Implementation has progressed through the internal M2.79 development sequence. JIT/dynarec host qualification is active on both tier-1 host architectures. In particular, AArch64 executable-cache coherency and persistent JIT mapping synchronization are implemented and qualified, native x86-64/AArch64 CI paths have been introduced, and recent fixes have hardened JIT helper integration.

Completed or substantially implemented work must still be treated separately from the M2 exit contract below; M2 remains open until the complete 68040-class execution path required for guest OS bring-up is qualified.

- Integrate initial 68040-class execution backend.
- Define the internal CPU-backend API independently from the chosen implementation.
- Reset vector and first-instruction execution from the bootstrap region.
- Supervisor/user state transitions.
- Exceptions and interrupt injection.
- MMU support required for Linux/m68k.
- CPU-profile/MMU dispatch: 68040/Hyper040 is the qualified M2 baseline; 68030 and 68060 MMU backends remain scaffolded until their model-specific table-walk, status and exception semantics are qualified. The external 68851 path now has an independent descriptor walker and fault/protection qualification, but remains scaffolded until the configurable PMMU table geometry, root-pointer formats and PMMU instruction/control-register contract are implemented.
- FPU baseline.
- Interpreter/reference mode for debugging where practical.
- Begin x86-64 and AArch64 JIT/dynarec qualification. **IN PROGRESS** — both tier-1 hosts are now exercised; AArch64 cache-coherency work is included.
- FS-UAE `.conf` import (`--config`) with explicit speed-policy separation, `--config-report` and `--strict-config`. **IN PROGRESS** — importer, model mapping and qualification test added.

### M2 exit criteria

M2 is complete when the Hyper/040 machine can execute a deterministic 68040-class bootstrap through the CPU-backend abstraction and the execution path has qualified reset/startup, supervisor/user transitions, exceptions and interrupt injection, the MMU functionality required for Linux/m68k bring-up, and the FPU baseline. Tier-1 host execution must be qualified on x86-64 and AArch64, with an interpreter/reference path retained where practical for differential debugging.

The immediate handoff to M3 is a guest-boot harness capable of loading the Linux/m68k bring-up payload and producing deterministic serial/early-console evidence.

## Cross-milestone qualification contracts

Two workload contracts apply across all later milestones and are release-significant:

- **Build/CI contract:** Linux/m68k must become capable of reproducible native package builds and test execution. Qualification must include a 68020 guest-visible baseline plus 68030/68040/68060 profiles as those contracts mature. Hyper profiles may be used for maximum-throughput builders.
- **Amiga workstation contract:** AmigaOS/AROS must be able to exploit high CPU/FPU throughput, large memory and low-overhead virtual I/O. Representative heavy workloads include Vista/VistaPro and Scenery Animator where software/test assets are legally available.

Performance work must measure these contracts directly. Cycle accuracy and historical chipset fidelity are not performance goals for Hyper profiles.

## M3 — 68k guest OS bring-up

Linux/m68k is the first bring-up guest, but the machine and device contracts must remain OS-neutral so BSD and Amiga-family guests can follow without redesigning Hyper/040.

- Define an OS-neutral Hyper/040 machine and boot contract.
- Linux/m68k kernel command line and initrd handoff.
- `vmserial` Linux driver or early-console equivalent.
- Timer and interrupt support.
- First Linux kernel boot to early userspace.
- Integrate AmiVM as an m68kDeb runtime and CI target.
- Boot and qualify Linux/m68k under an explicit 68020-compatible CPU profile plus the required external-MMU contract as the minimum build/test target; do not imply that a bare 68020 contains an MMU.
- Add 68030, 68040 and 68060 Linux/m68k qualification profiles as CPU support matures.
- Provide Hyper/040 and later Hyper/060 builder profiles for maximum native package-build throughput.
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

Performance success is defined by useful work completed, not by reproducing physical 68k timing. The two headline benchmark families are native m68k build throughput and heavy Amiga workstation throughput.

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


## M10 — ColdFire architecture and Linux qualification

ColdFire is a sister architecture to classic m68k in AmiVM, not an Amiga CPU mode. ColdFire support must not weaken or delay the primary 68020 → 68030 → 68040 → 68060, Linux/m68k, AmigaOS/AROS and Hyper-profile roadmap. Earlier CPU/backend interfaces should nevertheless avoid assumptions that would unnecessarily prevent a later ColdFire backend.

### CF1 — ColdFire ISA core

- Add a distinct ColdFire CPU-family/backend contract.
- Implement the initial V2 instruction, register, exception and interrupt model.
- Keep classic-m68k and ColdFire feature/exception semantics explicitly separated.
- Extend deterministic CPU tests and trace evidence for ColdFire.

### CF2 — MCF5208-compatible qualification profile

- Add an MCF5208-compatible machine/CPU profile sufficient for Linux/uClinux bring-up.
- Use QEMU's M5208EVB support as a differential reference where behavior is comparable.
- Add differential tests for registers, condition codes, exceptions, memory effects and representative instruction sequences.
- Boot the same qualified MCF5208 Linux/uClinux test image on QEMU and AmiVM where redistribution and configuration permit.
- Treat successful dual-platform boot with deterministic serial evidence as the CF2 headline acceptance test.

### CF3 — ColdFire V4e

- Add the V4e instruction-set extensions required by the selected target profile.
- Add/qualify the V4e FPU contract.
- Extend differential and architectural conformance testing.

### CF4 — ColdFire MMU and full Linux

- Implement the ColdFire MMU contract required by supported MMU-capable targets.
- Bring up full Linux/ColdFire rather than limiting AmiVM to no-MMU/uClinux guests.
- Add ColdFire Linux builds/tests to the CI qualification matrix.
- Integrate ColdFire as an optional m68kDeb build/test target where the distribution/toolchain work supports it.

### CF5 — High-performance ColdFire

- Extend the AmiVM JIT/dynarec architecture to ColdFire on x86-64 and AArch64.
- Reuse safe host-side execution, memory and paravirtual-I/O infrastructure without conflating guest ISA semantics.
- Benchmark native ColdFire Linux compilation and representative workloads.
- Track ColdFire interpreter/JIT differential regressions in CI.

### M10 exit criteria

M10 is complete when AmiVM exposes ColdFire as a first-class but separate CPU family, has a reproducibly qualified MCF5208-compatible Linux/uClinux path cross-checked against QEMU, supports the selected V4e/MMU Linux target, and exercises ColdFire interpreter/JIT correctness and build workloads in CI.
