# ADR 0002: Executive-owned pipeline requests

Status: accepted for milestone 0.10

Graph traversal and request state belong to `FVizExecutive`, not individual
filters. Algorithms process information, data-object, update-extent, and data
requests through one callback contract. A transaction identifies shared
upstream work, carries cancellation/error state, and keys caches by requested
output plus piece/extent/time metadata. Compatibility filters wrap this core.
