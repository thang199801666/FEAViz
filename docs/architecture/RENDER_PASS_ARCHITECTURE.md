# Render-pass architecture

FEAViz 0.13 makes pass ordering a renderer concern and device commands a
backend concern. Every renderer starts with six stable stages: clear, opaque,
translucent, edge, selection, and overlay. Custom passes are retained objects,
sorted by stage, and receive only a renderer, viewport metadata, and an opaque
backend context. Public actor and mapper APIs never expose OpenGL handles.

## Scene semantics

- Opaque actors write color and depth first.
- Translucent actors use source-alpha blending and preserve opaque depth.
- Surface edges and explicit line cells render in the edge stage with actor
  color and width.
- Point, cell, or field arrays may supply scalar values. Direct three/four
  component arrays map as RGB/RGBA. Cell colors are deterministically converted
  to shared point colors unless a later backend supports true flat attributes.
- Up to six world-space clipping planes are shared by color and ID shaders.
- Lookup-table NaN, below-range, and above-range colors remain the canonical
  special-value policy.

## Window and context lifecycle

Windows report Created, Initialized, Visible, Offscreen, or Finalized state.
Finalization releases all device resources before destroying the context;
initialization after finalization recreates them from retained scene objects.
Hidden offscreen windows use the same device semantics as onscreen windows and
support RGBA/depth readback plus binary PPM output. A render window can also be
created as an owned native child of a host window while the host keeps control
of its event loop.

## Coordinate and selection contract

Coordinate conversion is always scoped to one renderer and its normalized
viewport. The conversion chain is world to view, view to NDC, NDC to display,
and display to a world ray.

Hardware picking renders a depth-tested integer identity into RGBA8: eight bits
identify the actor and 24 bits identify the rendered triangle. The result is
resolved through `FVizOriginalCellIds` and `FVizOriginalFaceIds` when present.
The pass supports up to 255 actors and 16,777,215 triangles per actor; callers
receive `FVIZ_ERROR_NOT_SUPPORTED` on the legacy OpenGL fallback.

## Resource validity

GPU geometry, colors, edges, and selection data are invalidated by composite
actor MTime, which includes mapper and dataset MTime. Context finalization
deletes buffers, vertex arrays, and programs explicitly. Recreated contexts
rebuild resources lazily on the next render.
