# AmiVM MAX

AmiVM has one performance objective: MAX.

Each workload is normalized against its own valid baseline:

    normalized = current_throughput / baseline_throughput

A normalized value of 1.00 means baseline performance, greater than 1.00 means faster than baseline, and less than 1.00 means slower than baseline.

The aggregate MAX score is the arithmetic mean of the workload ratios. This score is only used when all workloads in the comparison are semantically comparable and pass correctness checks.

The reference interpreter is a correctness oracle, not the performance target. Optimized interpreter, JIT, native helpers, caching, and other implementation strategies compete for the same MAX target.
