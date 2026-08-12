# Thread-safety matrix

| Area | Concurrent reads | Concurrent mutation | Notes |
|---|---:|---:|---|
| Object retain/release, MTime | Yes | Yes | Atomic reference count and monotonic timestamp. |
| TLS last error | Yes | Yes | Isolated per calling thread. |
| Immutable arrays/data objects | Yes | No | Raw mutable access requires external locking and `Modified()`. |
| Pipeline graph/executive | Yes after construction | No | One update transaction per connected graph. |
| Parallel contexts | Yes | Yes through task APIs | User callbacks own synchronization for shared state. |
| Scene/renderer/interaction | Yes when idle | No | Mutate and dispatch on the render thread. |
| Render window/OpenGL | No | No | Bound to the creating/owning render thread. |
| Cancellation token | Yes | Yes | Cancellation request is cross-thread safe. |
| Readers/writers | Separate objects/files | Separate objects/files | Do not share a mutable dataset without locking. |

Unless an API states otherwise, “concurrent reads” assumes no thread mutates the
same object or a retained child that contributes to its composite MTime.
