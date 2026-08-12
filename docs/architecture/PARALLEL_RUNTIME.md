# Parallel runtime contract

FEAViz 0.12 separates scheduling state into opaque `FVizParallelContext`
instances. Two contexts own different pools and dispatch locks and can execute
concurrently. `fviz_parallel_default_context()` supplies the process-wide
compatibility runtime used by `fviz_parallel_for()`.

## Ownership and lifecycle

- Initialize `FVizParallelContextOptions`, then create and destroy a context on
  the same library ABI.
- Destroy waits for the active dispatch and joins every worker. Calling destroy
  concurrently with a new dispatch is outside the contract.
- A task group borrows its context and cancellation token. Both must outlive
  the group and any call to `fviz_task_group_wait()`.
- The default context is library-owned and must not be destroyed. It shuts down
  its workers during normal process termination.

## Execution semantics

- A result-returning range processes half-open `[begin, end)` chunks.
- Worker failures are reported from the lowest failing chunk start, independent
  of which worker happens to finish first.
- Cancellation is cooperative. No new chunk begins after a worker observes the
  token, but already-running callbacks may finish.
- Nested scheduling falls back to the same serial callback semantics and cannot
  deadlock waiting for its current worker pool.
- A context serializes its own dispatches. Separate contexts have no shared
  dispatch lock.

## Determinism

`fviz_parallel_sum_f64()` uses fixed-size chunks and combines partials in index
order. Its result is invariant to the configured worker count for the same
platform/compiler. Integer scans check overflow. Key/index sorting is stable,
preserving input order for equal keys.

## Scratch and affinity

Each callback thread has 64 KiB of 64-byte-aligned scratch. Allocations are
reset before every scheduled chunk and must not escape the callback. Windows
contexts may request compact or spread ideal-processor hints. Other platforms
return `FVIZ_ERROR_NOT_SUPPORTED` for a non-default affinity policy until a
portable implementation is available.

## Performance qualification

Enable `FVIZ_BUILD_BENCHMARKS` and run `FVizBenchmarkParallelHex`. The benchmark
emits CSV for structured cantilever domains of 512, 32,768, and 262,144 HEX8
cells at 1/2/4/8/hardware threads. Output hashes must agree across every thread
count; timing is recorded but deliberately not asserted by CTest.
