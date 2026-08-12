# Algorithm ports and executive

FEAViz 0.9.0 generalizes the original filter-only graph into three layers:

```text
FVizAlgorithm --owns--> output data
       |
       +--borrows--> FVizAlgorithmOutput(producer, port)
                          |
                          v
                downstream input port / mapper

FVizExecutive --coordinates--> update requests and cache state
```

## Port contract

Each input port declares an accepted `FVizDataObject` type plus optional and
repeatable flags. Each output port declares the type it produces. Connections
are checked before being retained, and a depth-first upstream check rejects
cycles. A direct input clears connections on that port; setting a connection
clears the direct input.

`FVizAlgorithmOutput` is a borrowed proxy owned by its producer. Consumers
retain the producer, not the proxy, so the proxy remains valid without creating
a producer/proxy ownership cycle.

The 0.8 `FVizFilter` connection and mapper functions are wrappers over port zero
and remain source-compatible.

## Update contract

The executive records the information, data-object, update-extent, and data
request stages before executing a pull. Upstream producers update first. An
algorithm executes only when its local MTime or the maximum composite input
MTime differs from the previous successful execution.

Progress callbacks receive zero and one only for actual executions, not cache
hits. Abort state is atomic so another thread may request cooperative abort.
Current filters check abort at the executive boundary; long-running kernels will
add finer-grained checks as they migrate to task groups.

Execution and cache-hit counters are diagnostic values and do not affect cache
validity.
