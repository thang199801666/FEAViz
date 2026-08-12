# IO round-trip and resource-limit contract

FEAViz 0.15 writes single-piece XML VTU in ASCII or raw appended mode. Both
modes preserve point/cell/field association, numeric type, component count,
array name, active roles, IEEE NaN/infinity, and 64-bit provenance values.
Connectivity is serialized as `UInt64`; the current in-memory cell array uses a
checked 32-bit point-ID representation, so a reader rejects wider IDs instead
of narrowing them.

The default writer uses raw appended data, `UInt64` block headers, little-endian
encoding, and no compressor. A caller can select ASCII and `UInt32` headers.
Requesting compression in a build without an approved compression backend
returns `FVIZ_ERROR_NOT_SUPPORTED` before creating a partial file.

`FVizVTUReaderOptions` bounds total file bytes, points, cells, connectivity
values, and numeric array values before large allocations. Defaults target
large engineering models while preventing size fields from becoming unbounded
allocation requests. Applications processing untrusted uploads should lower
these limits to their service quota.

PLY export is intentionally polygonal interchange: vertices and triangle faces
are written in ASCII or binary little-endian form. FEA arrays and provenance
should use VTU because base PLY cannot preserve those semantics portably.
