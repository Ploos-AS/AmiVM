# AmiVM profile policy

Profiles describe guest-visible machine compatibility and qualification. They are not performance modes.

## MAX policy

All profiles use:

- maximum practical host execution speed
- no artificial CPU throttling
- no emulated historical clock limit unless a guest requires it for correctness

Deterministic execution is an optional qualification mode and must not become the default performance mode.

## First profile

The aros-m68k profile is the first OS qualification profile.

Its purpose is to boot a real external AROS m68k guest and expose missing machine compatibility through guest-visible failures.

Once AROS reaches shell/filesystem qualification, additional profiles can target AmigaOS, m68k Linux and m68k NetBSD.
