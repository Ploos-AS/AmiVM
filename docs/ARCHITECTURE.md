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

The initial model provides:

- a low bootstrap/ROM area;
- guest RAM in large contiguous regions;
- an MMIO window reserved for AmiVM devices;
- optional framebuffer memory;
- explicit machine-description data available to the guest bootstrap.

The exact physical addresses are deliberately deferred until M1 so they can be validated against Linux/m68k bootstrap constraints and Amiga compatibility requirements before becoming ABI.

## 4. Device model

AmiVM devices use a simple versioned paravirtual bus rather than pretending to be historical Zorro, SCSI or Ethernet hardware.

Each device exposes:

- vendor/device identifier;
- ABI version;
- MMIO register region;
- feature bits;
- interrupt source;
- optional shared-memory queues.

The design should allow efficient descriptor-ring or shared-buffer I/O without requiring one guest trap per byte or sector.

Initial logical devices:

- `vmserial` — console/debug/control transport;
- `vmblock` — block storage;
- `vmnet` — networking;
- `vmgfx` — linear framebuffer/display transport;
- `vmfs` — host/shared filesystem transport;
- `vmaudio` — optional audio transport.

Names and IDs remain provisional until the M1 ABI freeze point.

## 5. Interrupts and timing

Hyper/040 gets a virtual interrupt controller and monotonic timer rather than cycle-derived chipset timing.

Goals:

- low interrupt overhead;
- deterministic interrupt injection;
- enough timer fidelity for Linux and Amiga-compatible OS scheduling;
- no requirement to emulate raster or CIA timing in Hyper mode.

Compatibility mode may later add legacy-facing timing surfaces separately.

## 6. Boot model

### Linux/m68k

Linux is the preferred first bring-up guest because it gives clear kernel-level validation of CPU, MMU, exceptions, interrupts, timers and I/O.

The M0/M1 boot contract should support:

- kernel image;
- optional initrd;
- command line;
- memory map;
- machine/device description;
- serial early console.

The final handoff format will be selected after validating what is least invasive for upstream or maintained Linux/m68k support.

### AmigaOS / AROS

These guests are not required to use the Linux boot contract.

AmiVM will provide a separate compatibility/bootstrap layer and native guest drivers where necessary. The Hyper machine should not become coupled to undocumented historical hardware behaviour merely to make this path work.

## 7. CPU execution strategy

M0 does not mandate writing a 68k CPU core from scratch.

Preferred evaluation order:

1. identify a proven 68k execution core whose license and architecture permit reuse;
2. require MMU and FPU correctness suitable for Linux/m68k;
3. establish a reference/debug execution path;
4. prioritize JIT/dynamic translation on x86-64 and AArch64;
5. keep the CPU backend behind an internal interface so it can be replaced or supplemented.

Raw benchmark speed is not sufficient if exception/MMU behaviour prevents modern kernels from running correctly.

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

## 10. M0 feasibility decision

The project is considered feasible if a reusable or implementable 68040-class execution path can satisfy MMU/FPU/kernel requirements and the virtual machine can expose a small, stable device ABI without depending on legacy chipset emulation.

The first proof point after M0 is therefore not Workbench graphics. It is a deterministic VM skeleton capable of exposing RAM, timer, interrupts and serial I/O to a 68k guest, followed by Linux/m68k kernel bring-up.
