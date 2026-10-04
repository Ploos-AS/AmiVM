# AmiVM MAX

AmiVM has one performance objective: MAX.

A MAX gate is strict. Every workload included in a comparison must have a valid result and baseline, and both must declare reproducible correctness metadata. A missing or invalid workload fails the gate rather than being silently omitted.

The aggregate score is the arithmetic mean of current throughput divided by baseline throughput for each workload. The gate passes only when the aggregate score is at least 1.00.

This is deliberately conservative: an optimization must preserve correctness and must not reduce the aggregate performance of the selected MAX workload set.
