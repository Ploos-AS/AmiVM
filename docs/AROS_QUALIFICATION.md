# AROS m68k qualification

AmiVM treats AROS qualification as evidence from the running guest, not as a synthetic host-side PASS.

## Evidence stages

1. **RESET** — the configured ROM was loaded and the guest reset vector was accepted.
2. **EXECUTION** — at least one guest instruction completed.
3. **OS_DETECTED** — the configured AROS boot marker was observed on the virtual serial console.
4. **FILESYSTEM** — the configured filesystem marker was observed.
5. **SHELL** — the configured shell marker was observed.
6. **INTERRUPTS** — independently verified by the guest/device qualification suite.
7. **DEVICE_IO** — independently verified by the guest/device qualification suite.
8. **WORKLOAD** — independently verified by the workload qualification suite.

The harness only reports a qualification as complete when every bit requested by the profile is present in the result mask.

## Important boundary

`profiles/aros-m68k.json` defines the target contract. It does **not** make AROS PASS by itself.

An AROS PASS requires:

- the actual external AROS ROM;
- the actual configured guest storage image;
- guest execution through AmiVM;
- guest-observed boot evidence;
- independently qualified device/interrupt requirements where requested.

Missing or unsupported hardware must remain visible as **NOT QUALIFIED**, rather than being silently converted to PASS.

## Serial markers

The first profile uses:

- OS: `AROS`
- filesystem: `Workbench`
- shell: `Shell`

These are deliberately profile data. If a particular AROS build emits different stable markers, the profile can change without modifying the VM engine.

## Current state

The qualification framework is implemented. A real AROS image has not yet been claimed as passing. The next qualification step is to run the actual AROS m68k assets and record the first failure boundary (CPU instruction, MMU, device, interrupt, storage, or guest boot stage).
