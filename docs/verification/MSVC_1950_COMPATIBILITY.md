# MSVC 19.50 / v145 compatibility fix

FEAViz 0.1.3 fixes the Phase 8/0.1.x Windows C build failure:

```text
error C2061: syntax error: identifier 'max_align_t'
```

The allocator and object runtime no longer reference `max_align_t` directly. They use an internal C17-compatible alignment carrier union plus compiler-specific alignof syntax (`__alignof` on MSVC, `_Alignof` elsewhere).

CMake 3.30 may also expose a stale `MSVC_TOOLSET_VERSION=143` when NMake is launched from Visual Studio 2026. FEAViz now derives the v145 family from `MSVC_VERSION` 1950..1959, which matches the actual compiler selected by the developer environment.

The configure step also prints the pointer-size architecture. A 32-bit build remains allowed for the MVP but emits a warning because x64 is strongly recommended for large FEA datasets.
