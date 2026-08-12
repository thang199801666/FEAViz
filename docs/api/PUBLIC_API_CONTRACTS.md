# FEAViz public API contracts

This document defines default behavior for the installed C17 API. A function's
specific documentation may strengthen these guarantees but may not silently
contradict them.

## Ownership vocabulary

- `create`, `copy`, and explicit `retain` operations return an owned reference.
- Successful object setters retain their input unless documented otherwise.
- Ordinary getters return borrowed pointers. They remain valid only while the
  documented owner and the relevant retained child remain alive.
- Successful replacement releases the previously retained child after the new
  child has been retained; self-assignment is valid.
- Output-port proxies are borrowed from their producer.
- Releasing `NULL` is valid.

## Result and output parameters

- Functions returning `FVizResult` return `FVIZ_OK` only after completing their
  documented state change.
- Required null inputs return `FVIZ_ERROR_INVALID_ARGUMENT`.
- Creation and read operations clear owned output pointers to `NULL` before
  work begins and leave them `NULL` on failure.
- Failed mutating operations leave the target valid and, unless explicitly
  documented as incremental, observably unchanged.
- The calling thread can query detailed failure state through the last-error
  API. A worker thread's error state does not replace another thread's state.

## Counts, IDs, and arithmetic

- Public counts use `FVizSize`; public persistent identities use `FVizId`.
- Every allocation derived from external or public counts checks addition and
  multiplication overflow before allocating.
- Conversion to a narrower file, platform, or GPU representation is checked.
- An unsupported representable range returns an explicit error; it is never
  silently truncated.

## Modification time

- An observable mutation advances the object's MTime.
- Composite objects report an MTime at least as new as retained children that
  affect their observable output.
- Raw mutable pointers cannot be tracked automatically; callers must call
  `fviz_object_modified()` on the documented owner after writing.
- Failed and cancelled pipeline execution does not mark an output current.

## Thread safety

- Reference counting, last-error state, and MTime operations are thread-safe.
- Immutable concurrent reads are allowed unless a type says otherwise.
- Container, dataset, pipeline-graph, renderer, and interaction mutation
  requires external synchronization.
- A render window and its OpenGL resources are used only from the owning render
  thread unless a platform API explicitly documents another mode.
- Cancellation request flags may be set from another thread.

## Compatibility

- Public structures are opaque or explicitly versioned before ABI 1.0.
- Compatibility wrappers use the same generalized core implementation.
- Source/API removals require a documented deprecation cycle after 1.0.
