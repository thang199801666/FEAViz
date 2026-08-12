# ADR 0003: 64-bit public topology identities

Status: accepted for milestone 0.11

FEAViz uses `FVizId` for persistent topology identities and retains 64-bit file
connectivity without truncation. GPU backends may choose narrower internal index
buffers only after checked conversion or mesh partitioning. Stable provenance
arrays map generated render primitives back to original FEA points, cells, and
faces.
