# Interaction, selection, and FEA widgets

FEAViz 0.14 keeps interaction host-driven. An application may use the native
window loop or inject `FVizInteractionEvent` values itself; neither observers
nor styles require ownership of the application's main loop.

## Event and timer contract

- Timers use stable nonzero IDs and an explicit monotonic `now_seconds` value.
- A late repeating timer fires once per polling call and advances to the first
  future interval. This prevents an event storm while preserving phase.
- One-shot timers remain resettable until explicitly destroyed.
- Mouse-button down captures the renderer under the pointer. Move and release
  events continue to target it even across viewport boundaries.
- Focus-out releases logical focus and capture. Win32 also translates native
  enter/leave, focus, expose, resize, and input messages into the same events.
- Observer and style changes are safe during nested dispatch. Newly added
  observers do not run in the dispatch that created them.

## Selection contract

Each record stores the actor, association, rendered ID, available original FEA
point/cell/face IDs, and the polygonal output MTime. Persistent records
re-resolve after recomputation by scanning the corresponding original-ID array.
Ephemeral records become invalid when their output changes. Refresh never
modifies source geometry.

`fviz_selection_probe` records the selected point or triangle-centroid world
position and up to four components from a named point/cell array. This fixed
payload keeps the public record versionable without borrowing array memory.

## Widgets

`FVizSelectionHighlight` creates independent transformed triangle geometry for
valid selected cells. It never changes source attributes, topology, or actor
properties. `FVizOrientationAxesWidget` owns a non-interactive overlay renderer,
tracks the target viewport and camera orientation, and can be enabled without
platform-native controls.
