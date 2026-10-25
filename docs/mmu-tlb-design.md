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
for deterministic cache cleanup. M335 uses C11 atomic allocation, so concurrent VM initialization cannot
assign duplicate IDs. Exhaustion at 64-bit wrap is rejected.

## M343 page-granular dirty tracking

TLB descriptor coherence is deliberately conservative: any tracked host/DMA
write on the RAM page containing a cached L1 or L2 descriptor changes that
page's generation, invalidating the cached translation on its next lookup,
even if the descriptor bytes themselves are unchanged. Writes to unrelated
RAM pages retain the cached translation (M342). Descriptor-granular tracking
would require a separate design and benchmark before changing this contract.

## M344: finer-grained coherence options (design only)

The current page-generation check is the correctness baseline. M341 proves
that a single dirty notification invalidates multiple translations sharing
a descriptor page; M342 proves unrelated RAM pages do not invalidate entries;
M343 confirms conservative invalidation on unrelated bytes within a descriptor
page. No optimization is authorized by these tests alone.

Possible alternatives, in increasing implementation complexity:

1. **Keep page generations.** Zero additional per-descriptor metadata,
   conservative invalidation for any write on a descriptor's RAM page.
   Recommended until a representative workload shows measurable cost.
2. **Validate descriptor words on generation mismatch.** Keep the existing
   page generation and store the raw L1/L2 descriptor values with each TLB
   entry. On a generation mismatch, re-read both descriptor words and retain
   the translation only if the values match. Still require safe bounds checks,
   matching VM lifetime, root, TC, privilege and write-protection semantics.
   This adds memory reads on generation changes and must not bypass fault
   reporting. A change away and back between lookups can retain a mapping
   only if the final descriptors are identical and all relevant translation
   context remains unchanged.
3. **Descriptor-granular generation tracking.** Maintain generations for
   descriptor-sized regions and update every overlapping region on guest,
   host and DMA writes. Handle unaligned/cross-page descriptors and all
   mutation paths. This adds memory and write-path overhead; do not implement
   without measured benefit.

Before any implementation: add a benchmark with writes to unrelated bytes
*within* active descriptor pages; measure TLB hits, invalidations, page walks
and ns/instruction in paired on/off runs. Add regressions for L1 and L2
updates, invalid/valid transitions, write-protection, cross-page dirty
ranges, VM switching, reset, and direct host writes with explicit dirty
notification. Keep TLB disabled by default until the complete correctness
gate passes.

## M347: measured DMA coherence cost and next experiment

CI run 2052 (M346, seven repetitions on one GitHub Actions runner)
reported a median of **15.040 ns/iteration** for dirty notifications
to unrelated RAM and **25.170 ns/iteration** for writes elsewhere on
the cached L2 descriptor's RAM page, a **67.35% relative increase**.
Each iteration includes a direct host byte mutation, dirty notification
and one MMU translation. The synthetic benchmark deliberately forces a
page-table rewalk in the latter case; it is not an estimate of real-world
Amiga performance. These figures should not be compared across runners.

**Decision:** keep the existing page-generation coherence mechanism as
the default implementation. Prototype descriptor-value revalidation only
as an **additional opt-in** behavior while retaining the current TLB
enablement gate. In particular, the current code's cached L1/L2 descriptor
addresses and page generations are sufficient to detect *when* a candidate
needs revalidation, but do not establish that its values are unchanged.

The proposed experiment should:

1. Store both raw descriptor words when filling a TLB entry, without
   caching faults or write-protection failures.
2. On a generation mismatch, safely re-read the L1 and L2 descriptor
   words. Reuse the cached mapping only when both words match the
   stored values and the existing VM ID, root, TC and privilege keys match.
   Refresh the cached generations after a successful revalidation.
3. Invalidate and perform the ordinary walk on any changed descriptor
   or failed read, preserving MMUSR and write-protection behavior.
4. Add a separate benchmark mode that enables this opt-in strategy,
   with paired seven-sample comparisons and no unstable speed gate.
5. Test descriptor remapping, invalidation, protection, L1 replacement,
   L2 replacement, DMA ranges crossing pages, VM lifetime changes,
   CPU reset, and explicit TLB flush. Preserve M343's conservative
   expectation when the experimental strategy is disabled.

Do not enable the experimental behavior by default on the strength of
this microbenchmark. Assess both steady-state TLB hit overhead and
dirty-heavy workload performance before considering promotion.

## M352: three-way DMA/TLB evaluation

CI run **2074** (M351, seven repeated runs on one GitHub Actions
runner) produced the following medians for the synthetic DMA workload:

| Scenario | Median ns/iteration |
| --- | ---: |
| Dirty unrelated RAM page, ordinary TLB | 15.100 |
| Dirty same descriptor RAM page, page-generation invalidation | 25.740 |
| Dirty same descriptor RAM page, opt-in descriptor-value revalidation | 22.140 |

Descriptor revalidation was **13.99% faster** than conservative
same-page invalidation, but **46.62% slower** than the unrelated-page
baseline in this run. Each iteration includes a host RAM byte mutation,
dirty notification, and translation; the benchmark does not measure
general application performance. The scenario ordering is fixed, so
cache and run-order effects remain possible despite seven repetitions.

**Decision:** retain the opt-in implementation for further study;
**do not enable TLB or descriptor revalidation by default**. The observed
gain in a dirty-heavy microbenchmark does not establish a net win for
ordinary TLB hits or guest execution.

Next qualification gates:

- Benchmark ordinary stable TLB hits with revalidation on and off to
  detect overhead when generations do not change.
- Interleave or alternate benchmark order, repeat on more than one
  runner, and report distributions rather than only one median.
- Exercise write-protection, descriptor invalidation, L1/L2 changes,
  VM switching/reuse, dirty ranges and explicit flush with the
  experimental mode enabled.
- Preserve a clear rollback path to conservative page generations.
