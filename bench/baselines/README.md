# Powerload baselines

Baselines are generated from measured Powerload result files.

## Generate

Run a workload and capture its result:

    amivm_powerload_runner --workload cpu.integer --mode FAST --output result.json

Generate a baseline artifact:

    amivm_powerload_baseline result.json cpu.integer.baseline.json

Review and commit the generated baseline only when the workload, VM configuration, measurement method, and correctness result are known to be valid.

Synthetic regression fixtures remain separate and must never become production baselines.
