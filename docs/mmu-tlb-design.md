# M315 — MMU translation cache: coherence requirements

Status: 16-slot per-CPU TLB implemented and **disabled by default**. Opt-in testing and CI benchmarks are available.

## Correctness gate

Existing M312–M314 tests require immediate visibility of L1 pointer changes,
L2 remapping and invalidation, write-protection updates, SRP/URP changes,
user/supervisor transitions, and MMU disable. Stack longwords crossing a
4 KiB page boundary must translate and check permissions on both pages.

**Remaining integration requirement:** direct writes to `vm.ram[]` bypass
`ram_page_generation` unless the caller invokes `amivm_ram_mark_dirty()`
after mutation and before CPU translation. M316–M321 provide tracked writes,
explicit dirty notification, and regression coverage, but external host/DMA
callers still must honor the contract.

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
3. M322 added page-table walk and MMU-bypass counters; M323 ensures CPU reset
   clears them. Implement a small per-CPU cache **opt-in and off by default**, with hit,
   miss and invalidation counters.
4. Run remapping/protection/root-switch regression tests with cache both
   disabled and enabled, including tracked and explicitly invalidated writes.
5. Benchmark cache on/off on the **same CI runner**, seven samples per mode,
   and compare medians; cross-runner comparisons are not causal evidence.

## Release criterion

Do not enable a TLB by default until all supported RAM mutation paths
preserve cache coherence and all MMU regression tests pass with caching.

## M327 CI benchmark baseline (run 1956)

The same GitHub Actions runner measured seven executions per scenario.
These are medians for the JSR/RTS/BRA microbenchmark, not a general
Amiga workload and not a basis for enabling TLB by default.

| Mode | Median ns/instruction | Page-table walks | TLB hits | TLB misses |
| --- | ---: | ---: | ---: | ---: |
| MMU, TLB off | 27.583 | 667051 | 0 | 0 |
| MMU, TLB on | 27.185 | 6 | 667045 | 6 |

The measured difference is approximately 1.44% lower ns/instruction
with TLB on, despite nearly eliminating page-table walks. This suggests
generation-validation overhead and other execution costs dominate this
microbenchmark. Re-run on multiple runners/workloads before drawing
performance conclusions.

Before enabling the cache by default, validate host/DMA dirty
notifications, page-table descriptors spanning boundaries, VM reuse,
and representative guest workloads.

## VM switching (M333)

TLB entries are owned by the CPU, not by the VM. Any host reusing the same
CPU state with a different `amivm_vm` must call `amivm_mmu_tlb_flush(cpu)`
**before** translation against the new VM. Page-generation counters alone
do not distinguish two independent VM instances. M333 adds a regression
with different mappings in two VMs. Automatic VM identity binding remains
a future improvement; the explicit flush contract is mandatory meanwhile.

## M334 VM lifetime isolation

Each initialized VM receives a process-local lifetime ID. TLB entries carry
this ID, and lookup rejects entries from a different VM even if the page-table
root and generation counters happen to match. Explicit flush remains useful
for deterministic cache cleanup. VM initialization is currently single-threaded;
parallel VM creation would require synchronizing the ID allocator.
