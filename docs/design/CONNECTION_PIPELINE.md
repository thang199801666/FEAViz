# Demand-driven connection pipeline

FEAViz 0.6.0 adds a small VTK-style execution graph while retaining the library's C17 API and reference-counted ownership model.

```text
FVizUnstructuredGrid
        |
        v
CellDataToPoint -> Warp -> Surface -> Mapper -> Actor -> Renderer
                            |
                            +---- FVizPolyData
```

## Output contracts

Each filter advertises one output type before it executes:

- Threshold, warp, and cell-data-to-point filters produce `FVizUnstructuredGrid`.
- Surface and slice filters produce `FVizPolyData`.
- Filter connections currently accept only unstructured-grid producers.
- Mapper connections accept only polygonal producers.

`FVIZ_FILTER_OUTPUT_NONE` is returned for a null filter. Use `fviz_filter_output()` for volumetric output and `fviz_filter_poly_data_output()` for polygonal output.

## Update semantics

`fviz_filter_update()` first updates its upstream producer, then executes only when its input object's composite MTime or its own parameters changed. The renderer calls `fviz_mapper_update()` for every actor, so normal rendering is the terminal pull operation. Camera fitting and picking also update the graph before reading scene geometry.

Changing a filter parameter marks that stage dirty. Its next update replaces the cached output, which changes the downstream input identity and invalidates each dependent stage. Repeated updates without changes retain the same cached output object.

## Ownership and graph safety

Connections are retained references. A downstream filter retains its producer, and a mapper retains its polygonal producer. Setting a direct input clears the corresponding connection. Releasing the final downstream owner therefore releases the connected chain unless the application holds additional references.

`fviz_filter_set_input_connection()` rejects connections that would create a cycle. Recursive execution also has a runtime cycle guard so corrupted or future graph implementations fail with `FVIZ_ERROR_INVALID_STATE` instead of recursing indefinitely.

## Current boundary

This milestone intentionally uses a single input port and synchronous execution. General multi-port algorithms, dataset-wide modification times, asynchronous scheduling, cancellation, and parallel surface/BVH construction remain future work.
