# FEAViz Core Runtime

Phase 1 establishes the ownership, allocation, diagnostics, and runtime-object rules used by every later FEAViz subsystem.

## Allocation model

`FVizAllocator` is a public callback-based allocator interface. An allocator contains allocate, reallocate, and deallocate callbacks plus opaque user data.

The default allocator is returned by `fviz_allocator_default()`. It supports power-of-two over-alignment and preserves alignment across reallocation. Convenience functions (`fviz_alloc`, `fviz_alloc_aligned`, `fviz_realloc`, `fviz_realloc_aligned`, and `fviz_free`) use the default allocator.

Rules:

- An alignment of zero means the platform/default C alignment.
- Explicit alignments must be powers of two.
- Allocation with size zero returns `NULL` and is not an error.
- Reallocation with a `NULL` input behaves as allocation.
- Reallocation to size zero releases the block and returns `NULL`.
- Memory must be released by the allocator that created it.
- Custom allocator callbacks that are shared across threads are responsible for making their own user state thread-safe.

`fviz_size_add` and `fviz_size_multiply` provide checked size arithmetic for container and mesh allocation code.

## Object model

All future reference-counted FEAViz objects begin with an internal `FVizObject` header. The header itself is opaque in the public ABI.

Public lifecycle operations are generic:

```c
FVizObject* object = NULL;
if (fviz_object_create(&object) == FVIZ_OK)
{
    fviz_retain(object);
    fviz_release(object);
    fviz_release(object);
}
```

The initial reference count is one. `fviz_retain(NULL)` returns `NULL` and `fviz_release(NULL)` is a no-op.

Reference counts use the Core atomic abstraction. The final release invokes the concrete type destructor and then returns the complete allocation through the allocator stored in the object header.

## Runtime type identity

`FVizTypeId` is a stable 64-bit identifier. `fviz_type_id_from_name()` uses a stable FNV-1a 64-bit mapping so type constants can be verified independently of pointer addresses or compiler RTTI.

The base object type is:

```c
FVIZ_TYPE_OBJECT
```

and is the stable hash of `"FVizObject"`.

Internal class descriptors form a parent chain, allowing `fviz_object_is_type()` to evolve naturally into base-type checks for later `FVizDataObject`, `FVizDataSet`, mapper, actor, and FEA object hierarchies without exposing class-layout details in the public ABI.

## Error model

Functions that naturally return a status use `FVizResult`. Pointer-returning convenience APIs set thread-local last-error state on failure.

Useful diagnostics:

```c
FVizResult code = fviz_last_error_code();
const char* text = fviz_last_error_message();
fviz_clear_last_error();
```

The error buffer is fixed-size thread-local storage and performs no heap allocation. A successful call does not implicitly erase an earlier error; callers may clear it explicitly when a clean diagnostic boundary is required.

## Logging model

Logging is deliberately small in Core. `fviz_log_message()` accepts a level, category, and already-formatted message. This avoids forcing formatting allocation or a third-party logging dependency into the base ABI.

The process logger supports:

- TRACE, DEBUG, INFO, WARNING, ERROR, FATAL, and OFF levels.
- A minimum-level filter.
- A user callback plus opaque user data.
- A default stderr sink.
- A lock-protected callback snapshot; callbacks are invoked after the lock is released, so a callback may safely re-enter FEAViz logging configuration.

## ABI rules

- `FVizObject` remains opaque publicly.
- Concrete object private fields remain under `internal/FViz`.
- `FVizAllocator` is intentionally public because allocator callbacks cross the ABI boundary.
- Type IDs do not depend on compiler RTTI or structure addresses.
- Error text pointers are thread-local and valid until the same thread changes its last-error state.
