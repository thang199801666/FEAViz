# FViz Core 0.51-0.60 verification

## Implemented production-renderer increments

- executable render graph, transient target aliasing, deterministic validation,
  custom-pass GL state isolation, and per-pass statistics;
- weighted OIT validation plus explicit sorted fallback/depth-peeling capability;
- glyph subrange uploads and stable line/edge coincident-topology behavior;
- CPU clipping with interpolated attributes, provenance, oriented watertight caps;
- backend-neutral retained overlay layout and viewport-correct scalar legend layout;
- four-viewport shared mapper residency and destruction-order regression coverage;
- bounded cancellable CPU region selection with explicit visible-only limitation;
- transactional camera/actor drag rollback on cancellation;
- exact/tolerance/RMSE/perceptual RGBA8 comparison and manual-only golden manifest.

## Gate commands

```text
cmake -S . -B out/build/windows-msvc-core-only -G "NMake Makefiles" \
  -DCMAKE_BUILD_TYPE=Release -DFVIZ_BUILD_FEA=OFF -DFVIZ_BUILD_TESTS=ON \
  -DFVIZ_BUILD_EXAMPLES=OFF -DFVIZ_BUILD_BENCHMARKS=ON \
  -DFVIZ_WARNINGS_AS_ERRORS=ON
cmake --build out/build/windows-msvc-core-only
ctest --test-dir out/build/windows-msvc-core-only --output-on-failure
```

## Remaining C0.60 platform gates

The repository tests cover deterministic correctness and short create/render/destroy
cycles. The release process must still execute the documented 8-hour hardware soak,
portable sanitizer lanes, clean install-tree consumer, and supported adapter matrix.
Cross-window public share groups, asynchronous visible-only region picking, and dual
depth peeling remain explicitly outside the current capability set.
