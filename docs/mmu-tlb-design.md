# M315 — MMU translation cache: coherence requirements

Status: design only; **no TLB is enabled**.

## Correctness gate

Existing M312–M314 tests require immediate visibility of L1 pointer changes,
L2 remapping and invalidation, write-protection updates, SRP/URP changes,
user/supervisor transitions, and MMU disable. Stack longwords crossing a
4 KiB page boundary must translate and check permissions on both pages.

**Critical gap:** tests and potentially host integrations modify `vm.ram[]`
directly. Such writes bypass `ram_page_generation`; therefore a TLB keyed
only on page generations is not coherent today.

## Proposed entry and validation

Each per-CPU cache entry should record logical page, translation context
(TC, active root, supervisor flag), physical page and write-protection bit.
A generation-validated entry additionally records physical RAM page(s)
containing its L1 and L2 descriptors and the observed generations.
Descriptors may straddle page boundaries; do not assume four-byte alignment
(the current descriptor path permits even addresses).

Never reuse a cached translation under a different root, mode or TC value.
Do not cache translation faults initially. Preserve MMUSR fault semantics.
Ensure reset and VM teardown cannot reuse stale entries.

## Implementation plan

1. Audit all production writes to `vm.ram[]` and distinguish setup,
   guest writes, DMA, host injection and tests.
2. Introduce tracked bulk RAM writes and an explicit invalidation API.
   Migrate production mutation sites; test generation increments.
3. Implement a small per-CPU cache **opt-in and off by default**, with hit,
   miss and invalidation counters.
4. Run remapping/protection/root-switch regression tests with cache both
   disabled and enabled, including tracked and explicitly invalidated writes.
5. Benchmark cache on/off on the **same CI runner**, seven samples per mode,
   and compare medians; cross-runner comparisons are not causal evidence.

## Release criterion

Do not enable a TLB by default until all supported RAM mutation paths
preserve cache coherence and all MMU regression tests pass with caching.
