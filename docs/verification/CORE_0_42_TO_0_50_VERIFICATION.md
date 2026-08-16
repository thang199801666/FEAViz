# Core 0.42-0.50 verification

Date: 2026-08-15  
Host toolchain: Windows, MSVC 19.51 / v145, C17 and C++17 header gates

## Implemented scope

- field/topology primitives, dirty-range and GPU subrange contracts;
- frame scheduler and interactive/still quality policy;
- expression grammar, diagnostics, parallel evaluation, and compiled LRU cache;
- provenance, masks, association conversion, extraction, and named selections;
- closest-point, batch spatial queries, parallel BVH leaf refit, and memory accounting;
- temporal futures, cancellation, bounded priority prefetch, direction-aware scrub
  cancellation, and byte/count LRU caching;
- GPU working-set budgets, LRU retention, mapper/glyph pinning, manual purge, and
  class-specific memory statistics;
- expression fuzzer target and expression/temporal benchmark smoke targets.

## Passing gates

- Core-only Release, FEA disabled, warnings-as-errors: build passed.
- Core-only CTest plus benchmark smoke: 117/117 passed.
- Full Core+FEA Release, warnings-as-errors: build passed.
- Full Core+FEA CTest plus examples: 113/113 passed.
- Every installed public header compiled independently as C17 and C++17.
- Clean installed package consumer with `COMPONENTS Core`: compile/link/run passed.
- Clean installed package consumer with `COMPONENTS Core FEA`: compile/link/run passed.
- TestVisualization Debug and Release: build passed; both `--smoke` runs exited 0.
- Core source audit found zero includes of `FViz/FEA`.

## Performance baselines

- Expression, 1,000,000 tuples: 20.476 ns/tuple.
- Temporal cache, 1,000,000 requests: 51.095 ns/request, 999,968 hits.
- BVH, 180,000 triangles: refit 2.181 ms, rebuild 36.883 ms, 16.91x ratio;
  10,000 ray queries at 1.568 microseconds/query.

These are workstation regression references, not portable API guarantees.

## Sanitizer and fuzz status

- MSVC AddressSanitizer configuration and the complete Core/test/header build passed.
- The ASan runtime on this host hangs before `main`, including for
  `FVizTestVersion`, and emits no runtime log. Runtime ASan tests are therefore
  **unavailable on this host**, not counted as passing.
- UBSan and libFuzzer execution require Clang; no Clang/clang-cl installation is
  available on this host. The expression fuzz target remains wired for Clang CI.

Portable CI must execute ASan+UBSan and libFuzzer before a tagged production
release. This host limitation does not replace those release gates.
