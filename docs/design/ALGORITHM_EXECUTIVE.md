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
are checked before being retained, and an iterative upstream graph check rejects
cycles without depending on the native C recursion depth. A direct input clears connections on that port; setting a connection
clears the direct input.

`FVizAlgorithmOutput` is a borrowed proxy owned by its producer. Consumers
retain the producer, not the proxy, so the proxy remains valid without creating
a producer/proxy ownership cycle.

The 0.8 `FVizFilter` connection and mapper functions are wrappers over port zero
and remain source-compatible.

## Update contract

The executive records the information, data-object, update-extent, and data
request stages before executing a pull. Upstream producers update first using an
explicit execution-frame stack. Before each connected input executes, an optional
per-input request mapper may transform piece/count, ghost level, extent, and time.
This lets structured extractors request expanded upstream extents while still
materializing the downstream requested region. An
algorithm executes only when its local MTime or the maximum composite input
MTime differs from the previous successful execution.

Progress callbacks receive zero and one only for actual executions, not cache
hits. Abort state is atomic so another thread may request cooperative abort.
Current filters check abort at the executive boundary; long-running kernels will
add finer-grained checks as they migrate to task groups.

Execution and cache-hit counters are diagnostic values and do not affect cache
validity.

## Streaming metadata and requests

Output ports may publish a whole extent independently of data modification time.
The public helpers `fviz_executive_update_piece()`, `fviz_executive_update_extent()`,
and `fviz_executive_update_time()` construct ordinary pipeline requests, so they
share the same cache, cancellation, transaction, and request-remapping behavior as
`fviz_executive_update()`.

Current concrete streaming filters cover StructuredGrid/RectilinearGrid extents
and UnstructuredGrid cell pieces. Unstructured pieces can request multiple topology-aware
ghost layers built from exact shared-facet cell adjacency. `FVizUnstructuredGridPartitionFilter`
materializes all pieces while reusing topology caches across pieces and deformation-only
updates. Piece point ownership is deterministic, so duplicate boundary nodes can be
excluded reliably from composite statistics.
