# Powerload baselines

Baselines are versioned benchmark references used by CI regression checks.

- Baselines must be replaced with measured results, not guessed performance numbers.
- Synthetic fixtures are kept separately to test the checker itself.
- A baseline is only comparable when workload ID, mode, VM configuration, and measurement methodology match.
- The initial cpu.integer.json value is a structural placeholder until a real benchmark run is captured.
