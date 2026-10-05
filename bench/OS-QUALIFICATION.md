# AmiVM OS-first qualification

AmiVM qualifies the machine by running a real guest OS as early as practical.

## First qualification target

**AROS m68k** is the first OS qualification target.

The ROM/boot image is supplied externally. Guest OS binaries, Kickstart/AmigaOS ROMs and other non-repository guest assets are never committed to this repository.

## Qualification layers

1. ROM load and reset
2. 68k CPU execution
3. RAM and memory map
4. exception vectors and interrupts
5. AGA/custom-device access required by the profile
6. filesystem/storage
7. boot to AROS
8. shell/userspace execution
9. deterministic smoke tests
10. real Amiga workloads

## Later targets

After AROS qualification, add external-asset qualification profiles for AmigaOS and then m68k Linux and m68k NetBSD.

The same AmiVM machine/execution core should be used across profiles. A profile describes guest-visible compatibility; it is not a performance throttle.

## Asset policy

Guest operating systems and ROMs remain external inputs. This keeps the repository clean and avoids bundling proprietary AmigaOS/Kickstart material.
