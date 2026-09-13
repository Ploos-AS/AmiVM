# AmiVM

AmiVM is a high-performance 68k Amiga-compatible virtual machine.

Its goal is not to reproduce every historical Amiga chipset cycle. Instead, AmiVM takes the best lessons from projects such as ARAnyM, UAE-family emulators and modern virtual-machine design to build the fastest practical Amiga-class 68k system for operating systems and productivity workloads.

## Project goal

> Build the fastest practical 68k Amiga-compatible virtual machine while preserving enough Amiga compatibility to run useful Amiga operating systems and software.

AmiVM is intended for:

- AmigaOS-class 68k operating systems
- AROS/m68k
- Linux/m68k, including m68kDeb development and qualification
- high-performance 68k development and CI workloads
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

## Planned virtual devices

Names are provisional during M0:

- `vmblock.device` — high-throughput block storage
- `vmnet.device` — host-backed networking
- `vmserial.device` — console and automation channel
- `vmfs` — host/shared filesystem transport
- `vmgfx` — linear framebuffer / RTG-class graphics
- `vmaudio.device` — low-overhead audio path

Linux/m68k may use native AmiVM drivers instead of AmigaOS device interfaces.

## Relationship to existing emulators

AmiVM complements rather than replaces projects such as FS-UAE, Amiberry and FellowNG.

Those projects are valuable when accurate Amiga-machine compatibility matters. AmiVM deliberately explores the other end of the design space: an Amiga-compatible 68k virtual workstation optimized for performance, modern I/O and operating-system workloads.

ARAnyM is an important architectural reference because it demonstrates the value of a fast extended 68k virtual machine rather than strict reproduction of one historical computer. AmiVM will adopt useful ideas while defining its own machine and device contracts.

## Status

**M0 — Architecture & Feasibility: in progress.**

See [ROADMAP.md](ROADMAP.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## License

MIT. Copyright Ploos AS.
