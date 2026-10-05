# AROS m68k boot qualification

The AROS qualification harness treats ROM and guest images as external inputs.

## Preflight

Run `tools/aros-boot-check.sh` with the AROS ROM supplied through `AMIVM_AROS_ROM`. An optional guest image may be supplied through `AMIVM_AROS_IMAGE`.

The preflight deliberately does not download or bundle AROS assets.

## Qualification sequence

1. Validate `aros-m68k` profile.
2. Load external ROM.
3. Apply ROM reset vector.
4. Start MAX-speed execution.
5. Detect AROS boot progress.
6. Verify shell/userspace availability.
7. Verify filesystem access.
8. Run deterministic smoke tests.
9. Run real Amiga workloads.

A failure at any stage is a machine-compatibility finding, not a reason to add a synthetic compatibility layer without evidence from the guest.

The final harness will emit machine-readable qualification results suitable for CI.
