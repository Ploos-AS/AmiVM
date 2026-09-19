# AmiVM

AmiVM is a high-performance 68k Amiga-compatible virtual machine.

Its goal is not to reproduce every historical Amiga chipset cycle. Instead, AmiVM takes the best lessons from projects such as ARAnyM, UAE-family emulators and modern virtual-machine design to build the fastest practical Amiga-class 68k system for operating systems and productivity workloads.

## Project goal

> Build the fastest practical 68k Amiga-compatible virtual machine while preserving enough Amiga compatibility to run useful Amiga operating systems and software.

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
3. **040/060-class baseline** — MMU and FPU are first-class requirements.
4. **Dynarec/JIT first** — x86-64 and AArch64 are tier-1 host architectures.
5. **Large memory** — do not impose historical Amiga RAM limits unless a compatibility profile requires them.
6. **Paravirtual I/O** — provide efficient virtual storage, networking, framebuffer, audio and host integration.
7. **Compatibility as a profile** — keep a separate compatibility machine for software that expects traditional Amiga hardware conventions.
8. **Automation first** — serial console, deterministic launch configuration, headless mode, snapshots and CI-friendly execution are part of the architecture.

## Initial machine profiles

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

## Current M1 machine map

- ROM/bootstrap: `0x00f00000`, 1 MiB
- RAM: starts at `0x10000000`, configurable with `--ram-mib`
- MMIO: starts at `0xff000000`
- `vmserial`: `0xff000000`
- timer: `0xff001000`
- interrupt controller: `0xff002000`

The M1 device registry and map are intentionally small and deterministic. They may evolve before the stable guest ABI milestone.

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

**Next: M2 — 68k execution.** The next proof point is a CPU-backend abstraction followed by reset-vector and first-instruction execution for the Hyper/040 guest.

See [ROADMAP.md](ROADMAP.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## License

MIT. Copyright Ploos AS.
