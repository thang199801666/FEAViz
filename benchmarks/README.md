# Benchmarks

Configure with `-DFVIZ_BUILD_BENCHMARKS=ON` and run `FVizBenchmarkParallelHex`.
The benchmark generates deterministic structured HEX8 cantilever domains with
512, 32,768, and 262,144 cells. It records point-warp wall time, worker count,
task count, transient bytes, and an output-equivalence hash for thread limits
1, 2, 4, 8, and the hardware maximum. Results are emitted as CSV so CI or a
benchmark dashboard can compare revisions without enforcing machine-specific
speed thresholds in CTest.
