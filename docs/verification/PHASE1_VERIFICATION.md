# Phase 1 Verification

Version: **FEAViz 0.0.6**  
ABI: **1**

## Implemented scope

- Public allocator interface and portable default allocator.
- Aligned allocate/reallocate/free convenience API.
- Checked size arithmetic.
- Opaque runtime object header and stable type identity.
- Atomic reference counting with overflow/underflow guards.
- Per-object allocator retention and final-release deallocation.
- Extended `FVizResult` values and result strings.
- Thread-local fixed-capacity last-error state.
- Logging levels, filtering, callback sink, and default stderr sink.
- Core runtime example.
- Allocation, object, error, logging, memory-stress, TLS, and threaded ref-count tests.

## Verification performed in the build environment

### Shared Debug / GCC 14.2

- C17 configure: PASS
- Warnings-as-errors: PASS
- Build: PASS
- Public-header isolation compile checks: PASS
- CTest: 8/8 PASS

### Sanitized Debug

- AddressSanitizer: PASS
- UndefinedBehaviorSanitizer: PASS
- Leak detection enabled: PASS
- CTest: 8/8 PASS

### Static Release / GCC 14.2

- Static FEAViz library: PASS
- LTO/IPO: PASS
- Warnings-as-errors: PASS
- Public-header isolation compile checks: PASS
- CTest: 8/8 PASS

### Clang 17 cross-compiler pass

- Shared Debug: PASS
- Warnings-as-errors: PASS
- Public-header isolation compile checks: PASS
- CTest: 8/8 PASS

### Install/package ABI pass

- `cmake --install`: PASS
- Installed public headers include all Phase 1 Core headers: PASS
- External C17 consumer using `find_package(FEAViz CONFIG REQUIRED)`: PASS
- Consumer aligned allocation/object lifecycle runtime check: PASS
- Dynamic export scan: only public `fviz_*` entry points exported; no `fviz_internal_*` symbols leaked: PASS
- Project-local `.bat` / `.cmd` files: 0

### Stress coverage

- Repeated aligned allocation/reallocation/free across multiple alignment classes.
- 32,768 allocation slots processed per memory-stress run (512 slots x 64 rounds), including reallocation.
- 100,000 sequential retain/release cycles on a custom-allocator object.
- Four concurrent worker threads x 50,000 retain/release cycles while the owning reference remains alive.
- Worker-thread errors verified not to overwrite the main thread's last-error state.

## Platform note

The verification host is Linux/GCC. The repository continues to carry the Windows CMake + NMake + MSVC v145 baseline. The MSVC-specific atomic implementation uses Visual C/C++ interlocked intrinsics and the Windows threaded test uses `CreateThread`/`WaitForSingleObject`; those paths require final execution on the user's Windows toolchain.
