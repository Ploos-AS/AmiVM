# AmiVM v1.0 scope and qualification gates

Status: proposed release contract (planning; not evidence of implementation).

## Purpose
AmiVM is a performance-first 68k virtual machine. Keep the Hyper/040 guest ABI and its high-performance goals separate from legacy Amiga machine compatibility. Do not silently redefine Hyper/040 as a cycle-accurate emulator.

## Included in the v1.0 plan
- Complete the Hyper/040 execution and guest bring-up path from M2 onward.
- Qualify supported original Commodore Amiga model profiles in the separate Compatibility path: A1000, A500, A500+, A600, A1200, A2000, A3000 and A4000, with variants documented individually.
- Document CPU, chipset, memory, ROM and peripheral coverage per profile. Never mark a profile complete based only on boot success.
- Target AmigaOS 1.x, 2.x and 3.x, AROS/m68k and AxiomicaOS/m68k using documented, appropriate machine profiles and legally supplied ROMs/test fixtures; qualify OS versions separately.
- Include Linux/m68k, NetBSD/m68k and OpenBSD/m68k as named guest targets, with explicit machine/port-specific boot and device requirements. OpenBSD support may require porting work.
- Preserve Linux/m68k and NetBSD/m68k Hyper targets from the existing roadmap.
- Provide reproducible headless smoke tests, CI results and a per-profile compatibility matrix.
- Publish known limitations and explicit pass/fail evidence before release.

## Excluded from v1.0
- Third-party accelerators and expansion CPU boards, including Vampire V2.
- Apollo 68080, AMMX, SAGA and Vampire V4 Standalone.
- Amiga-compatible third-party machines, including DraCo.
- A blanket promise of cycle-exact demo/game compatibility.
- CaffeineOS guest support and any Apollo/Vampire-specific distribution qualification.

## Release gates
1. CPU reference execution: reset vectors, instruction stepping, supervisor/user transitions, exceptions, interrupts, MMU and FPU tests.
2. Boot: documented, repeatable OS startup on each supported machine profile.
3. Devices: explicit compatibility coverage and regressions for required chipset, storage, video, input and audio paths.
4. Automation: clean builds and test runs on supported host platforms.
5. Performance: published repeatable benchmarks, without sacrificing architectural correctness.
6. Documentation: exact supported model variants, ROM requirements, known issues and release artifacts.

## Sequencing
M2 CPU backend -> M3 guest bring-up -> M4 I/O -> M5/M6 Amiga compatibility -> M7 performance -> M8 automation -> M9 release.

The original-model compatibility promise is a v1.0 planning goal and may require substantial new engineering beyond the current Hyper/040 design. Any change to that promise requires an explicit scope decision.

## Post-v1.0 backlog
1. Classic accelerator and expansion-card profiles.
2. Apollo 68080 CPU backend and AMMX qualification.
3. Vampire V2 accelerator profiles, SAGA and Vampire V4 Standalone.
4. CaffeineOS guest qualification on the appropriate Apollo/Vampire-compatible profile (dependent on verified release-specific CPU, ROM, graphics, storage and boot requirements).
5. Other compatible machines, including DraCo.

Do not add post-v1.0 hardware to the first-release acceptance criteria.

## Core engineering principle: performance AND compatibility
AmiVM prioritizes both high execution performance and high functional compatibility. Correct CPU/FPU/MMU behavior, exception semantics, memory ordering and guest-visible device contracts are required; optimizations must not knowingly break supported software. Record compatibility regressions and benchmark regressions independently, and require both gates to pass.

Cycle-accurate chipset/timing emulation is explicitly the responsibility of Amilea, not AmiVM. AmiVM should not spend its performance budget reproducing historical clock-by-clock behavior unless a narrowly scoped compatibility workaround is justified and measured. Workstation software, AmigaOS and AROS correctness matter more than raster-cycle fidelity.

