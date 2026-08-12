# Contributing to FEAViz

## Language and API rules

- Core language: C17.
- Public types use `FVizPascalCase`.
- Public functions use `fviz_snake_case`.
- Public constants/macros use `FVIZ_UPPER_CASE`.
- Public object implementations should be opaque once object-based APIs are introduced.
- Do not expose platform headers such as `Windows.h` through public headers.
- Avoid global mutable state.
- Ownership must be explicit in API documentation.

## Dependency direction

Lower layers must never depend on higher layers. In particular, Data/Mesh/Geometry must never include Rendering or Interaction.

## Completion rule

A feature is not considered complete without build integration, tests, documentation, and a warning-clean build on supported compilers.
