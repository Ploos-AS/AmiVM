# AmiVM Powerload regression checker

The regression checker compares a measured Powerload JSON result with a baseline JSON result.

Usage:

    powerload-check <baseline.json> <result.json> [max_regression_fraction]

The default maximum regression is 5 percent. A lower throughput than the baseline beyond the allowed fraction fails.

Both files must contain:

- workload_id
- mode
- throughput

The checker also requires matching workload ID and mode.
