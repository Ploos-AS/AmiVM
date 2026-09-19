# AmiVM architecture

## 1. Purpose

AmiVM is a performance-oriented 68k virtual machine with an Amiga-compatible personality.

The design intentionally separates **machine compatibility** from **performance-critical execution**. The primary machine, `Hyper/040`, is a clean virtual platform for modern 68k operating systems and workloads. A later `Compatibility` profile may expose additional Amiga conventions where required.

## 2. Reference machine: Hyper/040

The initial machine contract is intentionally small.

### CPU

- 68040-class architectural contract
- big-endian guest
- supervisor and user modes
- MMU required
- FPU required
- precise exception state sufficient for operating-system kernels
- execution backend may be interpreter, JIT or translated core as long as architectural behaviour is preserved

A future Hyper/060 profile may extend the model without changing the base device ABI unnecessarily.

### Host priorities

Tier 1:

- x86-64 Linux
- AArch64 Linux

Later hosts may be added after the guest ABI stabilizes.

## 3. Memory model

AmiVM does not reproduce historical Amiga RAM ceilings unless a compatibility profile explicitly requests them.

The M1 provisional physical map is:

| Region | Base | Initial size |
| --- | ---: | ---: |
| ROM/bootstrap | `0x00f00000` | 1 MiB |
| RAM | `0x10000000` | configurable, 128 MiB default |
| MMIO | `0xff000000` | device pages |
| `vmserial` | `0xff000000` | 4 KiB |
| timer | `0xff001000` | 4 KiB |
| interrupt controller | `0xff002000` | 4 KiB |

RAM is a contiguous host-backed region. ROM is readable but guest writes are rejected. The current host CLI can load a ROM/bootstrap image with `--rom PATH` and configure RAM with `--ram-mib N`.

These addresses are deterministic for M1 qualification, but remain provisional until the stable guest ABI milestone.

## 4. Device model

AmiVM devices use a simple versioned paravirtual bus rather than pretending to be historical Zorro, SCSI or Ethernet hardware.

M1 introduces an explicit host-side device registry. Each registered device currently has:

- stable logical name;
- MMIO base;
- MMIO region size;
- interrupt line.

The first registered devices are `vmserial`, `timer` and `irq`. Future devices will extend the descriptor with vendor/device identifiers, ABI version and feature bits before guest-driver ABI freeze.

The design should allow efficient descriptor-ring or shared-buffer I/O without requiring one guest trap per byte or sector.

Planned logical devices:

- `vmserial` — console/debug/control transport;
- `vmblock` — block storage;
- `vmnet` — networking;
- `vmgfx` — linear framebuffer/display transport;
- `vmfs` — host/shared filesystem transport;
- `vmaudio` — optional audio transport.

## 5. Interrupts and timing

Hyper/040 gets a virtual interrupt controller and monotonic timer rather than cycle-derived chipset timing.

M1 provides a 32-bit pending-interrupt bitmap plus explicit raise/clear operations and a monotonic 64-bit tick counter. This is intentionally a minimal host-side contract; M2 will connect interrupt injection to the CPU backend.

Goals:

- low interrupt overhead;
- deterministic interrupt injection;
- enough timer fidelity for Linux and Amiga-compatible OS scheduling;
- no requirement to emulate raster or CIA timing in Hyper mode.

Compatibility mode may later add legacy-facing timing surfaces separately.

## 6. Boot model

### Linux/m68k

Linux is the preferred first bring-up guest because it gives clear kernel-level validation of CPU, MMU, exceptions, interrupts, timers and I/O.

The boot contract should support:

- kernel image;
- optional initrd;
- command line;
- memory map;
- machine/device description;
- serial early console.

The final handoff format will be selected after validating what is least invasive for upstream or maintained Linux/m68k support.

### BSD guests

NetBSD/m68k is a Tier-1 guest target and should use the Hyper machine contracts where practical. OpenBSD/m68k is a supported target where its current m68k port and machine requirements permit practical bring-up. Guest-specific support should prefer native AmiVM drivers rather than changing Hyper into a historical machine model.

### AmigaOS / AROS

AROS/m68k and AmigaOS 3.x are Tier-1 guest targets. These guests are not required to use the Linux boot contract.

AmiVM will provide a separate compatibility/bootstrap layer and native guest drivers where necessary. The Hyper machine should not become coupled to undocumented historical hardware behaviour merely to make this path work.

CPU-heavy workstation applications are an explicit future use case for the Amiga-compatible path. That includes rendering, scenery generation and animation software where software availability permits qualification.

## 7. CPU execution strategy

M0/M1 do not mandate writing a 68k CPU core from scratch.

Preferred evaluation order:

1. identify a proven 68k execution core whose license and architecture permit reuse;
2. require MMU and FPU correctness suitable for Linux/m68k;
3. establish a reference/debug execution path;
4. prioritize JIT/dynamic translation on x86-64 and AArch64;
5. keep the CPU backend behind an internal interface so it can be replaced or supplemented.

M2 starts by defining that internal CPU-backend API, reset semantics and interrupt/exception boundary before coupling AmiVM to one implementation.

Raw benchmark speed is not sufficient if exception/MMU behaviour prevents modern kernels from running correctly.

### Native development workload

Fast native m68k development is a first-class AmiVM use case. Linux/m68k, m68kDeb, AROS/m68k and AmigaOS guests should be able to run native compilers, assemblers, linkers and build tools efficiently. Native GCC/binutils/make builds will therefore be used as both functional qualification and performance benchmarks. This complements cross-compilation: cross builds remain useful for throughput, while AmiVM provides fast execution in the actual target architecture for native builds, package construction and qualification.

### Classic workstation workload

The Amiga-compatible path explicitly targets CPU/FPU-intensive workstation applications. Vista, VistaPro and Scenery Animator are named qualification targets, subject to legal availability of the software and benchmark material. Reproducible rendering/scenery workloads should measure CPU/FPU execution, memory performance and storage behaviour without requiring cycle-exact chipset timing.

## 8. Lessons adopted from existing projects

### ARAnyM

Adopt:

- extended virtual 68k machine philosophy;
- performance over reproduction of unnecessary historical bottlenecks;
- host integration;
- large-memory workstation model.

Improve:

- explicitly version the machine/device ABI early;
- design Linux support as a first-class target;
- make x86-64 and AArch64 equally important;
- design CI, headless control and snapshots from the start;
- separate compatibility hardware from the fast VM core.

### UAE / Amiberry

Adopt:

- mature lessons about 68k execution and Amiga software expectations;
- RTG and high-performance memory concepts;
- proven host-side optimization ideas where licensing and architecture allow.

Avoid making full chipset emulation a prerequisite for Hyper mode.

### QEMU

Adopt:

- clean machine/device separation;
- versioned virtual hardware concepts;
- testable device models;
- automation-friendly operation.

AmiVM remains a focused Amiga-class 68k VM rather than a general system emulator.

## 9. Non-goals for Hyper/040

- cycle-exact Agnus/Alice/Denise/Lisa emulation;
- demo raster accuracy;
- floppy copy-protection fidelity;
- exact Paula audio timing;
- exact CIA timing;
- reproducing arbitrary accelerator-card bugs;
- pretending that modern virtual devices are historical hardware.

Existing emulators remain the correct tool for those workloads.

## 10. M1 implementation state

The M1 VM skeleton now provides:

- configurable RAM allocation;
- ROM/bootstrap loading and write protection;
- deterministic physical-address map;
- explicit core-device registration;
- `vmserial` host output/status baseline;
- interrupt pending/clear baseline;
- monotonic timer baseline;
- deterministic machine-description output;
- VM-core tests that remain active in Release builds.

M2 begins at the CPU boundary: reset vector, register state, instruction stepping, exceptions, interrupts, MMU and FPU integration.
