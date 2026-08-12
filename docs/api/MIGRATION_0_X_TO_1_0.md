# Migration from FEAViz 0.x to ABI 1

Applications written against 0.9-0.15 should rebuild against the 1.0 headers.
No compatibility shim is required for retained `fviz_*` entry points.

- Initialize every public options/record struct with its supplied initializer;
  `struct_size` is validated for forward-compatible extension.
- Treat getters as borrowed references and create/copy results as owned.
- Use `FVizId` for persistent FEA identity and `FVizSize` for counts; do not
  narrow either without checking.
- Replace assumptions that VTU attributes become floats: 0.15+ preserves the
  source numeric type.
- Drive embedded interaction with `process_events` and explicit timers; calling
  `start` transfers loop control until close.
- Refresh persistent selections after pipeline recomputation before probing or
  updating highlights.
- Check render capabilities before requiring GPU hardware selection.

ABI 1 begins semantic-versioning guarantees at the 1.0 release. Patch releases
preserve source and binary compatibility; public removal requires a future major
release and a documented deprecation cycle.
