# AmiVM

AmiVM is a high-performance Amiga-specific virtual machine for 68k Amiga-compatible systems.

Its goal is not to reproduce every historical Amiga chipset cycle. Instead, AmiVM takes the best lessons from projects such as ARAnyM, UAE-family emulators and modern virtual-machine design to build the fastest practical Amiga-class 68k system for operating systems and productivity workloads.

## Project boundary

AmiVM is intentionally Amiga-specific. Its CPU, exception-frame, MMU, FPU, device and machine-profile work is scoped to the Amiga roadmap. ColdFire, Atari, Macintosh, Sun, X68000 and other non-Amiga m68k systems are out of scope for AmiVM. General-purpose m68k execution technology may later be extracted into a separate mVM project; AmiVM itself does not become that project.

## CPU profiles and execution speed

AmiVM CPU profiles are **compatibility contracts**, not performance throttles. Selecting 68020, 68030, 68040 or 68060 determines the CPU facilities visible to guest software: ISA level, MMU/FPU availability and related architectural behavior. It does not ask AmiVM to reproduce the historical clock rate of that processor.

AmiVM therefore follows this rule:

> **CPU profile defines compatibility; AmiVM runs at full practical host speed.**

Cycle accounting exists only to provide deterministic, software-visible timing semantics where required by devices or guest behavior. It must never be interpreted as a host-side speed limiter. FS-UAE speed/throttle settings are likewise ignored by the execution engine.

Memory/device timing is likewise semantic rather than a host-speed limiter. Cacheability, serialization, MMIO and device-visible latency may contribute to deterministic guest timing when required, while the host executes the VM at full practical speed.


## AmiVM performance priorities

AmiVM has two primary goals:

1. **PRI 1 — maximum practical 68k/Amiga performance.** AmiVM is optimized for real Amiga CPU and system workloads, including raytracing, scene generation, software rendering, demo/scene workloads, compilation, image/audio processing, and other compute-heavy applications.
2. **PRI 2 — high-value m68k build and CI platform.** AmiVM provides practical native execution environments for Amiga, m68k Linux, and m68k NetBSD software, including native compilation and automated CI workloads.

Performance work must be measured with real workloads, not only synthetic CPU benchmarks.

### Performance modes

| Mode | Goal |
| --- | --- |
| **FAST** | Maximum throughput with JIT, aggressive caching, and minimal instrumentation |
| **DETERMINISTIC** | Reproducible execution for CI, qualification, snapshots, and scene verification |
| **DEBUG** | Breakpoints, watchpoints, tracing, memory inspection, and instruction-level debugging |

Debug and deterministic instrumentation must be isolated from the normal FAST path wherever practical.

### AmiVM Powerload Suite

The Powerload Suite is the primary workload-based performance benchmark for AmiVM.

| Workload group | Coverage |
| --- | --- |
| CPU | Integer, floating point, branches, memory throughput |
| Amiga Scene | Copper, Blitter, raster, sprites, audio |
| Rendering | Raytracing, rasterization, texture/image processing |
| Scene Generation | Geometry, transforms, procedural generation, sorting |
| Development | GCC, VBCC, assembler, linker, large project builds |
| m68k Linux | Native compilation, userland workloads, test suites |
| m68k NetBSD | Native compilation, userland workloads, test suites |

Powerload results should report throughput, wall-clock time, and reproducibility where applicable. The suite should be usable both for development benchmarking and regression detection.

## 68040 MMU/TLB capability matrix

| Capability | Status | Qualification evidence |
| --- | --- | --- |
| 4-entry translation cache | **PASS** | Dedicated 68040 qualification suite |
| 4 KiB translation pages | **PASS** | Geometry and lifecycle tests |
| Logical → physical translation | **PASS** | Fill/lookup/refill tests |
| Read/write permission metadata | **PASS** | Mixed-permission tests |
| Supervisor/user permission metadata | **PASS** | Privilege qualification tests |
| Collision/eviction | **PASS** | All-slot collision tests |
| Targeted invalidation | **PASS** | Mixed-state targeted invalidation tests |
| Full invalidation | **PASS** | Full refill/invalidation tests |
| MMU disable/enable lifecycle | **PASS** | 68040 lifecycle test |
| CPU reset lifecycle | **PASS** | 68040 reset qualification |
| 68040 MMU/TLB architectural completeness | **NOT QUALIFIED** | Additional architectural features remain |
| 68030 MMU | **SCAFFOLD** | No independent qualification suite yet |
| 68060 MMU | **SCAFFOLD** | No independent qualification suite yet |
| 68020 + 68851 | **SCAFFOLD** | No independent qualification suite yet |

**Qualification rule:** `PASS` means the specific capability has deterministic AmiVM test coverage. It does not mean the entire Motorola 68040 MMU is complete.

## 68040 MMU translation-cache qualification

The current translation-cache implementation has an explicit architectural boundary:

- **68040 MMU:** translation-cache path is qualified and enabled by the profile policy.
- **68030 MMU:** scaffold only; no qualified translation-cache claim.
- **68060 MMU:** scaffold only; no qualified translation-cache claim.
- **68020 + 68851:** scaffold only; the external MMU path is not treated as a qualified translation-cache implementation.

The cache implementation used by the qualified 68040 profile is a deterministic four-entry, 4 KiB-page translation cache. Its lifecycle is controlled by the cache API (`reconfigure`, `lookup`, `insert`, targeted invalidation and full invalidation). The generic API is intentionally reusable, but the qualification claim belongs only to the 68040 profile until the other MMU models have independent architectural tests.

This prevents generic cache infrastructure from being mistaken for completed 68030/68060/68851 MMU emulation.

## FS-UAE configuration compatibility

AmiVM can import an FS-UAE `.conf` as a machine compatibility description:

`amivm --config my-machine.conf --dump-machine`

`amivm --config my-machine.conf --config-report`

The importer deliberately separates guest-machine semantics from host/emulation-speed policy. CPU/MMU, memory and selected Amiga configuration keys are recognized where AmiVM has a corresponding contract. FS-UAE speed controls such as `uae_cpu_speed`, `uae_cpu_multiplier`, `uae_cpu_frequency`, `uae_cpu_throttle` and warp controls are intentionally ignored. AmiVM's speed policy is unlimited practical performance: a config saying `uae_cpu_speed=real` does not make AmiVM emulate a real 68020/040 clock rate.

Host presentation options such as video/audio/fullscreen settings are also ignored by the machine importer. Use `--config-report` to see recognized, speed-ignored, host-ignored and unsupported options. `--strict-config` turns unsupported or malformed options into an error, which is useful for CI.

> **FS-UAE config defines the Amiga machine; AmiVM defines the execution speed.**


The importer maps unambiguous FS-UAE models to AmiVM CPU profiles where possible: A1200 → 68020, A3000 → 68030 and A4000 → 68040. Explicit `cpu`/`uae_cpu_model` always wins. Older 68000-class models such as A500/A600/A1000/A2000 are reported as unsupported rather than silently pretending that AmiVM's 68020 minimum is equivalent.

The importer maps unambiguous FS-UAE models to AmiVM CPU profiles where possible: A1200 → 68020, A3000 → 68030 and A4000 → 68040. Explicit `cpu`/`uae_cpu_model` always wins. Older 68000-class models such as A500/A600/A1000/A2000 are reported as unsupported rather than silently pretending that AmiVM's 68020 minimum is equivalent.

Storage media are now imported into fixed AmiVM profile slots (`floppy_image_0..3` and `hard_drive_0..7`). The importer preserves the configured paths/identifiers; actual device backends remain a separate runtime layer.
## Project goal

> Build the fastest practical Amiga-specific 68k virtual machine while preserving enough Amiga compatibility to run useful Amiga operating systems and software.

AmiVM is intended for:

- AmigaOS-class 68k operating systems
- AROS/m68k
- Linux/m68k, including m68kDeb development and qualification
- NetBSD/m68k
- OpenBSD/m68k where the maintained port and machine requirements permit
- high-performance native 68k compilation, development and CI workloads
- workstation applications such as rendering, scenery generation and animation
- future workstation-style and appliance use

AmiVM is **not** primarily intended to replace cycle-accurate Amiga emulators for games, demos or hardware-timing validation.

## Design principles

1. **Performance first** — avoid emulating historical bottlenecks that software does not require.
2. **68k-native guest model** — the guest sees a 68k machine, not a translated non-68k ABI.
3. **CPU profiles, one fast engine** — expose the Amiga-relevant 68020, 68030, 68040 and 68060 guest contracts from one execution/IR/JIT architecture; MMU/FPU capabilities follow the selected profile. General m68k reuse is a future mVM concern, not an AmiVM scope requirement.
4. **Dynarec/JIT first** — x86-64 and AArch64 are tier-1 host architectures.
5. **Large memory** — do not impose historical Amiga RAM limits unless a compatibility profile requires them.
6. **Paravirtual I/O** — provide efficient virtual storage, networking, framebuffer, audio and host integration.
7. **Compatibility as a profile** — keep a separate compatibility machine for software that expects traditional Amiga hardware conventions.
8. **Automation first** — serial console, deterministic launch configuration, headless mode, snapshots and CI-friendly execution are part of the architecture.

## CPU/MMU qualification policy

AmiVM keeps the CPU profile and MMU implementation as separate qualification contracts. An integrated CPU MMU is not interchangeable with an external 68851, and an MMU feature marked as scaffold is not presented as fully qualified.

| CPU profile | MMU arrangement | Translation-cache qualification |
|---|---|---|
| 68020 | optional external 68851 | scaffold |
| 68030 | integrated MMU | scaffold |
| 68040 | integrated MMU | qualified |
| 68060 | integrated MMU | scaffold |

The current external-MMU contract is deliberately narrow:

- **68020 + 68851** is accepted.
- External 68851 configuration with 68030, 68040 or 68060 is rejected.
- A 68040 does not accept an external MMU because its MMU is integrated.
- The qualified translation-cache path currently targets the 68040-family policy only.
- The generic MMU/cache mechanisms are retained as scaffolding for the other CPU profiles until their architectural behavior is independently qualified.

This distinction is important for operating-system qualification: AmiVM must not claim that a guest is running against a fully qualified 68030/68060 MMU merely because a generic MMU path exists.

The policy is enforced during configuration resolution and VM initialization, and the complete matrix is covered by the MMU policy tests.

## CPU and machine profiles

AmiVM separates the guest-visible CPU contract from host execution speed. Selecting `68020` must not deliberately reproduce the performance of a physical 68020; it selects the ISA and architectural capabilities visible to the guest while AmiVM executes that contract as fast as practical.

The planned CPU profiles are:

- **68020** — minimum build/CI compatibility profile, especially for software intended for the broad Linux/m68k baseline.
- **68030** — 68020-class software plus the appropriate 68030 MMU/system contract.
- **68040** — integrated MMU/FPU workstation and OS profile; the first implementation baseline.
- **68060** — high-end classic CPU contract, including its architectural differences and missing/emulated instructions rather than treating it as a simple 68040 extension.
- **Hyper/040** — AmiVM high-performance workstation using a 68040-class guest contract with virtual hardware unconstrained by historical Amiga bottlenecks.
- **Hyper/060** — maximum-performance Amiga workstation profile using a 68060-class guest contract.

These are profiles over shared execution infrastructure, not separate emulator cores. The reference/interpreter path exists for correctness and differential testing; optimized JIT/dynarec execution is the normal high-performance path.

## Primary workloads

AmiVM has two co-equal primary workloads:

1. **m68k build and CI platform** — run native Linux/m68k and Amiga-family toolchains, package builds and test suites at high speed while allowing qualification against explicit 020/030/040/060 CPU contracts.
2. **Amiga power workstation** — run CPU-, FPU-, memory- and storage-intensive Amiga applications as fast as practical, including rendering, scenery generation, animation, compilation and other productivity workloads.

The project should reject architectural choices that improve historical emulation fidelity at a material cost to these workloads unless a Compatibility profile specifically requires that fidelity.

### AmiVM Hyper/040

The initial reference machine.

- Motorola 68040-class guest CPU contract
- MMU and FPU
- JIT/dynamic translation where available
- large contiguous FastRAM-style memory
- linear framebuffer / RTG-style display
- paravirtual block storage
- paravirtual Ethernet
- virtual serial console
- host-backed shared filesystem
- minimal compatibility hardware required for boot and OS integration

### AmiVM Compatibility

A later profile providing additional Amiga-compatible devices and conventions for AmigaOS software that cannot use the Hyper profile directly.

## Current machine map

- ROM/bootstrap: `0x00f00000`, 1 MiB
- RAM: starts at `0x10000000`, configurable with `--ram-mib`
- MMIO: starts at `0xff000000`
- `vmserial`: `0xff000000`
- timer: `0xff001000`
- interrupt controller: `0xff002000`

The device registry and map remain intentionally small and deterministic. They may evolve before the stable guest ABI milestone.

## Planned virtual devices

- `vmblock.device` — high-throughput block storage
- `vmnet.device` — host-backed networking
- `vmserial.device` — console and automation channel
- `vmfs` — host/shared filesystem transport
- `vmgfx` — linear framebuffer / RTG-class graphics
- `vmaudio.device` — low-overhead audio path

Linux/m68k, NetBSD/m68k and OpenBSD/m68k may use native AmiVM drivers instead of AmigaOS device interfaces. AROS/m68k and AmigaOS use Amiga-compatible bindings and, where appropriate, the Compatibility profile.

## Guest operating-system targets

AmiVM treats operating-system support as a first-class compatibility contract rather than an incidental side effect. The primary targets are:

- **Linux/m68k** — Tier 1; first kernel bring-up target and m68kDeb qualification platform.
- **NetBSD/m68k** — Tier 1 BSD target.
- **OpenBSD/m68k** — supported where the maintained m68k port and machine requirements make an AmiVM target practical.
- **AROS/m68k** — Tier 1 Amiga-compatible target and a key freely redistributable CI guest.
- **AmigaOS 3.x** — Tier 1 classic Amiga target through the Amiga-compatible bootstrap/device path.

The Hyper profile remains a clean high-performance 68k VM for operating systems able to use AmiVM-native devices. The Compatibility profile adds Amiga conventions only where required by AmigaOS-class software.

## Performance workloads

AmiVM explicitly targets workloads that benefit from a 68k machine unconstrained by historical hardware performance:

- native m68k compilation with GCC/binutils/make and other native toolchains;
- full native package builds inside Linux/m68k and m68kDeb;
- AmigaOS/AROS native software builds and qualification;
- CPU/FPU-heavy classic workstation software;
- Vista and VistaPro rendering;
- Scenery Animator workloads;
- reproducible build and benchmark jobs suitable for CI.

Native compilation is a first-class performance goal: AmiVM should make it practical to build m68k software inside a real m68k guest while approaching the convenience expected from modern development infrastructure.

## Relationship to existing emulators

AmiVM complements rather than replaces projects such as FS-UAE, Amiberry and FellowNG.

Those projects are valuable when accurate Amiga-machine compatibility matters. AmiVM deliberately explores the other end of the design space: an Amiga-compatible 68k virtual workstation optimized for performance, modern I/O and operating-system workloads.

ARAnyM is an important architectural reference because it demonstrates the value of a fast extended 68k virtual machine rather than strict reproduction of one historical computer. AmiVM will adopt useful ideas while defining its own machine and device contracts.

## Status

**M1 — VM core skeleton: complete.**

The host now has configurable RAM and ROM loading, a guest physical memory map, explicit MMIO device registration, interrupt/timer baselines, `vmserial`, deterministic machine description and Release-safe VM-core tests.

**M2 — 68k execution: in progress.**

M2 has advanced beyond the initial CPU-backend bring-up. The implementation now includes active JIT/dynarec work and qualification on the tier-1 x86-64 and AArch64 hosts. Recent work includes AArch64 executable-cache coherency, persistent JIT mapping synchronization, native host qualification and JIT helper integration fixes.

M2 is deliberately not marked complete until its full execution contract is qualified, including the 68040-class CPU path, exception/interrupt behavior, supervisor/user transitions, the MMU requirements needed by Linux/m68k, and the FPU baseline.

**Next proof point:** close the remaining M2 exit criteria and begin **M3 — 68k guest OS bring-up**. Linux/m68k is the first visible boot milestone, but the machine contract must remain OS-neutral for AmigaOS 3.x, AROS/m68k, NetBSD/m68k and compatible older OpenBSD/m68k guests.

See [ROADMAP.md](ROADMAP.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## License

MIT. Copyright Ploos AS.
